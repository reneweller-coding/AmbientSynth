// AmbientSynth self test: parameter table, tuning, envelope, engine render,
// cluster brain, determinism. Exit code 0 = all passed.
#include "ambient/Engine.h"
#include "ambient/Params.h"
#include "ambient/Tuning.h"
#include "ambient/Dsp.h"
#include "ambient/Effects.h"
#include "ambient/Cosmos.h"
#include "ambient/Presets.h"
#include "ambient/Gesture.h"
#include "ambient/Osc.h"
#include "ambient/Menu.h"
#include "ambient/Recorder.h"
#include "ambient/Sources.h"
#include "ambient/WavFile.h"
#include "ambient/PresetMap.h"
#include "ambient/PresetMeta.h"
#include <thread>
#include <chrono>
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
#endif
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>

using namespace ambient;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } } while (0)

namespace {

struct Stats { double rms = 0; float peak = 0; long nonFinite = 0; };

Stats render(Engine& e, double seconds, std::vector<float>* capture = nullptr)
{
    const int block = 256;
    const int sr = static_cast<int>(e.sampleRate());
    std::vector<float> L(block), R(block);
    Stats s; double sq = 0; long n = 0;
    const long total = static_cast<long>(seconds * sr);
    for (long done = 0; done < total; done += block) {
        const int cnt = static_cast<int>(std::min<long>(block, total - done));
        e.process(L.data(), R.data(), cnt);
        for (int i = 0; i < cnt; ++i) {
            const float l = L[static_cast<size_t>(i)], r = R[static_cast<size_t>(i)];
            if (!std::isfinite(l) || !std::isfinite(r)) ++s.nonFinite;
            sq += 0.5 * (l * l + r * r); ++n;
            s.peak = std::max(s.peak, std::max(std::fabs(l), std::fabs(r)));
            if (capture) { capture->push_back(l); capture->push_back(r); }
        }
    }
    s.rms = std::sqrt(sq / std::max<long>(n, 1));
    return s;
}

void testParams()
{
    const auto& t = paramTable();
    for (int i = 0; i < kNumParams; ++i) {
        CHECK(static_cast<int>(t[static_cast<size_t>(i)].id) == i, "parameter table out of order");
        CHECK(t[static_cast<size_t>(i)].def >= t[static_cast<size_t>(i)].min && t[static_cast<size_t>(i)].def <= t[static_cast<size_t>(i)].max, "default outside range");
        for (int j = 0; j < i; ++j) CHECK(std::strcmp(t[static_cast<size_t>(i)].key, t[static_cast<size_t>(j)].key) != 0, "duplicate parameter key");
    }
}

void testTuning()
{
    for (int i = 0; i < kNumScaleChoices - 1; ++i) {
        FixedScale s;
        CHECK(makeBuiltinScale(i, s), "builtin scale missing");
        CHECK(s.ratios[0] == 1.0, "scale must start at 1/1");
        for (int d = 1; d < s.count; ++d) CHECK(s.ratios[d] > s.ratios[d - 1] && s.ratios[d] < s.period, "scale not ascending / exceeds period");
    }
    FixedScale tet;
    makeBuiltinScale(0, tet);
    CHECK(std::fabs(scaleFrequency(tet, 69, 60, 440.0) - 440.0) < 1e-9, "12-TET A4");
    CHECK(std::fabs(scaleFrequency(tet, 57, 60, 440.0) - 220.0) < 1e-9, "12-TET A3");
    CHECK(std::fabs(scaleFrequency(tet, 60, 60, 440.0) - 261.6255653) < 1e-3, "12-TET C4");

    FixedScale ji;
    makeBuiltinScale(1, ji);   // 7-note JI major
    CHECK(ji.count == 7, "JI major has 7 degrees");
    // Consecutive mapping: 7 keys per octave.
    const double c = scaleFrequency(ji, 60, 60, 440.0, false);
    CHECK(std::fabs(scaleFrequency(ji, 64, 60, 440.0, false) / c - 1.5) < 1e-9, "degree 4 of JI major is 3/2");
    CHECK(std::fabs(scaleFrequency(ji, 67, 60, 440.0, false) / c - 2.0) < 1e-9, "7 keys up = octave");
    CHECK(std::fabs(scaleFrequency(ji, 53, 60, 440.0, false) / c - 0.5) < 1e-9, "7 keys down = octave down");
    // Snapped mapping: the familiar keys land on the nearest JI degree.
    const double cs = scaleFrequency(ji, 60, 60, 440.0, true);
    CHECK(std::fabs(cs - c) < 1e-9, "root identical in both mappings");
    CHECK(std::fabs(scaleFrequency(ji, 64, 60, 440.0, true) / cs - 1.25) < 1e-9, "E key snaps to 5/4");
    CHECK(std::fabs(scaleFrequency(ji, 67, 60, 440.0, true) / cs - 1.5) < 1e-9, "G key snaps to 3/2");
    CHECK(std::fabs(scaleFrequency(ji, 72, 60, 440.0, true) / cs - 2.0) < 1e-9, "12 keys up = octave");
    CHECK(std::fabs(scaleFrequency(ji, 48, 60, 440.0, true) / cs - 0.5) < 1e-9, "12 keys down = octave down");
    CHECK(std::fabs(scaleFrequency(ji, 71, 60, 440.0, true) / cs - 1.875) < 1e-9, "B key snaps to 15/8");
    FixedScale bp;
    makeBuiltinScale(9, bp);   // Bohlen-Pierce: period 3, snapping must fall back to consecutive mapping
    const double b0 = scaleFrequency(bp, 60, 60, 440.0, true);
    CHECK(std::fabs(scaleFrequency(bp, 73, 60, 440.0, true) / b0 - 3.0) < 1e-9, "BP: 13 keys = tritave");

    const char* scl = "! test.scl\nTest scale\n 5\n 9/8\n 5/4\n 3/2\n 884.35871\n 2/1\n";
    FixedScale user;
    CHECK(parseScala(scl, user), "parse scala");
    CHECK(user.count == 5 && user.period == 2.0, "scala count/period");
    CHECK(std::fabs(user.ratios[1] - 1.125) < 1e-12, "scala ratio 9/8");
    CHECK(std::fabs(user.ratios[4] - 5.0 / 3.0) < 1e-5, "scala cents entry");
    CHECK(!parseScala("garbage", user), "reject malformed scala");

    CHECK(intervalConsonance(1.0) == 1.0, "unison consonance 1");
    CHECK(intervalConsonance(1.5) > intervalConsonance(1.25), "fifth more consonant than third");
    CHECK(intervalConsonance(1.25) > intervalConsonance(16.0 / 15.0), "third more consonant than semitone");
    CHECK(intervalConsonance(2.0) == 1.0, "octave consonance 1");
}

void testEnvelope()
{
    Envelope env;
    env.setSampleRate(48000.0);
    env.setTimes(0.1f, 0.1f, 0.5f, 0.2f);
    env.noteOn();
    float lv = 0.0f;
    for (int i = 0; i < 4800; ++i) lv = env.process();
    CHECK(lv >= 0.99f || env.stage() != Envelope::Stage::Attack, "attack reaches 1 after attack time");
    for (int i = 0; i < 9600; ++i) lv = env.process();
    CHECK(std::fabs(lv - 0.5f) < 0.01f, "decay settles on sustain");
    env.noteOff();
    for (int i = 0; i < 48000; ++i) lv = env.process();
    CHECK(!env.isActive(), "release ends");
}

void testEngineMidi()
{
    Engine e;
    e.setParam(ParamId::BrainOn, 0.0f);
    e.setParam(ParamId::Attack, 1.0f);
    e.setParam(ParamId::Release, 2.0f);
    e.setParam(ParamId::FarDecay, 1.0f);
    e.setParam(ParamId::NearDecay, 0.5f);
    e.setParam(ParamId::DelayFeedback, 0.0f);
    e.prepare(48000.0, 256);
    Stats silence = render(e, 0.5);
    CHECK(silence.rms < 1e-6, "silent before any note");
    e.noteOn(57, 0.8f);
    Stats a = render(e, 0.3);
    Stats b = render(e, 2.0);
    CHECK(b.rms > 0.02, "note produces sound");
    CHECK(a.rms < b.rms, "attack ramps up");
    CHECK(b.peak <= 1.0f, "soft clipper keeps peak <= 1");
    CHECK(b.nonFinite == 0, "no NaN/inf");
    CHECK(e.activeVoices() == 1, "one voice");
    e.noteOff(57);
    render(e, 6.0);
    Stats tail = render(e, 1.0);
    CHECK(tail.rms < 1e-3, "silent after release + reverb tail");
    CHECK(e.activeVoices() == 0, "voice freed after release");

    // Voice stealing: 40 notes, still at most kMaxVoices, no NaN.
    for (int n = 30; n < 70; ++n) e.noteOn(n, 0.7f);
    Stats many = render(e, 0.5);
    CHECK(e.activeVoices() <= Engine::kMaxVoices, "voice count capped");
    CHECK(many.nonFinite == 0 && many.peak <= 1.0f, "stable under full polyphony");
    e.allNotesOff();
}

void testBrain()
{
    Engine e;
    e.setParam(ParamId::BrainOn, 1.0f);
    e.setParam(ParamId::BrainRate, 3.0f);
    e.setParam(ParamId::BrainHoldMin, 8.0f);
    e.setParam(ParamId::BrainHoldMax, 15.0f);
    e.setParam(ParamId::BrainDensity, 4.0f);
    e.setParam(ParamId::Attack, 0.5f);
    e.prepare(48000.0, 256);
    render(e, 20.0);
    CHECK(e.activeVoices() >= 2, "brain has started several notes after 20 s");
    Stats s = render(e, 5.0);
    CHECK(s.rms > 0.01 && s.nonFinite == 0, "brain output audible and finite");
    bool notes[128];
    e.soundingNotes(notes);
    int count = 0; for (bool b : notes) count += b ? 1 : 0;
    CHECK(count >= 2, "sounding-note mask populated");
    e.setParam(ParamId::BrainOn, 0.0f);
    render(e, 30.0);
    CHECK(e.activeVoices() == 0, "all brain notes released when switched off");
}

void testDeterminism()
{
    std::vector<float> a, b;
    for (int pass = 0; pass < 2; ++pass) {
        Engine e;
        e.setParam(ParamId::BrainRate, 2.0f);
        e.setParam(ParamId::Seed, 7.0f);
        e.prepare(44100.0, 128);
        render(e, 4.0, pass == 0 ? &a : &b);
    }
    CHECK(a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0, "same seed -> identical output");
}

void testUserScale()
{
    Engine e;
    e.prepare(48000.0, 256);
    FixedScale s;
    CHECK(parseScala("Fifths only\n 2\n 3/2\n 2/1\n", s), "parse 2-degree scale");
    e.setUserScale(s);
    e.setParam(ParamId::Scale, static_cast<float>(kNumScaleChoices - 1));
    e.setParam(ParamId::KeyMap, 1.0f);   // consecutive degrees
    e.setParam(ParamId::RootNote, 0.0f);
    render(e, 0.05);   // applies pending scale + params
    const double c = e.frequencyOf(60);
    CHECK(std::fabs(e.frequencyOf(61) / c - 1.5) < 1e-9, "user scale degree 1 = 3/2");
    CHECK(std::fabs(e.frequencyOf(62) / c - 2.0) < 1e-9, "user scale wraps after 2 degrees");
}

void testDelay()
{
    StereoDelay d;
    d.prepare(48000.0);
    d.set(0.1f, 0.15f, 0.0f, 0.0f, 0.0f);
    std::vector<float> inL(48000, 0.0f), inR(48000, 0.0f), wl(48000), wr(48000);
    inL[0] = 1.0f; inR[0] = 1.0f;
    d.process(inL.data(), inR.data(), wl.data(), wr.data(), 48000);
    int pl = 0, pr = 0;
    for (int i = 1; i < 48000; ++i) { if (std::fabs(wl[static_cast<size_t>(i)]) > std::fabs(wl[static_cast<size_t>(pl)])) pl = i; if (std::fabs(wr[static_cast<size_t>(i)]) > std::fabs(wr[static_cast<size_t>(pr)])) pr = i; }
    CHECK(std::abs(pl - 4800) < 24, "left echo at 100 ms");
    CHECK(std::abs(pr - 7200) < 24, "right echo at 150 ms (asymmetric)");
    float echo = 0.0f;   // the impulse is spread over two samples by the fractional read
    for (int i = pl - 2; i <= pl + 2; ++i) echo += std::fabs(wl[static_cast<size_t>(i)]);
    CHECK(echo > 0.9f, "echo level");
}

void testMidSide()
{
    auto sideRatio = [](float hz) {
        MidSide ms; ms.prepare(48000.0); ms.set(150.0f, 0.0f, 1.0f);
        std::vector<float> L(48000), R(48000);
        for (int i = 0; i < 48000; ++i) { const float s = std::sin(kTwoPi * hz * i / 48000.0f); L[static_cast<size_t>(i)] = s; R[static_cast<size_t>(i)] = -s; }
        ms.process(L.data(), R.data(), 48000);
        double sq = 0; for (int i = 9600; i < 48000; ++i) { const float s = 0.5f * (L[static_cast<size_t>(i)] - R[static_cast<size_t>(i)]); sq += s * s; }
        return std::sqrt(sq / (48000 - 9600)) / 0.7071;
    };
    CHECK(sideRatio(50.0f) < 0.25, "side content at 50 Hz collapses to mono (> 12 dB down)");
    CHECK(sideRatio(2000.0f) > 0.9, "side content at 2 kHz passes");
    MidSide ms; ms.prepare(48000.0); ms.set(150.0f, 6.0f, 1.0f);
    std::vector<float> L(48000), R(48000);
    for (int i = 0; i < 48000; ++i) { const float s = std::sin(kTwoPi * 3000.0f * i / 48000.0f); L[static_cast<size_t>(i)] = s; R[static_cast<size_t>(i)] = -s; }
    ms.process(L.data(), R.data(), 48000);
    double sq = 0; for (int i = 9600; i < 48000; ++i) { const float s = 0.5f * (L[static_cast<size_t>(i)] - R[static_cast<size_t>(i)]); sq += s * s; }
    const double lift = 20.0 * std::log10(std::sqrt(sq / (48000 - 9600)) / 0.7071);
    CHECK(lift > 4.0 && lift < 7.0, "side air lifts 3 kHz by about 6 dB");
}

void testPresets()
{
    CHECK(numPresets() == 136, "exactly 136 presets");
    for (int p = 0; p < numPresets(); ++p)
        for (int q = 0; q < p; ++q) CHECK(std::strcmp(preset(p).name, preset(q).name) != 0, "preset names unique");
    for (int p = 0; p < numPresets(); ++p) {
        bool touched[kNumParams] = {};
        const bool ok = applyPreset(preset(p), [&](ParamId id, float) { touched[static_cast<int>(id)] = true; });
        CHECK(ok, "preset settings all refer to known parameters");
        int count = 0; for (bool t : touched) count += t ? 1 : 0;
        CHECK(count == kNumParams - 15, "preset sets every parameter except morph controls, the eight macros and the map cursor");
    }
    Engine e;
    CHECK(e.applyPreset(1), "apply preset 1");
    CHECK(e.getParam(ParamId::BrainDensity) == 6.0f, "Sleep Concert density");
    CHECK(e.applyPreset(2), "apply preset 2");
    CHECK(e.getParam(ParamId::Scale) == 6.0f, "Glass Cathedral selects Harmonic 8-16 by name");
    CHECK(e.getParam(ParamId::RootNote) == 4.0f, "root E by name");
    CHECK(!e.applyPreset(999), "out of range preset rejected");
}

void testSpace()
{
    Engine e;
    e.setParam(ParamId::BrainRate, 2.0f);
    e.setParam(ParamId::Attack, 0.5f);
    e.setParam(ParamId::Depth, 1.0f);
    e.prepare(48000.0, 256);
    Stats s = render(e, 15.0);
    CHECK(s.nonFinite == 0 && s.rms > 0.01, "spatial engine renders");
    bool notes[128]; e.soundingNotes(notes);
    int seen = 0, farCount = 0;   // ("far" is a Windows macro)
    for (int n = 0; n < 128; ++n) if (notes[n]) { const float d = e.noteDistance(n); CHECK(d >= 0.0f && d <= 1.0f, "distance in range"); ++seen; if (d > 0.5f) ++farCount; }
    CHECK(seen >= 2, "several notes sounding");
    CHECK(farCount >= 1, "at least one note in the background plane");
    // MIDI notes take the keys depth.
    e.setParam(ParamId::KeysDepth, 0.9f);
    render(e, 0.1);
    e.noteOn(100, 0.5f);
    render(e, 0.1);
    CHECK(std::fabs(e.noteDistance(100) - 0.9f) < 1e-5f, "keys depth applied to MIDI note");
}

double goertzel(const float* x, int n, double hz, double sr)
{
    const double w = 2.0 * 3.14159265358979 * hz / sr;
    const double c = 2.0 * std::cos(w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

// Source slots: wavetable of spectra, FM pair, texture grains; WAV reader round trip.
void testSources()
{
    const int sr = 48000;
    auto quietVoice = [](Engine& e) {   // main bank silent (Source 1 level 0), everything dry
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::OscLevel, 0.0f);
        e.setParam(ParamId::Attack, 0.2f);
        e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 9.0f);   // 12-TET, A: key 57 = 220 Hz
        e.setParam(ParamId::Partials, 1.0f); e.setParam(ParamId::Unison, 1.0f);
        e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f); e.setParam(ParamId::Shimmer, 0.0f);
        e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::Cutoff, 18000.0f); e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
        e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
        e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f); e.setParam(ParamId::Spread, 0.0f);
        e.setParam(ParamId::Src2Pan, 0.0f); e.setParam(ParamId::Src2PosDrift, 0.0f); e.setParam(ParamId::Src2Level, 1.0f);
    };
    auto monoSecond = [&](Engine& e, int note, std::vector<float>& mono) {
        e.prepare(sr, 256);
        e.noteOn(note, 0.8f);
        std::vector<float> cap;
        render(e, 2.0, &cap);
        mono.resize(sr);
        for (int i = 0; i < sr; ++i) mono[static_cast<size_t>(i)] = cap[static_cast<size_t>((sr + i) * 2)];
    };
    {   // Wavetable Classic: position 0 is a sine, position 0.5 a saw (second partial at half).
        std::vector<float> m;
        Engine sine; quietVoice(sine);
        sine.setParam(ParamId::Src2Type, 1.0f); sine.setParam(ParamId::Src2Table, 0.0f); sine.setParam(ParamId::Src2Position, 0.0f);
        monoSecond(sine, 57, m);
        const double s1 = goertzel(m.data(), sr, 220.0, sr), s2 = goertzel(m.data(), sr, 440.0, sr);
        CHECK(s1 > 100.0 * s2, "wavetable position 0 (sine) has no second partial");
        Engine saw; quietVoice(saw);
        saw.setParam(ParamId::Src2Type, 1.0f); saw.setParam(ParamId::Src2Table, 0.0f); saw.setParam(ParamId::Src2Position, 0.5f);
        monoSecond(saw, 57, m);
        const double w1 = goertzel(m.data(), sr, 220.0, sr), w2 = goertzel(m.data(), sr, 440.0, sr), w3 = goertzel(m.data(), sr, 660.0, sr);
        CHECK(w2 > 0.15 * w1 && w2 < 0.4 * w1 && w3 > 0.05 * w1, "wavetable position 0.5 (saw) has 1/h partials");
        // Ratio 3/2 and octave +1 move the slot: 220 * 1.5 * 2 = 660 Hz.
        Engine moved; quietVoice(moved);
        moved.setParam(ParamId::Src2Type, 1.0f); moved.setParam(ParamId::Src2Position, 0.0f);
        moved.setParam(ParamId::Src2Ratio, 5.0f); moved.setParam(ParamId::Src2Octave, 1.0f);
        monoSecond(moved, 57, m);
        CHECK(goertzel(m.data(), sr, 660.0, sr) > 50.0 * goertzel(m.data(), sr, 440.0, sr), "slot ratio 3/2 and octave +1 land at 660 Hz");
    }
    {   // FM: index 0 is a pure carrier, index 3 has sidebands at carrier +- modulator.
        std::vector<float> m;
        Engine pure; quietVoice(pure);
        pure.setParam(ParamId::Src2Type, 2.0f); pure.setParam(ParamId::Src2FmIndex, 0.0f); pure.setParam(ParamId::Src2FmRatio, 2.0f);
        monoSecond(pure, 57, m);
        const double c0 = goertzel(m.data(), sr, 220.0, sr), sb0 = goertzel(m.data(), sr, 660.0, sr);
        CHECK(c0 > 100.0 * sb0, "FM index 0 is a plain sine");
        Engine fm; quietVoice(fm);
        fm.setParam(ParamId::Src2Type, 2.0f); fm.setParam(ParamId::Src2FmIndex, 3.0f); fm.setParam(ParamId::Src2FmRatio, 2.0f);
        monoSecond(fm, 57, m);
        const double c1 = goertzel(m.data(), sr, 220.0, sr), sb1 = goertzel(m.data(), sr, 660.0, sr);
        CHECK(sb1 > 0.1 * c1 && sb1 > 100.0 * sb0, "FM index 3 puts energy on the sidebands");
        // Ratio 1 with a deep index carries a DC term (J1 of the index); the slot must block it.
        Engine dc; quietVoice(dc);
        dc.setParam(ParamId::Src2Type, 2.0f); dc.setParam(ParamId::Src2FmIndex, 1.5f); dc.setParam(ParamId::Src2FmRatio, 1.0f);
        monoSecond(dc, 57, m);
        double mean = 0; for (float v : m) mean += v; mean /= static_cast<double>(m.size());
        CHECK(std::fabs(mean) < 0.002, "FM at ratio 1 has no DC offset");
    }
    {   // Texture: a 440 Hz sample; Free plays it as is, Note pitches it to the key (A3 = 220 with base C4 = 261.6 -> 370 Hz).
        std::vector<float> sample(sr * 2);
        for (int i = 0; i < sr * 2; ++i) sample[static_cast<size_t>(i)] = 0.5f * std::sin(kTwoPi * 440.0f * i / sr);
        std::vector<float> m;
        Engine freeT; quietVoice(freeT);
        freeT.setParam(ParamId::Src2Type, 3.0f); freeT.setParam(ParamId::Src2Follow, 0.0f); freeT.setParam(ParamId::Src2Density, 20.0f);
        freeT.setTexture(sample.data(), static_cast<int>(sample.size()), sr);
        monoSecond(freeT, 57, m);
        const double f440 = goertzel(m.data(), sr, 440.0, sr), f370 = goertzel(m.data(), sr, 370.0, sr);
        CHECK(f440 > 20.0 * f370 && f440 > 1.0, "texture Free plays the sample at its own pitch");
        Engine noteT; quietVoice(noteT);
        noteT.setParam(ParamId::Src2Type, 3.0f); noteT.setParam(ParamId::Src2Follow, 1.0f); noteT.setParam(ParamId::Src2Density, 20.0f);
        noteT.setTexture(sample.data(), static_cast<int>(sample.size()), sr);
        monoSecond(noteT, 57, m);
        const double n440 = goertzel(m.data(), sr, 440.0, sr), n370 = goertzel(m.data(), sr, 370.0, sr);
        CHECK(n370 > 20.0 * n440, "texture Note pitches the sample to the key");
        Engine none; quietVoice(none);
        none.setParam(ParamId::Src2Type, 3.0f);
        monoSecond(none, 57, m);
        double sq = 0; for (float v : m) sq += v * v;
        CHECK(sq < 1e-9, "texture slot without a loaded texture is silent");
    }
    {   // User wavetable from frames: frame 0 sine, frame 1 square -> analysed, position 1 shows odd partials.
        std::vector<float> frames(4096);
        for (int i = 0; i < 2048; ++i) { frames[static_cast<size_t>(i)] = std::sin(kTwoPi * i / 2048.0f); frames[static_cast<size_t>(2048 + i)] = i < 1024 ? 1.0f : -1.0f; }
        Wavetable t;
        CHECK(t.analyse(frames.data(), 4096) && t.frames == 2, "wavetable analysis finds two frames");
        float spec[kTablePartials]; t.spectrumAt(1.0f, spec);
        CHECK(spec[0] > 0.5f && spec[2] > 0.25f * spec[0] && spec[1] < 0.05f * spec[0], "square frame has odd partials only");
        t.spectrumAt(0.0f, spec);
        CHECK(spec[0] > 0.9f && spec[1] < 0.02f, "sine frame is a single partial");
    }
    {   // Base pitch from a TextureGen file name.
        CHECK(std::fabs(baseHzFromName("C:\\Textures\\bowed_metal_sao_123_A3.wav") - 220.0) < 1e-6, "_A3 suffix -> 220 Hz");
        CHECK(std::fabs(baseHzFromName("flute-C#4.wav") - 277.1826) < 1e-3, "C#4 -> 277.18 Hz");
        CHECK(std::fabs(baseHzFromName("drone Bb2.wav") - 116.5409) < 1e-3, "Bb2 -> 116.54 Hz");
        CHECK(baseHzFromName("rain_on_roof_sao_77.wav") == 0.0 && baseHzFromName("x_G.wav") == 0.0 && baseHzFromName("") == 0.0, "no note token -> 0");
    }
    {   // WAV reader: write with the recorder, read back.
        const char* path = "selftest_wav_roundtrip.wav";
        {
            WavRecorder rec;
            CHECK(rec.start(path, sr, 2), "recorder starts");
            std::vector<float> L(4800), R(4800);
            for (int i = 0; i < 4800; ++i) { L[static_cast<size_t>(i)] = 0.25f; R[static_cast<size_t>(i)] = 0.75f; }
            rec.write(L.data(), R.data(), 4800);
            rec.stop();
        }
        std::vector<float> mono; int rate = 0;
        CHECK(readWavMono(path, mono, rate) && rate == sr && mono.size() == 4800 && std::fabs(mono[100] - 0.5f) < 1e-6f, "WAV reader mixes a float file to mono");
        std::remove(path);
    }
}

