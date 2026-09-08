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
#include <cstring>
#include <cmath>
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

    // ---------------------------------------------------------------- which preset it says it is
    // The state carried every knob but never the name of the preset they came from, so a restored
    // session played the right sound under the label "Init" and looked like a feature that did
    // not work. The name is stored, not the index: a pack added between two sessions renumbers
    // every preset behind it.
    {
        auto a = std::make_unique<AmbientSynthProcessor>();
        a->prepareToPlay(48000.0, 256);
        int which = -1;
        for (int i = 0; i < numPresets(); ++i) if (juce::String(preset(i).name) == "Cathedral Bell") which = i;
        check(which > 0, "the preset the round trip is built on exists");
        a->applySoundPreset(which);
        a->applyCosmosPreset(3);
        juce::MemoryBlock blob;
        a->getStateInformation(blob);

        auto b = std::make_unique<AmbientSynthProcessor>();
        b->prepareToPlay(48000.0, 256);
        b->setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        check(b->soundPresetIndex() == which, "a restored session still knows which sound preset it holds");
        check(b->cosmosPresetIndex() == 3, "a restored session still knows which cosmos preset it holds");

        // A state from before the names existed must not blank the boxes: no name means keep
        // whatever the default is, not "nothing selected".
        auto xml = juce::AudioProcessor::getXmlFromBinary(blob.getData(), static_cast<int>(blob.getSize()));
        check(xml != nullptr, "the state is readable as XML");
        xml->removeAttribute("soundPreset");
        xml->removeAttribute("cosmosPreset");
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary(*xml, old);
        auto c = std::make_unique<AmbientSynthProcessor>();
        c->prepareToPlay(48000.0, 256);
        c->setStateInformation(old.getData(), static_cast<int>(old.getSize()));
        check(c->soundPresetIndex() >= 0 && c->cosmosPresetIndex() >= 0, "a state without preset names leaves the boxes on their default");
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

    {   // A preset transition is a crossfade between two engines, not a parameter morph. The
        // morph flipped every switch at half way and every source restarted -- audibly a cut in
        // the middle. What is checked here is what the ear checks: that the level never jumps
        // from one block to the next while the change travels, that the displays know what is
        // travelling and when it has arrived, and that both engines stay finite throughout.
        const double sr = 48000.0; const int block = 256;
        auto p = std::make_unique<AmbientSynthProcessor>();
        p->prepareToPlay(sr, block);
        p->setMorphSelectSeconds(2.0f);
        // Two presets that share as little as possible: the first built-in bank, and the first
        // pack preset with a grain texture in its first slot if a library is loaded.
        int a = 1, b = -1;
        for (int i = 0; i < numPresets() && b < 0; ++i) {
            const char* st = preset(i).settings;
            if (st != nullptr && std::strstr(st, "src1_type=Texture") != nullptr) b = i;
        }
        if (b < 0) b = 2;
        juce::AudioBuffer<float> buf(2, block);
        juce::MidiBuffer midi;
        auto rmsOf = [&]() {
            double e = 0.0;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) { const float v = buf.getReadPointer(c)[i]; e += v * v; }
            return std::sqrt(e / (2.0 * block)) + 1e-9;
        };
        p->selectPreset(a, false);
        midi.addEvent(juce::MidiMessage::noteOn(1, 57, 0.8f), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.7f), 0);
        for (int k = 0; k < 400; ++k) { buf.clear(); p->processBlock(buf, midi); midi.clear(); }   // settle, chord held
        const double before = rmsOf();
        p->selectPreset(b, true);
        check(p->morphingTo() == b, "a travelling preset change names its destination");
        // The ramp waits for the incoming engine to be audible (up to a capped head start) and only
        // then runs its two seconds; the checks below count from where it starts.
        // Level is compared between neighbouring windows of sixteen blocks (85 ms): one block is
        // shorter than a period of the sub these presets carry, and the block RMS of a 40 Hz tone
        // swings by several dB on its own. A cut in the middle would still show as a step of many
        // dB between two 85 ms windows; the same measure on the arrived preset alone, sounding
        // steadily, is printed beside it as the yardstick for what "no step" looks like here.
        bool ok = true; double worstJump = 0.0, mid = -1.0, jumpFrom = 0.0, jumpTo = 0.0; int jumpAt = -1, head = 0;
        double win = 0.0, prevWin = -1.0;
        auto step = [&](int k, double& worst, int* at, double* from, double* to) {
            win += rmsOf() * rmsOf();
            if ((k + 1) % 16 != 0) return;
            const double now = std::sqrt(win / 16.0); win = 0.0;
            if (prevWin > 0.0 && std::max(now, prevWin) > 3e-4) {   // a step in the noise floor is not a step
                const double j = std::fabs(20.0 * std::log10(now / prevWin));
                if (j > worst) { worst = j; if (at) { *at = k; *from = prevWin; *to = now; } }
            }
            prevWin = now;
        };
        while (p->morphProgress() <= 0.0f && head < 2000) { buf.clear(); p->processBlock(buf, midi); ++head; }
        for (int k = 0; k < 416; ++k) {
            buf.clear(); p->processBlock(buf, midi);
            ok = ok && finite(buf);
            step(k, worstJump, &jumpAt, &jumpFrom, &jumpTo);
            if (k == 187) mid = p->morphProgress();
        }
        const double arrived = rmsOf();
        double steadyJump = 0.0; win = 0.0; prevWin = -1.0;
        for (int k = 0; k < 800; ++k) { buf.clear(); p->processBlock(buf, midi); step(k, steadyJump, nullptr, nullptr, nullptr); }
        const double later = rmsOf();
        std::printf("  [probe] transition %d -> %d: level before %.1f dBFS, head start %d blocks, worst 85 ms step %.2f dB at block %d (%.1f -> %.1f dBFS; steady preset alone %.2f dB), half way at %.2f, on arrival %.1f dBFS, 4 s later %.1f dBFS, voices %d\n",
                    a, b, 20.0 * std::log10(before), head, worstJump, jumpAt, 20.0 * std::log10(jumpFrom), 20.0 * std::log10(jumpTo), steadyJump, mid,
                    20.0 * std::log10(arrived), 20.0 * std::log10(later), p->engine().activeVoices());
        check(ok, "both engines stay finite through a transition");
        check(head < 2000, "the incoming engine speaks and the ramp starts");
        check(worstJump < 3.0, "a transition never steps in level from one 85 ms window to the next");
        check(mid > 0.3 && mid < 0.7, "half way through the time given, the fade is about half way");
        check(arrived > 1e-3, "when the old preset is gone the new one is already audible");
        check(p->morphingTo() < 0 && p->morphProgress() >= 1.0f, "when the time is up the change has arrived and nothing travels");
        p->releaseResources();
    }

    if (failures == 0) std::printf("hosttest: all checks passed\n");
    else std::printf("hosttest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
