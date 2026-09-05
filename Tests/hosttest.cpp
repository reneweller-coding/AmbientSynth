// AmbientSynth -- the host contract.
//
// The self test measures the instrument; this one measures the plugin around it. Everything here
// is something a host does and a synth has to survive: prepare and release at rates and block
// sizes nobody develops at, a state that has to come back exactly as it went out, programs
// changed while audio is running, an editor opened and closed under load, and parameters written
// from the message thread while the audio thread reads them. None of it was ever checked, because
// the standalone only ever does one of these things at a time and always at 48 kHz.
//
// This is not a replacement for pluginval (which exercises the VST3 wrapper itself, and which is
// worth running on the built plugin); it is the part that can live in the repository and run on
// every build.
#include "PluginProcessor.h"
#include <atomic>
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>

using namespace ambient;

namespace {
int failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}

bool finite(const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (!std::isfinite(b.getReadPointer(c)[i])) return false;
    return true;
}

// A block of audio with a couple of notes and some expression, which is what a host really sends.
void feed(AmbientSynthProcessor& p, juce::AudioBuffer<float>& buf, int blocks, bool notes)
{
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b) {
        midi.clear();
        if (notes && b == 1) {
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(2, 67, 0.6f), 4);
            midi.addEvent(juce::MidiMessage::channelPressureChange(2, 90), 8);
            midi.addEvent(juce::MidiMessage::pitchWheel(2, 9000), 12);
            midi.addEvent(juce::MidiMessage::controllerEvent(2, 74, 100), 16);
        }
        if (notes && b == blocks - 2) {
            midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            midi.addEvent(juce::MidiMessage::noteOff(2, 67), 0);
        }
        buf.clear();
        p.processBlock(buf, midi);
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // ---------------------------------------------------------------- rates and block sizes
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        for (int block : { 16, 64, 512, 2048 }) {
            auto p = std::make_unique<AmbientSynthProcessor>();
            p->setPlayConfigDetails(0, 2, sr, block);
            p->prepareToPlay(sr, block);
            juce::AudioBuffer<float> buf(2, block);
            feed(*p, buf, 12, true);
            const juce::String what = "renders finite at " + juce::String(static_cast<int>(sr)) + " Hz, block " + juce::String(block);
            check(finite(buf), what.toRawUTF8());
            // A block smaller or larger than the one it was prepared with: hosts do this.
            juce::AudioBuffer<float> odd(2, juce::jmax(1, block / 3));
            feed(*p, odd, 4, false);
            check(finite(odd), (what + " (short block)").toRawUTF8());
            p->releaseResources();
            // Prepared, released and prepared again is the sequence around every device change.
            p->prepareToPlay(sr, block);
            feed(*p, buf, 4, true);
            check(finite(buf), (what + " (after a device change)").toRawUTF8());
            p->releaseResources();
        }
    }

    // ---------------------------------------------------------------- state round trip
    {
        auto a = std::make_unique<AmbientSynthProcessor>();
        a->prepareToPlay(48000.0, 256);
        juce::Random rng(1234);
        // Move every parameter somewhere unusual, so a state that silently drops one shows up.
        for (const ParamDesc& d : paramTable())
            if (auto* par = a->apvts.getParameter(d.key)) par->setValueNotifyingHost(0.15f + 0.7f * rng.nextFloat());
        a->setMorphSlotFromCurrent(0);
        a->engine().setModMatrixText("lfo2>cutoff:0.4;env3>z_x:-0.2:macro_a");
        a->engine().setEnvShape(1, "0:0/2:1/5:-0.5!s1");
        juce::MemoryBlock blob;
        a->getStateInformation(blob);

        auto b = std::make_unique<AmbientSynthProcessor>();
        b->prepareToPlay(48000.0, 256);
        b->setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        int wrong = 0;
        for (const ParamDesc& d : paramTable()) {
            const float x = a->engine().getParam(d.id), y = b->engine().getParam(d.id);
            if (std::fabs(x - y) > 1.0e-4f * juce::jmax(1.0f, std::fabs(x))) { ++wrong; if (wrong < 4) std::printf("  state differs: %s %g vs %g\n", d.key, x, y); }
        }
        check(wrong == 0, "every parameter survives a state round trip");
        char m1[4096], m2[4096];
        a->engine().writeModMatrix(m1, sizeof(m1));
        b->engine().writeModMatrix(m2, sizeof(m2));
        check(juce::String(m1) == juce::String(m2), "the modulation matrix survives a state round trip");
        char e1[512], e2[512];
        a->engine().writeEnvShape(1, e1, sizeof(e1));
        b->engine().writeEnvShape(1, e2, sizeof(e2));
        check(juce::String(e1) == juce::String(e2), "an envelope shape survives a state round trip");
        float va[kNumParams], vb[kNumParams];
        a->engine().morphSlot(0, va);
        b->engine().morphSlot(0, vb);
        int mw = 0;
        for (int i = 0; i < kNumParams; ++i) if (std::fabs(va[i] - vb[i]) > 1.0e-4f * juce::jmax(1.0f, std::fabs(va[i]))) ++mw;
        check(mw == 0, "a morph snapshot survives a state round trip");
    }

    // ---------------------------------------------------------------- programs while playing
    {
        auto p = std::make_unique<AmbientSynthProcessor>();
        p->prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> buf(2, 256);
        bool ok = true;
        const int n = p->getNumPrograms();
        for (int i = 0; i < juce::jmin(n, 40); ++i) {
            p->setCurrentProgram(i * juce::jmax(1, n / 40));
            feed(*p, buf, 2, i % 4 == 0);
            if (!finite(buf)) ok = false;
        }
        check(ok, "programs can be changed while audio is running");
        p->releaseResources();
    }

    // ---------------------------------------------------------------- editor, and writes from
    // the message thread while the audio thread renders
    {
        auto p = std::make_unique<AmbientSynthProcessor>();
        p->prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> buf(2, 256);
        std::atomic<bool> stop{ false };
        std::atomic<bool> bad{ false };
        // The "audio thread": a separate thread, so the parameter writes below really are
        // concurrent rather than interleaved by the scheduler on one thread.
        std::thread audio([&] {
            juce::AudioBuffer<float> local(2, 256);
            juce::MidiBuffer midi;
            while (!stop.load()) {
                midi.clear();
                local.clear();
                p->processBlock(local, midi);
                if (!finite(local)) bad.store(true);
            }
        });
        juce::Random rng(99);
        for (int i = 0; i < 400; ++i) {
            const ParamDesc& d = paramTable()[static_cast<size_t>(rng.nextInt(kNumParams))];
            if (auto* par = p->apvts.getParameter(d.key)) par->setValueNotifyingHost(rng.nextFloat());
        }
        {   // An editor opened and thrown away twice while the audio thread keeps going.
            for (int i = 0; i < 2; ++i) {
                auto* ed = p->createEditorIfNeeded();
                check(ed != nullptr, "the editor can be created");
                if (ed != nullptr) {
                    // Lay it out and paint it. A plugin may not spin a message loop, so the
                    // editor's timers cannot be driven here; painting synchronously is what
                    // actually exercises every display, and it is where a bad pointer would show.
                    ed->setBounds(0, 0, 1600, 950);
                    const juce::Image shot = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
                    check(shot.isValid() && shot.getWidth() > 100, "the editor paints");
                    p->editorBeingDeleted(ed);
                    delete ed;
                }
            }
        }
        stop.store(true);
        audio.join();
        check(!bad.load(), "the output stays finite while parameters are written from another thread");
        p->releaseResources();
    }

    if (failures == 0) std::printf("hosttest: all checks passed\n");
    else std::printf("hosttest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