// Preset map: neighbours, blend, and the engine's map mode.
void testPresetMap()
{
    PresetMap::warmup();
    CHECK(PresetMap::ready(), "preset map warmed up");
    CHECK(numPresetMeta() == 0 || numPresetMeta() == numPresets(), "preset meta covers every preset (or is the stub)");
    if (numPresetMeta() == 0) { std::printf("  (preset map not measured yet: run Tools/preset_map.py)\n"); return; }
    for (int i = 0; i < numPresetMeta(); ++i) {
        const PresetMeta& m = presetMeta(i);
        CHECK(m.x >= 0.0f && m.x <= 1.0f && m.y >= 0.0f && m.y <= 1.0f, "map position inside the plane");
        CHECK(m.family >= 0 && m.family < numPresetFamilies(), "family index valid");
    }
    {   // On a preset's point the blend is that preset (within the radius the others fade out).
        const int p = 1;   // Sleep Concert
        const PresetMeta& m = presetMeta(p);
        const PresetMap::Blend b = PresetMap::neighbours(m.x, m.y, 0.005f);
        CHECK(b.count > 0 && b.index[0] == p && b.weight[0] > 0.99f, "cursor on a point selects that preset");
        float v[kNumParams];
        PresetMap::blend(b, v);
        const float* pv = PresetMap::presetValues(p);
        bool same = true;
        for (const ParamDesc& d : paramTable()) if (!isMapParam(d.id) && std::fabs(v[static_cast<int>(d.id)] - pv[static_cast<int>(d.id)]) > 1e-3f * std::max(1.0f, std::fabs(pv[static_cast<int>(d.id)]))) same = false;
        CHECK(same, "blend on a point reproduces the preset's parameters");
    }
    {   // Between two points a float parameter lies between the two values.
        int a = -1, bIdx = -1;
        for (int i = 0; i < numPresets() && bIdx < 0; ++i) for (int j = i + 1; j < numPresets(); ++j)
            if (std::fabs(PresetMap::presetValues(i)[static_cast<int>(ParamId::FarDecay)] - PresetMap::presetValues(j)[static_cast<int>(ParamId::FarDecay)]) > 20.0f) { a = i; bIdx = j; break; }
        CHECK(a >= 0, "two presets with different far decay exist");
        if (a >= 0) {
            PresetMap::Blend b; b.count = 2; b.index[0] = a; b.index[1] = bIdx; b.weight[0] = b.weight[1] = 0.5f;
            float v[kNumParams]; PresetMap::blend(b, v);
            const float fa = PresetMap::presetValues(a)[static_cast<int>(ParamId::FarDecay)], fb = PresetMap::presetValues(bIdx)[static_cast<int>(ParamId::FarDecay)];
            const float f = v[static_cast<int>(ParamId::FarDecay)];
            CHECK(f > std::min(fa, fb) && f < std::max(fa, fb), "half-way blend lies between the two presets");
        }
    }
    {   // Engine map mode: the effective parameters glide to the blend and stay put when the map is left.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.prepare(48000.0, 256);
        const int p = 7;   // Distant Storm: depth 1, far decay 60
        const PresetMeta& m = presetMeta(p);
        const float before = e.effectiveParam(ParamId::FarDecay);
        e.setParam(ParamId::MorphGlide, 0.5f);
        e.setParam(ParamId::MapActive, 1.0f); e.setParam(ParamId::MapX, m.x); e.setParam(ParamId::MapY, m.y); e.setParam(ParamId::MapRadius, 0.005f);
        render(e, 0.05);
        const float early = e.effectiveParam(ParamId::FarDecay);
        render(e, 3.0);
        const float target = PresetMap::presetValues(p)[static_cast<int>(ParamId::FarDecay)];
        const float late = e.effectiveParam(ParamId::FarDecay);
        CHECK(e.mapActive(), "map mode active");
        CHECK(std::fabs(early - before) < std::fabs(late - before), "map glides rather than jumps");
        CHECK(std::fabs(late - target) < 0.05f * std::max(1.0f, target), "after the glide the engine plays the preset under the cursor");
        CHECK(std::fabs(e.blendValue(ParamId::FarDecay) - late) < 1e-4f, "blendValue reports the gliding value");
        e.setParam(ParamId::MapActive, 0.0f);
        render(e, 0.05);
        CHECK(!e.mapActive(), "map mode off again");
    }
}

// Stack: strands at pure ratios; Rate Wander: the movement rates themselves move.
void testStackAndWander()
{
    const int sr = 48000;
    {   // Major stack, three strands, one partial each: A3 gives 220 + 330 + 275 Hz and nothing else.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Attack, 0.2f);
        e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 9.0f);   // 12-TET, A: key 57 = 220 Hz
        e.setParam(ParamId::Partials, 1.0f);
        e.setParam(ParamId::Unison, 3.0f); e.setParam(ParamId::Stack, 3.0f);      // Major: 1, 3/2, 5/4
        e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f); e.setParam(ParamId::Shimmer, 0.0f);
        e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::Cutoff, 18000.0f); e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
        e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
        e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f); e.setParam(ParamId::Spread, 0.0f);
        e.prepare(sr, 256);
        e.noteOn(57, 0.8f);
        std::vector<float> cap;
        render(e, 2.0, &cap);
        std::vector<float> mono(sr);
        for (int i = 0; i < sr; ++i) mono[static_cast<size_t>(i)] = cap[static_cast<size_t>((sr + i) * 2)];
        const double p220 = goertzel(mono.data(), sr, 220.0, sr), p330 = goertzel(mono.data(), sr, 330.0, sr), p275 = goertzel(mono.data(), sr, 275.0, sr);
        const double p262 = goertzel(mono.data(), sr, 261.6, sr), p440 = goertzel(mono.data(), sr, 440.0, sr);
        CHECK(p220 > 20.0 * p262 && p330 > 20.0 * p262 && p275 > 20.0 * p262, "major stack puts the strands at 1, 3/2 and 5/4");
        CHECK(p220 > 20.0 * p440, "one partial per strand: no octave in the stack");
    }
    {   // Rate wander changes the movement (different output), stays finite, and off means unchanged rates.
        auto capture = [&](float wander, int seed) {
            Engine e;
            e.setParam(ParamId::RateWander, wander); e.setParam(ParamId::Seed, static_cast<float>(seed));
            e.prepare(sr, 256);
            std::vector<float> cap;
            Stats s = render(e, 20.0, &cap);
            CHECK(s.nonFinite == 0, "rate wander renders finite");
            return cap;
        };
        const auto a = capture(0.0f, 5), b = capture(1.0f, 5);
        double diff = 0; for (size_t i = 0; i < a.size(); ++i) diff += std::fabs(a[i] - b[i]);
        CHECK(diff > 1.0, "rate wander changes the movement");
    }
}

// The feedback loop: mix -> (tone, drive, throttle) -> near bus and/or partial phase modulation.
void testFeedback()
{
    const int sr = 48000;
    {   // Silence stays silence, a held drone with a hot loop stays bounded and gets fuller.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::FeedbackBus, 1.0f); e.setParam(ParamId::FeedbackDrive, 1.0f);
        e.prepare(sr, 256);
        Stats quiet = render(e, 3.0);
        CHECK(quiet.peak == 0.0f, "feedback alone makes no sound");
        e.noteOn(57, 0.8f);
        Stats hot = render(e, 20.0);
        CHECK(hot.nonFinite == 0 && hot.peak < 0.95f, "bus feedback at 1.0 stays bounded (throttled)");
        Engine d;
        d.setParam(ParamId::BrainOn, 0.0f);
        d.prepare(sr, 256);
        d.noteOn(57, 0.8f);
        Stats dry = render(d, 20.0);
        CHECK(hot.rms > dry.rms, "the loop adds energy to the drone");
    }
    {   // Phase modulation: with the loop on the pitch, energy leaves the exact harmonics.
        auto harmonicShare = [&](float fm) {
            Engine e;
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Attack, 0.2f);
            e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 0.0f);
            e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::Shimmer, 0.0f);
            e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f);
            e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f); e.setParam(ParamId::Cutoff, 18000.0f);
            e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
            e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f);
            e.setParam(ParamId::FeedbackFm, fm); e.setParam(ParamId::FeedbackTone, 8000.0f);
            e.prepare(sr, 256);
            e.noteOn(60, 0.8f);
            std::vector<float> cap;
            render(e, 3.0, &cap);
            std::vector<float> mono(sr);
            double total = 0;
            for (int i = 0; i < sr; ++i) { mono[static_cast<size_t>(i)] = cap[static_cast<size_t>((2 * sr + i) * 2)]; total += mono[static_cast<size_t>(i)] * mono[static_cast<size_t>(i)]; }
            double harm = 0;
            for (int h = 1; h <= 32; ++h) harm += goertzel(mono.data(), sr, 261.6256 * h, sr);
            return harm / (total * sr + 1e-12);   // Goertzel power ~ N * energy at the bin
        };
        const double clean = harmonicShare(0.0f), modulated = harmonicShare(1.0f);
        CHECK(modulated < 0.7 * clean, "pitch feedback spreads energy away from the exact harmonics");
    }
}

// Rich's foreground/background carving inside the voice: presence bell, pad low cut,
// breathing distance, and the ghost-tone source of the foundation.
void testRichCarving()
{
    const int sr = 48000;
    auto pureVoice = [](Engine& e) {
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Attack, 0.2f);
        e.setParam(ParamId::Brightness, 1.0f); e.setParam(ParamId::Tilt, 0.3f); e.setParam(ParamId::Partials, 32.0f);
        e.setParam(ParamId::Cutoff, 18000.0f); e.setParam(ParamId::Resonance, 0.0f);
        e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 0.0f);   // 12-TET, C
        e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::Shimmer, 0.0f);
        e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f);
        e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
        e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
        e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f);
    };
    auto bandPower = [&](Engine& e, int note, double f0, int hFrom, int hTo) {
        e.prepare(sr, 256);
        e.noteOn(note, 0.8f);
        std::vector<float> cap;
        render(e, 2.0, &cap);
        std::vector<float> mono(sr);
        for (int i = 0; i < sr; ++i) mono[static_cast<size_t>(i)] = cap[static_cast<size_t>((sr + i) * 2)];
        double p = 0; for (int h = hFrom; h <= hTo; ++h) p += goertzel(mono.data(), sr, f0 * h, sr);
        return p;
    };
    {   // Presence: +6 dB bell lifts the 2-5 kHz partials of a near voice against its low ones.
        const double f0 = 261.6256;   // key 60
        Engine flat; pureVoice(flat);
        const double flatRatio = bandPower(flat, 60, f0, 8, 20) / bandPower(flat, 60, f0, 1, 4);
        Engine pres; pureVoice(pres); pres.setParam(ParamId::Presence, 6.0f);
        const double presRatio = bandPower(pres, 60, f0, 8, 20) / bandPower(pres, 60, f0, 1, 4);
        CHECK(presRatio > 1.8 * flatRatio, "presence lifts the 2-5 kHz band of a near voice");
        Engine farV; pureVoice(farV); farV.setParam(ParamId::Presence, 6.0f); farV.setParam(ParamId::KeysDepth, 1.0f);
        farV.setParam(ParamId::Cutoff, 18000.0f);
        Engine farFlat; pureVoice(farFlat); farFlat.setParam(ParamId::KeysDepth, 1.0f);
        const double farRatio = bandPower(farV, 60, f0, 8, 20) / bandPower(farV, 60, f0, 1, 4);
        const double farFlatRatio = bandPower(farFlat, 60, f0, 8, 20) / bandPower(farFlat, 60, f0, 1, 4);
        CHECK(farRatio < 1.15 * farFlatRatio, "presence does nothing on the far plane");
    }
    {   // Pad low cut: the fundamental of a C3 falls away below a 300 Hz cut, the third partial does not.
        const double f0 = 130.8128;   // key 48
        Engine full; pureVoice(full);
        const double fullRatio = bandPower(full, 48, f0, 1, 1) / bandPower(full, 48, f0, 3, 3);
        Engine cut; pureVoice(cut); cut.setParam(ParamId::PadLowCut, 300.0f);
        const double cutRatio = bandPower(cut, 48, f0, 1, 1) / bandPower(cut, 48, f0, 3, 3);
        CHECK(cutRatio < 0.2 * fullRatio, "pad low cut removes the fundamental below the cut (12 dB/oct)");
    }
    {   // Breath: the distance wanders, continuously; without breath it stands still.
        auto sweep = [&](float breath, float& spread, float& maxStep) {
            Engine e; pureVoice(e);
            e.setParam(ParamId::KeysDepth, 0.5f);
            e.setParam(ParamId::Breath, breath); e.setParam(ParamId::BreathRate, 0.2f);
            e.prepare(sr, 256);
            e.noteOn(60, 0.8f);
            float lo = 2.0f, hi = -1.0f, prev = -1.0f; maxStep = 0.0f;
            for (int k = 0; k < 150; ++k) {
                render(e, 0.1);
                const float d = e.noteDistance(60);
                lo = std::min(lo, d); hi = std::max(hi, d);
                if (prev >= 0.0f) maxStep = std::max(maxStep, std::fabs(d - prev));
                prev = d;
            }
            spread = hi - lo;
        };
        float spread = 0.0f, step = 0.0f;
        sweep(0.0f, spread, step);
        CHECK(spread < 1e-6f, "without breath the distance is fixed");
        sweep(1.0f, spread, step);
        CHECK(spread > 0.1f, "breath moves the voice's distance");
        CHECK(step < 0.05f, "breathing is continuous (no jump per 100 ms)");
    }
    {   // Ghost tone: with Source = Difference the sub doubles f2 - f1 of the two lowest voices,
        // folded into the root sub's octave. A3 (220) and D4 (4:3 = 293.33): 73.33 Hz -> 146.67 Hz.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::SubLevel, 0.8f); e.setParam(ParamId::SubBinaural, 0.0f); e.setParam(ParamId::SubGlide, 0.1f);
        e.setParam(ParamId::SubSource, 1.0f);
        e.setParam(ParamId::RootNote, 9.0f);   // A
        e.setParam(ParamId::Scale, 1.0f);      // JI Major (Ptolemy)
        e.setParam(ParamId::Air, 0.0f);
        e.prepare(sr, 256);
        e.noteOn(57, 0.6f); e.noteOn(62, 0.6f);
        std::vector<float> cap;
        render(e, 4.0, &cap);
        std::vector<float> L(sr);
        for (int i = 0; i < sr; ++i) L[static_cast<size_t>(i)] = cap[static_cast<size_t>((3 * sr + i) * 2)];
        const double pGhost = goertzel(L.data(), sr, 146.667, sr), pRoot = goertzel(L.data(), sr, 110.0, sr), pDiff = goertzel(L.data(), sr, 73.333, sr);
        CHECK(pGhost > 10.0 * pRoot && pGhost > 10.0 * pDiff, "foundation follows the folded difference tone of the two lowest voices");
        e.noteOff(62);
        e.setParam(ParamId::Release, 0.1f);
        cap.clear();
        render(e, 4.0, &cap);
        for (int i = 0; i < sr; ++i) L[static_cast<size_t>(i)] = cap[static_cast<size_t>((3 * sr + i) * 2)];
        const double pGhost2 = goertzel(L.data(), sr, 146.667, sr), pRoot2 = goertzel(L.data(), sr, 110.0, sr);
        CHECK(pRoot2 > 10.0 * pGhost2, "with one voice left the foundation falls back to the root");
    }
}

void testCosmos()
{
    const int sr = 48000;
    {
        FreqShifter fs; fs.prepare(sr); fs.set(100.0f, 1.0f);
        std::vector<float> L(sr), R(sr);
        for (int i = 0; i < sr; ++i) { L[static_cast<size_t>(i)] = std::sin(kTwoPi * 440.0f * i / sr); R[static_cast<size_t>(i)] = L[static_cast<size_t>(i)]; }
        fs.process(L.data(), R.data(), sr);
        const double p540 = goertzel(L.data() + sr / 2, sr / 2, 540.0, sr), p440 = goertzel(L.data() + sr / 2, sr / 2, 440.0, sr), p340 = goertzel(L.data() + sr / 2, sr / 2, 340.0, sr);
        CHECK(p540 > 20.0 * p440 && p540 > 20.0 * p340, "frequency shifter moves 440 Hz to 540 Hz (single sideband)");
    }
    {
        PitchShifter ps; ps.prepare(sr); ps.setSemitones(12.0f);
        std::vector<float> in(sr), out(sr);
        for (int i = 0; i < sr; ++i) in[static_cast<size_t>(i)] = std::sin(kTwoPi * 220.0f * i / sr);
        ps.process(in.data(), out.data(), sr);
        const double p440 = goertzel(out.data() + sr / 2, sr / 2, 440.0, sr), p220 = goertzel(out.data() + sr / 2, sr / 2, 220.0, sr);
        CHECK(p440 > 4.0 * p220, "pitch shifter +12 doubles the frequency");
    }
    {
        Nebula nb; nb.prepare(sr, 5);
        nb.set(0.0f);
        std::vector<float> L(sr), R(sr), wl(sr), wr(sr);
        for (int i = 0; i < sr; ++i) { L[static_cast<size_t>(i)] = 0.5f * std::sin(kTwoPi * 440.0f * i / sr); R[static_cast<size_t>(i)] = L[static_cast<size_t>(i)]; }
        nb.process(L.data(), R.data(), wl.data(), wr.data(), sr);
        double sq = 0; for (int i = sr / 2; i < sr; ++i) sq += wl[static_cast<size_t>(i)] * wl[static_cast<size_t>(i)];
        const double rmsFollow = std::sqrt(sq / (sr / 2));
        CHECK(rmsFollow > 0.2 && rmsFollow < 0.7, "nebula reproduces the input level");
        const double p440 = goertzel(wl.data() + sr / 2, sr / 2, 440.0, sr), p600 = goertzel(wl.data() + sr / 2, sr / 2, 600.0, sr);
        CHECK(p440 > 10.0 * p600, "nebula keeps the spectrum");
        nb.set(1.0f);   // freeze
        std::fill(L.begin(), L.end(), 0.0f); std::fill(R.begin(), R.end(), 0.0f);
        nb.process(L.data(), R.data(), wl.data(), wr.data(), sr);
        sq = 0; for (int i = sr / 2; i < sr; ++i) sq += wl[static_cast<size_t>(i)] * wl[static_cast<size_t>(i)];
        CHECK(std::sqrt(sq / (sr / 2)) > 0.15, "frozen nebula keeps sounding without input");
    }
    {
        Engine e;
        e.setParam(ParamId::CosmosSend, 1.0f);
        e.setParam(ParamId::CosmosShift, 50.0f);
        e.setParam(ParamId::CosmosRes, 0.6f);
        e.setParam(ParamId::CosmosResFeedback, 0.97f);
        e.setParam(ParamId::CosmosVowel, 0.6f);
        e.setParam(ParamId::CosmosNebula, 0.6f);
        e.setParam(ParamId::CosmosShimmer, 1.0f);
        e.setParam(ParamId::BrainRate, 2.0f);
        e.setParam(ParamId::Attack, 0.5f);
        e.prepare(sr, 256);
        render(e, 10.0);
        Stats a = render(e, 5.0);
        Stats b = render(e, 5.0);
        CHECK(a.nonFinite == 0 && b.nonFinite == 0, "cosmos path finite");
        CHECK(b.peak <= 1.0f, "cosmos path clipped safely");
        CHECK(b.rms < 0.5 && b.rms < a.rms * 2.0 + 0.05, "shimmer + resonator feedback does not run away");
        CHECK(a.rms > 0.01, "cosmos path audible");
    }
}

void testCloudAndLayers()
{
    const int sr = 48000;
    {
        // pitch = 0: grains keep the pitch; pitch = 1: octaves/fifths appear.
        auto energy = [&](float pitch, double hz) {
            GrainCloud c; c.prepare(sr, 3);
            c.set(20.0f, 200.0f, pitch, 0.5f, 1.0f);
            std::vector<float> L(2 * sr), R(2 * sr), oL(2 * sr, 0.0f), oR(2 * sr, 0.0f);
            for (int i = 0; i < 2 * sr; ++i) { L[static_cast<size_t>(i)] = 0.5f * std::sin(kTwoPi * 440.0f * i / sr); R[static_cast<size_t>(i)] = L[static_cast<size_t>(i)]; }
            c.process(L.data(), R.data(), oL.data(), oR.data(), 2 * sr);
            for (int i = 0; i < 2 * sr; ++i) CHECK(std::isfinite(oL[static_cast<size_t>(i)]), "cloud finite");
            return goertzel(oL.data() + sr, sr, hz, sr);
        };
        const double p440 = energy(0.0f, 440.0), p880 = energy(0.0f, 880.0);
        CHECK(p440 > 0.0 && p440 > 20.0 * p880, "cloud without pitch keeps 440 Hz");
        const double q880 = energy(1.0f, 880.0), q440 = energy(1.0f, 440.0);
        CHECK(q880 > 0.2 * q440, "cloud with pitch adds the octave");
    }
    {
        // Independent layers: a sound preset must not touch Cosmos, and vice versa.
        Engine e;
        e.setParam(ParamId::CosmosSend, 0.9f);
        e.setParam(ParamId::Partials, 4.0f);
        CHECK(e.applySoundPreset(1), "sound preset applies");
        CHECK(e.getParam(ParamId::CosmosSend) == 0.9f, "sound preset leaves the Cosmos layer alone");
        CHECK(e.getParam(ParamId::Partials) == 16.0f, "sound preset resets sound parameters to the preset");
        CHECK(numCosmosPresets() >= 16, "cosmos bank exists");
        for (int p = 0; p < numCosmosPresets(); ++p) {
            bool onlyCosmos = true;
            const bool ok = applyPreset(cosmosPreset(p), [&](ParamId id, float) { if (!isCosmosParam(id)) onlyCosmos = false; }, PresetScope::Cosmos);
            CHECK(ok && onlyCosmos, "cosmos preset parses and stays in its layer");
        }
        e.setParam(ParamId::Attack, 33.0f);
        CHECK(e.applyCosmosPreset(3), "cosmos preset applies");
        CHECK(e.getParam(ParamId::Attack) == 33.0f, "cosmos preset leaves the sound layer alone");
        CHECK(e.getParam(ParamId::CosmosSend) == 1.0f, "cosmos preset sets its own parameters");
        e.applyCosmosPreset(0);
        CHECK(e.getParam(ParamId::CosmosSend) == 0.0f && e.getParam(ParamId::CosmosShimmer) == 0.0f, "Cosmos Off resets the layer");
    }
    {
        Engine e;
        e.setParam(ParamId::Delay2Mix, 0.5f);
        e.setParam(ParamId::CloudSend, 1.0f);
        e.setParam(ParamId::BrainRate, 2.0f);
        e.setParam(ParamId::Attack, 0.5f);
        e.prepare(sr, 256);
        Stats s = render(e, 12.0);
        CHECK(s.nonFinite == 0 && s.rms > 0.01 && s.peak <= 1.0f, "delay 2 + cloud render finite and audible");
    }
}

void testMorph()
{
    Engine e;
    e.prepare(48000.0, 256);
    float a[kNumParams], b[kNumParams];
    for (int i = 0; i < kNumParams; ++i) a[i] = b[i] = paramTable()[static_cast<size_t>(i)].def;
    a[static_cast<int>(ParamId::Cutoff)] = 200.0f;   b[static_cast<int>(ParamId::Cutoff)] = 8000.0f;
    a[static_cast<int>(ParamId::Partials)] = 4.0f;   b[static_cast<int>(ParamId::Partials)] = 20.0f;
    a[static_cast<int>(ParamId::Scale)] = 0.0f;      b[static_cast<int>(ParamId::Scale)] = 5.0f;
    a[static_cast<int>(ParamId::BrainOn)] = 0.0f;    b[static_cast<int>(ParamId::BrainOn)] = 1.0f;
    e.setMorphSlot(0, a);
    e.setMorphSlot(1, b);
    e.setParam(ParamId::Cutoff, 999.0f);
    CHECK(e.effectiveParam(ParamId::Cutoff) == 999.0f, "morph off: live parameter plays");
    e.setParam(ParamId::MorphActive, 1.0f);
    e.setParam(ParamId::MorphGlide, 0.0f);
    e.setParam(ParamId::MorphPos, 0.0f);
    render(e, 0.05);
    CHECK(std::fabs(e.effectiveParam(ParamId::Cutoff) - 200.0f) < 1e-3f, "position 0 plays slot A");
    e.setParam(ParamId::MorphPos, 1.0f);
    render(e, 0.05);
    CHECK(std::fabs(e.effectiveParam(ParamId::Cutoff) - 8000.0f) < 1e-2f, "position 1 plays slot B");
    CHECK(e.effectiveParam(ParamId::Partials) == 20.0f && e.effectiveParam(ParamId::Scale) == 5.0f, "ints and choices follow");
    e.setParam(ParamId::MorphPos, 0.5f);
    render(e, 0.05);
    const float mid = e.effectiveParam(ParamId::Cutoff);
    CHECK(mid > 1000.0f && mid < 3000.0f, "halfway cutoff sits between in the perceptual (skewed) domain");
    CHECK(e.effectiveParam(ParamId::Partials) == 12.0f, "int rounds");
    CHECK(e.effectiveParam(ParamId::Scale) == 5.0f && e.effectiveParam(ParamId::BrainOn) == 1.0f, "choice/switch flip at 0.5");
    CHECK(std::fabs(e.morphPosition() - 0.5f) < 1e-6f, "morph position observer");
    // Glide: with 10 s glide, 1 s of audio moves the position by 0.1.
    e.setParam(ParamId::MorphGlide, 10.0f);
    e.setParam(ParamId::MorphPos, 1.0f);
    render(e, 1.0);
    CHECK(std::fabs(e.morphPosition() - 0.6f) < 0.01f, "glide moves 0.1 per second at 10 s glide");
    // Presets never touch morph controls.
    e.applyPreset(2);
    CHECK(e.getParam(ParamId::MorphActive) == 1.0f && e.getParam(ParamId::MorphGlide) == 10.0f, "presets leave morph state alone");
    // Capture: slot B takes the live parameters.
    e.setParam(ParamId::MorphActive, 0.0f);
    e.setParam(ParamId::Detune, 42.0f);
    e.captureMorphSlot(1);
    float out[kNumParams];
    e.morphSlot(1, out);
    CHECK(out[static_cast<int>(ParamId::Detune)] == 42.0f, "capture copies live values");
    Stats s = render(e, 1.0);
    CHECK(s.nonFinite == 0, "morph render finite");
}

struct TestSink : OscSink {
    float last[kNumParams] = {};
    bool  set[kNumParams] = {};
    int   events = 0;
    ControlEvent lastEvent{ ControlEvent::Type::NoteOn, 0, 0.0f };
    void setParam(ParamId id, float v) override { last[static_cast<int>(id)] = v; set[static_cast<int>(id)] = true; }
    void setParamNormalised(ParamId id, float n) override { const ParamDesc& d = paramDesc(id); last[static_cast<int>(id)] = d.min + (d.max - d.min) * n; set[static_cast<int>(id)] = true; }
    void event(const ControlEvent& e) override { ++events; lastEvent = e; }
};

// Build an OSC message the way a sender would (big-endian, 4-byte padded).
static size_t oscBuild(char* out, const char* address, const char* tags, const float* floats, const char* const* strings)
{
    size_t pos = 0;
    auto putStr = [&](const char* s) { const size_t n = std::strlen(s) + 1; std::memcpy(out + pos, s, n); pos += n; while (pos & 3) out[pos++] = 0; };
    putStr(address);
    char t[16]; t[0] = ','; std::strcpy(t + 1, tags); putStr(t);
    int fi = 0, si = 0;
    for (const char* c = tags; *c; ++c) {
        if (*c == 'f') { uint32_t u; std::memcpy(&u, &floats[fi++], 4); out[pos++] = static_cast<char>(u >> 24); out[pos++] = static_cast<char>(u >> 16); out[pos++] = static_cast<char>(u >> 8); out[pos++] = static_cast<char>(u); }
        else if (*c == 'i') { const int32_t v = static_cast<int32_t>(floats[fi++]); out[pos++] = static_cast<char>(v >> 24); out[pos++] = static_cast<char>(v >> 16); out[pos++] = static_cast<char>(v >> 8); out[pos++] = static_cast<char>(v); }
        else if (*c == 's') putStr(strings[si++]);
    }
    return pos;
}

void testOscAndGestures()
{
    // Parser
    char buf[512];
    const float f1[] = { 1234.5f };
    size_t n = oscBuild(buf, "/ambient/param/cutoff", "f", f1, nullptr);
    int seen = 0; OscMessage got;
    CHECK(parseOscPacket(buf, n, [&](const OscMessage& m) { ++seen; got = m; }) == 1, "parse one message");
    CHECK(seen == 1 && std::strcmp(got.address, "/ambient/param/cutoff") == 0 && got.numArgs == 1 && got.types[0] == 'f' && std::fabs(got.floats[0] - 1234.5f) < 1e-3f, "message decoded");
    const float f2[] = { 60.0f, 100.0f };
    n = oscBuild(buf, "/ambient/note", "ii", f2, nullptr);
    parseOscPacket(buf, n, [&](const OscMessage& m) { got = m; });
    CHECK(got.numArgs == 2 && got.types[1] == 'i' && got.floats[1] == 100.0f, "ints decoded");
    const char* s1[] = { "Sleep Concert" };
    n = oscBuild(buf, "/ambient/preset", "s", nullptr, s1);
    parseOscPacket(buf, n, [&](const OscMessage& m) { got = m; });
    CHECK(got.numArgs == 1 && got.types[0] == 's' && std::strcmp(got.strings[0], "Sleep Concert") == 0, "string decoded");
    // Bundle of two
    char bundle[512]; size_t bp = 0;
    std::memcpy(bundle, "#bundle\0", 8); bp = 8; std::memset(bundle + bp, 0, 8); bp += 8;
    char m1[128]; const float g1[] = { 0.7f }; const size_t l1 = oscBuild(m1, "/ambient/morph", "f", g1, nullptr);
    char m2[128]; const float gb[] = { 0.2f, 1.4f, -0.5f, 1.0f, 0.3f }; const size_t l2 = oscBuild(m2, "/ambient/hand/R", "fffff", gb, nullptr);
    auto putLen = [&](size_t l) { bundle[bp++] = 0; bundle[bp++] = 0; bundle[bp++] = static_cast<char>(l >> 8); bundle[bp++] = static_cast<char>(l); };
    putLen(l1); std::memcpy(bundle + bp, m1, l1); bp += l1;
    putLen(l2); std::memcpy(bundle + bp, m2, l2); bp += l2;
    CHECK(parseOscPacket(bundle, bp, [&](const OscMessage&) {}) == 2, "bundle unpacked");
    CHECK(parseOscPacket("garbage", 7, [&](const OscMessage&) {}) < 0, "garbage rejected");

    // Dispatch
    TestSink sink; GestureLayer gl;
    OscMessage m; m.address = "/ambient/param/cutoff"; m.numArgs = 1; m.types[0] = 'f'; m.floats[0] = 1234.5f;
    CHECK(dispatchOsc(m, sink, gl) && sink.set[static_cast<int>(ParamId::Cutoff)] && std::fabs(sink.last[static_cast<int>(ParamId::Cutoff)] - 1234.5f) < 1e-3f, "param dispatched");
    m.address = "/ambient/paramn/brightness"; m.floats[0] = 0.25f;
    CHECK(dispatchOsc(m, sink, gl) && std::fabs(sink.last[static_cast<int>(ParamId::Brightness)] - 0.25f) < 1e-6f, "normalised param dispatched");
    m.address = "/ambient/param/scale"; m.types[0] = 's'; m.strings[0] = "JI Minor";
    CHECK(dispatchOsc(m, sink, gl) && sink.last[static_cast<int>(ParamId::Scale)] == 2.0f, "choice by name");
    m.address = "/ambient/hand/R"; m.numArgs = 5; for (int i = 0; i < 5; ++i) m.types[i] = 'f';
    m.floats[0] = 0.2f; m.floats[1] = 1.7f; m.floats[2] = -0.45f; m.floats[3] = 1.0f; m.floats[4] = 0.0f;
    CHECK(dispatchOsc(m, sink, gl), "hand dispatched");
    CHECK(std::fabs(gl.input(GestureInput::RightHeight) - 1.0f) < 1e-6f, "hand height at the top of the range");
    CHECK(std::fabs(gl.input(GestureInput::RightForward) - 0.5f) < 1e-6f, "reach halfway");
    CHECK(gl.input(GestureInput::RightPinch) == 1.0f && std::fabs(gl.input(GestureInput::RightTilt) - 0.5f) < 1e-6f, "pinch and tilt passed");
    m.address = "/ambient/hand/L"; m.floats[0] = -0.2f;
    dispatchOsc(m, sink, gl);
    CHECK(std::fabs(gl.input(GestureInput::HandDistance) - (0.4f - 0.1f) / 0.7f) < 1e-5f, "hand distance from both hands");
    m.address = "/ambient/note"; m.numArgs = 2; m.types[0] = 'i'; m.types[1] = 'i'; m.floats[0] = 64.0f; m.floats[1] = 0.0f;
    CHECK(dispatchOsc(m, sink, gl) && sink.lastEvent.type == ControlEvent::Type::NoteOff && sink.lastEvent.a == 64, "note off event");
    m.address = "/ambient/cosmos"; m.numArgs = 1; m.types[0] = 's'; m.strings[0] = "Alien Choir";
    CHECK(dispatchOsc(m, sink, gl) && sink.lastEvent.type == ControlEvent::Type::CosmosPreset && sink.lastEvent.a == 9, "cosmos preset by name");
    m.address = "/ambient/nonsense";
    CHECK(!dispatchOsc(m, sink, gl), "unknown address rejected");

    // Gesture mapping: clutch, dead-zone, smoothing, text round trip.
    GestureLayer g2;
    g2.clearMappings();
    g2.addMapping({ GestureInput::LeftHeight, ParamId::Depth, 0.0f, 1.0f, 0.0f, 0.05f, GestureInput::RightPinch, false });
    float depth = -1.0f; int writes = 0;
    auto sinkFn = [&](ParamId id, float v) { if (id == ParamId::Depth) { depth = v; ++writes; } };
    g2.setInput(GestureInput::LeftHeight, 0.5f);
    g2.update(0.01, sinkFn);
    g2.setInput(GestureInput::LeftHeight, 0.8f);
    g2.update(0.01, sinkFn);
    CHECK(writes == 0, "nothing moves while the clutch is open");
    g2.setInput(GestureInput::RightPinch, 1.0f);
    g2.update(0.01, sinkFn);
    CHECK(writes == 1 && std::fabs(depth - 0.8f) < 1e-6f, "clutch closed and hand moved: value follows (no smoothing)");
    g2.setInput(GestureInput::LeftHeight, 0.82f);
    g2.update(0.01, sinkFn);
    CHECK(writes == 1, "jitter below the dead-zone is ignored");
    g2.setInput(GestureInput::LeftHeight, 0.3f);
    g2.setInput(GestureInput::RightPinch, 0.0f);
    g2.update(0.01, sinkFn);
    CHECK(writes == 1 && std::fabs(depth - 0.8f) < 1e-6f, "clutch released: the last value holds");
    GestureLayer g3;
    g3.clearMappings();
    g3.addMapping({ GestureInput::Custom0, ParamId::Brightness, 0.0f, 1.0f, 1.0f, 0.0f, GestureInput::Count, false });
    float b = 0.0f;
    g3.setInput(GestureInput::Custom0, 0.0f);
    g3.update(0.01, [&](ParamId, float v) { b = v; });   // first value primes without a glide
    CHECK(b == 0.0f, "first target is taken as is");
    g3.setInput(GestureInput::Custom0, 1.0f);
    for (int i = 0; i < 100; ++i) g3.update(0.01, [&](ParamId, float v) { b = v; });   // 1 s at 1 s smoothing
    CHECK(b > 0.6f && b < 0.7f, "smoothing: one time constant reaches ~63 %");
    {   // Default mappings through the hand path, as the simulator drives them.
        GestureLayer gd;
        float depthH = -1.0f, width = -1.0f, morph = -1.0f;
        auto sk = [&](ParamId id, float v) { if (id == ParamId::Depth) depthH = v; if (id == ParamId::Width) width = v; if (id == ParamId::MorphPos) morph = v; };
        gd.setHand(0, -0.1f, 1.30f, -0.45f, 0.0f, 0.0f);   // rest position first ...
        gd.setHand(1,  0.1f, 1.30f, -0.45f, 1.0f, 0.0f);
        gd.setHead(0.0f, 0.0f, 0.0f);
        gd.update(0.005, sk);
        gd.setHand(0, -0.2f, 0.96f, -0.45f, 0.0f, 0.0f);   // ... then the hands move
        gd.setHand(1,  0.2f, 1.30f, -0.45f, 1.0f, 0.0f);   // right pinch closed = clutch
        gd.setHead(40.0f, 0.0f, 0.0f);
        for (int i = 0; i < 400; ++i) gd.update(0.005, sk);   // 2 s: smoothing settles
        CHECK(std::fabs(depthH - 0.075f) < 0.01f, "default mapping: left height drives depth while the right hand pinches");
        CHECK(width > 1.2f, "head yaw drives width without clutch");
        CHECK(morph >= 0.0f, "hand distance drives morph");
    }
    char text[2048];
    GestureLayer g4;   // defaults
    const int len = g4.writeMappings(text, sizeof(text));
    CHECK(len > 0 && g4.numMappings() >= 6, "default mappings written");
    GestureLayer g5;
    CHECK(g5.parseMappings(text) && g5.numMappings() == g4.numMappings(), "mappings round-trip through text");
    CHECK(g5.mapping(0).input == GestureInput::HandDistance && g5.mapping(0).param == ParamId::MorphPos && g5.mapping(0).clutch == GestureInput::RightPinch, "first default mapping: hand distance -> morph, clutch right pinch");
    CHECK(!g5.parseMappings("Nonsense morph 0 1"), "unknown input rejected");

    // Real UDP loopback through the server.
    OscServer server; TestSink netSink; GestureLayer netGl;
    const bool started = server.start(19877, netSink, netGl);
    CHECK(started, "OSC server binds a port");
    if (started) {
#if defined(_WIN32)
        WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
        SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
        int s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
        sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(19877); to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const float fv[] = { 0.42f };
        const size_t ln = oscBuild(buf, "/ambient/morph", "f", fv, nullptr);
        ::sendto(s, buf, static_cast<int>(ln), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
        for (int i = 0; i < 100 && server.messagesReceived() == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        CHECK(server.messagesReceived() == 1 && std::fabs(netSink.last[static_cast<int>(ParamId::MorphPos)] - 0.42f) < 1e-6f, "UDP message reached the sink");
#if defined(_WIN32)
        closesocket(s); WSACleanup();
#else
        ::close(s);
#endif
        server.stop();
        CHECK(!server.running(), "server stops");
    }
}

void testFeaturesRound7()
{
    const int sr = 48000;
    {   // Hold: keys latch, pressing again releases, switching Hold off releases everything.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Hold, 1.0f);
        e.setParam(ParamId::Attack, 0.1f);
        e.setParam(ParamId::Release, 0.2f);
        e.setParam(ParamId::FarDecay, 1.0f); e.setParam(ParamId::NearDecay, 0.3f); e.setParam(ParamId::DelayFeedback, 0.0f);
        e.prepare(sr, 256);
        render(e, 0.05);
        e.noteOn(60, 0.8f); e.noteOff(60);
        render(e, 1.0);
        CHECK(e.activeVoices() == 1, "hold keeps the note after note-off");
        e.noteOn(60, 0.8f);   // second press releases
        render(e, 2.0);
        CHECK(e.activeVoices() == 0, "pressing a held key again releases it");
        e.noteOn(62, 0.8f); e.noteOff(62); e.noteOn(65, 0.8f); e.noteOff(65);
        render(e, 0.5);
        CHECK(e.activeVoices() == 2, "two latched notes");
        e.setParam(ParamId::Hold, 0.0f);
        render(e, 2.5);
        CHECK(e.activeVoices() == 0, "hold off releases the latched notes");
    }
    {   // Foundation: sub follows the root an octave below, binaural offset splits L/R.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::SubLevel, 0.8f);
        e.setParam(ParamId::SubBinaural, 0.0f);
        e.setParam(ParamId::RootNote, 9.0f);      // A
        e.setParam(ParamId::Scale, 0.0f);         // 12-TET
        e.prepare(sr, 256);
        std::vector<float> cap;
        render(e, 4.0, &cap);
        std::vector<float> L(sr);
        for (int i = 0; i < sr; ++i) L[static_cast<size_t>(i)] = cap[static_cast<size_t>((3 * sr + i) * 2)];
        // brain root = 48 + 9 = 57 (A3, 220 Hz) -> sub one octave below = 110 Hz
        const double p110 = goertzel(L.data(), sr, 110.0, sr), p220 = goertzel(L.data(), sr, 220.0, sr), p55 = goertzel(L.data(), sr, 55.0, sr);
        CHECK(p110 > 10.0 * p220 && p110 > 10.0 * p55, "sub sits one octave below the root");
        e.setParam(ParamId::SubBinaural, 6.0f);
        cap.clear();
        render(e, 4.0, &cap);
        std::vector<float> Lb(2 * sr), Rb(2 * sr);
        for (int i = 0; i < 2 * sr; ++i) { Lb[static_cast<size_t>(i)] = cap[static_cast<size_t>((2 * sr + i) * 2)]; Rb[static_cast<size_t>(i)] = cap[static_cast<size_t>((2 * sr + i) * 2 + 1)]; }
        const double l107 = goertzel(Lb.data(), 2 * sr, 107.0, sr), l113 = goertzel(Lb.data(), 2 * sr, 113.0, sr);
        const double r107 = goertzel(Rb.data(), 2 * sr, 107.0, sr), r113 = goertzel(Rb.data(), 2 * sr, 113.0, sr);
        CHECK(l107 > 4.0 * l113 && r113 > 4.0 * r107, "binaural: left ear 3 Hz below, right ear 3 Hz above");
    }
    {   // Bloom: a voice starts dark and opens.
        auto centroidAt = [&](float bloom) {
            Engine e;
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Bloom, bloom);
            e.setParam(ParamId::BloomTime, 5.0f);
            e.setParam(ParamId::Attack, 0.2f);
            e.setParam(ParamId::Brightness, 1.0f);
            e.setParam(ParamId::Cutoff, 18000.0f);
            e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 0.0f);   // 12-TET, C: note 48 = 130.81 Hz
            e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::Shimmer, 0.0f);   // no random partial drift: the spectrum must be reproducible
            e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f);   // one strand, on-grid partials
            e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
            e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
            e.prepare(sr, 256);
            e.noteOn(48, 0.8f);
            std::vector<float> cap;
            render(e, 1.0, &cap);   // early
            auto highRatio = [&](const std::vector<float>& c, int from) {
                double hi = 0, lo = 0;
                std::vector<float> mono(sr);
                for (int i = 0; i < sr; ++i) mono[static_cast<size_t>(i)] = c[static_cast<size_t>((from + i) * 2)];
                for (int h = 1; h <= 24; ++h) { const double p = goertzel(mono.data(), sr, 130.81 * h, sr); if (h <= 4) lo += p; else hi += p; }
                return hi / (lo + 1e-12);
            };
            const double early = highRatio(cap, 0);
            cap.clear();
            render(e, 6.0, &cap);   // after bloom time
            const double late = highRatio(cap, 5 * sr);
            return std::make_pair(early, late);
        };
        const auto withBloom = centroidAt(1.0f);
        const auto without = centroidAt(0.0f);
        CHECK(withBloom.first < 0.2 * withBloom.second, "bloom: far fewer high partials at the start than after bloom time");
        CHECK(without.first > 0.5 * without.second, "no bloom: spectrum steady from the start");
    }
    {   // Macros act only once moved, then drive several parameters.
        GestureLayer g;   // defaults incl. macro mappings
        int writes = 0; float farLevel = -1.0f, depthM = -1.0f;
        auto sk = [&](ParamId id, float v) { ++writes; if (id == ParamId::FarLevel) farLevel = v; if (id == ParamId::Depth) depthM = v; };
        g.setInput(GestureInput::Custom0, 0.0f);
        for (int i = 0; i < 10; ++i) g.update(0.01, sk);
        CHECK(writes == 0, "a macro at rest writes nothing (presets stay intact)");
        g.setInput(GestureInput::Custom0, 1.0f);
        for (int i = 0; i < 300; ++i) g.update(0.01, sk);   // 3 s
        CHECK(farLevel > 0.95f && depthM > 0.95f, "macro A drives far level and depth to their maxima");
    }
}

void testCalibrationMenuRecorder()
{
    {   // Calibration: the ranges follow the explored extremes; nothing moves meanwhile.
        GestureLayer g;
        int writes = 0;
        auto sk = [&](ParamId, float) { ++writes; };
        g.startCalibration(2.0f);
        CHECK(g.calibrating(), "calibration running");
        // sweep: hands together low, then apart high, then far forward
        for (int i = 0; i <= 40; ++i) {
            const float t = i / 40.0f;
            g.setHand(0, -0.05f - 0.35f * t, 0.7f + 0.9f * t, -0.3f - 0.3f * t, 0.0f, 0.0f);
            g.setHand(1,  0.05f + 0.35f * t, 0.7f + 0.9f * t, -0.3f - 0.3f * t, 1.0f, 0.0f);
            g.update(0.05, sk);
        }
        CHECK(writes == 0, "no parameter writes during calibration");
        CHECK(!g.calibrating() && g.calibrationProgress() >= 1.0f, "calibration finished after its time");
        CHECK(g.heightLow() > 0.7f && g.heightLow() < 0.8f && g.heightHigh() > 1.5f && g.heightHigh() < 1.6f, "height range from extremes with margin");
        CHECK(g.distNear() > 0.1f && g.distNear() < 0.2f && g.distFar() > 0.7f && g.distFar() < 0.8f, "distance range from extremes");
        char text[128];
        g.writeCalibration(text, sizeof(text));
        GestureLayer g2;
        CHECK(g2.parseCalibration(text) && std::fabs(g2.heightLow() - g.heightLow()) < 1e-5f && std::fabs(g2.distFar() - g.distFar()) < 1e-5f, "calibration round-trips through text");
        CHECK(!g2.parseCalibration("1 0 0 1 0 1"), "inverted range rejected");
    }
    {   // Hand menu: open with the left pinch, choose by right height, activate with the right pinch.
        GestureLayer g;
        HandMenu menu;
        g.setInput(GestureInput::LeftPinch, 0.0f);
        g.setInput(GestureInput::RightPinch, 0.0f);
        g.setInput(GestureInput::RightHeight, 0.95f);
        CHECK(menu.update(0.02, g) == MenuAction::None && !menu.isOpen(), "closed at rest");
        g.setInput(GestureInput::LeftPinch, 1.0f);
        for (int i = 0; i < 20; ++i) menu.update(0.02, g);
        CHECK(menu.isOpen() && menu.highlighted() == 0 && g.suspended(), "open: top item highlighted, mappings suspended");
        g.setInput(GestureInput::RightHeight, 0.05f);
        menu.update(0.02, g);
        CHECK(menu.highlighted() == kMenuItems - 1, "hand low: last item");
        g.setInput(GestureInput::RightPinch, 1.0f);
        const MenuAction a = menu.update(0.02, g);
        CHECK(a == MenuAction::Calibrate, "right pinch activates the highlighted item");
        CHECK(menu.update(0.02, g) == MenuAction::None, "holding the pinch does not repeat");
        g.setInput(GestureInput::RightPinch, 0.0f); menu.update(0.02, g);
        g.setInput(GestureInput::LeftPinch, 0.0f);
        menu.update(0.02, g);
        CHECK(!menu.isOpen() && !g.suspended(), "closes when the left pinch opens; mappings resume");
        // A right pinch that was already closed when the menu opens must not fire.
        g.setInput(GestureInput::RightPinch, 1.0f); menu.update(0.02, g);
        g.setInput(GestureInput::LeftPinch, 1.0f);
        MenuAction fired = MenuAction::None;
        for (int i = 0; i < 30; ++i) { const MenuAction r = menu.update(0.02, g); if (r != MenuAction::None) fired = r; }
        CHECK(fired == MenuAction::None, "a pinch held from before the menu opened does not select");
    }
    {   // Recorder: writes a valid float WAV with the right sizes.
        WavRecorder rec;
        const char* path = "selftest_rec.wav";
        CHECK(rec.start(path, 48000, 2), "recorder starts");
        std::vector<float> L(480), R(480);
        for (int i = 0; i < 480; ++i) { L[static_cast<size_t>(i)] = 0.25f; R[static_cast<size_t>(i)] = -0.25f; }
        for (int b = 0; b < 100; ++b) rec.write(L.data(), R.data(), 480);   // 1 s
        rec.stop();
        CHECK(rec.framesWritten() == 48000 && rec.framesDropped() == 0, "all frames written, none dropped");
        FILE* f = std::fopen(path, "rb");
        CHECK(f != nullptr, "wav exists");
        if (f) {
            char hdr[44]; std::fread(hdr, 1, 44, f);
            uint32_t dataBytes; std::memcpy(&dataBytes, hdr + 40, 4);
            uint16_t fmt; std::memcpy(&fmt, hdr + 20, 2);
            CHECK(std::memcmp(hdr, "RIFF", 4) == 0 && fmt == 3 && dataBytes == 48000u * 8u, "header: float format, data size 1 s stereo");
            float first[2]; std::fread(first, 4, 2, f);
            CHECK(first[0] == 0.25f && first[1] == -0.25f, "interleaved samples intact");
            std::fclose(f);
            std::remove(path);
        }
    }
    {   // Per-note level observer.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Attack, 0.05f);
        e.prepare(48000.0, 256);
        e.noteOn(60, 1.0f);
        render(e, 0.5);
        CHECK(e.noteLevel(60) > 0.9f && e.noteLevel(61) == 0.0f, "note level follows the envelope of the sounding note");
    }
}

} // namespace

int main()
{
    testCalibrationMenuRecorder();
    testFeaturesRound7();
    testOscAndGestures();
    testMorph();
    testCloudAndLayers();
    testCosmos();
    testParams();
    testTuning();
    testEnvelope();
    testEngineMidi();
    testBrain();
    testDeterminism();
    testUserScale();
    testDelay();
    testMidSide();
    testPresets();
    testSpace();
    testRichCarving();
    testFeedback();
    testStackAndWander();
    testSources();
    testPresetMap();
    if (failures == 0) std::printf("selftest: all checks passed\n");
    else std::printf("selftest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
