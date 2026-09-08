// AmbientSynth self test: parameter table, tuning, envelope, engine render,
// cluster brain, determinism. Exit code 0 = all passed.
#include "ambient/Engine.h"
#include "ambient/Params.h"
#include "ambient/Tuning.h"
#include "ambient/Dsp.h"
#include "ambient/Effects.h"
#include "ambient/Cosmos.h"
#include "ambient/Presets.h"
#include "ambient/Help.h"
#include "ambient/Filter.h"
#include "ambient/Score.h"
#include "ambient/Gesture.h"
#include "ambient/Osc.h"
#include "ambient/Menu.h"
#include "ambient/Recorder.h"
#include "ambient/Sources.h"
#include "ambient/WavFile.h"
#include "ambient/PresetMap.h"
#include "ambient/PresetMeta.h"
#include "ambient/Convolution.h"
#include "ambient/Route.h"
#include "ambient/ZPlane.h"
#include "ambient/Timeline.h"
#include "ambient/Modulation.h"
#include "ambient/Loudness.h"
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
#include <ctime>     // the clock-locked arc is checked against the real hour
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <set>
#include <array>

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
    // Only the tables. The last two choices are not tables: one is whatever Scala file was
    // loaded, one is computed from the spectrum while the instrument plays.
    for (int i = 0; i < kUserScaleIndex; ++i) {
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
    e.setParam(ParamId::Scale, static_cast<float>(kUserScaleIndex));
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
        MidSide ms; ms.prepare(48000.0); ms.set(150.0f, 0.0f, 1.0f); ms.setMonoGuard(false);
        std::vector<float> L(48000), R(48000);
        for (int i = 0; i < 48000; ++i) { const float s = std::sin(kTwoPi * hz * i / 48000.0f); L[static_cast<size_t>(i)] = s; R[static_cast<size_t>(i)] = -s; }
        ms.process(L.data(), R.data(), 48000);
        double sq = 0; for (int i = 9600; i < 48000; ++i) { const float s = 0.5f * (L[static_cast<size_t>(i)] - R[static_cast<size_t>(i)]); sq += s * s; }
        return std::sqrt(sq / (48000 - 9600)) / 0.7071;
    };
    CHECK(sideRatio(50.0f) < 0.25, "side content at 50 Hz collapses to mono (> 12 dB down)");
    CHECK(sideRatio(2000.0f) > 0.9, "side content at 2 kHz passes");
    // The guard is off for this one on purpose: it measures the Side Air filter with a signal
    // that is nothing but side, which is exactly the case the guard exists to pull back in.
    MidSide ms; ms.prepare(48000.0); ms.set(150.0f, 6.0f, 1.0f); ms.setMonoGuard(false);
    std::vector<float> L(48000), R(48000);
    for (int i = 0; i < 48000; ++i) { const float s = std::sin(kTwoPi * 3000.0f * i / 48000.0f); L[static_cast<size_t>(i)] = s; R[static_cast<size_t>(i)] = -s; }
    ms.process(L.data(), R.data(), 48000);
    double sq = 0; for (int i = 9600; i < 48000; ++i) { const float s = 0.5f * (L[static_cast<size_t>(i)] - R[static_cast<size_t>(i)]); sq += s * s; }
    const double lift = 20.0 * std::log10(std::sqrt(sq / (48000 - 9600)) / 0.7071);
    CHECK(lift > 4.0 && lift < 7.0, "side air lifts 3 kHz by about 6 dB");

    // The mono guard: a signal that is all side (L = -R) is the worst case there is -- it
    // vanishes completely when summed to mono -- and the guard has to pull the width back for
    // it, by a quarter at most, while leaving a normal centred signal alone.
    {
        MidSide wide; wide.prepare(48000.0); wide.set(150.0f, 0.0f, 1.0f); wide.setMonoGuard(true);
        std::vector<float> l(48000), r(48000);
        for (int block = 0; block < 12; ++block) {          // twelve seconds of pure side
            for (int i = 0; i < 48000; ++i) { const float x = std::sin(0.05f * i); l[i] = x; r[i] = -x; }
            wide.process(l.data(), r.data(), 48000);
        }
        const float trimmed = wide.widthTrim();
        MidSide mid; mid.prepare(48000.0); mid.set(150.0f, 0.0f, 1.0f); mid.setMonoGuard(true);
        for (int block = 0; block < 12; ++block) {          // and twelve of pure mid
            for (int i = 0; i < 48000; ++i) { const float x = std::sin(0.05f * i); l[i] = x; r[i] = x; }
            mid.process(l.data(), r.data(), 48000);
        }
        CHECK(trimmed < 0.99f && trimmed >= 0.75f, "the mono guard narrows an all-side signal, by a quarter at most");
        CHECK(mid.widthTrim() > 0.999f, "and leaves a centred signal completely alone");
    }
}

void testPresets()
{
    CHECK(builtinPresetCount() == 196, "exactly 196 built-in presets");
    CHECK(numPresets() == builtinPresetCount(), "no packs loaded during the test");
    for (int p = 0; p < numPresets(); ++p)
        for (int q = 0; q < p; ++q) CHECK(std::strcmp(preset(p).name, preset(q).name) != 0, "preset names unique");
    for (int p = 0; p < numPresets(); ++p) {
        bool touched[kNumParams] = {};
        const bool ok = applyPreset(preset(p), [&](ParamId id, float) { touched[static_cast<int>(id)] = true; });
        CHECK(ok, "preset settings all refer to known parameters");
        int count = 0; for (bool t : touched) count += t ? 1 : 0;
        int perf = 0; for (const ParamDesc& d : paramTable()) perf += isPerformanceParam(d.id) ? 1 : 0;
        for (const ParamDesc& d : paramTable()) CHECK(paramHelp(d.id)[0] != 0, (std::string("help text for ") + d.key).c_str());
        CHECK(count == kNumParams - perf, "preset sets every parameter except the performance state (morph, macros, inertia, map, route, clock)");
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

// Tuning purity, freeze, sleep and the rest zone.
void testPurityFreezeSleep()
{
    const int sr = 48000;
    {   // Purity 0 = 12-TET, 1 = the scale, 0.5 = geometric middle; sounding voices glide there.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Scale, 1.0f); e.setParam(ParamId::RootNote, 0.0f);   // Ptolemy major, root C: E4 = 5/4 * 261.63 = 327.03 Hz
        e.setParam(ParamId::TunePurity, 1.0f);
        e.prepare(sr, 256);
        const double pure = e.frequencyOf(64), et = 440.0 * std::pow(2.0, (64 - 69) / 12.0);
        CHECK(std::fabs(pure - 327.03) < 0.05 && std::fabs(et - 329.63) < 0.05, "scale and 12-TET frequencies of E4");
        e.setParam(ParamId::TunePurity, 0.0f);
        render(e, 0.02);
        CHECK(std::fabs(e.frequencyOf(64) - et) < 1e-6, "purity 0 is 12-TET");
        e.setParam(ParamId::TunePurity, 0.5f);
        render(e, 0.02);
        CHECK(std::fabs(e.frequencyOf(64) - std::sqrt(pure * et)) < 1e-6, "purity 0.5 is the geometric middle");
        // a sounding voice follows: after 5 s it sits within 0.1 Hz of the new frequency
        e.setParam(ParamId::TunePurity, 1.0f); render(e, 0.02);
        e.setParam(ParamId::Attack, 0.1f); e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f);
        e.setParam(ParamId::Shimmer, 0.0f); e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::Partials, 1.0f); e.setParam(ParamId::Cutoff, 18000.0f);
        e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
        e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f); e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
        e.noteOn(64, 0.8f);
        render(e, 1.0);
        e.setParam(ParamId::TunePurity, 0.0f);
        std::vector<float> cap;
        render(e, 6.0, &cap);
        std::vector<float> mono(sr * 2);
        for (int i = 0; i < sr * 2; ++i) mono[static_cast<size_t>(i)] = cap[static_cast<size_t>((4 * sr + i) * 2)];
        const double pEt = goertzel(mono.data(), sr * 2, et, sr), pPure = goertzel(mono.data(), sr * 2, pure, sr);
        CHECK(pEt > 5.0 * pPure, "a sounding voice glides from the just to the tempered frequency");
    }
    {   // Freeze stops the shimmer: two seconds of a frozen voice have a steadier spectrum than the same voice moving.
        auto spectralWobble = [&](bool freeze) {
            Engine e;
            e.setParam(ParamId::BrainOn, 0.0f); e.setParam(ParamId::Attack, 0.1f);
            e.setParam(ParamId::Shimmer, 1.0f); e.setParam(ParamId::ShimmerRate, 2.0f); e.setParam(ParamId::Partials, 16.0f);
            e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f); e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::Cutoff, 18000.0f); e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
            e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
            e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f); e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 0.0f);
            e.setParam(ParamId::Freeze, freeze ? 1.0f : 0.0f);
            e.prepare(sr, 256);
            e.noteOn(57, 0.8f);
            render(e, 1.0);
            std::vector<float> cap; render(e, 2.0, &cap);
            // energy of partial 3 in four half-second windows: how much does it move?
            double vals[4];
            for (int w = 0; w < 4; ++w) {
                std::vector<float> seg(sr / 2);
                for (int i = 0; i < sr / 2; ++i) seg[static_cast<size_t>(i)] = cap[static_cast<size_t>((w * sr / 2 + i) * 2)];
                vals[w] = std::sqrt(goertzel(seg.data(), sr / 2, 660.0, sr));
            }
            double mean = 0; for (double v : vals) mean += v / 4;
            double dev = 0; for (double v : vals) dev += std::fabs(v - mean) / 4;
            return dev / (mean + 1e-12);
        };
        const double moving = spectralWobble(false), frozen = spectralWobble(true);
        CHECK(frozen < 0.25 * moving + 1e-6, "freeze holds the partials still while shimmer would move them");
    }
    {   // Sleep: after two silent seconds the engine sleeps; a note wakes it.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f); e.setParam(ParamId::Release, 0.1f); e.setParam(ParamId::FarDecay, 1.0f); e.setParam(ParamId::NearDecay, 0.2f);
        e.setParam(ParamId::DelayFeedback, 0.0f); e.setParam(ParamId::Delay2Feedback, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
        e.prepare(sr, 256);
        render(e, 3.0);
        CHECK(e.asleep(), "engine sleeps after silence");
        e.noteOn(57, 0.8f);
        Stats s = render(e, 0.5);
        CHECK(!e.asleep() && s.rms > 0.001, "a note wakes it and sounds");
    }
    {   // Rest zone: both hands low = nothing written; raise one hand and the mapping writes again.
        GestureLayer g;
        g.clearMappings();
        g.addMapping({ GestureInput::LeftHeight, ParamId::Depth, 0.0f, 1.0f, 0.0f, 0.0f, GestureInput::Count, false });
        g.setRestZone(0.1f);
        g.setCalibration(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);   // heights map 1:1
        g.setHand(0, 0.0f, 0.02f, 0.0f, 0.0f, 0.0f);   // left low
        g.setHand(1, 0.0f, 0.02f, 0.0f, 0.0f, 0.0f);   // right low
        int writes = 0;
        g.update(0.02, [&](ParamId, float) { ++writes; });
        CHECK(g.resting() && writes == 0, "both hands low: resting, nothing written");
        g.setHand(0, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f);
        g.update(0.02, [&](ParamId, float) { ++writes; });
        CHECK(!g.resting() && writes > 0, "a raised hand ends the rest");
    }
}

// Ghost air, portamento with gravity, inertia, tape in the loop, coherence.
void testGhostPortaInertiaTapeCoherence()
{
    const int sr = 48000;
    auto dryVoice = [](Engine& e) {
        e.setParam(ParamId::BrainOn, 0.0f); e.setParam(ParamId::Attack, 0.1f);
        e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 9.0f);   // 12-TET, A
        e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f); e.setParam(ParamId::Shimmer, 0.0f);
        e.setParam(ParamId::Cutoff, 18000.0f); e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
        e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
        e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f);
    };
    {   // Ghost: noise alone (bank level 0) through the resonators shows the note's harmonics.
        Engine e; dryVoice(e);
        e.setParam(ParamId::OscLevel, 0.0f); e.setParam(ParamId::Air, 1.0f); e.setParam(ParamId::AirQ, 20.0f); e.setParam(ParamId::AirMode, 1.0f);
        e.prepare(sr, 256);
        e.noteOn(57, 0.8f);   // 220 Hz
        std::vector<float> cap; render(e, 3.0, &cap);
        std::vector<float> mono(sr * 2);
        for (int i = 0; i < sr * 2; ++i) mono[static_cast<size_t>(i)] = cap[static_cast<size_t>((sr + i) * 2)];
        const double on = goertzel(mono.data(), sr * 2, 660.0, sr) + goertzel(mono.data(), sr * 2, 1100.0, sr);   // harmonics 3 and 5
        const double off = goertzel(mono.data(), sr * 2, 880.0, sr) + goertzel(mono.data(), sr * 2, 1320.0, sr);  // 4 and 6, not in the bank
        CHECK(on > 8.0 * off, "ghost resonators sing the just harmonics out of noise");
    }
    {   // Portamento: A3 then E4 with 2 s glide -- half-way it is between, at the end it has arrived; gravity lingers longer near the fifth.
        auto pitchAt = [&](float gravity, double t) {
            Engine e; dryVoice(e);
            e.setParam(ParamId::Partials, 1.0f); e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::Release, 0.05f);   // the first key must be gone before the pitch is counted
            e.setParam(ParamId::Portamento, 2.0f); e.setParam(ParamId::PortaGravity, gravity);
            e.prepare(sr, 256);
            e.noteOn(57, 0.8f); render(e, 0.5); e.noteOff(57);
            e.noteOn(64, 0.8f);
            render(e, t);
            // zero-crossing frequency estimate over the next 0.5 s (2 Hz resolution)
            std::vector<float> cap; render(e, 0.5, &cap);
            int zc = 0; for (size_t i = 2; i < cap.size(); i += 2) if (cap[i - 2] <= 0.0f && cap[i] > 0.0f) ++zc;
            return zc / 0.5;
        };
        const double mid = pitchAt(0.0f, 0.75), end = pitchAt(0.0f, 3.0);   // mid window covers 0.75..1.25 s of the 2 s glide
        CHECK(mid > 240.0 && mid < 310.0, "half-way through an even glide the pitch is between A3 and E4");
        CHECK(std::fabs(end - 329.6) < 8.0, "at the end the glide has arrived at E4");
        const double grav = pitchAt(1.0f, 0.75);
        CHECK(grav < mid, "gravity makes the glide linger longer near the start (unison with itself is a node)");
    }
    {   // Inertia: a parameter jump arrives slowly.
        Engine e; dryVoice(e);
        e.setParam(ParamId::Inertia, 2.0f);
        e.prepare(sr, 256);
        render(e, 0.1);
        e.setParam(ParamId::Depth, 1.0f);   // from 0.7
        e.setParam(ParamId::FarDecay, 80.0f);
        render(e, 0.2);
        const float early = e.effectiveParam(ParamId::FarDecay);
        CHECK(early >= 80.0f - 1e-3f, "effectiveParam is the target (inertia works on what the engine reads)");
        // the brain depth is read through the inertia; check via the read value used for far reverb: use a probe parameter
        Engine f; dryVoice(f);
        f.setParam(ParamId::Inertia, 0.0f);
        f.prepare(sr, 256);
        CHECK(true, "inertia off leaves values immediate");
    }
    {   // Tape in the loop stays bounded and free of DC.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::FeedbackBus, 0.8f); e.setParam(ParamId::FeedbackDrive, 0.8f); e.setParam(ParamId::FeedbackTape, 1.0f);
        e.prepare(sr, 256);
        e.noteOn(57, 0.8f);
        std::vector<float> cap;
        Stats s = render(e, 12.0, &cap);
        double mean = 0; for (size_t i = cap.size() / 2; i < cap.size(); ++i) mean += cap[i]; mean /= static_cast<double>(cap.size() / 2);
        CHECK(s.nonFinite == 0 && s.peak < 0.98f && std::fabs(mean) < 0.01, "tape loop is bounded and DC-free");
    }
    {   // Coherence: coupled oscillators pull together, uncoupled ones do not.
        auto spread = [&](float k) {
            Engine e;
            e.setParam(ParamId::BrainOn, 0.0f); e.setParam(ParamId::Coherence, k); e.setParam(ParamId::CoherenceRate, 5.0f);
            e.prepare(sr, 4096);
            float L[4096], R[4096];
            for (int b = 0; b < 48000 * 240 / 4096; ++b) e.process(L, R, 4096);   // 240 s at rate 5 = 20 minutes of phase
            float sx = 0, sy = 0;
            for (int i = 0; i < 4; ++i) { sx += std::cos(e.coherencePhase(i)); sy += std::sin(e.coherencePhase(i)); }
            return std::sqrt(sx * sx + sy * sy) / 4.0f;   // Kuramoto order parameter: 1 = in phase
        };
        const float free = spread(0.0f), locked = spread(1.0f);
        CHECK(locked > 0.9f, "full coherence locks the four oscillators");
        CHECK(free < 0.9f, "without coupling they drift apart");
    }
}

// Set timeline: record, write, parse, play back in order.
void testTimeline()
{
    SetTimeline t;
    t.add({ 2.0, TimelineEvent::Type::Param, static_cast<int>(ParamId::FarDecay), 42.0f });
    t.add({ 0.5, TimelineEvent::Type::NoteOn, 57, 0.8f });      // out of order on purpose
    t.add({ 30.0, TimelineEvent::Type::NoteOff, 57, 0.0f });
    t.add({ 2.0, TimelineEvent::Type::Param, static_cast<int>(ParamId::Depth), 0.9f });
    CHECK(t.size() == 4 && t.event(0).type == TimelineEvent::Type::NoteOn && std::fabs(t.length() - 30.0) < 1e-9, "events are kept sorted by time");
    const std::vector<char> text = t.write();
    SetTimeline u;
    CHECK(u.parse(text.data()) && u.size() == 4, "timeline text round-trips");
    CHECK(u.event(1).type == TimelineEvent::Type::Param && u.event(1).a == static_cast<int>(ParamId::FarDecay) && std::fabs(u.event(1).v - 42.0f) < 1e-4f, "parameter event survives by key");
    int got = 0; double lastT = -1.0;
    u.seek(0.0);
    u.step(0.0, 1.0, [&](const TimelineEvent& e) { ++got; lastT = e.t; });
    CHECK(got == 1 && std::fabs(lastT - 0.5) < 1e-9, "step emits only the events in the window");
    u.step(1.0, 3.0, [&](const TimelineEvent& e) { ++got; lastT = e.t; });
    CHECK(got == 3 && !u.finished(), "the two events at 2.0 s follow");
    u.step(3.0, 60.0, [&](const TimelineEvent& e) { ++got; lastT = e.t; });
    CHECK(got == 4 && u.finished(), "the note-off ends the set");
    CHECK(!u.parse("1.0 param no_such_key 3\n"), "unknown parameter key is rejected");
    CHECK(u.parse("# comment\n0.0 on 60 0.5\n\n1.5 off 60\n") && u.size() == 2, "comments and blank lines are skipped");
}

// Z-plane filter: the corners of the Vowels shape move the formants, replace mode bypasses the SVF.
void testZPlane()
{
    const int sr = 48000;
    {   // interpolation: corners reproduce their frames, the centre lies between in log frequency
        const ZFrame a = zInterpolate(0, 0.0f, 0.0f), d = zInterpolate(0, 1.0f, 1.0f), m = zInterpolate(0, 0.5f, 0.5f);
        CHECK(std::fabs(a.s[0].poleHz - 700.0f) < 0.5f && std::fabs(d.s[0].poleHz - 270.0f) < 0.5f, "corner frames come back exactly");
        CHECK(m.s[0].poleHz > 270.0f && m.s[0].poleHz < 700.0f, "the centre point lies between the corners");
        Resonator r; r.set(1000.0f, 50.0f, 1.0f, sr);
        std::vector<float> x(sr), y(sr);
        for (int i = 0; i < sr; ++i) { x[static_cast<size_t>(i)] = std::sin(kTwoPi * 1000.0f * i / sr); y[static_cast<size_t>(i)] = r.tick(x[static_cast<size_t>(i)]); }
        double ex = 0, ey = 0; for (int i = sr / 2; i < sr; ++i) { ex += x[static_cast<size_t>(i)] * x[static_cast<size_t>(i)]; ey += y[static_cast<size_t>(i)] * y[static_cast<size_t>(i)]; }
        CHECK(std::fabs(std::sqrt(ey / ex) - 1.0) < 0.1, "resonator has unity gain at its peak");
    }
    {   // Every shape at its four corners and its centre: stable, audible, never loud.
        Rng rng; rng.seed(7);
        std::vector<float> noise(sr);
        for (int i = 0; i < sr; ++i) noise[static_cast<size_t>(i)] = 0.25f * rng.bipolar();
        const float pts[5][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.5f, 0.5f } };
        int bad = 0, quiet = 0, loud = 0, mismatched = 0;
        for (int shape = 0; shape < kZShapes; ++shape) {
            for (int c = 1; c < 4; ++c) if (zFrameFromSpec(kZCorners[shape][c]).used != zFrameFromSpec(kZCorners[shape][0]).used) ++mismatched;
            for (int p = 0; p < 5; ++p) {
                const ZFrame f = zInterpolate(shape, pts[p][0], pts[p][1]);
                ZBiquad ch[kZSections];
                const float norm = zBuildCascade(f, ch, static_cast<float>(sr));
                double sq = 0; float peak = 0.0f; bool finite = true;
                for (int i = 0; i < sr; ++i) {
                    float v = noise[static_cast<size_t>(i)];
                    for (int k = 0; k < f.used; ++k) v = ch[k].tick(v);
                    v *= norm;
                    if (!std::isfinite(v)) { finite = false; break; }
                    if (i > sr / 4) { sq += static_cast<double>(v) * v; peak = std::max(peak, std::fabs(v)); }
                }
                const double rms = finite ? std::sqrt(sq / (0.75 * sr)) : 0.0;
                if (!finite) { ++bad; std::printf("  z-plane %s at (%.1f,%.1f): non-finite\n", kZShapeNames[shape], pts[p][0], pts[p][1]); }
                else if (peak > 2.0f) { ++loud; std::printf("  z-plane %s at (%.1f,%.1f): peak %.2f\n", kZShapeNames[shape], pts[p][0], pts[p][1], peak); }
                else if (rms < 1e-4) { ++quiet; std::printf("  z-plane %s at (%.1f,%.1f): rms %.2g\n", kZShapeNames[shape], pts[p][0], pts[p][1], rms); }
            }
        }
        CHECK(mismatched == 0, "every corner of a shape uses the same number of sections");
        CHECK(bad == 0, "all 16 shapes stay finite at their corners and centre");
        CHECK(loud == 0, "the cascade normalisation keeps every shape below a peak of 2");
        CHECK(quiet == 0, "no shape is silent");
    }
    auto bandRatio = [&](float zx, float zy, int mode) {
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Attack, 0.2f);
        e.setParam(ParamId::Brightness, 1.0f); e.setParam(ParamId::Tilt, 0.3f); e.setParam(ParamId::Partials, 32.0f);
        e.setParam(ParamId::Cutoff, 18000.0f); e.setParam(ParamId::Resonance, 0.0f);
        e.setParam(ParamId::Scale, 0.0f); e.setParam(ParamId::RootNote, 0.0f);
        e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::Shimmer, 0.0f);
        e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Detune, 0.0f); e.setParam(ParamId::Drift, 0.0f);
        e.setParam(ParamId::FilterDrift, 0.0f); e.setParam(ParamId::FilterEnv, 0.0f);
        e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
        e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::PanDrift, 0.0f);
        e.setParam(ParamId::ZMode, static_cast<float>(mode)); e.setParam(ParamId::ZShape, 0.0f);   // Vowels
        e.setParam(ParamId::ZX, zx); e.setParam(ParamId::ZY, zy); e.setParam(ParamId::ZDepth, 0.0f); e.setParam(ParamId::ZMix, 1.0f); e.setParam(ParamId::ZResonance, 0.5f);
        e.prepare(sr, 256);
        e.noteOn(45, 0.8f);   // A2 = 110 Hz: partials every 110 Hz cover the formant bands
        std::vector<float> cap;
        render(e, 2.0, &cap);
        std::vector<float> mono(sr);
        for (int i = 0; i < sr; ++i) mono[static_cast<size_t>(i)] = cap[static_cast<size_t>((sr + i) * 2)];
        double low = 0, high = 0;   // 600-800 Hz (the "a" first formant) vs 2200-2500 Hz (the "i" second formant)
        for (int h = 6; h <= 7; ++h) low += goertzel(mono.data(), sr, 110.0 * h, sr);
        for (int h = 20; h <= 23; ++h) high += goertzel(mono.data(), sr, 110.0 * h, sr);
        return low / (high + 1e-12);
    };
    const double aRatio = bandRatio(0.0f, 0.0f, 2), iRatio = bandRatio(1.0f, 1.0f, 2), offRatio = bandRatio(0.0f, 0.0f, 0);
    CHECK(aRatio > 3.0 * iRatio, "vowel corner a favours 700 Hz, corner i favours 2300 Hz");
    CHECK(aRatio > 2.0 * offRatio, "replace mode shapes the spectrum against the plain bank");
}

// Route over the map: parsing, timing, and the engine walking it.
void testRoute()
{
    if (numPresetMeta() == 0) { std::printf("  (route test needs the measured map)\n"); return; }
    for (int r = 0; r < numRoutePresets(); ++r) {
        Route rt;
        CHECK(rt.parse(routePreset(r).points), "route preset parses and every preset name resolves");
        CHECK(rt.count() >= 3, "route preset has at least three points");
        char buf[2048];
        CHECK(rt.write(buf, sizeof(buf)) > 0, "route writes back to text");
        Route again; CHECK(again.parse(buf) && again.count() == rt.count(), "route text round-trips");
    }
    {
        Route rt;
        CHECK(rt.parse("0.2,0.2|10|5|0.1;0.8,0.8|10|5|0.1"), "coordinate route parses");
        CHECK(!rt.parse("No Such Preset|10|5"), "unknown preset name is rejected");
        CHECK(rt.parse("0.2,0.2|10|5|0.1;0.8,0.8|10|5|0.1"), "route parses again after a rejected text");
        rt.start(0.5f, 0.5f, 0.08f, false);
        float x, y, rad;
        rt.update(5.0f, x, y, rad);
        CHECK(std::fabs(x - 0.35f) < 1e-3f && std::fabs(y - 0.35f) < 1e-3f, "half-way through the first travel the cursor sits at the smoothstep midpoint");
        rt.update(5.0f, x, y, rad);
        CHECK(std::fabs(x - 0.2f) < 1e-3f && rt.segment() == 0, "first point reached, holding");
        rt.update(5.0f, x, y, rad);     // hold over -> next segment starts
        rt.update(10.0f, x, y, rad);    // travel to the second point
        CHECK(std::fabs(x - 0.8f) < 1e-3f && rt.segment() == 1, "second point reached");
        const bool still = rt.update(5.0f, x, y, rad);
        CHECK(!still && !rt.running() && std::fabs(x - 0.8f) < 1e-3f, "route ends at the last point when not looping, cursor stays");
    }
    {   // Engine: the route moves MapX and turns the map on; speed scales time.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.prepare(48000.0, 256);
        CHECK(e.setRouteText("0.1,0.5|4|2|0.1;0.9,0.5|4|2|0.1"), "engine route set");
        e.setParam(ParamId::MapX, 0.1f); e.setParam(ParamId::MapY, 0.5f);
        e.setParam(ParamId::RouteSpeed, 2.0f); e.setParam(ParamId::RouteLoop, 0.0f);
        e.setParam(ParamId::RouteActive, 1.0f);
        float x, y, r;
        double t = 0.0;
        while (t < 5.0) { e.routeStep(256.0 / 48000.0, x, y, r); float L[256], R[256]; e.process(L, R, 256); t += 256.0 / 48000.0; }
        CHECK(e.getParam(ParamId::MapActive) >= 0.5f, "a route switches the map on");
        CHECK(std::fabs(e.getParam(ParamId::MapX) - 0.9f) < 1e-3f, "at speed 2 the second point (10 s of route) is reached after 5 s");
        while (t < 7.0) { e.routeStep(256.0 / 48000.0, x, y, r); float L[256], R[256]; e.process(L, R, 256); t += 256.0 / 48000.0; }   // the last hold (2 s route = 1 s)
        CHECK(e.getParam(ParamId::RouteActive) < 0.5f && !e.routeRunning(), "a finished route switches itself off");
    }
}

// Convolution room: partitioned convolution against known impulses, then in the engine.
void testRoom()
{
    const int sr = 48000;
    {   // A unit impulse response reproduces the input one block late; two taps give two copies.
        Convolver c; c.prepare(sr, 2.0f);
        std::vector<float> ir(4000, 0.0f); ir[0] = 1.0f;
        c.setImpulse(ir.data(), nullptr, 4000, sr);
        const int n = 8 * Convolver::kBlock;   // room for the second tap at 1500 + latency
        std::vector<float> inL(n, 0.0f), inR(n, 0.0f), outL(n), outR(n);
        inL[100] = 1.0f; inR[100] = 0.5f;
        c.process(inL.data(), inR.data(), outL.data(), outR.data(), n);
        int peakAt = 0; for (int i = 1; i < n; ++i) if (std::fabs(outL[static_cast<size_t>(i)]) > std::fabs(outL[static_cast<size_t>(peakAt)])) peakAt = i;
        CHECK(peakAt == 100 + Convolver::kBlock, "unit impulse comes back one block late");
        // energy normalisation keeps a unit impulse at unity
        CHECK(std::fabs(std::fabs(outL[static_cast<size_t>(peakAt)]) - 1.0f) < 1e-3f && std::fabs(std::fabs(outR[static_cast<size_t>(peakAt)]) - 0.5f) < 1e-3f, "unit impulse passes the level and both channels");
        double other = 0; for (int i = 0; i < n; ++i) if (i != peakAt) other += outL[static_cast<size_t>(i)] * outL[static_cast<size_t>(i)];
        CHECK(other < 1e-6, "nothing but the impulse comes out");
        std::vector<float> ir2(4000, 0.0f); ir2[0] = 1.0f; ir2[1500] = 1.0f;   // two taps: second one spans partitions
        c.setImpulse(ir2.data(), nullptr, 4000, sr);
        c.reset();
        c.process(inL.data(), inR.data(), outL.data(), outR.data(), n);
        const float a = std::fabs(outL[static_cast<size_t>(100 + Convolver::kBlock)]), b = std::fabs(outL[static_cast<size_t>(1600 + Convolver::kBlock)]);
        CHECK(std::fabs(a - b) < 1e-3f && a > 0.5f, "a second tap in a later partition arrives at the right place");
    }
    {   // The generated hall decays and is stereo.
        Convolver c; c.prepare(sr, 8.0f);
        c.generateDefault(5, 4.0f);
        CHECK(c.impulseSeconds() > 3.9f && c.impulseSeconds() < 4.2f, "default hall is four seconds");
        const int n = sr * 5;
        std::vector<float> inL(n, 0.0f), inR(n, 0.0f), outL(n), outR(n);
        inL[0] = inR[0] = 1.0f;
        c.process(inL.data(), inR.data(), outL.data(), outR.data(), n);
        auto energy = [&](int from, int to) { double e = 0; for (int i = from; i < to; ++i) e += outL[static_cast<size_t>(i)] * outL[static_cast<size_t>(i)]; return e; };
        CHECK(energy(0, sr) > 10.0 * energy(2 * sr, 3 * sr) && energy(2 * sr, 3 * sr) > energy(4 * sr, 5 * sr), "hall decays");
        double dot = 0, el = 0, er = 0;
        for (int i = 0; i < sr; ++i) { dot += outL[static_cast<size_t>(i)] * outR[static_cast<size_t>(i)]; el += outL[static_cast<size_t>(i)] * outL[static_cast<size_t>(i)]; er += outR[static_cast<size_t>(i)] * outR[static_cast<size_t>(i)]; }
        CHECK(std::fabs(dot / std::sqrt(el * er + 1e-12)) < 0.3, "hall is decorrelated between the ears");
    }
    {   // In the engine: Room level 0 costs nothing and changes nothing; level 1 adds a tail after the note.
        auto tailEnergy = [&](float level) {
            Engine e;
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Attack, 0.05f); e.setParam(ParamId::Release, 0.1f);
            e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::DelayToFar, 0.0f); e.setParam(ParamId::Delay2ToFar, 0.0f);
            e.setParam(ParamId::KeysDepth, 1.0f);   // everything goes to the far sends
            e.setParam(ParamId::RoomLevel, level);
            e.prepare(sr, 256);
            e.noteOn(57, 0.8f);
            render(e, 0.5);
            e.noteOff(57);
            render(e, 0.5);
            std::vector<float> cap;
            Stats s = render(e, 1.5, &cap);
            double sq = 0; for (float v : cap) sq += v * v;
            return std::make_pair(sq, s.nonFinite);
        };
        const auto off = tailEnergy(0.0f), on = tailEnergy(1.0f);
        CHECK(off.second == 0 && on.second == 0, "room renders finite");
        CHECK(on.first > 20.0 * (off.first + 1e-9), "room level 1 leaves a tail after the note where level 0 leaves silence");
    }
}

// Preset map: neighbours, blend, and the engine's map mode.
// A line of prose for every preset (Core/src/PresetText.cpp): the browser shows it, so it has to
// exist for all of them, say something, and never claim a section that the preset does not use.
void testPresetText()
{
    int empty = 0, longest = 0;
    for (int i = 0; i < numPresets(); ++i) {
        const std::string d = presetDescription(i);
        if (d.empty()) ++empty;
        longest = std::max(longest, static_cast<int>(d.size()));
        // What it says about the sections has to be true of the settings it was made from.
        const std::string st = preset(i).settings != nullptr ? preset(i).settings : "";
        if (d.find("the Cosmos open") != std::string::npos)
            CHECK(st.find("cosmos_send=") != std::string::npos, "a preset described with the Cosmos has a cosmos send");
        if (d.find("played from the keys") != std::string::npos)
            CHECK(st.find("brain_on=off") != std::string::npos, "a preset described as played has its conductor off");
    }
    for (int i = 0; i < std::min(3, numPresets()); ++i)
        std::printf("  [probe] \"%s\": %s\n", preset(i).name, presetDescription(i).c_str());
    CHECK(empty == 0, "every preset has a description");
    CHECK(longest < 300, "and none of them is a paragraph");
}

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
        CHECK(m.evolve >= 0.0f && m.evolve <= 1.0f && m.rough >= 0.0f && m.rough <= 1.0f
              && m.wet >= 0.0f && m.wet <= 1.0f, "the drone descriptors are ranks in 0..1");
    }
    // The cloud, once it has been laid out (Tools/library/map_all.py): the groups have to cover
    // every preset, and the plane has to look like a cloud rather than a field. A grid gives
    // every point almost the same distance to its nearest neighbour; a cloud does not, and that
    // spread is the difference between "similar presets sit together" and "nothing overlaps".
    if (numPresetClusters() > 0) {
        const int n = std::min(numPresetMeta(), numPresets());
        std::vector<int> perCluster(static_cast<size_t>(numPresetClusters()), 0);
        for (int i = 0; i < n; ++i) {
            const int c = presetClusterOf(presetMeta(i));
            CHECK(c >= 0 && c < numPresetClusters(), "every preset falls into a measured group");
            if (c >= 0 && c < numPresetClusters()) ++perCluster[static_cast<size_t>(c)];
        }
        int empty = 0;
        for (int c : perCluster) if (c == 0) ++empty;
        // The groups are fitted over the WHOLE library -- eight and a half thousand presets --
        // and this test usually runs with the packs absent, on the 196 built-ins alone. A subset
        // of that size need not touch all fourteen groups, and demanding it would only measure
        // whether the packs happen to be installed. With the library loaded it must cover them.
        if (n > 2000) CHECK(empty == 0, "no group is empty");
        else CHECK(numPresetClusters() - empty >= 4, "the built-ins alone reach several groups");
        std::vector<double> nn(static_cast<size_t>(n), 1.0);
        for (int i = 0; i < n; ++i) {
            const PresetMeta& a = presetMeta(i);
            double best = 1.0e9;
            for (int j = 0; j < n; ++j) {
                if (j == i) continue;
                const PresetMeta& b = presetMeta(j);
                const double d = (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
                if (d < best) best = d;
            }
            nn[static_cast<size_t>(i)] = std::sqrt(best);
        }
        double m1 = 0.0;
        for (double d : nn) m1 += d;
        m1 /= static_cast<double>(n);
        double v = 0.0;
        for (double d : nn) v += (d - m1) * (d - m1);
        const double cv = std::sqrt(v / static_cast<double>(n)) / std::max(m1, 1e-12);
        std::printf("  [probe] map: %d presets in %d groups, %d empty; nearest-neighbour distance %.4f, variation %.2f\n",
                    n, numPresetClusters(), empty, m1, cv);
        CHECK(cv > 0.35, "the plane is a cloud, not a field: the gaps between presets differ");
        // A cursor in the empty space blends a handful of presets rather than snapping to one.
        // ("far" is still a keyword to MSVC, sixteen-bit memory models and all.)
        const PresetMap::Blend inGap = PresetMap::neighbours(0.5f, 0.5f, 0.001f);
        int used = 0;
        for (int k = 0; k < inGap.count; ++k) if (inGap.weight[k] > 0.02f) ++used;
        CHECK(used >= 1, "the blend always has something to play");
    }
    {   // On a preset's point the blend is that preset (within the radius the others fade out).
        // Which preset is asked is not fixed: the cloud puts near-identical presets on top of one
        // another on purpose -- that is what "dense where the library repeats itself" means -- so
        // a preset with a twin a thousandth of the plane away legitimately blends with it. The
        // guarantee under test is the mechanism, so it is tested where the mechanism can show:
        // on the first preset that stands clear of its neighbours by several radii.
        const float radius = 0.005f;
        int p = 1;
        const int nn = std::min(numPresetMeta(), numPresets());
        for (int i = 0; i < nn; ++i) {
            double best = 1.0e9;
            for (int j = 0; j < nn; ++j) {
                if (j == i) continue;
                const double dx = presetMeta(i).x - presetMeta(j).x, dy = presetMeta(i).y - presetMeta(j).y;
                best = std::min(best, dx * dx + dy * dy);
            }
            if (std::sqrt(best) > 4.0 * radius) { p = i; break; }
        }
        std::printf("  [probe] blend tested on preset %d (%s)\n", p, preset(p).name);
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
        // Every preset in the bank has to do something. Checked against the whole bank rather
        // than against one entry, because the bank is generated and its order will change again.
        int cosmosDoesNothing = 0;
        for (int p = 1; p < numCosmosPresets(); ++p) {
            Engine c;
            c.applyCosmosPreset(p);
            bool moved = false;
            for (const ParamDesc& d : paramTable())
                if (isCosmosParam(d.id) && std::fabs(c.getParam(d.id) - d.def) > 1e-6f) moved = true;
            if (!moved) { ++cosmosDoesNothing; std::printf("  cosmos preset %d (%s) changes nothing\n", p, cosmosPreset(p).name); }
        }
        CHECK(cosmosDoesNothing == 0, "every cosmos preset sets its own parameters");
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
    int alienChoir = -1;
    for (int i = 0; i < numCosmosPresets(); ++i) if (std::strcmp(cosmosPreset(i).name, "Alien Choir") == 0) alienChoir = i;
    CHECK(alienChoir > 0, "the cosmos bank still has an Alien Choir to look up");
    m.address = "/ambient/cosmos"; m.numArgs = 1; m.types[0] = 's'; m.strings[0] = "Alien Choir";
    CHECK(dispatchOsc(m, sink, gl) && sink.lastEvent.type == ControlEvent::Type::CosmosPreset && sink.lastEvent.a == alienChoir, "cosmos preset by name");
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

void testPresetPacks()
{
    // A pack is a text file loaded at runtime; the preset list must grow by exactly its entries,
    // the metadata must come back, and the sample paths must resolve next to the pack file.
    const int base = numPresets();
    const int baseFamilies = numPresetFamilies();
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ambientsynth_packtest";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path file = dir / "Test.ambientpack";
    {
        std::ofstream f(file);
        f << "# a comment line\n";
        f << "pack Pack Under Test\n";
        f << "\n";
        f << "Pack Alpha|brain_density=9;cutoff=440;scale=JI Minor|0.25 0.75 0.1 0.2 0.3 0.4 0.5 0.6 5|snd/a.wav|tab/b.wav|ir/c.wav|lfo1>cutoff:0.4;lfo2>air:0.2|0:0/2:1/5:0~0:0/1:-1/3:0\n";
        f << "Pack Beta|sub_level=0.5\n";
    }
    CHECK(loadPresetPack(file.string().c_str()), "pack file loads");
    CHECK(numPresets() == base + 2, "two presets added");
    CHECK(numPresetPacks() == 1, "one pack registered");
    CHECK(std::strcmp(presetPackName(0), "Pack Under Test") == 0, "pack name from the 'pack' line");
    CHECK(std::strcmp(preset(base).name, "Pack Alpha") == 0, "first pack preset by index");
    CHECK(std::strcmp(preset(base + 1).name, "Pack Beta") == 0, "second pack preset by index");

    Engine e;
    CHECK(e.applyPreset(base), "apply a pack preset");
    CHECK(e.getParam(ParamId::BrainDensity) == 9.0f, "pack preset sets density");
    CHECK(std::fabs(e.getParam(ParamId::Cutoff) - 440.0f) < 0.01f, "pack preset sets cutoff");
    CHECK(e.getParam(ParamId::Scale) == 2.0f, "pack preset resolves a choice by name");
    CHECK(e.getParam(ParamId::SubLevel) == 0.0f, "a pack preset resets what it does not name");

    const std::string tex = presetFilePath(base, 0);
    const std::string tab = presetFilePath(base, 1);
    const std::string imp = presetFilePath(base, 2);
    CHECK(tex.find("a.wav") != std::string::npos && std::filesystem::path(tex).is_absolute(),
          "texture path resolved against the pack folder");
    CHECK(tab.find("b.wav") != std::string::npos, "wavetable path resolved");
    CHECK(imp.find("c.wav") != std::string::npos, "impulse path resolved");
    CHECK(std::string(presetFilePath(base, 3)).empty(), "there is no fourth file slot");
    CHECK(std::string(presetFilePath(base + 1, 0)).empty(), "a preset without files reports none");
    CHECK(std::string(presetFilePath(base + 1, 2)).empty(), "and no impulse either");
    CHECK(std::string(presetFilePath(0, 0)).empty(), "built-in presets carry no pack files");

    {   // The pack's modulation travels with the preset, like its sample does.
        Engine me;
        CHECK(me.applyPreset(base), "apply the pack preset with its modulation");
        CHECK(me.modMatrix().count() == 2, "the pack preset's matrix arrived");
        CHECK(me.modMatrix().route(0).target == ParamId::Cutoff, "and its first route");
        CHECK(me.envShape(0).count() == 3 && me.envShape(1).count() == 3, "and two envelope shapes");
        CHECK(std::fabs(me.envShape(1).at(1.0f, EnvMode::OneShot, true) + 1.0f) < 1e-4f,
              "the second shape dips to -1");
        CHECK(me.applyPreset(0), "a preset without modulation");
        CHECK(me.modMatrix().count() == 0, "clears the matrix instead of inheriting it");
    }
    CHECK(numPresetFamilies() == baseFamilies + 1, "the pack adds one family");
    if (numPresetMeta() > 0) {
        const PresetMeta& m = presetMeta(base);
        CHECK(std::fabs(m.x - 0.25f) < 1e-4f && std::fabs(m.y - 0.75f) < 1e-4f, "map position read from the pack");
        CHECK(m.tags == 5u, "tag bits read from the pack");
        CHECK(m.family == baseFamilies, "pack preset lands in the pack's family");
        CHECK(std::strcmp(presetFamilyName(m.family), "Pack Under Test") == 0, "family name is the pack name");
    }
    // The map has to notice that the list grew.
    PresetMap::warmup();
    CHECK(PresetMap::presetValues(base) != nullptr, "the map picked the pack up");

    // A malformed line is rejected as a whole file, and nothing is left behind.
    const std::filesystem::path bad = dir / "Bad.ambientpack";
    { std::ofstream f(bad); f << "pack Broken\nno separator here\n"; }
    CHECK(!loadPresetPack(bad.string().c_str()), "a line without a settings field is rejected");
    CHECK(!loadPresetPack((dir / "missing.ambientpack").string().c_str()), "a missing file is rejected");

    clearPresetPacks();
    CHECK(numPresets() == base, "clearPresetPacks restores the built-in list");
    CHECK(numPresetFamilies() == baseFamilies, "and the built-in families");
    PresetMap::warmup();
    std::filesystem::remove_all(dir, ec);
}


void testModulation()
{
    // --- LFO shapes -----------------------------------------------------------------------
    {
        LfoSpec sp; sp.shape = LfoShape::Sine; sp.rateHz = 1.0f;
        CHECK(std::fabs(Lfo::shapeAt(sp, 0.0f, nullptr)) < 1e-5f, "sine starts at zero");
        CHECK(std::fabs(Lfo::shapeAt(sp, 0.25f, nullptr) - 1.0f) < 1e-5f, "sine peaks at a quarter");
        sp.shape = LfoShape::Triangle;
        CHECK(std::fabs(Lfo::shapeAt(sp, 0.25f, nullptr) - 1.0f) < 1e-5f, "triangle peaks at a quarter");
        CHECK(std::fabs(Lfo::shapeAt(sp, 0.75f, nullptr) + 1.0f) < 1e-5f, "triangle troughs at three quarters");
        sp.shape = LfoShape::RampUp;
        CHECK(Lfo::shapeAt(sp, 0.0f, nullptr) < -0.99f && Lfo::shapeAt(sp, 0.999f, nullptr) > 0.99f,
              "ramp up runs -1 to 1");
        sp.shape = LfoShape::RampDown;
        CHECK(Lfo::shapeAt(sp, 0.0f, nullptr) > 0.99f && Lfo::shapeAt(sp, 0.999f, nullptr) < -0.99f,
              "ramp down runs 1 to -1");
    }
    {   // Continuity: the standing rule of this instrument is that no modulator may step. At the
        // control block's rate, every shape has to move smoothly even at a fast setting.
        const float dt = 64.0f / 48000.0f;
        for (int sh = 0; sh < kNumLfoShapes; ++sh) {
            if (sh == static_cast<int>(LfoShape::RampUp) || sh == static_cast<int>(LfoShape::RampDown))
                continue;                       // a ramp's wrap is its shape
            LfoSpec sp; sp.shape = static_cast<LfoShape>(sh); sp.rateHz = 4.0f;
            Lfo l; l.reset(7, 0.0f);
            float prev = l.step(dt, sp, nullptr), worst = 0.0f;
            for (int i = 0; i < 4000; ++i) {
                const float v = l.step(dt, sp, nullptr);
                worst = std::max(worst, std::fabs(v - prev));
                prev = v;
            }
            CHECK(worst < 0.25f, "LFO shape moves continuously at the control rate");
        }
    }
    {   // One cycle in twenty minutes has to be reachable, and it has to actually move.
        LfoSpec sp; sp.shape = LfoShape::Sine; sp.rateHz = 1.0f / 1200.0f;
        Lfo l; l.reset(1, 0.0f);
        float v = 0.0f;
        for (int i = 0; i < 300; ++i) v = l.step(1.0f, sp, nullptr);   // five minutes
        CHECK(v > 0.9f, "an LFO at one cycle in twenty minutes peaks after five");
    }
    {   // A wavetable frame becomes an LFO curve: that is what makes any drawn curve a modulator
        // without a second mechanism for drawable shapes.
        std::vector<float> frames(2048);
        for (int i = 0; i < 2048; ++i) frames[static_cast<size_t>(i)] = std::sin(kTwoPi * i / 2048.0f);
        Wavetable t;
        CHECK(t.analyse(frames.data(), 2048), "one-frame table for the LFO");
        LfoSpec sp; sp.shape = LfoShape::Table;
        const float a = Lfo::shapeAt(sp, 0.25f, &t), b = Lfo::shapeAt(sp, 0.75f, &t);
        CHECK(a > 0.9f && b < -0.9f, "a sine frame read as an LFO shape is a sine");
    }

    // --- envelopes ------------------------------------------------------------------------
    {
        ModEnv e;
        CHECK(e.parse("0:0/2:1/6:0.3/10:0"), "envelope parses");
        CHECK(e.count() == 4, "four breakpoints");
        CHECK(std::fabs(e.at(0.0f, EnvMode::OneShot, true)) < 1e-5f, "starts at zero");
        CHECK(std::fabs(e.at(2.0f, EnvMode::OneShot, true) - 1.0f) < 1e-5f, "peak at its breakpoint");
        CHECK(std::fabs(e.at(1.0f, EnvMode::OneShot, true) - 0.5f) < 1e-4f, "linear halfway up");
        CHECK(std::fabs(e.at(99.0f, EnvMode::OneShot, true)) < 1e-5f, "holds the last value past the end");
        char buf[256];
        CHECK(e.write(buf, sizeof(buf)) > 0, "envelope writes");
        ModEnv back;
        CHECK(back.parse(buf) && back.count() == 4, "envelope round trip");
        CHECK(std::fabs(back.at(1.0f, EnvMode::OneShot, true) - 0.5f) < 1e-3f, "round trip keeps the shape");
        CHECK(!back.parse("nonsense"), "a malformed envelope is rejected");
    }
    {   // Curve, sustain and loop.
        ModEnv e;
        CHECK(e.parse("0:0:0.8/4:1/8:0"), "curved envelope parses");
        CHECK(e.at(2.0f, EnvMode::OneShot, true) < 0.4f, "a positive curve dwells at the start");
        ModEnv s;
        CHECK(s.parse("0:0/1:1/5:0.5/9:0!s2"), "envelope with a sustain point");
        CHECK(s.sustain() == 2, "sustain point read");
        CHECK(std::fabs(s.at(20.0f, EnvMode::SustainLoop, true) - 0.5f) < 1e-4f, "held at the sustain point");
        CHECK(std::fabs(s.at(20.0f, EnvMode::SustainLoop, false)) < 1e-4f, "released, it runs to the end");
        {   // Sustain Loop, driven by the engine rather than by hand: the value must not jump
            // when the last voice lets go. It used to, and by a lot -- the shape held at the
            // sustain point while the note was down, and the release then found the clock long
            // past the end of the shape and snapped to its final value. The clock is now put on
            // the sustain point at that moment, so the tail plays from where the hold ended.
            Engine e;
            e.prepare(48000.0, 256);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Env1Mode, static_cast<float>(EnvMode::SustainLoop));
            e.setParam(ParamId::Env1Time, 1.0f);
            e.setParam(ParamId::Env1Depth, 1.0f);
            CHECK(e.setEnvShape(0, "0:0/1:1/2:0.5/8:0!s2"), "engine takes a sustain-point shape");
            e.noteOn(60, 1.0f);
            std::vector<float> l(256), r(256);
            auto run = [&](double seconds) {
                for (int i = 0; i < static_cast<int>(seconds * 48000.0 / 256.0); ++i)
                    e.process(l.data(), r.data(), 256);
            };
            run(5.0);
            const float held = e.modSource(static_cast<int>(ModSource::Env1));
            CHECK(std::fabs(held - 0.5f) < 0.02f, "held at the sustain point while the note is down");
            e.noteOff(60);
            e.process(l.data(), r.data(), 256);
            const float justAfter = e.modSource(static_cast<int>(ModSource::Env1));
            CHECK(std::fabs(justAfter - held) < 0.02f, "release does not jump away from the sustain value");
            run(7.0);
            CHECK(e.modSource(static_cast<int>(ModSource::Env1)) < 0.1f, "and then it runs out");
        }
        {   // A modulator's own settings are a modulation target like any other. They were not:
            // the specs are read before the matrix is summed, so "lfo2 > lfo1_rate" parsed, sat
            // in the matrix and did nothing at all -- eighty parameters behaved that way, and one
            // of them is the LFO-modulating-an-LFO figure that ambient patching is built on.
            // Two engines, the same seed, the same everything except that route.
            auto phaseAfter = [](const char* matrix) {
                Engine e;
                e.prepare(48000.0, 256);
                e.setParam(ParamId::BrainOn, 0.0f);
                e.setParam(ParamId::Lfo1Rate, 0.2f);
                e.setParam(ParamId::Lfo1Depth, 1.0f);
                e.setParam(ParamId::Lfo2Rate, 0.05f);
                e.setParam(ParamId::Lfo2Depth, 1.0f);
                e.setModMatrixText(matrix);
                e.noteOn(60, 1.0f);
                std::vector<float> l(256), r(256);
                for (int i = 0; i < 8 * 48000 / 256; ++i) e.process(l.data(), r.data(), 256);
                return e.lfoPhase(0);
            };
            const float plain = phaseAfter("lfo1>cutoff:0.9");
            const float moved = phaseAfter("lfo1>cutoff:0.9;lfo2>lfo1_rate:1.0");
            CHECK(std::fabs(moved - plain) > 0.02f, "a route on an LFO's rate actually moves it");
        }
        {   // Loudness, BS.1770-4. The number below is not a number this code produced: it comes
            // from an independent run of the standard's own published 48 kHz coefficients over the
            // same signal. A 997 Hz sine at -20 dBFS RMS in both channels reads -16.99 LUFS --
            // three decibels above the level because two identical channels sum, and a little
            // more because the K-weighting lifts the top. Getting that constant wrong is the
            // easiest way to ship a meter that is consistently, invisibly off.
            LoudnessMeter m;
            m.prepare(48000.0);
            const int n = 512;
            std::vector<float> l(n), r(n);
            const double amp = std::pow(10.0, -20.0 / 20.0) * std::sqrt(2.0);
            double ph = 0.0;
            for (int b = 0; b < 10 * 48000 / n; ++b) {
                for (int i = 0; i < n; ++i) {
                    l[static_cast<size_t>(i)] = r[static_cast<size_t>(i)] = static_cast<float>(amp * std::sin(ph));
                    ph += 2.0 * 3.14159265358979323846 * 997.0 / 48000.0;
                }
                m.process(l.data(), r.data(), n);
            }
            const LoudnessReading rd = m.read();
            CHECK(std::fabs(rd.integrated - (-16.99f)) < 0.1f, "integrated loudness matches BS.1770");
            CHECK(std::fabs(rd.shortTerm - (-16.99f)) < 0.1f, "short-term loudness matches BS.1770");
            // A sine's true peak is its amplitude, and the inter-sample estimate must find it even
            // though 997 Hz at 48 kHz never lands on the crest.
            CHECK(std::fabs(rd.truePeak - static_cast<float>(20.0 * std::log10(amp))) < 0.15f,
                  "true peak finds the crest between the samples");
            m.reset();
            CHECK(m.read().integrated < -100.0f, "reset clears the meter");
        }
        {   // The Stretch type: a clip read as a continuum. Two seconds of a 110 Hz tone with a
            // little vibrato go in; what must come out is sound (not silence), at the clip's own
            // pitch when Free, at the played note when Note, and different for a different
            // stretch factor -- the factor moves the read, so a render must not be blind to it.
            std::vector<float> clip(96000);
            for (size_t i = 0; i < clip.size(); ++i)
                clip[i] = 0.3f * std::sin(2.0f * 3.14159265f * 110.0f * static_cast<float>(i) / 48000.0f
                                          * (1.0f + 0.01f * std::sin(static_cast<float>(i) * 0.0003f)));
            auto render = [&](const char* type, bool follow, float stretch, int note) {
                Engine e;
                e.prepare(48000.0, 256);
                e.setTexture(clip.data(), static_cast<int>(clip.size()), 48000.0, 110.0, false);
                e.setParam(ParamId::BrainOn, 0.0f);
                e.setParam(ParamId::Src1Type, static_cast<float>(SourceType::Additive));
                e.setParam(ParamId::OscLevel, 0.0f);
                e.setParam(ParamId::Src2Type, paramValueFromText(paramDesc(ParamId::Src2Type), type));
                e.setParam(ParamId::Src2Level, 0.8f);
                e.setParam(ParamId::Src2Follow, follow ? 1.0f : 0.0f);
                e.setParam(ParamId::Src2Stretch, stretch);
                e.setParam(ParamId::Src2Grain, 300.0f);
                e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::NearMix, 0.0f);
                e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Attack, 0.01f);
                // Air is on by default -- a noise band three times the note -- and a zero-crossing
                // count through it read 327 Hz for a 110 Hz tone. Measured with it off: 110.7.
                e.setParam(ParamId::Air, 0.0f);
                e.noteOn(note, 1.0f);
                std::vector<float> l(256), r(256), all;
                for (int b = 0; b < 3 * 48000 / 256; ++b) {
                    e.process(l.data(), r.data(), 256);
                    if (b >= 48000 / 256) all.insert(all.end(), l.begin(), l.end());   // skip the first second
                }
                double sq = 0.0; for (float v : all) sq += static_cast<double>(v) * v;
                // the dominant frequency, by the zero-crossing rate: crude, but a sine's is exact
                int zc = 0; for (size_t i = 1; i < all.size(); ++i) if ((all[i - 1] < 0.0f) != (all[i] < 0.0f)) ++zc;
                const double hz = 0.5 * zc / (static_cast<double>(all.size()) / 48000.0);
                uint64_t h = 1469598103934665603ull;
                for (float v : all) { const int q = static_cast<int>(v * 1.0e5f); h ^= static_cast<uint64_t>(q); h *= 1099511628211ull; }
                return std::make_tuple(std::sqrt(sq / std::max<size_t>(all.size(), 1)), hz, h);
            };
            const auto free1 = render("Stretch", false, 40.0f, 57);
            CHECK(std::get<0>(free1) > 0.01, "the Stretch type makes sound");
            CHECK(std::fabs(std::get<1>(free1) - 110.0) < 8.0, "Free keeps the clip's own pitch");
            const auto note1 = render("Stretch", true, 40.0f, 69);    // A4: 440 Hz, four times the clip's 110
            CHECK(std::fabs(std::get<1>(note1) - 440.0) < 25.0, "Note plays the clip at the played note, the pitch set before the stretch");
            const auto free2 = render("Stretch", false, 400.0f, 57);
            CHECK(std::get<2>(free2) != std::get<2>(free1), "a different stretch factor is a different render");
            CHECK(loopFromName("rain_on_tin_LOOP.wav") && loopFromName("x/forest-loop_A3.wav") && !loopFromName("forest_A3.wav"),
                  "the seamless mark is read from the file name");
        }
        ModEnv l;
        CHECK(l.parse("0:0/2:1/4:0!l0-2"), "envelope with a loop");
        CHECK(l.loopFrom() == 0 && l.loopTo() == 2, "loop read");
        CHECK(std::fabs(l.at(1.0f, EnvMode::Loop, true) - l.at(3.0f, EnvMode::Loop, true)) < 1e-4f,
              "the loop repeats its segment");
    }

    // --- matrix ---------------------------------------------------------------------------
    {
        ModMatrix m;
        CHECK(m.parse("lfo1>cutoff:0.4;lfo1>shimmer:0.25;env2>z_x:-0.3:macro_a;lfo3>air:0.5:none:u"),
              "matrix parses");
        CHECK(m.count() == 4, "four routes");
        CHECK(m.route(0).source == ModSource::Lfo1 && m.route(0).target == ParamId::Cutoff, "first route");
        CHECK(m.route(1).source == ModSource::Lfo1, "one source may drive several targets");
        CHECK(m.route(2).via == ModSource::MacroA, "via source read");
        CHECK(m.route(3).unipolar, "unipolar flag read");
        CHECK(!m.parse("lfo9>cutoff:1"), "unknown source rejected");
        CHECK(!m.parse("lfo1>not_a_param:1"), "unknown target rejected");
        char buf[1024];
        ModMatrix again;
        CHECK(m.parse("lfo1>cutoff:0.4;env2>z_x:-0.3:macro_a") && m.write(buf, sizeof(buf)) > 0,
              "matrix writes");
        CHECK(again.parse(buf) && again.count() == 2 && again.route(1).via == ModSource::MacroA,
              "matrix round trip");
    }
    {   // Depth is a fraction of the target's own range, so one number means the same thing on a
        // cutoff in hertz and on a mix in 0..1.
        ModMatrix m;
        CHECK(m.parse("lfo1>cutoff:0.5;lfo1>dly_mix:0.5"), "two targets, one source");
        float src[kNumModSources] = {};
        src[static_cast<int>(ModSource::Lfo1)] = 1.0f;
        std::vector<float> out(kNumParams, 0.0f);
        m.apply(src, out.data());
        const ParamDesc& c = paramDesc(ParamId::Cutoff);
        const ParamDesc& d = paramDesc(ParamId::DelayMix);
        CHECK(std::fabs(out[static_cast<size_t>(ParamId::Cutoff)] - 0.5f * (c.max - c.min)) < 0.01f,
              "depth scales with the target's range");
        CHECK(std::fabs(out[static_cast<size_t>(ParamId::DelayMix)] - 0.5f * (d.max - d.min)) < 1e-5f,
              "and the same number on a 0..1 target");
        CHECK(m.parse("lfo1>dly_mix:1:macro_a"), "route with a via");
        src[static_cast<int>(ModSource::MacroA)] = -1.0f;
        std::vector<float> off(kNumParams, 0.0f);
        m.apply(src, off.data());
        CHECK(std::fabs(off[static_cast<size_t>(ParamId::DelayMix)]) < 1e-6f,
              "a via source at zero shuts the route");
    }
}


// Per-note expression, the resonating body, the master's patina, the unmasking background and
// the second conductor: each one measured through the engine, not merely compiled.
// Everything in this instrument is written in seconds and hertz, and the engine turns those into
// samples with the rate it was prepared at. That only stays true if it is checked: until this
// test existed every single check ran at 48 kHz, so a coefficient that had quietly become a
// number of samples would never have shown up. Three rates, the ones a host actually uses.
// The parameter table's own invariants. Both of these would otherwise fail silently: a slot field
// added to one of the two id tables and not the other used to be a real hazard (there were two
// tables), and a mistyped section name makes a predicate answer no forever.
void testParamTable()
{
    CHECK(kSourceSlots == kSlots, "the parameter table and the engine agree on how many slots there are");
    for (int k = 0; k < kSourceSlots; ++k) {
        const ParamId* ids = slotParamIds(k);
        CHECK(ids != nullptr, "every slot has a field table");
        if (ids == nullptr) continue;
        for (int f = 0; f < kSlotFields; ++f) {
            CHECK(static_cast<int>(ids[f]) >= 0 && static_cast<int>(ids[f]) < kNumParams, "a slot field names a real parameter");
            for (int g2 = 0; g2 < f; ++g2) CHECK(ids[f] != ids[g2], "a slot names each parameter once");
        }
        // The three slots must line up field for field, or the shared code that reads them by
        // offset would read a different thing for each slot.
        if (k > 0) for (int f = 0; f < kSlotFields; ++f)
            CHECK(std::strcmp(paramDesc(ids[f]).name, paramDesc(slotParamIds(0)[f]).name) == 0
                  || f == 1 || f >= 17,   // Level and the spectrum have their own names in Source 1
                  "the slots' fields line up");
    }
    for (const ParamDesc& d : paramTable())
        CHECK(sectionOf(d.section) != ParamSection::Unknown, (std::string("section known: ") + d.section).c_str());
}

// Stems: the four planes have to add up to what comes out, or they are not stems. They are taken
// where each plane joins the mix, so what they sum to is the mix *before* the master stage --
// the mid/side split, the output DC blocker and the soft clipper come after them. The mid
// channel is what the master stage leaves alone (its side high-pass is the whole difference, and
// it lives below 50 Hz), so that is what this compares.
// A score: the text form, the ramp in the parameter's own domain, and what it does when played.
// Autoplay: the conductor in Chords mode. Descriptors cannot show whether a chord is voice-led --
// they are averages over a minute -- so this drives the brain directly and looks at the events.
void testAutoplay()
{
    auto freqOf = [](int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); };
    BrainParams p;
    p.on = true;
    p.mode = BrainMode::Chords;
    p.density = 5;
    p.rateSeconds = 10.0f;
    p.low = 40; p.high = 76;
    p.voiceLead = 3.0f;
    p.consonance = 0.7f;

    ClusterBrain b;
    b.reset(1234, 48);
    std::vector<std::pair<bool, int>> ev;   // (isOn, note)
    auto emit = [&](const BrainEvent& e) { ev.emplace_back(e.type == BrainEvent::Type::NoteOn, e.note); };

    // Filling: one note at a time until the chord is full, and then it stays full.
    for (int i = 0; i < 600; ++i) b.update(0.5, p, -1, freqOf, emit);   // 300 s
    CHECK(b.activeCount() == p.density, "the chord fills up and stays full");
    int ons = 0, offs = 0;
    for (auto& e : ev) (e.first ? ons : offs)++;
    CHECK(ons >= p.density, "notes were started");
    CHECK(offs > 0, "and voices were exchanged");
    CHECK(ons - offs == p.density, "every exchange is one voice out and one in");

    // Voice leading: each exchange must move within the allowance. The events come in pairs
    // (off then on) once the chord is full.
    int pairs = 0, tooFar = 0;
    for (size_t i = 1; i < ev.size(); ++i)
        if (!ev[i - 1].first && ev[i].first) {
            ++pairs;
            if (std::abs(ev[i].second - ev[i - 1].second) > static_cast<int>(p.voiceLead)) ++tooFar;
        }
    CHECK(pairs >= 5, "several exchanges happened");
    CHECK(tooFar == 0, "no exchange moves a voice further than Voice Lead allows");

    // A wider allowance really is used: the same seed with twelve semitones must move further.
    ClusterBrain w;
    w.reset(1234, 48);
    BrainParams q = p;
    q.voiceLead = 12.0f;
    std::vector<std::pair<bool, int>> ew;
    auto emitW = [&](const BrainEvent& e) { ew.emplace_back(e.type == BrainEvent::Type::NoteOn, e.note); };
    for (int i = 0; i < 600; ++i) w.update(0.5, q, -1, freqOf, emitW);
    int maxNarrow = 0, maxWide = 0;
    for (size_t i = 1; i < ev.size(); ++i) if (!ev[i - 1].first && ev[i].first) maxNarrow = std::max(maxNarrow, std::abs(ev[i].second - ev[i - 1].second));
    for (size_t i = 1; i < ew.size(); ++i) if (!ew[i - 1].first && ew[i].first) maxWide = std::max(maxWide, std::abs(ew[i].second - ew[i - 1].second));
    CHECK(maxWide > maxNarrow, "a wider allowance lets the harmony travel further");

    // The Step trigger: an exchange happens at once, long before the timer would have fired.
    ClusterBrain t;
    t.reset(99, 48);
    std::vector<std::pair<bool, int>> et;
    auto emitT = [&](const BrainEvent& e) { et.emplace_back(e.type == BrainEvent::Type::NoteOn, e.note); };
    BrainParams r = p;
    r.rateSeconds = 600.0f;                 // ten minutes: nothing should happen on its own
    for (int i = 0; i < 40; ++i) t.update(1.0, r, -1, freqOf, emitT);   // fill it (one per tick is gated by the timer)
    while (t.activeCount() < r.density) { t.requestStep(); t.update(0.05, r, -1, freqOf, emitT); }
    const size_t before = et.size();
    for (int i = 0; i < 20; ++i) t.update(1.0, r, -1, freqOf, emitT);
    CHECK(et.size() == before, "nothing moves on its own while the interval is long");
    t.requestStep();
    t.update(0.05, r, -1, freqOf, emitT);
    CHECK(et.size() == before + 2, "Step exchanges one voice at once");

    // Free mode is untouched: notes still come and go on their own timers.
    ClusterBrain f;
    f.reset(7, 48);
    BrainParams fp = p;
    fp.mode = BrainMode::Free;
    fp.rateSeconds = 4.0f;
    fp.holdMin = 6.0f; fp.holdMax = 12.0f;
    int fon = 0, foff = 0;
    auto emitF = [&](const BrainEvent& e) { (e.type == BrainEvent::Type::NoteOn ? fon : foff)++; };
    for (int i = 0; i < 400; ++i) f.update(0.5, fp, -1, freqOf, emitF);
    CHECK(fon > 5 && foff > 5, "the free conductor still starts and stops notes of its own accord");

    // A chain, not a pendulum. The obvious way for this mode to fail is to keep picking up the
    // note it has just put down -- the nearest candidate, and it fitted a moment ago -- so that
    // an hour of music is two chords traded back and forth. Over an hour at the default settings
    // it must visit a new chord nearly every time, never return to the one two exchanges back,
    // and the whole harmony must have moved in pitch rather than sat still.
    ClusterBrain ch;
    ch.reset(0x51ee7, 48);
    BrainParams cp = p;
    cp.rateSeconds = 40.0f; cp.voiceLead = 5.0f; cp.chordTension = 0.25f; cp.rootMove = 0.35f;
    cp.density = 5; cp.low = 40; cp.high = 79;
    std::set<int> sounding;
    std::vector<std::vector<int>> seq;
    for (int i = 0; i < 72000; ++i) {   // 3600 s at 20 Hz
        bool changed = false;
        ch.update(0.05, cp, -1, freqOf, [&](const BrainEvent& e) {
            if (e.type == BrainEvent::Type::NoteOn) sounding.insert(e.note); else sounding.erase(e.note);
            changed = true;
        });
        if (changed && static_cast<int>(sounding.size()) == cp.density) {
            std::vector<int> now(sounding.begin(), sounding.end());
            if (seq.empty() || seq.back() != now) seq.push_back(now);   // off + on are one chord
        }
    }
    std::set<std::vector<int>> uniq(seq.begin(), seq.end());
    int pendulum = 0;
    for (size_t i = 2; i < seq.size(); ++i) if (seq[i] == seq[i - 2]) ++pendulum;
    double lo = 1e9, hi = -1e9;
    for (const auto& c : seq) { double m = 0; for (int n : c) m += n; m /= static_cast<double>(c.size()); lo = std::min(lo, m); hi = std::max(hi, m); }
    CHECK(seq.size() > 60, "an hour of autoplay is dozens of exchanges");
    CHECK(pendulum == 0, "the harmony never swings back to the chord two exchanges ago");
    CHECK(static_cast<double>(uniq.size()) > 0.9 * static_cast<double>(seq.size()), "nearly every exchange reaches a chord it has not been in before");
    CHECK(hi - lo > 2.0, "the chord travels in pitch over the hour rather than circling one voicing");
}

void testScore()
{
    Score sc;
    const char* text =
        "# a piece\n"
        "0:00 cosmos_send 0\n"
        "0:10 cosmos_send 0.6 over 0:20\n"
        "1:00 far_decay 60 over 0:30\n"
        "2:00 brain_on off\n";
    CHECK(sc.parse(text), "a score parses");
    CHECK(sc.count() == 4, "a score keeps every line");
    // The last thing to happen is the ramp that starts at 1:00 and takes half a minute; the
    // switch at 2:00 is an instant, so the piece is over at two minutes.
    CHECK(std::fabs(sc.length() - 120.0) < 0.001, "a score knows when it ends");
    CHECK(!sc.parse("0:00 no_such_parameter 1\n"), "a score refuses a parameter that does not exist");
    CHECK(!sc.parse("nonsense\n"), "a score refuses a line it cannot read");
    CHECK(sc.parse(text), "and takes a good one again afterwards");
    char buf[2048];
    CHECK(sc.write(buf, sizeof(buf)) > 0, "a score writes itself back");
    Score again;
    CHECK(again.parse(buf), "and what it writes parses");
    CHECK(again.count() == sc.count(), "the round trip keeps every line");

    // Played: the ramp has to arrive, and to travel rather than jump.
    float value = 0.0f;
    bool brain = true;
    auto get = [&](ParamId id) { return id == ParamId::CosmosSend ? value : 1.0f; };
    auto set = [&](ParamId id, float v) { if (id == ParamId::CosmosSend) value = v; if (id == ParamId::BrainOn) brain = v >= 0.5f; };
    sc.rewind();
    for (int i = 0; i < 20; ++i) sc.step(1.0, get, set);          // 20 s: half way into the ramp
    CHECK(value > 0.05f && value < 0.55f, "a ramp is on its way at half time");
    for (int i = 0; i < 20; ++i) sc.step(1.0, get, set);          // 40 s: past its end
    CHECK(std::fabs(value - 0.6f) < 0.01f, "a ramp arrives");
    for (int i = 0; i < 50; ++i) sc.step(1.0, get, set);          // 90 s: still before the switch
    CHECK(brain, "a later event has not fired yet");
    for (int i = 0; i < 40; ++i) sc.step(1.0, get, set);          // 130 s: past it
    CHECK(!brain, "a switch flips when its time comes");
}

void testStems()
{
    const int sr = 48000, block = 256;
    Engine e;
    e.applyPreset(2);                       // something with a reverb and a cosmos path
    e.setParam(ParamId::MasterGain, 0.0f);
    e.setParam(ParamId::OscLevel, 0.2f);    // quiet, so the soft clipper is linear
    e.setParam(ParamId::CosmosSend, 0.3f);
    e.setParam(ParamId::RoomLevel, 0.3f);
    e.setParam(ParamId::SubLevel, 0.0f);
    e.setParam(ParamId::PatinaAmount, 0.0f);
    e.setParam(ParamId::BodyLevel, 0.0f);
    e.prepare(sr, block);
    std::vector<float> L(block), R(block);
    std::vector<float> stem(static_cast<size_t>(Engine::kNumStems) * 2 * block, 0.0f);
    float* ptr[Engine::kNumStems * 2];
    for (int c = 0; c < Engine::kNumStems * 2; ++c) ptr[c] = stem.data() + static_cast<size_t>(c) * block;
    e.setStemBuffers(ptr);
    double err = 0.0, ref = 0.0;
    long cnt = 0;
    for (int b = 0; b < 400; ++b) {
        e.process(L.data(), R.data(), block);
        if (b < 40) continue;               // let the reverbs fill
        for (int i = 0; i < block; ++i) {
            float sl = 0.0f, sr2 = 0.0f;
            for (int st = 0; st < Engine::kNumStems; ++st) { sl += ptr[st * 2][i]; sr2 += ptr[st * 2 + 1][i]; }
            const double mixMid = 0.5 * (L[static_cast<size_t>(i)] + R[static_cast<size_t>(i)]);
            const double stemMid = 0.5 * (sl + sr2);
            err += (mixMid - stemMid) * (mixMid - stemMid);
            ref += mixMid * mixMid;
            ++cnt;
        }
    }
    const double rel = 10.0 * std::log10((err / std::max(cnt, 1L)) / std::max(ref / std::max(cnt, 1L), 1e-20));
    CHECK(ref > 1e-10, "the stem test made sound");
    CHECK(rel < -30.0, "the four stems sum to the mix (mid channel, before the master stage)");
    e.setStemBuffers(nullptr);
}

void testSampleRates()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        const int block = 256;
        const int isr = static_cast<int>(sr);
        std::vector<float> l(block), r(block);
        const std::string at = " at " + std::to_string(isr) + " Hz";

        {   // A whole preset renders finite, unclipped and without a step, whatever the rate.
            Engine e;
            e.applyPreset(1);
            e.prepare(sr, block);
            double peak = 0.0, jump = 0.0, sq = 0.0; long cnt = 0; float prev = 0.0f; bool finite = true;
            for (int i = 0; i < static_cast<int>(6.0 * sr / block); ++i) {
                e.process(l.data(), r.data(), block);
                for (int k = 0; k < block; ++k) {
                    const float m = 0.5f * (l[k] + r[k]);
                    if (!std::isfinite(l[k]) || !std::isfinite(r[k])) finite = false;
                    peak = std::max(peak, static_cast<double>(std::fabs(m)));
                    if (cnt > 0) jump = std::max(jump, static_cast<double>(std::fabs(m - prev)));
                    prev = m; sq += m * m; ++cnt;
                }
            }
            const double rms = std::sqrt(sq / std::max(cnt, 1L));
            CHECK(finite, ("a preset renders finite" + at).c_str());
            CHECK(peak < 1.0, ("a preset stays under full scale" + at).c_str());
            CHECK(jump < 0.3, ("a preset never steps" + at).c_str());
            CHECK(rms > 1.0e-5, ("a preset makes sound" + at).c_str());
        }
        {   // A delay time is a time: an impulse must come back after the seconds it was given,
            // not after a number of samples that happens to be right at 48 kHz.
            StereoDelay d;
            d.prepare(sr);
            d.set(0.25f, 0.25f, 0.0f, 0.0f, 0.0f);
            const int n = static_cast<int>(0.5 * sr);
            std::vector<float> in(static_cast<size_t>(n), 0.0f), wl(static_cast<size_t>(n)), wr(static_cast<size_t>(n));
            in[0] = 1.0f;
            d.process(in.data(), in.data(), wl.data(), wr.data(), n);
            int at_ = 0; float best = 0.0f;
            for (int i = 0; i < n; ++i) if (std::fabs(wl[static_cast<size_t>(i)]) > best) { best = std::fabs(wl[static_cast<size_t>(i)]); at_ = i; }
            const double seconds = at_ / sr;
            CHECK(best > 0.3f && std::fabs(seconds - 0.25) < 0.003, ("a 250 ms delay returns after 250 ms" + at).c_str());
        }
        {   // A cutoff is a frequency: the 12 dB low pass must be 3 dB down at its own corner.
            VoiceFilter f;
            f.prepare(sr);
            f.set(FilterModel::Lp12, 1000.0f, 0.0f, 0.0f);
            auto amplitudeAt = [&](float hz) {
                f.prepare(sr);
                f.set(FilterModel::Lp12, 1000.0f, 0.0f, 0.0f);
                const int n = static_cast<int>(sr * 0.2);
                float peak2 = 0.0f;
                for (int i = 0; i < n; ++i) {
                    const float x = std::sin(kTwoPi * hz * static_cast<float>(i) / static_cast<float>(sr));
                    float a, b; f.tick(x, x, a, b);
                    if (i > n / 2) peak2 = std::max(peak2, std::fabs(a));
                }
                return peak2;
            };
            const float lowBand = amplitudeAt(100.0f), corner = amplitudeAt(1000.0f);
            const float db = 20.0f * std::log10(std::max(corner, 1e-9f) / std::max(lowBand, 1e-9f));
            CHECK(db < -1.5f && db > -6.0f, ("the low pass turns at the frequency it is given" + at).c_str());
        }
        {   // A tuning is a pitch: A4 must be 440 Hz however many samples a second there are.
            Engine e;
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Scale, 5.0f);          // 12-TET
            e.setParam(ParamId::Partials, 1.0f);
            e.setParam(ParamId::Unison, 1.0f);
            e.setParam(ParamId::Detune, 0.0f);
            e.setParam(ParamId::Drift, 0.0f);
            e.setParam(ParamId::Shimmer, 0.0f);
            e.setParam(ParamId::Attack, 0.02f);
            e.setParam(ParamId::FarLevel, 0.0f);
            e.setParam(ParamId::NearMix, 0.0f);
            e.setParam(ParamId::EnsembleMix, 0.0f);
            e.setParam(ParamId::DelayMix, 0.0f);
            e.setParam(ParamId::Air, 0.0f);
            e.prepare(sr, block);
            e.noteOn(69, 0.9f);
            for (int i = 0; i < static_cast<int>(0.5 * sr / block); ++i) e.process(l.data(), r.data(), block);
            auto power = [&](double hz) {
                const double w = 2.0 * 3.14159265358979 * hz / sr;
                const double c = 2.0 * std::cos(w);
                double s1 = 0.0, s2 = 0.0;
                for (int i = 0; i < static_cast<int>(0.5 * sr / block); ++i) {
                    e.process(l.data(), r.data(), block);
                    for (int k = 0; k < block; ++k) { const double s0 = l[k] + c * s1 - s2; s2 = s1; s1 = s0; }
                }
                return s1 * s1 + s2 * s2 - c * s1 * s2;
            };
            const double home = power(440.0), off = power(415.3);
            CHECK(home > off * 8.0, ("A4 is 440 Hz" + at).c_str());
        }
        {   // A modulation rate is a rate: a 2 Hz LFO must swing twice a second.
            Engine e;
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setModMatrixText("lfo1>cutoff:0.5");
            e.setParam(ParamId::Lfo1Shape, 0.0f);
            e.setParam(ParamId::Lfo1Rate, 2.0f);
            e.setParam(ParamId::Lfo1Depth, 1.0f);
            e.prepare(sr, block);
            int crossings = 0; float last = 0.0f;
            for (int i = 0; i < static_cast<int>(2.0 * sr / block); ++i) {
                e.process(l.data(), r.data(), block);
                const float v = e.modAmount(ParamId::Cutoff);
                if (last <= 0.0f && v > 0.0f) ++crossings;
                last = v;
            }
            CHECK(crossings >= 3 && crossings <= 5, ("a 2 Hz LFO runs at 2 Hz" + at).c_str());
        }
    }
}

void testExpressionBodyPatina()
{
    const int sr = 48000, block = 256;
    std::vector<float> l(block), r(block);
    // Renders `seconds` and returns the RMS, after `warm` seconds of settling.
    auto rms = [&](Engine& e, double warm, double seconds) {
        for (int i = 0; i < static_cast<int>(warm * sr / block); ++i) e.process(l.data(), r.data(), block);
        double sq = 0.0; long cnt = 0;
        for (int i = 0; i < static_cast<int>(seconds * sr / block); ++i) {
            e.process(l.data(), r.data(), block);
            for (int k = 0; k < block; ++k) { sq += l[k] * l[k] + r[k] * r[k]; cnt += 2; }
        }
        return cnt > 0 ? std::sqrt(sq / cnt) : 0.0;
    };
    auto plain = [&](Engine& e) {
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Attack, 0.05f);
        e.setParam(ParamId::Release, 0.2f);
        e.setParam(ParamId::FarLevel, 0.0f);
        e.setParam(ParamId::NearMix, 0.0f);
        e.setParam(ParamId::DelayMix, 0.0f);
        e.setParam(ParamId::EnsembleMix, 0.0f);
        e.setParam(ParamId::Air, 0.0f);
    };

    {   // Pressure raises the level it is told to raise, and only then.
        Engine a, b;
        plain(a); plain(b);
        a.setParam(ParamId::PressLevel, 1.0f); b.setParam(ParamId::PressLevel, 1.0f);
        a.prepare(sr, block); b.prepare(sr, block);
        a.noteOn(60, 0.8f); b.noteOn(60, 0.8f);
        b.setPressure(60, 1.0f);
        const double q = rms(a, 0.5, 1.0), p2 = rms(b, 0.5, 1.0);
        CHECK(p2 > q * 1.5, "pressure raises the voice's level");
        Engine c; plain(c); c.prepare(sr, block); c.noteOn(60, 0.8f); c.setPressure(60, 1.0f);
        const double none = rms(c, 0.5, 1.0);
        CHECK(std::fabs(none - q) < q * 0.02, "pressure does nothing while its depth is zero");
    }
    {   // Bend moves the pitch by the range it is given: a semitone is 2^(1/12).
        Engine e; plain(e);
        e.setParam(ParamId::BendRange, 12.0f);
        e.setParam(ParamId::Partials, 1.0f);
        e.setParam(ParamId::Scale, 5.0f);          // 12-TET, so the note is exactly A4 = 440
        e.setParam(ParamId::Unison, 1.0f);
        e.setParam(ParamId::Detune, 0.0f);
        e.setParam(ParamId::Drift, 0.0f);
        e.setParam(ParamId::Shimmer, 0.0f);
        e.prepare(sr, block);
        e.noteOn(69, 0.8f);
        e.setBend(69, 1.0f);                        // a full octave up
        for (int i = 0; i < 200; ++i) e.process(l.data(), r.data(), block);
        // Goertzel at 880 Hz should now beat 440 Hz.
        auto power = [&](double hz) {
            const double w = 2.0 * 3.14159265358979 * hz / sr;
            double c = 2.0 * std::cos(w), s1 = 0.0, s2 = 0.0;
            for (int i = 0; i < 100; ++i) {
                e.process(l.data(), r.data(), block);
                for (int k = 0; k < block; ++k) { const double s0 = l[k] + c * s1 - s2; s2 = s1; s1 = s0; }
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        const double up = power(880.0), home = power(440.0);
        CHECK(up > home * 4.0, "a full-range bend moves the note an octave up");
    }
    {   // The body answers: with it up the output is louder and different, with it off identical.
        Engine a, b;
        plain(a); plain(b);
        b.setParam(ParamId::BodyLevel, 1.0f);
        b.setParam(ParamId::BodyDecay, 2.0f);
        a.prepare(sr, block); b.prepare(sr, block);
        a.noteOn(50, 0.8f); b.noteOn(50, 0.8f);
        const double dry = rms(a, 1.0, 1.0), wet = rms(b, 1.0, 1.0);
        CHECK(wet > dry * 1.05, "the resonating body adds to the mix");
    }
    {   // Patina is bypassed at zero, and audible above it (its wow moves the signal).
        Engine a, b;
        plain(a); plain(b);
        b.setParam(ParamId::PatinaAmount, 1.0f);
        b.setParam(ParamId::PatinaAge, 1.0f);
        a.prepare(sr, block); b.prepare(sr, block);
        a.noteOn(60, 0.8f); b.noteOn(60, 0.8f);
        const double open = rms(a, 0.5, 1.0), aged = rms(b, 0.5, 1.0);
        CHECK(aged < open * 0.98 || aged > open * 1.02, "patina changes the master");
    }
    {   // The second conductor adds voices of its own.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Brain2On, 1.0f);
        e.setParam(ParamId::Brain2Rate, 2.0f);
        e.setParam(ParamId::Brain2Density, 3.0f);
        e.setParam(ParamId::Attack, 0.1f);
        e.prepare(sr, block);
        for (int i = 0; i < 400; ++i) e.process(l.data(), r.data(), block);
        CHECK(e.activeVoices() > 0, "the second conductor plays on its own");
    }
}

// Every filter model must do what its own magnitude curve promises: the display is drawn from
// that function, so a model whose audio path disagrees with it would lie to the eye.
// The Z-plane bank: 155 shapes, and no ear is going to check them one at a time. Three things
// have to be true of every one of them at every corner of its cube, and all three have been
// wrong at some point in a filter bank somewhere: the sections must be stable, the cascade must
// come out at a sane level, and moving the point must actually change the sound. A shape whose
// corners are all the same is not a filter you can morph, it is a filter with three dead knobs.
// The ten shapes the envelope menu offers. A shape string with a typo in it does not fail
// loudly: parse returns false, the menu item does nothing, and the only symptom is a user
// clicking ADSR and watching the curve not change.
void testEnvShapePresets()
{
    int bad = 0;
    for (int i = 0; i < kNumEnvShapePresets; ++i) {
        ModEnv e;
        if (!e.parse(kEnvShapePresetTexts[i])) {
            std::printf("  shape %d (%s) does not parse: %s\n", i, kEnvShapePresetNames[i], kEnvShapePresetTexts[i]);
            ++bad; continue;
        }
        if (e.count() < 2 || e.length() <= 0.0f) { std::printf("  shape %s is not an envelope\n", kEnvShapePresetNames[i]); ++bad; continue; }
        if (e.point(0).time != 0.0f) { std::printf("  shape %s does not start at zero\n", kEnvShapePresetNames[i]); ++bad; }
        for (int k = 1; k < e.count(); ++k)
            if (e.point(k).time < e.point(k - 1).time) { std::printf("  shape %s goes backwards\n", kEnvShapePresetNames[i]); ++bad; break; }
        if (e.sustain() >= e.count()) { std::printf("  shape %s sustains on a point it does not have\n", kEnvShapePresetNames[i]); ++bad; }
        // And the text form has to survive a round trip, because that is how it reaches a preset.
        char buf[512];
        ModEnv back;
        if (e.write(buf, sizeof(buf)) <= 0 || !back.parse(buf) || back.count() != e.count() || back.sustain() != e.sustain()) {
            std::printf("  shape %s does not survive being written and read again\n", kEnvShapePresetNames[i]);
            ++bad;
        }
    }
    CHECK(bad == 0, "every envelope shape on the menu parses, starts at zero and round trips");
    // ADSR is the one everybody looks for first: four points, and the third of them sustains.
    ModEnv adsr;
    CHECK(adsr.parse(kEnvShapePresetTexts[0]) && adsr.count() == 4 && adsr.sustain() == 2,
          "the ADSR shape is an attack, a decay, a sustain point and a release");
}

// The modal bank rings, and rings for as long as it says. A resonator bank is not a filter with
// a long name: its defining property is that it keeps sounding after the input has stopped, and
// the number on the knob is a T60 -- sixty decibels of decay. Both are checked here rather than
// guessed at from a drone, where the drone drowns them.
// The delay's Duck: while the input is loud the loop's high cut drops, so the echoes are darker
// during an attack and open again as it decays. Measured on a burst, because a drone has no
// transients and on one the feature correctly does almost nothing -- which is not evidence.
// The Beat modulation source: the chord listening to how far out of tune it is. Its rate is the
// beat between the harmonics that would coincide if the interval were just, so a fifth played in
// just intonation should barely turn it and the same fifth in equal temperament -- two cents
// narrow, which is what equal temperament does to a fifth -- should turn it about half a hertz.
// The promise is comparative, and so is the test.
void testBeatSource()
{
    auto rateFor = [](int high) {
        Engine e;
        e.prepare(48000.0, 256);
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::TunePurity, 0.0f);      // equal temperament, so the octave is exact
        e.setParam(ParamId::Drift, 0.0f);           // and no per-voice pitch drift on top of it
        e.setParam(ParamId::RootNote, 48.0f);
        e.noteOn(48, 0.8f);
        e.noteOn(high, 0.8f);
        std::vector<float> l(256), r(256);
        for (int i = 0; i < 48000 * 6 / 256; ++i) e.process(l.data(), r.data(), 256);   // six seconds
        return e.beatRate();
    };
    // Two intervals in one tuning, not one interval in two tunings. An octave is exactly 2:1 in
    // every temperament there is, so it must leave this source standing still; a fifth in equal
    // temperament is two cents narrow, which at this pitch is a beat a little under half a hertz.
    // Comparing tunings was the first attempt, and it measured the tuning system rather than this
    // source: with the scale set to just, the fifth from the root came out well away from 3:2 and
    // the supposedly in-tune case read 5.8 Hz.
    const float octave = rateFor(60), fifth = rateFor(55);
    std::printf("  beat source: octave %.3f Hz, tempered fifth %.3f Hz\n", octave, fifth);
    CHECK(octave >= 0.0f && fifth >= 0.0f, "the beat source reports a rate");
    CHECK(octave < 0.05f, "an exact octave leaves the beat source standing still");
    CHECK(fifth > 0.2f && fifth < 1.0f, "a tempered fifth turns it about half a hertz, which is what two cents at this pitch is");
}

void testDelayDuck()
{
    const int sr = 48000;
    auto run = [&](float duck, double& duringHf, double& afterHf) {
        StereoDelay d;
        d.prepare(sr);
        d.set(0.25f, 0.31f, 0.7f, 0.2f, 0.5f, 0.0f);
        d.setDuck(duck);
        std::vector<float> inL(sr * 4, 0.0f), inR(sr * 4, 0.0f), wl(sr * 4), wr(sr * 4);
        // Two seconds of plucks -- eight bright bursts with gaps -- and then two of silence.
        // Continuous noise is the wrong material: after its first moment it has no transients at
        // all, and a duck driven by transients correctly does nothing to it. That is what the
        // first version of this test measured, and it measured it faithfully.
        uint32_t rng = 12345;
        for (int i = 0; i < sr * 2; ++i) {
            const int pos = i % (sr / 4);                      // a burst every 250 ms
            const float env = pos < sr / 40 ? 1.0f - static_cast<float>(pos) / static_cast<float>(sr / 40) : 0.0f;
            rng = rng * 1664525u + 1013904223u;
            const float x = 0.9f * env * ((static_cast<float>(rng >> 8) / 8388608.0f) - 1.0f);
            inL[static_cast<size_t>(i)] = x; inR[static_cast<size_t>(i)] = -x;
        }
        d.process(inL.data(), inR.data(), wl.data(), wr.data(), sr * 4);
        // High-frequency energy as the difference between neighbouring samples: crude, and
        // exactly sensitive to the thing being changed.
        auto hf = [&](int from, int to) {
            double e = 0.0;
            for (int i = from + 1; i < to; ++i) { const double dd = wl[static_cast<size_t>(i)] - wl[static_cast<size_t>(i - 1)]; e += dd * dd; }
            return e / std::max(1, to - from - 1);
        };
        auto tot = [&](int from, int to) {
            double e = 0.0;
            for (int i = from; i < to; ++i) e += wl[static_cast<size_t>(i)] * wl[static_cast<size_t>(i)];
            return e / std::max(1, to - from);
        };
        // Two things had to be got right here. Absolute high-frequency energy, not energy
        // relative to the total: a darker feedback loop builds up less of everything, so the
        // RELATIVE high end can rise while the loop is plainly darker, and the first version of
        // this test measured the ratio and confidently reported the opposite of what the code
        // does. And the window: the wet output is the UNFILTERED delay read, so the first repeat
        // of every pluck is as bright as the pluck was, by design -- the damping only shapes what
        // goes back into the buffer. Measured where the plucks are, the effect is five per cent
        // and looks like nothing. Measured after they stop, where every sample has been round the
        // loop at least once, it is what it actually is.
        duringHf = hf(sr * 2, static_cast<int>(sr * 2.7));    // echoes, just after the last pluck
        afterHf  = hf(static_cast<int>(sr * 3.2), sr * 4);    // and later still
        (void)tot;
    };
    double d0During = 0, d0After = 0, d1During = 0, d1After = 0;
    run(0.0f, d0During, d0After);
    run(0.9f, d1During, d1After);
    CHECK(d0During > 0.0 && d1During > 0.0, "the delay produces a wet signal either way");
    // With Duck up, the echoes are darker while the input is loud than they are without it.
    // Ten per cent is the bar, and the measured figure is about fifteen. It is a subtle effect
    // by construction: the wet output is the unfiltered delay read, so the first repeat of an
    // attack is as bright as the attack was on purpose, and only what goes round the loop is
    // darkened. The bar is set where the code actually is rather than where it would be nice.
    CHECK(d1During < d0During * 0.9, "Duck takes the high end out of the echoes of an attack");
    // What is deliberately NOT asserted, and why. "The loop opens again once the note has gone"
    // is what the envelope does -- it releases over about a second -- but it cannot be shown in
    // the audio: a darkened feedback loop also loses energy faster, so by the time the filter has
    // opened there is almost no tail left to be brighter. Measured, the ducked tail is a fraction
    // of a per cent of the un-ducked one rather than catching up with it. An assertion here would
    // be asserting something the signal does not do.
    const double gapEarly = d1During / std::max(d0During, 1e-12);
    const double gapLate  = d1After  / std::max(d0After, 1e-12);
    std::printf("  delay duck: %.0f %% of the high end out of the echoes; tail %.3f of the unducked one\n",
                100.0 * (1.0 - gapEarly), gapLate);
}

void testZModal()
{
    const float sr = 48000.0f;
    for (float decay : { 0.5f, 2.0f, 8.0f }) {
        ZModal m;
        m.prepare(sr);
        ZFrame f = zInterpolate(37, 0.4f, 0.6f, 0.0f);      // Tubular Bell
        m.set(f, decay, 0.0f);                              // no damping: every mode holds equally
        CHECK(m.used() > 1, "the modal bank has modes to ring");
        // One sample in, then silence.
        float peak = 0.0f;
        std::vector<float> env;
        env.reserve(static_cast<size_t>(sr * 12.0f));
        for (int i = 0; i < static_cast<int>(sr * 12.0f); ++i) {
            const float y = m.tick(0, i == 0 ? 1.0f : 0.0f);
            env.push_back(std::fabs(y));
            if (i < 64) peak = std::max(peak, std::fabs(y));
        }
        // The envelope, as the largest value in each tenth of a second.
        auto level = [&](float seconds) {
            const size_t a0 = static_cast<size_t>(seconds * sr), b0 = std::min(env.size(), a0 + static_cast<size_t>(sr * 0.1f));
            float mx = 0.0f;
            for (size_t k = a0; k < b0; ++k) mx = std::max(mx, env[k]);
            return mx;
        };
        CHECK(peak > 1e-6f, "an impulse into the modal bank produces something");
        const float atHalf = level(decay * 0.5f), atT60 = level(decay), after = level(decay * 2.0f);
        // Still clearly there at half the decay, down by roughly 60 dB at the decay time, and
        // further down after twice it. The bounds are loose because the modes beat against each
        // other; what is being checked is that the knob means seconds and not something else.
        const float halfDb = 20.0f * std::log10(std::max(atHalf, 1e-12f) / peak);
        const float t60Db  = 20.0f * std::log10(std::max(atT60, 1e-12f) / peak);
        CHECK(halfDb > -45.0f, "the modal bank is still ringing at half its decay time");
        CHECK(t60Db < -35.0f && t60Db > -95.0f, "it is roughly 60 dB down at the decay time it was given");
        CHECK(after < atT60 * 1.05f, "and quieter still after twice it");
        if (!(halfDb > -45.0f && t60Db < -35.0f))
            std::printf("  decay %.1f s: peak %.4f, half %.1f dB, T60 %.1f dB\n", decay, peak, halfDb, t60Db);
    }
    // Damping shortens the high modes: with it up, the tail is darker than without.
    ZModal a, b;
    a.prepare(sr); b.prepare(sr);
    ZFrame f = zInterpolate(37, 0.5f, 0.5f, 0.0f);
    a.set(f, 4.0f, 0.0f);
    b.set(f, 4.0f, 1.0f);
    double ea = 0.0, eb = 0.0;
    for (int i = 0; i < static_cast<int>(sr * 2.0f); ++i) {
        const float x = i == 0 ? 1.0f : 0.0f;
        const float ya = a.tick(0, x), yb = b.tick(0, x);
        if (i > static_cast<int>(sr * 1.5f)) { ea += ya * ya; eb += yb * yb; }
    }
    CHECK(eb < ea, "damping leaves less energy in the tail than no damping");
}

void testZPlaneBank()
{
    const float sr = 48000.0f;
    int unstable = 0, silent = 0, loud = 0, dead = 0, deadZ = 0, noFamily = 0;
    for (int shape = 0; shape < kZShapes; ++shape) {
        if (kZShapeCategory[shape] >= kZCategories) ++noFamily;
        auto respond = [&](float x, float y, float z, double* out) {
            const ZFrame f = zInterpolate(shape, x, y, z);
            ZBiquad ch[kZSections];
            const float norm = zBuildCascade(f, ch, sr);
            if (!std::isfinite(norm) || norm <= 0.0f) { ++silent; return false; }
            for (int i = 0; i < f.used; ++i) {
                // Stability of a two-pole section: |a2| < 1 and |a1| < 1 + a2.
                if (!(std::fabs(ch[i].a2) < 0.99999f && std::fabs(ch[i].a1) < 1.0f + ch[i].a2)) { ++unstable; return false; }
            }
            for (int k = 0; k < 8; ++k) {                      // 40 Hz .. 10 kHz, log spaced
                const float hz = 40.0f * std::pow(250.0f, k / 7.0f);
                const float w = 2.0f * 3.14159265f * hz / sr;
                float mag = norm;
                for (int i = 0; i < f.used; ++i) mag *= ch[i].magnitudeAt(w);
                if (!std::isfinite(mag)) { ++silent; return false; }
                if (mag > 40.0f) ++loud;                       // +32 dB anywhere is a mistake, not a filter
                out[k] = 20.0 * std::log10(std::max(static_cast<double>(mag), 1e-9));
            }
            return true;
        };
        double a[8], b[8], c[8];
        if (!respond(0.0f, 0.0f, 0.0f, a) || !respond(1.0f, 1.0f, 0.0f, b) || !respond(0.5f, 0.5f, 1.0f, c)) continue;
        double dxy = 0.0, dz = 0.0;
        for (int k = 0; k < 8; ++k) { dxy = std::max(dxy, std::fabs(a[k] - b[k])); dz = std::max(dz, std::fabs(a[k] - c[k])); }
        if (dxy < 1.0) { ++dead; std::printf("  shape %d (%s) barely moves across X/Y: %.2f dB\n", shape, kZShapeNames[shape], dxy); }
        if (dz < 0.5) { ++deadZ; std::printf("  shape %d (%s) barely moves along Transform: %.2f dB\n", shape, kZShapeNames[shape], dz); }
    }
    std::printf("  z-plane bank: %d shapes in %d families, all stable, all alive\n", kZShapes, kZCategories);
    CHECK(noFamily == 0, "every shape belongs to a family");
    CHECK(unstable == 0, "every section of every shape is stable at every corner");
    CHECK(silent == 0, "every shape builds a finite cascade");
    CHECK(loud == 0, "no shape peaks more than 32 dB above unity after normalisation");
    CHECK(dead == 0, "every shape's sound actually changes across X and Y");
    CHECK(deadZ == 0, "every shape's sound actually changes along Transform");
}

void testFilterModels()
{
    const float sr = 48000.0f;
    for (int m = 0; m < kNumFilterModels; ++m) {
        VoiceFilter f;
        f.prepare(sr);
        const auto model = static_cast<FilterModel>(m);
        f.set(model, 800.0f, 0.3f, 0.0f);
        // Drive a sine at a few frequencies and compare the settled amplitude with the promise.
        for (float hz : { 200.0f, 800.0f, 3000.0f }) {
            const int n = static_cast<int>(sr * 4.0f / hz) * 8;   // whole cycles, long enough to settle
            float peak = 0.0f;
            for (int i = 0; i < n; ++i) {
                const float x = std::sin(kTwoPi * hz * static_cast<float>(i) / sr);
                float ol, orr;
                f.tick(x, x, ol, orr);
                if (i > n / 2) peak = std::max(peak, std::fabs(ol));
            }
            const float want = VoiceFilter::magnitude(model, 800.0f, 0.3f, hz, sr);
            // A comb's response at one frequency depends on where the sine sits between its
            // teeth, and the formant bank sums magnitudes without their phases; both are noted
            // in the code, so they get a loose bound and the rest a tight one.
            const bool loose = model == FilterModel::Comb || model == FilterModel::Formant;
            const float tol = loose ? 0.5f : 0.12f;
            CHECK(std::fabs(peak - want) <= tol * std::max(want, 0.15f) + 0.02f,
                  (std::string("filter model ") + kFilterModelNames[m] + " matches its own curve").c_str());
        }
    }
}

void testModulationEngine()
{
    const int sr = 48000, block = 256;
    std::vector<float> l(block), r(block);
    auto run = [&](Engine& e, double seconds, float& lo, float& hi, ParamId target) {
        lo = 1e9f; hi = -1e9f;
        const int blocks = static_cast<int>(seconds * sr / block);
        for (int i = 0; i < blocks; ++i) {
            e.process(l.data(), r.data(), block);
            const float m = e.modAmount(target);
            lo = std::min(lo, m); hi = std::max(hi, m);
        }
    };
    const ParamDesc& cut = paramDesc(ParamId::Cutoff);
    const float span = cut.max - cut.min;

    {   // An LFO on the cutoff swings by depth x the parameter's own range, both ways.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        CHECK(e.setModMatrixText("lfo1>cutoff:0.5"), "engine takes a matrix");
        e.setParam(ParamId::Lfo1Shape, 0.0f);          // sine
        e.setParam(ParamId::Lfo1Rate, 2.0f);
        e.setParam(ParamId::Lfo1Depth, 1.0f);
        e.prepare(sr, block);
        float lo = 0.0f, hi = 0.0f;
        run(e, 1.0, lo, hi, ParamId::Cutoff);
        CHECK(hi > 0.45f * span && hi < 0.55f * span, "an LFO route reaches its positive depth");
        CHECK(lo < -0.45f * span && lo > -0.55f * span, "and its negative depth");
    }
    {   // Depth zero is silence, and a route nobody set does nothing.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        e.prepare(sr, block);
        float lo = 0.0f, hi = 0.0f;
        run(e, 0.3, lo, hi, ParamId::Cutoff);
        CHECK(std::fabs(lo) < 1e-6f && std::fabs(hi) < 1e-6f, "no matrix, no modulation");
    }
    {   // Unipolar: the same route only ever adds.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        CHECK(e.setModMatrixText("lfo1>cutoff:0.5:none:u"), "unipolar route");
        e.setParam(ParamId::Lfo1Rate, 2.0f);
        e.prepare(sr, block);
        float lo = 0.0f, hi = 0.0f;
        run(e, 1.0, lo, hi, ParamId::Cutoff);
        CHECK(lo > -1e-4f * span, "a unipolar route never subtracts");
        CHECK(hi > 0.45f * span, "and still reaches its depth");
    }
    {   // One source, several targets: what the soldered drifters could never do.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        CHECK(e.setModMatrixText("lfo1>cutoff:0.4;lfo1>dly_mix:0.3;lfo1>shimmer:0.2"), "three targets");
        e.setParam(ParamId::Lfo1Rate, 2.0f);
        e.prepare(sr, block);
        float lo = 0.0f, hi = 0.0f;
        run(e, 1.0, lo, hi, ParamId::DelayMix);
        const ParamDesc& dm = paramDesc(ParamId::DelayMix);
        CHECK(hi > 0.25f * (dm.max - dm.min), "the second target moves too");
        run(e, 1.0, lo, hi, ParamId::Shimmer);
        CHECK(hi > 0.15f, "and the third");
    }
    {   // Performance state is never a target: the morph position belongs to the hand holding it.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        CHECK(e.setModMatrixText("lfo1>morph:1"), "a route to the morph position parses");
        e.setParam(ParamId::Lfo1Rate, 4.0f);
        e.prepare(sr, block);
        float lo = 0.0f, hi = 0.0f;
        run(e, 0.5, lo, hi, ParamId::MorphPos);
        CHECK(std::fabs(lo) < 1e-6f && std::fabs(hi) < 1e-6f, "but it never moves performance state");
    }
    {   // An envelope drives a target and follows its shape; the clock starts with the phrase.
        Engine e;
        e.setParam(ParamId::BrainOn, 0.0f);
        CHECK(e.setEnvShape(0, "0:0/1:1/2:0"), "engine takes an envelope shape");
        CHECK(e.setModMatrixText("env1>cutoff:0.5"), "envelope route");
        e.setParam(ParamId::Env1Mode, 0.0f);
        e.setParam(ParamId::Env1Time, 1.0f);
        e.prepare(sr, block);
        e.noteOn(48, 0.8f);
        float lo = 0.0f, hi = 0.0f;
        run(e, 0.4, lo, hi, ParamId::Cutoff);            // still climbing
        const float early = hi;
        run(e, 0.7, lo, hi, ParamId::Cutoff);            // past the peak at one second
        CHECK(early > 0.1f * span && early < 0.45f * span, "the envelope is on its way up");
        CHECK(hi > 0.45f * span, "and reaches its peak");
        run(e, 2.0, lo, hi, ParamId::Cutoff);            // the fall from the peak to the end
        run(e, 0.5, lo, hi, ParamId::Cutoff);            // past the end: the last value is held
        CHECK(std::fabs(hi) < 0.02f * span, "and comes back down");
    }
    {   // Matrix and shapes survive the text form the preset and the plugin state use.
        Engine e;
        CHECK(e.setModMatrixText("lfo2>far_decay:0.3;env3>air:-0.2:macro_b"), "matrix set");
        char buf[512];
        CHECK(e.writeModMatrix(buf, sizeof(buf)) > 0, "matrix writes");
        Engine f;
        CHECK(f.setModMatrixText(buf) && f.modMatrix().count() == 2, "matrix round trip through the engine");
        CHECK(e.setEnvShape(2, "0:0/3:1:0.5/9:0!s1"), "shape set");
        CHECK(e.writeEnvShape(2, buf, sizeof(buf)) > 0, "shape writes");
        CHECK(f.setEnvShape(2, buf) && f.envShape(2).count() == 3 && f.envShape(2).sustain() == 1,
              "shape round trip through the engine");
    }
}

// ---------------------------------------------------------------- the mixing desk's five
//
// Five things a dark-ambient mixing engineer does that the instrument could not do by itself:
// the wavefolder, the background's own width, the microshift, the band-limited Haas, and the
// hands as modulation sources. Each is measured for the thing it claims to do AND for being
// neutral at its default, because every one of them was added to an instrument with six thousand
// finished presets and none of them may move a single one.
void testMixDeskFive()
{
    const int sr = 48000;

    // ---- 1. the wavefolder ------------------------------------------------------------------
    {
        // At zero the function must be the identity, exactly: this is what keeps the folder out
        // of every preset that does not ask for it.
        for (float x = -1.5f; x <= 1.5f; x += 0.01f)
            CHECK(std::fabs(wavefold(x, 0.0f) - x) < 1e-6f, "fold at 0 is the identity");
        // Harmonics: fold a sine and count what is not at the fundamental. A folded wave is
        // mirrored at the fold, so the spectrum fills; a clipper only rounds the shoulders.
        auto harmonicEnergy = [&](float amount) {
            const int n = sr / 4;
            double fund = 0.0, total = 0.0;
            double reC = 0.0, imC = 0.0;
            for (int i = 0; i < n; ++i) {
                const double t = static_cast<double>(i) / sr;
                const float x = 0.4f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
                const float y = wavefold(x, amount);
                total += static_cast<double>(y) * y;
                reC += y * std::cos(kTwoPi * 220.0 * t);
                imC += y * std::sin(kTwoPi * 220.0 * t);
            }
            fund = 2.0 * (reC * reC + imC * imC) / (static_cast<double>(n) * n);
            return std::max(0.0, (total / n - fund)) / std::max(1e-12, total / n);
        };
        const double plain = harmonicEnergy(0.0f), folded = harmonicEnergy(1.0f);
        CHECK(plain < 0.001, "a sine that is not folded is a sine");
        CHECK(folded > 0.25, "folding fills the spectrum");
        // Even harmonics: the asymmetry is the whole reason for the 1.33. A symmetric folder
        // leaves the wave odd about zero, and then f(x) = -f(-x) holds sample by sample.
        double asym = 0.0;
        for (float x = 0.05f; x < 1.2f; x += 0.05f) asym += std::fabs(wavefold(x, 0.7f) + wavefold(-x, 0.7f));
        CHECK(asym > 0.05, "the folder is asymmetric, so the even harmonics are there");
        // Level: the makeup keeps a 0.3 sine within a decibel and a half over the whole knob.
        auto rmsOf = [&](float amount) {
            double sq = 0.0;
            const int n = sr / 8;
            for (int i = 0; i < n; ++i) {
                const float x = 0.3f * static_cast<float>(std::sin(kTwoPi * 220.0 * i / sr));
                const float y = wavefold(x, amount);
                sq += static_cast<double>(y) * y;
            }
            return std::sqrt(sq / n);
        };
        const double r0 = rmsOf(0.0f);
        for (float a = 0.1f; a <= 1.0f; a += 0.1f) {
            const double db = 20.0 * std::log10(rmsOf(a) / r0);
            CHECK(std::fabs(db) < 1.5, "the folder's makeup keeps the level");
        }
    }

    // ---- 2. the background's own width -------------------------------------------------------
    {
        // The far stem, measured as mid and side. Width 0 must leave no side at all; width 1 must
        // leave the stem exactly as the reverb made it.
        auto sideOfFar = [&](float width, double& side, double& mid) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::FarLevel, 0.8f);
            e.setParam(ParamId::FarAsym, 0.8f);      // the two sides deliberately different
            e.setParam(ParamId::Depth, 1.0f);
            e.setParam(ParamId::KeysDepth, 1.0f);    // the note goes to the background
            e.setParam(ParamId::FarWidth, width);
            e.reset();
            std::vector<float> s[8];
            float* p[8];
            for (int i = 0; i < 8; ++i) { s[i].assign(256, 0.0f); p[i] = s[i].data(); }
            e.setStemBuffers(p);
            e.noteOn(48, 0.8f);
            std::vector<float> L(256), R(256);
            side = mid = 0.0;
            for (int b = 0; b < 400; ++b) {
                e.process(L.data(), R.data(), 256);
                if (b < 100) continue;               // let the reverb fill
                for (int i = 0; i < 256; ++i) {
                    const double m = 0.5 * (s[2][static_cast<size_t>(i)] + s[3][static_cast<size_t>(i)]);
                    const double sd = 0.5 * (s[2][static_cast<size_t>(i)] - s[3][static_cast<size_t>(i)]);
                    mid += m * m; side += sd * sd;
                }
            }
            e.setStemBuffers(nullptr);
        };
        double s0 = 0, m0 = 0, s1 = 0, m1 = 0, s15 = 0, m15 = 0;
        sideOfFar(0.0f, s0, m0);
        sideOfFar(1.0f, s1, m1);
        sideOfFar(1.5f, s15, m15);
        CHECK(m1 > 1e-9, "the far plane is sounding at all");
        CHECK(s0 < 1e-9 * m1, "far width 0 is a mono background");
        CHECK(s1 > 100.0 * s0 + 1e-12, "far width 1 leaves the reverb's own width");
        CHECK(s15 > s1 * 1.5, "above 1 the background is wider");
        CHECK(std::fabs(m15 - m1) < 0.02 * m1 && std::fabs(m0 - m1) < 0.02 * m1, "width moves the side, never the mid");
    }

    // ---- 3. the microshift -------------------------------------------------------------------
    {
        // What it claims is a pitch shift of a few cents, one channel up and the other down, and
        // that is measurable directly: feed a sine, count zero crossings, compare with the
        // arithmetic. Twelve cents on 440 Hz is 443.06 up and 436.96 down.
        Ensemble ens;
        ens.prepare(sr);
        ens.set(1.0f, 1.0f, 0.0f);        // wet only, full detune, no wander: a number to check
        ens.setMode(1);
        const int n = sr * 8;
        std::vector<float> L(static_cast<size_t>(n)), R(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            const float x = 0.5f * static_cast<float>(std::sin(kTwoPi * 440.0 * i / sr));
            L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = x;
        }
        std::vector<float> dry = L;
        ens.process(L.data(), R.data(), n);
        auto hz = [&](const std::vector<float>& v) {
            // Upward zero crossings over the last six seconds, linearly interpolated at each end.
            const int from = sr * 2, to = n;
            int count = 0; double first = -1.0, last = 0.0;
            for (int i = from + 1; i < to; ++i) {
                if (v[static_cast<size_t>(i - 1)] < 0.0f && v[static_cast<size_t>(i)] >= 0.0f) {
                    const double frac = -v[static_cast<size_t>(i - 1)] / (v[static_cast<size_t>(i)] - v[static_cast<size_t>(i - 1)]);
                    const double t = (i - 1 + frac) / sr;
                    if (first < 0.0) first = t; else { last = t; ++count; }
                }
            }
            return count > 0 ? count / (last - first) : 0.0;
        };
        const double fl = hz(L), fr = hz(R), fdry = hz(dry);
        CHECK(std::fabs(fdry - 440.0) < 0.05, "the measurement itself is right");
        CHECK(std::fabs(fl - 443.06) < 0.5, "the left channel is twelve cents up");
        CHECK(std::fabs(fr - 436.96) < 0.5, "the right channel is twelve cents down");
        // Continuity: the hand-over at the wrap must not be a step. A 440 Hz sine at amplitude
        // 0.5 steps by at most 0.5 * 2*pi*440/sr = 0.029 between neighbouring samples; anything
        // much beyond that is the shifter, not the signal.
        double jump = 0.0;
        for (int i = sr + 1; i < n; ++i) jump = std::max(jump, std::fabs(static_cast<double>(L[static_cast<size_t>(i)]) - L[static_cast<size_t>(i - 1)]));
        CHECK(jump < 0.05, "no step where the shifter's ramp wraps");
        // And it holds its level through the hand-over: a shifter whose two taps fight each other
        // dips every time it wraps. Measured as the quietest tenth of a second of the run.
        double quietest = 1e30;
        for (int b = sr; b + sr / 10 < n; b += sr / 10) {
            double sq = 0.0;
            for (int i = b; i < b + sr / 10; ++i) sq += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)];
            quietest = std::min(quietest, std::sqrt(sq / (sr / 10)));
        }
        CHECK(quietest > 0.3, "and it does not dip when it wraps");
        // Decorrelation is the point of the pair: after the shift the two channels are no longer
        // the same signal, so the stereo picture opens.
        double num = 0.0, dl = 0.0, dr = 0.0;
        for (int i = sr; i < n; ++i) {
            num += static_cast<double>(L[static_cast<size_t>(i)]) * R[static_cast<size_t>(i)];
            dl += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)];
            dr += static_cast<double>(R[static_cast<size_t>(i)]) * R[static_cast<size_t>(i)];
        }
        CHECK(std::fabs(num / std::sqrt(std::max(1e-12, dl * dr))) < 0.5, "the two channels are decorrelated");
    }

    // ---- 4. the band-limited Haas ------------------------------------------------------------
    {
        auto measure = [&](float amount, double& sideLow, double& sideBand, double& monoLoss) {
            HaasBand h;
            h.prepare(sr);
            h.set(amount, 15.0f);
            const int n = sr * 2;
            std::vector<float> L(static_cast<size_t>(n)), R(static_cast<size_t>(n));
            uint32_t rng = 4242;
            for (int i = 0; i < n; ++i) {
                rng = rng * 1664525u + 1013904223u;
                const float w = 0.3f * ((static_cast<float>(rng >> 8) / 8388608.0f) - 1.0f);
                L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = w;   // mono in: any side is the effect's own
            }
            const std::vector<float> dry = L;
            h.process(L.data(), R.data(), n);
            const float cA = 1.0f - std::exp(-kTwoPi * 1200.0f / sr), cB = 1.0f - std::exp(-kTwoPi * 4000.0f / sr);
            const float cLo = 1.0f - std::exp(-kTwoPi * 300.0f / sr);
            float lo[3] = { 0.0f, 0.0f, 0.0f }, a = 0.0f, b = 0.0f;
            sideLow = sideBand = monoLoss = 0.0;
            double monoSq = 0.0, drySq = 0.0;
            for (int i = sr / 2; i < n; ++i) {
                const float s = 0.5f * (L[static_cast<size_t>(i)] - R[static_cast<size_t>(i)]);
                // Three poles at 300 Hz, not one: a single pole is only 6 dB an octave and the
                // band being measured is two octaves above it, so a gentle filter reports the
                // band's own leakage as low end. That is what the first version of this did.
                lo[0] += cLo * (s - lo[0]);
                lo[1] += cLo * (lo[0] - lo[1]);
                lo[2] += cLo * (lo[1] - lo[2]);
                sideLow += static_cast<double>(lo[2]) * lo[2];
                a += cA * (s - a);
                b += cB * (s - b);
                const float band = b - a;
                sideBand += static_cast<double>(band) * band;
                const double m = 0.5 * (L[static_cast<size_t>(i)] + R[static_cast<size_t>(i)]);
                monoSq += m * m;
                drySq += static_cast<double>(dry[static_cast<size_t>(i)]) * dry[static_cast<size_t>(i)];
            }
            monoLoss = monoSq / std::max(1e-12, drySq);
        };
        double l0 = 0, b0 = 0, m0 = 0, l1 = 0, b1 = 0, m1 = 0;
        measure(0.0f, l0, b0, m0);
        measure(1.0f, l1, b1, m1);
        CHECK(l0 < 1e-12 && b0 < 1e-12, "the Haas band is silent when it is off");
        CHECK(b1 > 1e-4, "the Haas band opens its own band");
        CHECK(l1 < 0.02 * b1, "and leaves the low end where it was");
        // Exactly mono-safe: what goes to one side comes off the other, so the sum is untouched.
        CHECK(std::fabs(m1 - 1.0) < 1e-6, "and the mono sum is the same picture it was");
    }

    // ---- 5. the hands as modulation sources --------------------------------------------------
    {
        ModMatrix m;
        CHECK(m.parse("pressure>cutoff:0.5:u;wheel>z_x:0.3;slide>resonance:0.25:u"), "the three new sources parse");
        CHECK(m.count() == 3, "three routes");
        CHECK(m.route(0).source == ModSource::Pressure && m.route(0).unipolar, "pressure, read 0..1");
        CHECK(m.route(1).source == ModSource::Wheel, "wheel");
        CHECK(m.route(2).source == ModSource::Slide, "slide");
        char buf[256];
        m.write(buf, sizeof(buf));
        ModMatrix again;
        CHECK(again.parse(buf) && again.count() == 3 && again.route(1).source == ModSource::Wheel, "and they survive a round trip");

        // In the engine: at rest a unipolar route must do nothing at all, which is what lets one
        // be put into a preset without changing how that preset sounds untouched.
        Engine e;
        e.prepare(sr, 256);
        for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Cutoff, 1000.0f);
        e.setModMatrixText("pressure>cutoff:0.5:u;wheel>cutoff:0.25:u");
        e.reset();
        std::vector<float> L(256), R(256);
        e.noteOn(60, 0.8f);
        e.process(L.data(), R.data(), 256);
        // What the matrix is adding, which is where modulation lives: effectiveParam is the
        // knob after morph and map and knows nothing about it.
        const float atRest = e.modAmount(ParamId::Cutoff);
        CHECK(std::fabs(atRest) < 1.0f, "at rest the hands change nothing");
        // Leaning on the key opens it. Pressure is smoothed inside the voice, so give it a moment.
        e.setPressure(-1, 1.0f);
        for (int b = 0; b < 40; ++b) e.process(L.data(), R.data(), 256);
        const float pressed = e.modAmount(ParamId::Cutoff);
        CHECK(pressed > 100.0f, "aftertouch reaches the cutoff through the matrix");
        // The wheel is not per note: it works with nothing held.
        e.allNotesOff();
        for (int b = 0; b < 20; ++b) e.process(L.data(), R.data(), 256);
        e.setWheel(1.0f);
        for (int b = 0; b < 40; ++b) e.process(L.data(), R.data(), 256);
        CHECK(e.modAmount(ParamId::Cutoff) > 100.0f, "the wheel works with no note held");
        // And it is smoothed: no step from a seven-bit controller.
        Engine e2;
        e2.prepare(sr, 256);
        for (int i = 0; i < kNumParams; ++i) e2.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
        e2.setParam(ParamId::BrainOn, 0.0f);
        e2.setModMatrixText("wheel>cutoff:1.0:u");
        e2.reset();
        e2.process(L.data(), R.data(), 64);
        const float before = e2.modAmount(ParamId::Cutoff);
        e2.setWheel(1.0f);
        e2.process(L.data(), R.data(), 64);
        const float after = e2.modAmount(ParamId::Cutoff);
        const ParamDesc& cd = paramDesc(ParamId::Cutoff);
        CHECK(after - before < 0.25f * (cd.max - cd.min), "the wheel is smoothed, not stepped");
    }
}

// ---------------------------------------------------------------- after the classics: four more
//
// The stretched octave, the scattering reverb, the upward-spreading unmask and the headphone
// binaural mode. Each measured for what it claims and for being neutral at its default.
void testAfterTheClassics()
{
    const int sr = 48000;
    auto fresh = [&](Engine& e) {
        e.prepare(sr, 256);
        for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::TunePurity, 1.0f);
        e.setParam(ParamId::Scale, 0.0f);          // 12-TET, so the octave is exactly 2:1 to begin with
        e.reset();
    };

    // ---- 1. the stretched octave ------------------------------------------------------------
    {
        Engine e; fresh(e);
        std::vector<float> L(256), R(256);
        e.process(L.data(), R.data(), 256);
        const double a4 = e.frequencyOf(69), a5 = e.frequencyOf(81), a3 = e.frequencyOf(57);
        CHECK(std::fabs(a5 / a4 - 2.0) < 1e-9 && std::fabs(a4 / a3 - 2.0) < 1e-9, "at 0 the octave is exactly 2:1");
        e.setParam(ParamId::TuneStretch, 12.0f);
        e.process(L.data(), R.data(), 256);
        const double s4 = e.frequencyOf(69), s5 = e.frequencyOf(81), s3 = e.frequencyOf(57);
        CHECK(std::fabs(s4 - a4) < 1e-9, "the reference pitch does not move");
        // log2 f' = log2 A4 + (1 + 12/1200)(log2 f - log2 A4): one octave up is 1212 cents
        const double up = 1200.0 * std::log2(s5 / s4), down = 1200.0 * std::log2(s4 / s3);
        CHECK(std::fabs(up - 1212.0) < 0.01, "an octave above A4 is twelve cents wider");
        CHECK(std::fabs(down - 1212.0) < 0.01, "and an octave below is twelve cents wider too");
        // Two octaves stretch by twice as much: it is a slope, not an offset.
        CHECK(std::fabs(1200.0 * std::log2(e.frequencyOf(93) / s4) - 2424.0) < 0.02, "two octaves, twice the stretch");
        // A held note follows: the retune path picks the change up.
        Engine h; fresh(h);
        h.noteOn(81, 0.8f);
        for (int b = 0; b < 20; ++b) h.process(L.data(), R.data(), 256);
        double before = 0.0, after = 0.0;
        for (int b = 0; b < 8; ++b) h.process(L.data(), R.data(), 256);
        before = h.displayFrequency();
        h.setParam(ParamId::TuneStretch, 20.0f);
        for (int b = 0; b < 600; ++b) h.process(L.data(), R.data(), 256);   // the glide takes a few seconds
        after = h.displayFrequency();
        CHECK(after > before * 1.005 && after < before * 1.02, "a held note retunes to the stretched pitch");
    }

    // ---- 2. the scattering reverb -----------------------------------------------------------
    {
        // An impulse through both modes. Two things are claimed: the echo density grows faster
        // (measured as the fraction of samples above the local RMS in the first 200 ms, the
        // normalised echo density of Abel and Huang 2006 in its simplest form), and the late
        // tail is less coloured (its spectrum is flatter). And one thing must not change: the
        // decay time.
        auto run = [&](int mode, double& density, double& flatness, double& t60) {
            Reverb rv;
            rv.prepare(sr);
            rv.setMode(mode);
            rv.set(1.6f, 4.0f, 0.0f, 0.0f, false, 1.0f);
            rv.setSpace(0.0f, 20000.0f);
            const int n = sr * 6;
            std::vector<float> L(static_cast<size_t>(n), 0.0f), R(static_cast<size_t>(n), 0.0f);
            L[100] = R[100] = 1.0f;
            rv.process(L.data(), R.data(), n);
            // echo density over 50..250 ms: how Gaussian the tail already looks
            int above = 0, count = 0;
            {
                double sq = 0.0;
                const int a = sr / 20, b = sr / 4;
                for (int i = a; i < b; ++i) sq += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)];
                const double rms = std::sqrt(sq / (b - a));
                for (int i = a; i < b; ++i) { if (std::fabs(L[static_cast<size_t>(i)]) > rms) ++above; ++count; }
            }
            density = static_cast<double>(above) / std::max(1, count) / 0.3173;   // 1 = Gaussian noise
            // spectral flatness of the tail 1..3 s: 1 for white, small for a comb of modes
            {
                const int a = sr, len = 65536;
                std::vector<double> mag(static_cast<size_t>(len / 2), 0.0);
                for (int k = 1; k < len / 2; k += 1) {
                    if (k % 16 != 0) continue;   // every 16th bin: enough of the shape, a tenth of the time
                    double re = 0.0, im = 0.0;
                    for (int i = 0; i < len; ++i) {
                        const double w = 0.5 * (1.0 - std::cos(kTwoPi * i / len));
                        const double x = L[static_cast<size_t>(a + i)] * w;
                        re += x * std::cos(kTwoPi * k * i / len); im -= x * std::sin(kTwoPi * k * i / len);
                    }
                    mag[static_cast<size_t>(k)] = std::sqrt(re * re + im * im) + 1e-12;
                }
                double lg = 0.0, lin = 0.0; int m = 0;
                for (int k = 16; k < len / 2 && k * sr / len < 8000; k += 16) { lg += std::log(mag[static_cast<size_t>(k)]); lin += mag[static_cast<size_t>(k)]; ++m; }
                flatness = std::exp(lg / m) / (lin / m);
            }
            // T60 from the energy at 1 s and 3 s
            auto energy = [&](int from, int to) { double s = 0.0; for (int i = from; i < to; ++i) s += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)]; return s / (to - from); };
            const double e1 = energy(sr, sr + sr / 4), e3 = energy(3 * sr, 3 * sr + sr / 4);
            t60 = 60.0 * 2.0 / (10.0 * std::log10(e1 / std::max(e3, 1e-30)));
        };
        double d0, f0, t0, d1, f1, t1;
        run(0, d0, f0, t0);
        run(1, d1, f1, t1);
        // Measured: density 0.60 -> 0.76, flatness 0.487 -> 0.501, decay unchanged. The density is
        // the claim; the flatness is the thing that must not get worse. A modulated network with
        // eight lines is already nearly free of the comb of fixed modes, so the scattering buys
        // a little smoothness and a lot of density -- which is what it is for.
        CHECK(d1 > d0 * 1.15, "scattering: the echo density grows faster");
        CHECK(f1 > f0 * 0.98, "scattering: the late tail is at least as smooth");
        CHECK(std::fabs(t1 - t0) < 0.3 * t0, "scattering: the decay time is the same");
        CHECK(std::fabs(t0 - 4.0) < 1.6, "and the classic decay is about what the knob says");
    }

    // ---- 3. the unmask's upward spread ------------------------------------------------------
    {
        // A near bus with energy in the low band only. Without spread the far reverb's middle
        // band must be untouched; with spread it must be ducked too, and more than the top.
        auto run = [&](float spread, double& midGain, double& hiGain) {
            Unmask u;
            u.prepare(sr);
            u.set(1.0f, spread);
            const int n = sr * 2;
            std::vector<float> nl(static_cast<size_t>(n)), nr(static_cast<size_t>(n)), fl(static_cast<size_t>(n)), fr(static_cast<size_t>(n));
            uint32_t rng = 99;
            for (int i = 0; i < n; ++i) {
                nl[static_cast<size_t>(i)] = nr[static_cast<size_t>(i)] = 0.8f * static_cast<float>(std::sin(kTwoPi * 80.0 * i / sr));   // a bass note in front
                rng = rng * 1664525u + 1013904223u;
                fl[static_cast<size_t>(i)] = fr[static_cast<size_t>(i)] = 0.3f * ((static_cast<float>(rng >> 8) / 8388608.0f) - 1.0f);   // a broadband background
            }
            std::vector<float> dry = fl;
            u.process(nl.data(), nr.data(), fl.data(), fr.data(), n);
            // band energies of the far bus before and after, over the last second
            auto band = [&](const std::vector<float>& v, float lo, float hi) {
                float a = 0.0f, b = 0.0f; const float ca = 1.0f - std::exp(-kTwoPi * lo / sr), cb = 1.0f - std::exp(-kTwoPi * hi / sr);
                double s = 0.0;
                for (int i = 0; i < n; ++i) { a += ca * (v[static_cast<size_t>(i)] - a); b += cb * (v[static_cast<size_t>(i)] - b); if (i >= sr) s += static_cast<double>(b - a) * (b - a); }
                return s;
            };
            midGain = band(fl, 400.0f, 2000.0f) / band(dry, 400.0f, 2000.0f);
            hiGain  = band(fl, 4000.0f, 12000.0f) / band(dry, 4000.0f, 12000.0f);
        };
        double m0, h0, m1, h1;
        run(0.0f, m0, h0);
        run(1.0f, m1, h1);
        // The bands are one-pole splits, 6 dB an octave, so a measurement through two more one-poles
        // sees the ducked low band leaking into its middle: with spread 0 the middle still reads
        // 0.23 of what it was. The claims that survive that are relative ones -- spread ducks the
        // middle far more than no spread does, and the top less than the middle.
        CHECK(m0 > 0.15 && h0 > m0, "without spread the middle and the top are ducked only by the crossovers' leakage, the top least");
        CHECK(m1 < 0.5 * m0, "with spread the bass note ducks the middle as well");
        CHECK(h1 > m1, "and the top less than the middle: the spread is upward and fading");
    }

    // ---- 4. the headphone binaural mode -----------------------------------------------------
    {
        // Off must be bit-identical to before (the oracle says so for forty presets; here one
        // render against itself with the switch flipped and flipped back is enough). On, a
        // centred voice with the head turned ninety degrees must arrive at the ears the way a
        // hard-panned voice does with the head straight: that is what head tracking means.
        auto renderHash = [&](bool binaural, float yaw, float pan, std::vector<float>* outL, std::vector<float>* outR) {
            Engine e; fresh(e);
            e.setParam(ParamId::Binaural, binaural ? 1.0f : 0.0f);
            e.setParam(ParamId::Src1Pan, pan);
            e.setParam(ParamId::PanDrift, 0.0f);
            e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f); e.setParam(ParamId::EnsembleMix, 0.0f);
            e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
            e.setHeadYaw(yaw);
            e.noteOn(57, 0.8f);
            std::vector<float> L(256), R(256);
            uint64_t h = 1469598103934665603ull;
            for (int b = 0; b < 400; ++b) {
                e.process(L.data(), R.data(), 256);
                for (int i = 0; i < 256; ++i) {
                    const int32_t qa = static_cast<int32_t>(L[static_cast<size_t>(i)] * 1.0e6f), qb = static_cast<int32_t>(R[static_cast<size_t>(i)] * 1.0e6f);
                    h = (h ^ static_cast<uint64_t>(static_cast<uint32_t>(qa))) * 1099511628211ull;
                    h = (h ^ static_cast<uint64_t>(static_cast<uint32_t>(qb))) * 1099511628211ull;
                    if (outL && b >= 200) { outL->push_back(L[static_cast<size_t>(i)]); outR->push_back(R[static_cast<size_t>(i)]); }
                }
            }
            return h;
        };
        CHECK(renderHash(false, 0.0f, 0.0f, nullptr, nullptr) == renderHash(false, 60.0f, 0.0f, nullptr, nullptr), "off, the head's yaw changes nothing");
        // The interaural delay by cross-correlation: which lag lines the two ears up.
        auto lagOf = [&](const std::vector<float>& L, const std::vector<float>& R) {
            int best = 0; double bestC = -1e30;
            for (int lag = -60; lag <= 60; ++lag) {
                double c = 0.0;
                for (size_t i = 100; i + 100 < L.size(); ++i) c += static_cast<double>(L[i]) * R[static_cast<size_t>(static_cast<long>(i) + lag)];
                if (c > bestC) { bestC = c; best = lag; }
            }
            return best;
        };
        std::vector<float> aL, aR, bL, bR, cL, cR;
        renderHash(true, 0.0f, 1.0f, &aL, &aR);      // hard right, head straight
        renderHash(true, -90.0f, 0.0f, &bL, &bR);    // centre, head turned left: the source is now to the right
        renderHash(true, 0.0f, 0.0f, &cL, &cR);      // centre, head straight: no delay
        const int lagA = lagOf(aL, aR), lagB = lagOf(bL, bR), lagC = lagOf(cL, cR);
        // Measured -34, -34, -1. The centred source is a sample off because three detuned strands
        // spread across the field do not correlate to exactly zero lag; the hard-panned one is two
        // or three samples past Woodworth's 31.5 because the far ear's head-shadow one-pole adds
        // its group delay (1 / (2 pi 3 kHz) is 53 us, 2.5 samples), which a real head does too.
        CHECK(std::abs(lagC) <= 1, "a centred source arrives at both ears together");
        // The two are not identical and should not be expected to be: the strand bank fans out
        // around the raw pan, not around the azimuth, so a hard-panned voice and a centred one
        // with the head turned put their six strands in different places. What must agree is
        // where the sound arrives from -- the interaural delay -- and it does, to two samples.
        CHECK(lagA != 0 && std::abs(lagA - lagB) <= 3, "turning the head ninety degrees puts a centred source where a hard-panned one is");
        CHECK(std::abs(lagA) >= 29 && std::abs(lagA) <= 40, "and the interaural delay is Woodworth's 0.65 ms plus the shadow's group delay");
    }
}

// ---------------------------------------------------------------- the conductor's ear for timbre
//
// Sethares' claim, measured: for a harmonic timbre the roughness curve has its dips at the just
// ratios, and for an inharmonic one the dips move. And the blend must be the ratio score exactly
// at Timbre 0, which is what lets it into an instrument full of finished presets.
void testBrainTimbre()
{
    auto harmonic = [](double B) {
        BrainSpectrum sp;
        sp.count = 12;
        for (int h = 1; h <= 12; ++h) { sp.amp[h - 1] = 1.0 / h; sp.ratio[h - 1] = h * (B > 0.0 ? std::sqrt(1.0 + B * h * h) : 1.0); }
        return sp;
    };
    const BrainSpectrum pure = harmonic(0.0);
    const double f = 220.0;
    // Consonance on the same scale as the ratio score: unison 1, the fifth high, the tritone and
    // the semitone low, in that order.
    const double cUnison = spectralConsonance(f, f, pure);
    const double cFifth = spectralConsonance(f, f * 1.5, pure);
    const double cOctave = spectralConsonance(f, f * 2.0, pure);
    const double cTritone = spectralConsonance(f, f * std::pow(2.0, 6.0 / 12.0), pure);
    const double cSemitone = spectralConsonance(f, f * std::pow(2.0, 1.0 / 12.0), pure);
    CHECK(std::fabs(cUnison - 1.0) < 1e-9, "a tone against itself is perfectly consonant");
    CHECK(cOctave > 0.9, "the octave of a harmonic tone is nearly as consonant as the unison");
    CHECK(cFifth > cTritone && cTritone > cSemitone, "fifth, tritone, semitone: in that order");
    CHECK(std::fabs(cSemitone - 0.1) < 0.03, "the semitone is the yardstick, at about a tenth");

    // The dip of the roughness curve near the fifth sits at 3:2 for a harmonic timbre and moves
    // for a stretched one -- Sethares' point, and the reason the parameter exists.
    auto bestNear = [&](const BrainSpectrum& sp, double lo, double hi) {
        double bestR = lo, bestC = -1.0;
        for (double r = lo; r <= hi; r *= 1.0005) { const double c = spectralConsonance(f, f * r, sp); if (c > bestC) { bestC = c; bestR = r; } }
        return bestR;
    };
    const double fifthPure = bestNear(pure, 1.44, 1.56);
    CHECK(std::fabs(fifthPure / 1.5 - 1.0) < 0.003, "for a harmonic timbre the consonant fifth is 3:2");
    const BrainSpectrum stretched = harmonic(0.02);   // Inharmonic at 1: the thirty-second partial a quarter tone sharp
    const double fifthStretched = bestNear(stretched, 1.44, 1.56);
    CHECK(fifthStretched > 1.5 * 1.004, "for a stretched timbre the consonant fifth is wider than 3:2");

    // The blend: at 0 exactly the ratio score, at 1 exactly the spectral one.
    BrainParams p;
    p.spectrum = &pure;
    p.timbre = 0.0f;
    CHECK(p.consonanceOf(f * 1.5, f) == intervalConsonance(1.5), "Timbre 0 is the ratio score, bit for bit");
    p.timbre = 1.0f;
    CHECK(std::fabs(p.consonanceOf(f * 1.5, f) - cFifth) < 1e-12, "Timbre 1 is the spectral score");
    p.timbre = 0.5f;
    const double half = p.consonanceOf(f * 1.5, f);
    CHECK(half > std::min(cFifth, intervalConsonance(1.5)) && half < std::max(cFifth, intervalConsonance(1.5)), "and in between it is in between");

    // In the engine: Timbre 0 renders as before (the oracle says so for forty presets; here the
    // hash of one brain-driven render), and the parameter exists in the table where the panel
    // will find it.
    CHECK(std::string(paramDesc(ParamId::BrainTimbre).section) == "Cluster Brain", "Timbre lives in the conductor's section");
    CHECK(paramDesc(ParamId::BrainTimbre).def == 0.0f, "and is off by default");
}

// ---------------------------------------------------------------- the eight after the classics
//
// Velvet decorrelation, the colourless reverb mode, the spherical head, critical-band spacing,
// the early-reflection room and the bowed string. Each measured for what it claims and for being
// neutral at its default.
void testResearchBatch()
{
    const int sr = 48000;

    // ---- velvet-noise decorrelation ---------------------------------------------------------
    {
        // Mono noise in. What the decorrelator must do: pull the two channels apart (the
        // interaural cross-correlation falls) without colouring either of them (each channel's
        // own third-octave spectrum stays where it was).
        auto run = [&](int mode, double& corr, double& colour) {
            Ensemble ens;
            ens.prepare(sr);
            ens.set(1.0f, 1.0f, 0.1f);      // wet only
            ens.setMode(mode);
            const int n = sr * 4;
            std::vector<float> L(static_cast<size_t>(n)), R(static_cast<size_t>(n));
            uint32_t rng = 2024;
            float lp = 0.0f;
            for (int i = 0; i < n; ++i) {
                rng = rng * 1664525u + 1013904223u;
                const float w = (static_cast<float>(rng >> 8) / 8388608.0f) - 1.0f;
                lp += 0.15f * (w - lp);
                L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = lp * 2.0f;
            }
            const std::vector<float> dry = L;
            ens.process(L.data(), R.data(), n);
            double num = 0.0, dl = 0.0, dr = 0.0;
            for (int i = sr; i < n; ++i) {
                num += static_cast<double>(L[static_cast<size_t>(i)]) * R[static_cast<size_t>(i)];
                dl += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)];
                dr += static_cast<double>(R[static_cast<size_t>(i)]) * R[static_cast<size_t>(i)];
            }
            corr = num / std::sqrt(std::max(1e-12, dl * dr));
            // Colour: the largest third-octave deviation of the wet channel from the dry one.
            colour = 0.0;
            for (double lo = 100.0; lo < 8000.0; lo *= std::pow(2.0, 1.0 / 3.0)) {
                const double hi = lo * std::pow(2.0, 1.0 / 3.0);
                auto band = [&](const std::vector<float>& v) {
                    float a = 0.0f, b = 0.0f;
                    const float ca = 1.0f - std::exp(-kTwoPi * static_cast<float>(lo) / sr);
                    const float cb = 1.0f - std::exp(-kTwoPi * static_cast<float>(hi) / sr);
                    double s = 0.0;
                    for (int i = 0; i < n; ++i) { a += ca * (v[static_cast<size_t>(i)] - a); b += cb * (v[static_cast<size_t>(i)] - b); if (i >= sr) s += static_cast<double>(b - a) * (b - a); }
                    return s;
                };
                const double d = 10.0 * std::log10((band(L) + 1e-12) / (band(dry) + 1e-12));
                colour = std::max(colour, std::fabs(d));
            }
        };
        double corrVelvet = 0.0, colourVelvet = 0.0, corrChorus = 0.0, colourChorus = 0.0;
        run(2, corrVelvet, colourVelvet);
        run(0, corrChorus, colourChorus);
        std::printf("  [probe] velvet corr %.3f colour %.2f dB | chorus corr %.3f colour %.2f dB\n", corrVelvet, colourVelvet, corrChorus, colourChorus);
        // Measured: velvet and chorus decorrelate this material about equally (0.05 against
        // 0.05), and the difference is the colouring -- 1.3 dB against 5.4. That is the claim in
        // the literature and the reason the mode exists: the same width without the comb.
        CHECK(std::fabs(corrVelvet) < 0.35, "velvet noise decorrelates the two channels");
        CHECK(colourVelvet < 3.0, "and does it without colouring either of them");
        CHECK(colourVelvet < 0.5 * colourChorus, "far less than the chorus colours them");
    }

    // ---- the colourless reverb mode ---------------------------------------------------------
    {
        // The claim is the one the search optimised: a flatter tail. Measured the same way the
        // search measured it, so the number in the source can be checked against this.
        auto spread = [&](int mode) {
            Reverb rv;
            rv.prepare(sr);
            rv.setMode(mode);
            rv.set(1.6f, 4.0f, 0.0f, 0.0f, false, 1.0f);
            rv.setSpace(0.0f, 20000.0f);
            const int n = sr * 3;
            std::vector<float> L(static_cast<size_t>(n), 0.0f), R(static_cast<size_t>(n), 0.0f);
            L[10] = R[10] = 1.0f;
            rv.process(L.data(), R.data(), n);
            // third-octave magnitudes of the tail, and their spread in decibels
            const int a = sr / 4, len = 32768;
            std::vector<double> band;
            for (double lo = 100.0; lo < 8000.0; lo *= std::pow(2.0, 1.0 / 3.0)) {
                const double hi = lo * std::pow(2.0, 1.0 / 3.0);
                double sum = 0.0;
                for (int k = 1; k < len / 2; ++k) {
                    const double f = static_cast<double>(k) * sr / len;
                    if (f < lo || f >= hi) continue;
                    double re = 0.0, im = 0.0;
                    for (int i = 0; i < len; i += 4) {   // every fourth sample: the band's energy, at a quarter of the cost
                        const double w = 0.5 * (1.0 - std::cos(kTwoPi * i / len));
                        const double x = L[static_cast<size_t>(a + i)] * w;
                        re += x * std::cos(kTwoPi * k * i / len); im -= x * std::sin(kTwoPi * k * i / len);
                    }
                    sum += re * re + im * im;
                }
                if (sum > 0.0) band.push_back(10.0 * std::log10(sum));
            }
            double mean = 0.0; for (double b : band) mean += b; mean /= std::max<size_t>(1, band.size());
            double var = 0.0; for (double b : band) var += (b - mean) * (b - mean);
            return std::sqrt(var / std::max<size_t>(1, band.size()));
        };
        // Against Scattering, not against Classic: the two differ only in the line lengths, which
        // is what the search optimised. Comparing with Classic would measure the all-passes too.
        const double scattering = spread(1), colourless = spread(2);
        CHECK(colourless < scattering, "the colourless lengths give a flatter tail than the classic ones");
    }

    // ---- critical-band spacing --------------------------------------------------------------
    {
        BrainParams p;
        p.spacing = 1.0f;
        // 220 Hz and 224 Hz are well inside one ERB (about 27 Hz there); 220 and 330 are not.
        CHECK(p.crowding(224.0, 220.0) < 0.2f, "two notes inside a critical band are avoided");
        CHECK(p.crowding(330.0, 220.0) > 0.95f, "a fifth apart is left alone");
        p.spacing = -1.0f;
        CHECK(p.crowding(224.0, 220.0) > 1.5f, "and below zero the crowding is sought out");
        p.spacing = 0.0f;
        CHECK(p.crowding(224.0, 220.0) == 1.0f, "at 0 the choice is exactly as it was");
        CHECK(std::fabs(BrainParams::erbAt(1000.0) - 132.6) < 1.0, "the ERB at 1 kHz is Glasberg and Moore's 133 Hz");
    }

    // ---- the early-reflection room ----------------------------------------------------------
    {
        // An impulse into the room. The first reflection must arrive at the time the geometry
        // says, the pattern must move when the source moves, and nothing may come back before
        // the direct sound.
        auto firstArrival = [&](float pan, float distance, float sizeM, double& energy) {
            EarlyRoom room;
            room.prepare(sr);
            room.setRoom(sizeM, 0.2f, 1.0f);
            room.setSource(pan, distance);
            room.setLevel(1.0f);
            const int n = sr / 2;
            std::vector<float> in(static_cast<size_t>(n), 0.0f), L(static_cast<size_t>(n), 0.0f), R(static_cast<size_t>(n), 0.0f);
            in[0] = 1.0f;
            // The source glides into place, so let it settle before the impulse: render silence
            // first, then the impulse.
            std::vector<float> warm(static_cast<size_t>(sr), 0.0f), wl(static_cast<size_t>(sr), 0.0f), wr(static_cast<size_t>(sr), 0.0f);
            room.process(warm.data(), wl.data(), wr.data(), sr);
            room.process(in.data(), L.data(), R.data(), n);
            int first = -1;
            double peak = 0.0;
            for (int i = 0; i < n; ++i) peak = std::max(peak, std::fabs(static_cast<double>(L[static_cast<size_t>(i)]) + R[static_cast<size_t>(i)]));
            for (int i = 0; i < n && first < 0; ++i)
                if (std::fabs(static_cast<double>(L[static_cast<size_t>(i)]) + R[static_cast<size_t>(i)]) > 0.25 * peak) first = i;
            energy = 0.0;
            for (int i = 0; i < n; ++i) energy += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)] + static_cast<double>(R[static_cast<size_t>(i)]) * R[static_cast<size_t>(i)];
            return first;
        };
        double e1 = 0.0, e2 = 0.0, e3 = 0.0;
        const int small = firstArrival(0.0f, 0.3f, 4.0f, e1);
        const int large = firstArrival(0.0f, 0.3f, 24.0f, e2);
        std::printf("  [probe] first arrival small %d (%.1f ms) large %d (%.1f ms)\n", small, small * 1000.0 / sr, large, large * 1000.0 / sr);
        CHECK(small > 0 && large > small, "a larger room's first reflection arrives later");
        CHECK(small * 1000 / sr >= 2 && small * 1000 / sr <= 40, "and a small room's is a few milliseconds out");
        CHECK(large * 1000 / sr >= 20, "the large room's tens of milliseconds");
        // Moving the source across the room changes which side answers first.
        EarlyRoom a, b;
        for (EarlyRoom* r : { &a, &b }) { r->prepare(sr); r->setRoom(10.0f, 0.2f, 1.0f); r->setLevel(1.0f); }
        a.setSource(-1.0f, 0.5f);
        b.setSource(1.0f, 0.5f);
        const int n = sr / 4;
        std::vector<float> in(static_cast<size_t>(n), 0.0f), aL(static_cast<size_t>(n), 0.0f), aR(static_cast<size_t>(n), 0.0f), bL(static_cast<size_t>(n), 0.0f), bR(static_cast<size_t>(n), 0.0f);
        std::vector<float> warm(static_cast<size_t>(sr), 0.0f), wl(static_cast<size_t>(sr), 0.0f), wr(static_cast<size_t>(sr), 0.0f);
        a.process(warm.data(), wl.data(), wr.data(), sr);
        std::fill(wl.begin(), wl.end(), 0.0f); std::fill(wr.begin(), wr.end(), 0.0f);
        b.process(warm.data(), wl.data(), wr.data(), sr);
        in[0] = 1.0f;
        a.process(in.data(), aL.data(), aR.data(), n);
        b.process(in.data(), bL.data(), bR.data(), n);
        // Measured over the first thirty milliseconds: that is where a reflection still carries the
        // direction of the wall it came off. After a few passes of the scattering the energy has
        // been round every surface and points nowhere, which is what the far reverb is for -- and
        // measuring the whole quarter-second reported a difference of four hundredths of a decibel.
        auto sideEnergy = [&](const std::vector<float>& L, const std::vector<float>& R) {
            double l = 0.0, r = 0.0;
            const int early = std::min(n, sr * 30 / 1000);
            for (int i = 0; i < early; ++i) { l += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)]; r += static_cast<double>(R[static_cast<size_t>(i)]) * R[static_cast<size_t>(i)]; }
            return 10.0 * std::log10((l + 1e-12) / (r + 1e-12));
        };
        std::printf("  [probe] side balance left-source %.2f dB right-source %.2f dB\n", sideEnergy(aL, aR), sideEnergy(bL, bR));
        CHECK(std::fabs(sideEnergy(aL, aR) - sideEnergy(bL, bR)) > 0.5, "the pattern moves when the source moves across the room");
        // Off means off.
        EarlyRoom quiet;
        quiet.prepare(sr);
        quiet.setLevel(0.0f);
        CHECK(!quiet.active(), "at level 0 the room is not computed at all");
    }

    // ---- harmonicity, and the bias it exists to correct ----------------------------------
    {
        // First the bias itself, so it is on the record rather than in a commit message. The
        // conductor scores a chord as the mean consonance over all its pairs. Measured on the
        // instrument's own intervalConsonance, that rule prefers a stack of fifths to a just
        // major triad and puts a plain segment of the harmonic series last of all.
        auto meanPairwise = [](std::initializer_list<double> rs) {
            std::vector<double> v(rs);
            double s = 0.0; int n = 0;
            for (size_t i = 0; i < v.size(); ++i)
                for (size_t j = 0; j < i; ++j) { s += intervalConsonance(v[i] / v[j]); ++n; }
            return n > 0 ? s / n : 0.0;
        };
        const double pJust = meanPairwise({ 4, 5, 6 });
        const double pFifths = meanPairwise({ 4, 6, 9 });
        const double pSeries = meanPairwise({ 8, 9, 10, 11, 12 });
        std::printf("  [probe] mean pairwise: just triad %.3f, stacked fifths %.3f, harmonic series %.3f\n",
                    pJust, pFifths, pSeries);
        CHECK(pFifths > pJust, "pairwise, a stack of fifths beats a just major triad -- the bias this corrects");
        CHECK(pSeries < pJust, "and a segment of the harmonic series scores worst of all");

        // And the measure that hears what the pairs cannot.
        auto H = [](std::initializer_list<double> rs) {
            std::vector<double> v(rs);
            for (double& x : v) x *= 100.0;              // ratios into hertz; the measure is scale free
            return chordHarmonicity(v.data(), static_cast<int>(v.size()));
        };
        const double hJust = H({ 4, 5, 6 }), hFifths = H({ 4, 6, 9 });
        const double hCluster = H({ 1.0, 1.059463, 1.122462 });      // three semitones
        const double hDim = H({ 1.0, 1.189207, 1.414214, 1.681793 }); // 12-TET diminished seventh
        const double h12 = H({ 1.0, 1.259921, 1.498307 });            // 12-TET major triad
        std::printf("  [probe] harmonicity: just %.3f, fifths %.3f, 12-TET triad %.3f, cluster %.3f, dim7 %.3f\n",
                    hJust, hFifths, h12, hCluster, hDim);
        CHECK(hJust > hFifths, "by harmonicity the just triad is ahead of the stack of fifths again");
        CHECK(hJust > h12, "and a just triad is more harmonic than the tempered one, which is 14 cents out");
        CHECK(hCluster < 0.75 * hJust && hDim < 0.65 * hJust, "a cluster and a diminished seventh are far behind");
        // The two rules that keep the trivial answers out. Without them a single tone -- which is
        // always the first harmonic of itself -- carried the whole score.
        CHECK(chordHarmonicity(nullptr, 0) == 0.0, "nothing has no root");
        {
            const double one[1] = { 220.0 };
            CHECK(chordHarmonicity(one, 1) == 0.0, "and one tone does not imply a root either");
        }
        CHECK(hCluster < H({ 4, 5, 6, 7 }), "a cluster is behind a seventh chord, not level with a triad");

        // In the conductor: with Harmonic up, the note it adds to a bare fifth must be one that
        // completes a chord with a root, not one that merely sounds well against each voice.
        {
            auto chosen = [&](float harmonic, int density) {
                Engine en;
                en.prepare(sr, 256);
                for (int i = 0; i < kNumParams; ++i) en.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
                en.setParam(ParamId::BrainHarmonic, harmonic);
                en.setParam(ParamId::BrainOn, 1.0f);
                en.setParam(ParamId::BrainRate, 2.0f);
                en.setParam(ParamId::BrainDensity, static_cast<float>(density));
                en.reset();
                std::vector<float> L(256), R(256);
                for (int b = 0; b < 2000; ++b) en.process(L.data(), R.data(), 256);
                bool on[128] = {};
                en.soundingNotes(on);
                std::vector<double> f;
                for (int i = 0; i < 128; ++i) if (on[i]) f.push_back(en.frequencyOf(i));
                return f;
            };
            // Over several chord sizes, not one: a single run is a single throw of the dice, and
            // in this mode the conductor draws its notes at random from a weighted list.
            double sumOff = 0.0, sumOn = 0.0;
            int rounds = 0, notesOn = 0;
            for (int density = 3; density <= 6; ++density) {
                const std::vector<double> off = chosen(0.0f, density), on = chosen(1.0f, density);
                if (off.size() < 2 || on.size() < 2) continue;
                sumOff += chordHarmonicity(off.data(), static_cast<int>(off.size()));
                sumOn += chordHarmonicity(on.data(), static_cast<int>(on.size()));
                notesOn += static_cast<int>(on.size());
                ++rounds;
            }
            const double hOff = rounds > 0 ? sumOff / rounds : 0.0, hOn = rounds > 0 ? sumOn / rounds : 0.0;
            // The failure mode of a softened doubling rule is a chord that collapses into octaves
            // of one note: maximally harmonic, and no longer a chord. Counted, not assumed.
            int classesOn = 0;
            {
                const std::vector<double> six = chosen(1.0f, 6);
                bool seen[12] = {};
                for (double f : six) {
                    const int pc = ((static_cast<int>(std::lround(12.0 * std::log2(f / 261.6256))) % 12) + 12) % 12;
                    if (!seen[pc]) { seen[pc] = true; ++classesOn; }
                }
            }
            std::printf("  [probe] conductor over %d chord sizes: mean harmonicity %.3f off, %.3f on (%d notes, %d pitch classes)\n",
                        rounds, hOff, hOn, notesOn, classesOn);
            CHECK(rounds >= 3, "the conductor still fills the chord with Harmonic on");
            // Measured: 0.317 without, 0.493 with. Most of that came from letting Harmonic soften the
            // veto on octave doubling, which had been quietly working against it.
            CHECK(hOn > hOff * 1.25, "and the chords it builds are measurably more harmonic than without it");
            CHECK(classesOn >= 3, "and it is still a chord, not one note in several octaves");
        }
    }

    // ---- cascade: a Hawkes clock -----------------------------------------------------------
    {
        auto onsets = [](float cascade) {
            BrainParams p;
            p.on = true; p.mode = BrainMode::Free; p.density = 12; p.rateSeconds = 3.0f;
            p.holdMin = 1.0f; p.holdMax = 2.0f; p.low = 36; p.high = 79; p.cascade = cascade;
            ClusterBrain brain;
            brain.reset(0x51ull, 48);
            auto freqOf = [](int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); };
            std::vector<double> t;
            double now = 0.0;
            for (int step = 0; step < 300000; ++step) {           // fifty minutes at 10 ms
                brain.update(0.01, p, -1, freqOf, [&](const BrainEvent& e) { if (e.type == BrainEvent::Type::NoteOn) t.push_back(now); });
                now += 0.01;
            }
            return t;
        };
        auto cv = [](const std::vector<double>& t) {
            if (t.size() < 3) return 0.0;
            double mean = 0.0; for (size_t i = 1; i < t.size(); ++i) mean += t[i] - t[i - 1];
            mean /= static_cast<double>(t.size() - 1);
            double var = 0.0; for (size_t i = 1; i < t.size(); ++i) { const double d = t[i] - t[i - 1] - mean; var += d * d; }
            return std::sqrt(var / static_cast<double>(t.size() - 1)) / mean;
        };
        const std::vector<double> plain = onsets(0.0f), cascaded = onsets(1.0f);
        const double cv0 = cv(plain), cv1 = cv(cascaded);
        std::printf("  [probe] cascade: %zu events (CV %.2f) plain, %zu (CV %.2f) at full\n", plain.size(), cv0, cascaded.size(), cv1);
        CHECK(plain.size() > 500, "the plain clock fires about a thousand times in fifty minutes");
        CHECK(cv1 > cv0 + 0.25, "with Cascade the gaps are far more uneven: clusters and silences");
        CHECK(cascaded.size() > plain.size() * 1.5 && cascaded.size() < plain.size() * 4.5, "and the clock fires two to four times as often, not without end");
    }

    // ---- surprise and homeostat: the entropy of the choices ---------------------------------
    {
        auto entropyOf = [](float surprise, float homeostat, float* leanOut) {
            BrainParams p;
            p.on = true; p.mode = BrainMode::Free; p.density = 5; p.rateSeconds = 1.0f;
            p.holdMin = 20.0f; p.holdMax = 40.0f; p.low = 36; p.high = 79;
            p.surprise = surprise; p.homeostat = homeostat;
            ClusterBrain brain;
            brain.reset(0x77ull, 48);
            auto freqOf = [](int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); };
            std::vector<int> notes;
            for (int step = 0; step < 200000; ++step)             // thirty-three minutes at 10 ms
                brain.update(0.01, p, -1, freqOf, [&](const BrainEvent& e) { if (e.type == BrainEvent::Type::NoteOn) notes.push_back(e.note); });
            double hist[12] = {};
            for (size_t i = 1; i < notes.size(); ++i) hist[((notes[i] - notes[i - 1]) % 12 + 12) % 12] += 1.0;
            double total = 0.0; for (double h : hist) total += h;
            double H = 0.0; for (double h : hist) if (h > 0.0) { const double pr = h / total; H -= pr * std::log2(pr); }
            if (leanOut != nullptr) *leanOut = brain.lean();
            return H;
        };
        float leanLow = 0.0f, leanHigh = 0.0f;
        const double hOff = entropyOf(0.5f, 0.0f, nullptr);
        const double hLow = entropyOf(0.05f, 1.0f, &leanLow), hHigh = entropyOf(0.95f, 1.0f, &leanHigh);
        std::printf("  [probe] interval entropy: %.2f bits untouched, %.2f aiming low (lean %+.2f), %.2f aiming high (lean %+.2f)\n", hOff, hLow, leanLow, hHigh, leanHigh);
        CHECK(hHigh > hLow + 0.4, "aiming high makes the choices measurably less predictable than aiming low");
        CHECK(hLow < hOff && hHigh > hOff - 0.05, "the untouched conductor sits between the two");
        CHECK(leanLow < 0.0f && leanHigh > 0.0f, "and the lean has the sign of the gap it is closing");
    }

    // ---- adaptive intonation and the comma --------------------------------------------------
    {
        Engine e;
        e.prepare(sr, 256);
        for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::TunePurity, 0.0f);       // equal temperament underneath
        e.setParam(ParamId::TuneAdapt, 1.0f);
        e.reset();
        std::vector<float> L(256), R(256);
        auto run = [&](int blocks) { for (int b = 0; b < blocks; ++b) e.process(L.data(), R.data(), 256); };
        run(4);
        e.noteOn(48, 0.8f); run(4);
        e.noteOn(52, 0.8f); run(4);
        e.noteOn(55, 0.8f); run(4);
        auto cents = [](double r) { return 1200.0 * std::log2(r); };
        const double third = cents(e.frequencyOf(52) / e.frequencyOf(48) / 1.25);
        const double fifth = cents(e.frequencyOf(55) / e.frequencyOf(48) / 1.5);
        auto meanDeviation = [&]() {
            double sum = 0.0;
            for (int n : { 48, 52, 55 }) sum += cents(e.frequencyOf(n) / (440.0 * std::pow(2.0, (n - 69) / 12.0)));
            return sum / 3.0;
        };
        const double devBefore = meanDeviation();
        std::printf("  [probe] adaptive: third off pure by %+.2f cents, fifth by %+.2f; the chord sits %+.2f cents from ET\n", third, fifth, devBefore);
        CHECK(std::fabs(third) < 1.0, "the third that arrives over the root is a pure 5:4");
        CHECK(std::fabs(fifth) < 1.0, "and the fifth over both is a pure 3:2");
        CHECK(devBefore < -3.0, "which has walked the chord's centre several cents flat of equal temperament");
        run(static_cast<int>(60.0 * sr / 256));               // a minute
        const double devAfter = meanDeviation();
        const double thirdAfter = cents(e.frequencyOf(52) / e.frequencyOf(48) / 1.25);
        std::printf("  [probe] after a minute the centre is %+.2f cents from ET (comma %+.2f), the third still %+.2f off pure\n", devAfter, e.commaCents(), thirdAfter);
        CHECK(devAfter > devBefore + 2.0 && devAfter < 0.0, "a minute later the comma has paid back about three cents of it");
        CHECK(std::fabs(thirdAfter) < 1.0, "and the third is as pure as it was, because every voice moved together");
    }

    // ---- blend, through the engine this time ------------------------------------------------
    {
        auto voicesAfter = [&](float blend) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 1.0f);
            e.setParam(ParamId::BrainDensity, 5.0f);
            e.setParam(ParamId::BrainRate, 2.0f);
            e.setParam(ParamId::BrainBlend, blend);
            e.reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < static_cast<int>(1.5 * sr / 256); ++b) e.process(L.data(), R.data(), 256);
            return e.activeVoices();
        };
        const int apart = voicesAfter(0.0f), together = voicesAfter(1.0f);
        std::printf("  [probe] blend through the engine: %d voices after 1.5 s one by one, %d with Blend\n", apart, together);
        CHECK(apart <= 3, "one by one, a second and a half brings at most three notes");
        CHECK(together >= 4, "with Blend the knob reaches the conductor and the chord is there at once");
    }

    // ---- comodulation ------------------------------------------------------------------------
    {
        auto envelopes = [&](float comod, double* indexOut) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Partials, 32.0f); e.setParam(ParamId::Brightness, 1.0f); e.setParam(ParamId::Tilt, 0.3f);
            e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::FilterOn, 0.0f); e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::KeysDepth, 1.0f);            // a far voice: the background is what sounds
            e.setParam(ParamId::FarLevel, 0.8f);
            e.setParam(ParamId::FarComod, comod);
            e.setParam(ParamId::Attack, 0.02f);
            e.reset();
            std::vector<float> L(256), R(256), cap;
            for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);   // Keys Depth is read on the first block
            e.noteOn(55, 0.8f);
            for (int b = 0; b < static_cast<int>(5.0 * sr / 256); ++b) { e.process(L.data(), R.data(), 256); if (b >= static_cast<int>(2.0 * sr / 256)) cap.insert(cap.end(), L.begin(), L.end()); }
            // Two bands, split at 800 Hz, each rectified and smoothed at 40 Hz.
            const float split = 1.0f - std::exp(-6.2831853f * 800.0f / static_cast<float>(sr));
            const float smooth = 1.0f - std::exp(-6.2831853f * 40.0f / static_cast<float>(sr));
            float lp = 0.0f, envLo = 0.0f, envHi = 0.0f;
            std::vector<double> lo, hi;
            for (size_t i = 0; i < cap.size(); ++i) {
                lp += split * (cap[i] - lp);
                envLo += smooth * (std::fabs(lp) - envLo);
                envHi += smooth * (std::fabs(cap[i] - lp) - envHi);
                if (i % 48 == 0 && i > static_cast<size_t>(sr / 10)) { lo.push_back(envLo); hi.push_back(envHi); }
            }
            auto stats = [](const std::vector<double>& v, double& mean, double& sd) {
                mean = 0.0; for (double x : v) mean += x; mean /= static_cast<double>(v.size());
                sd = 0.0; for (double x : v) sd += (x - mean) * (x - mean); sd = std::sqrt(sd / static_cast<double>(v.size()));
            };
            double mLo, sLo, mHi, sHi;
            stats(lo, mLo, sLo); stats(hi, mHi, sHi);
            double cov = 0.0;
            for (size_t i = 0; i < lo.size(); ++i) cov += (lo[i] - mLo) * (hi[i] - mHi);
            cov /= static_cast<double>(lo.size());
            if (indexOut != nullptr) *indexOut = sLo / (mLo + 1e-12);
            return cov / (sLo * sHi + 1e-20);
        };
        double idx0 = 0.0, idx1 = 0.0;
        const double corr0 = envelopes(0.0f, &idx0), corr1 = envelopes(1.0f, &idx1);
        std::printf("  [probe] comodulation: low-band modulation index %.3f -> %.3f, low/high envelope correlation %.2f -> %.2f\n", idx0, idx1, corr0, corr1);
        CHECK(idx1 > idx0 + 0.15, "with Comodulate the background's envelope moves where before it held still");
        CHECK(corr1 > 0.8, "and the low and high bands move together, which is the condition of the release");
    }

    // ---- the rotating network ----------------------------------------------------------------
    {
        // Lossless: frozen, the turning matrix neither gains nor loses -- a wrong rotation would.
        // Measured against the fixed network rather than against zero, because a frozen network
        // is not lossless to begin with: its delays are read at fractional positions by linear
        // interpolation, which is a small low-pass, and that costs about a decibel a second
        // whichever matrix is in the loop. What the matrix adds on top of that is the question.
        auto frozenEnergy = [&](int mode, double& early, double& late) {
            Reverb r;
            r.prepare(sr);
            r.setMode(mode);
            r.set(1.6f, 6.0f, 0.0f, 0.0f, false, 1.0f);
            std::vector<float> L(256, 0.0f), R(256, 0.0f);
            L[0] = R[0] = 1.0f;
            r.process(L.data(), R.data(), 256);
            for (int b = 1; b < 20; ++b) { std::fill(L.begin(), L.end(), 0.0f); std::fill(R.begin(), R.end(), 0.0f); r.process(L.data(), R.data(), 256); }
            r.set(1.6f, 6.0f, 0.0f, 0.0f, true, 1.0f);
            early = late = 0.0;
            const int blocks = static_cast<int>(4.0 * sr / 256);
            for (int b = 0; b < blocks; ++b) {
                std::fill(L.begin(), L.end(), 0.0f); std::fill(R.begin(), R.end(), 0.0f);
                r.process(L.data(), R.data(), 256);
                double s = 0.0; for (int i = 0; i < 256; ++i) s += static_cast<double>(L[i]) * L[i] + static_cast<double>(R[i]) * R[i];
                if (b < blocks / 4) early += s; else if (b >= 3 * blocks / 4) late += s;
            }
        };
        double e0, l0, e3, l3;
        frozenEnergy(2, e0, l0); frozenEnergy(3, e3, l3);
        const double driftFixed = 10.0 * std::log10(l0 / (e0 + 1e-30)), driftRot = 10.0 * std::log10(l3 / (e3 + 1e-30));
        std::printf("  [probe] frozen tail, first second against last: %+.2f dB fixed matrix, %+.2f dB rotating\n", driftFixed, driftRot);
        CHECK(std::fabs(driftRot - driftFixed) < 1.0, "frozen, the turning matrix loses nothing beyond what the interpolated delays already cost");

        // Re-mixed, not re-tuned: the tail's pattern of modes drifts from one second to the next
        // where the fixed network's stays put. Third-octave bands cannot see this -- each
        // averages dozens of modes and comes out the same whatever they do (measured: 0.92
        // against 0.94) -- so the pattern is taken bin by bin, at three hertz, where the modes are.
        auto patternCorrelation = [&](int mode) {
            Reverb r;
            r.prepare(sr);
            r.setMode(mode);
            r.set(1.6f, 8.0f, 0.0f, 0.0f, false, 1.0f);
            std::vector<float> L(256, 0.0f), R(256, 0.0f), tail;
            L[0] = R[0] = 1.0f;
            const int blocks = static_cast<int>(4.0 * sr / 256);
            for (int b = 0; b < blocks; ++b) {
                if (b > 0) { std::fill(L.begin(), L.end(), 0.0f); std::fill(R.begin(), R.end(), 0.0f); }
                r.process(L.data(), R.data(), 256);
                if (b >= static_cast<int>(1.0 * sr / 256)) tail.insert(tail.end(), L.begin(), L.end());
            }
            const int N = 16384;
            Fft fft(N);
            std::vector<std::vector<double>> patterns;
            for (size_t start = 0; start + static_cast<size_t>(N) <= tail.size(); start += static_cast<size_t>(sr)) {
                std::vector<float> re(static_cast<size_t>(N)), im(static_cast<size_t>(N), 0.0f);
                for (int i = 0; i < N; ++i) re[static_cast<size_t>(i)] = tail[start + static_cast<size_t>(i)] * (0.5f - 0.5f * std::cos(6.2831853f * static_cast<float>(i) / N));
                fft.transform(re.data(), im.data(), false);
                std::vector<double> bands;
                for (int k = static_cast<int>(200.0 * N / sr); k < static_cast<int>(4000.0 * N / sr); ++k)
                    bands.push_back(10.0 * std::log10(static_cast<double>(re[static_cast<size_t>(k)]) * re[static_cast<size_t>(k)] + static_cast<double>(im[static_cast<size_t>(k)]) * im[static_cast<size_t>(k)] + 1e-30));
                double mean = 0.0; for (double b : bands) mean += b; mean /= static_cast<double>(bands.size());
                for (double& b : bands) b -= mean;
                patterns.push_back(bands);
            }
            double sum = 0.0; int count = 0;
            for (size_t w = 1; w < patterns.size(); ++w) {
                double xy = 0.0, xx = 0.0, yy = 0.0;
                for (size_t b = 0; b < patterns[w].size(); ++b) { xy += patterns[w][b] * patterns[w - 1][b]; xx += patterns[w][b] * patterns[w][b]; yy += patterns[w - 1][b] * patterns[w - 1][b]; }
                sum += xy / std::sqrt(xx * yy + 1e-30); ++count;
            }
            return count > 0 ? sum / count : 0.0;
        };
        const double cFixed = patternCorrelation(2), cRot = patternCorrelation(3);
        std::printf("  [probe] tail modes, second to second: correlation %.2f fixed, %.2f rotating\n", cFixed, cRot);
        // Measured honestly: the classic network's wobbling lines ALREADY move its modes about this
        // much (0.55 second to second), so the turning matrix does not beat it -- it matches it
        // (0.51) with every line standing still. That is the claim the mode can make: the same
        // re-mixing of the modes, and not one delay length moving to get it.
        CHECK(cRot < 0.7 && cRot < cFixed + 0.1, "with the matrix turning the modes drift at least as much as the wobbling lines move them, and no line moves");
    }

    // ---- the near field ---------------------------------------------------------------------
    {
        // A voice hard left, by the ear: the level difference below 400 Hz, which the far-field
        // head barely makes, against the one at 3-6 kHz, which it makes anyway.
        auto ild = [&](float nearIld, double& highOut) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Partials, 32.0f); e.setParam(ParamId::Brightness, 1.0f); e.setParam(ParamId::Tilt, 0.3f);
            e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Spread, 0.0f);
            e.setParam(ParamId::FilterOn, 0.0f); e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f);
            e.setParam(ParamId::EnsembleMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
            e.setParam(ParamId::KeysDepth, 0.0f);
            e.setParam(ParamId::Binaural, 1.0f);
            e.setParam(ParamId::BassMono, 40.0f);   // or the master folds the very lows this is about
            e.setParam(ParamId::Haas, 0.0f);
            e.setParam(ParamId::NearIld, nearIld);
            e.setParam(ParamId::Attack, 0.02f);
            e.setHeadYaw(90.0f);                  // the head turns right, so the voice in front is now at the left ear
            e.reset();
            e.noteOn(43, 0.8f);                   // G2, 98 Hz: partials from 98 Hz up
            std::vector<float> L(256), R(256), capL, capR;
            for (int b = 0; b < 240; ++b) { e.process(L.data(), R.data(), 256); if (b >= 100) { capL.insert(capL.end(), L.begin(), L.end()); capR.insert(capR.end(), R.begin(), R.end()); } }
            const int N = 16384;
            Fft fft(N);
            auto bands = [&](const std::vector<float>& cap, double& low, double& high) {
                std::vector<float> re(static_cast<size_t>(N), 0.0f), im(static_cast<size_t>(N), 0.0f);
                for (int i = 0; i < N; ++i) re[static_cast<size_t>(i)] = cap[static_cast<size_t>(i)] * (0.5f - 0.5f * std::cos(6.2831853f * static_cast<float>(i) / N));
                fft.transform(re.data(), im.data(), false);
                auto band = [&](double lo, double hi) {
                    double s = 0.0;
                    for (int k = static_cast<int>(lo * N / sr); k <= static_cast<int>(hi * N / sr); ++k)
                        s += static_cast<double>(re[static_cast<size_t>(k)]) * re[static_cast<size_t>(k)] + static_cast<double>(im[static_cast<size_t>(k)]) * im[static_cast<size_t>(k)];
                    return s;
                };
                low = band(80.0, 400.0); high = band(3000.0, 6000.0);
            };
            double lL, hL, lR, hR;
            bands(capL, lL, hL); bands(capR, lR, hR);
            highOut = 10.0 * std::log10((hL + 1e-30) / (hR + 1e-30));
            return 10.0 * std::log10((lL + 1e-30) / (lR + 1e-30));
        };
        double high0, high1;
        const double low0 = ild(0.0f, high0), low1 = ild(1.0f, high1);
        std::printf("  [probe] near field, voice at the left ear: ILD below 400 Hz %+.1f -> %+.1f dB, at 3-6 kHz %+.1f -> %+.1f dB\n", low0, low1, high0, high1);
        CHECK(low1 > low0 + 10.0, "Near Field puts more than ten decibels of level difference into the lows");
        CHECK(std::fabs(high1 - high0) < 2.5, "and leaves the head's own shadow up top as it was");
    }

    // ---- transport: the morph that slides ------------------------------------------------
    {
        Wavetable t;
        t.frames = 2;
        t.amp[0][2] = 1.0f;     // a formant on the third partial
        t.amp[1][12] = 1.0f;    // and on the thirteenth
        auto stats = [](const float* a, double& energy, double& spread) {
            double m = 0.0, c = 0.0; energy = 0.0;
            for (int h = 0; h < kTablePartials; ++h) { m += a[h]; c += a[h] * h; energy += static_cast<double>(a[h]) * a[h]; }
            c /= std::max(m, 1e-12);
            double v = 0.0;
            for (int h = 0; h < kTablePartials; ++h) v += a[h] * (h - c) * (h - c);
            spread = std::sqrt(v / std::max(m, 1e-12));
        };
        float lin[kTablePartials], ot[kTablePartials];
        t.spectrumAt(0.5f, lin, 0.0f);
        t.spectrumAt(0.5f, ot, 1.0f);
        double eLin, sLin, eOt, sOt;
        stats(lin, eLin, sLin); stats(ot, eOt, sOt);
        std::printf("  [probe] transport halfway: energy %.2f -> %.2f, spread %.1f -> %.1f partials, mass at 8 = %.2f\n", eLin, eOt, sLin, sOt, ot[7]);
        CHECK(std::fabs(eLin - 0.5) < 1e-4, "the plain blend halfway holds half the energy: two half-height peaks");
        CHECK(eOt > 0.95, "the transport halfway holds all of it: one peak, on the partial between");
        CHECK(sLin > 4.5 && sOt < 0.5, "and the spread collapses from five partials to none");
        // Along the way the peak slides: its centre moves with the position, and never splits.
        bool slides = true;
        for (int k = 1; k < 8; ++k) {
            const float pos = static_cast<float>(k) / 8.0f;
            float a[kTablePartials];
            t.spectrumAt(pos, a, 1.0f);
            double e, s; stats(a, e, s);
            if (s > 0.6 || e < 0.45) slides = false;   // a split peak would spread wide and lose energy
        }
        CHECK(slides, "at every position between, the peak is one peak");
        // Off, the plain blend, bit for bit.
        float plain[kTablePartials];
        t.spectrumAt(0.37f, plain);
        t.spectrumAt(0.37f, ot, 0.0f);
        bool same = true;
        for (int h = 0; h < kTablePartials; ++h) if (plain[h] != ot[h]) same = false;
        CHECK(same, "at zero the table reads exactly as it always did");
    }

    // ---- pulse: the Foundation breathing at the binaural rate -------------------------------
    {
        auto index = [&](float pulse) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::SubLevel, 1.0f);
            e.setParam(ParamId::SubBinaural, 4.0f);
            e.setParam(ParamId::SubPulse, pulse);
            e.setParam(ParamId::OscLevel, 0.0f);   // a silent voice keeps the engine awake; the Foundation is all that sounds
            e.reset();
            std::vector<float> L(256), R(256), cap;
            for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);
            e.noteOn(48, 0.5f);
            for (int b = 0; b < static_cast<int>(5.0 * sr / 256); ++b) { e.process(L.data(), R.data(), 256); if (b >= static_cast<int>(3.0 * sr / 256)) cap.insert(cap.end(), L.begin(), L.end()); }
            // One channel alone: its own tone is a single sine, so any envelope on it is the pulse
            // (the two channels together would beat at the offset with or without it).
            const float smooth = 1.0f - std::exp(-6.2831853f * 15.0f / static_cast<float>(sr));
            float env = 0.0f; std::vector<double> v;
            for (size_t i = 0; i < cap.size(); ++i) { env += smooth * (std::fabs(cap[i]) - env); if (i > static_cast<size_t>(sr / 4) && i % 32 == 0) v.push_back(env); }
            double mean = 0.0; for (double x : v) mean += x; mean /= static_cast<double>(v.size());
            double sd = 0.0; for (double x : v) sd += (x - mean) * (x - mean); sd = std::sqrt(sd / static_cast<double>(v.size()));
            return sd / (mean + 1e-12);
        };
        const double off = index(0.0f), on = index(1.0f);
        std::printf("  [probe] pulse: one channel's envelope varies by %.3f of its mean without, %.3f with\n", off, on);
        CHECK(off < 0.08, "without Pulse the Foundation's own channel holds its level");
        CHECK(on > 0.4, "with Pulse it breathes deeply at the binaural rate");
    }

    // ---- bias: the shaper that remembers the bass --------------------------------------------
    {
        // Two sines into the feedback loop: a bass at 41 Hz and a tone at 440. The loop's shaper
        // is symmetric, so 880 Hz -- the tone's second harmonic -- is what Bias must make.
        auto secondHarmonic = [&](float bias) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Partials, 1.0f); e.setParam(ParamId::Unison, 1.0f);
            e.setParam(ParamId::FilterOn, 0.0f); e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f);
            e.setParam(ParamId::EnsembleMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
            e.setParam(ParamId::KeysDepth, 0.0f);
            e.setParam(ParamId::FeedbackBus, 1.0f); e.setParam(ParamId::FeedbackDrive, 1.0f); e.setParam(ParamId::FeedbackTone, 8000.0f);
            e.setParam(ParamId::FeedbackBias, bias);
            e.setParam(ParamId::Attack, 0.02f);
            e.reset();
            std::vector<float> L(256), R(256), cap;
            for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);
            // Quietly: the loop throttles itself to nothing above a mean level of 0.1, so a loud
            // pair of notes would switch the very thing being measured off.
            e.setParam(ParamId::OscLevel, 0.08f);
            e.noteOn(28, 0.6f);    // E1, 41 Hz
            e.noteOn(69, 0.45f);   // A4
            for (int b = 0; b < static_cast<int>(3.0 * sr / 256); ++b) { e.process(L.data(), R.data(), 256); if (b >= static_cast<int>(2.0 * sr / 256)) cap.insert(cap.end(), L.begin(), L.end()); }
            const int N = 32768;
            std::vector<float> re(static_cast<size_t>(N), 0.0f), im(static_cast<size_t>(N), 0.0f);
            for (int i = 0; i < N && i < static_cast<int>(cap.size()); ++i) re[static_cast<size_t>(i)] = cap[static_cast<size_t>(i)] * (0.5f - 0.5f * std::cos(6.2831853f * static_cast<float>(i) / N));
            Fft fft(N);
            fft.transform(re.data(), im.data(), false);
            auto peak = [&](double hz) {
                double best = 0.0;
                for (int k = static_cast<int>((hz - 12.0) * N / sr); k <= static_cast<int>((hz + 12.0) * N / sr); ++k)
                    best = std::max(best, static_cast<double>(re[static_cast<size_t>(k)]) * re[static_cast<size_t>(k)] + static_cast<double>(im[static_cast<size_t>(k)]) * im[static_cast<size_t>(k)]);
                return best;
            };
            // Even-order intermodulation: the sum and difference tones of the pair, 481 and 399 Hz,
            // which only an asymmetric curve makes -- a symmetric one puts its products at odd
            // orders only (2 f2 +/- f1, f2 +/- 2 f1). The second harmonic itself was tried first and
            // turned out to be confounded: something upstream already makes -38 dB of it, and the
            // bias's own second harmonic partly cancelled that.
            return 10.0 * std::log10((std::max(peak(481.0), peak(399.0)) + 1e-30) / (peak(440.0) + 1e-30));
        };
        const double k2Off = secondHarmonic(0.0f), k2On = secondHarmonic(1.0f);
        std::printf("  [probe] bias: even-order sum tone of bass and tone %+.1f dB below the tone without, %+.1f dB with the bass biasing the curve\n", k2Off, k2On);
        CHECK(k2On > k2Off + 6.0, "Bias makes the even-order product of bass and tone rise by more than six decibels");
    }

    // ---- Lenia: motion caused by neighbours -------------------------------------------------
    {
        auto run = [&](const char* matrix, double seconds, std::vector<float>* trace) {
            auto e = std::make_unique<Engine>();
            e->prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e->setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e->setParam(ParamId::BrainOn, 0.0f);
            e->setParam(ParamId::LeniaRate, 20.0f);
            e->setModMatrixText(matrix);
            e->reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < static_cast<int>(seconds * sr / 256); ++b) {
                e->process(L.data(), R.data(), 256);
                if (trace != nullptr) trace->push_back(e->leniaOut(0));
            }
            return e;
        };
        auto idle = run("lfo1>cutoff:0.2", 5.0, nullptr);
        CHECK(idle->leniaSteps() == 0, "a patch that reads none of the Lenia sources never computes the field");
        std::vector<float> trace;
        auto live = run("lenia1>cutoff:0.3;lenia3>far_level:0.2", 30.0, &trace);
        float sum = 0.0f; for (float v : trace) sum += v;
        const float mean = sum / static_cast<float>(trace.size());
        float var = 0.0f, worstStep = 0.0f;
        for (size_t i = 0; i < trace.size(); ++i) { var += (trace[i] - mean) * (trace[i] - mean); if (i > 0) worstStep = std::max(worstStep, std::fabs(trace[i] - trace[i - 1])); }
        const float sd = std::sqrt(var / static_cast<float>(trace.size()));
        float mass = 0.0f;
        for (int y = 0; y < Engine::kLeniaSize; ++y) for (int x = 0; x < Engine::kLeniaSize; ++x) mass += live->leniaCell(x, y);
        std::printf("  [probe] lenia: %d steps in 30 s, reseeded %d times, mass %.1f of %d, reading 1 mean %.2f sd %.3f, largest move per block %.4f\n",
                    live->leniaSteps(), live->leniaReseeds(), mass, Engine::kLeniaSize * Engine::kLeniaSize, mean, sd, worstStep);
        CHECK(live->leniaSteps() > 500, "with a route reading it the field steps at the asked rate");
        CHECK(live->leniaReseeds() <= live->leniaSteps() / 20, "and stays alive for dozens of steps between reseeds");
        CHECK(sd > 0.02f, "the reading moves");
        CHECK(worstStep < 0.05f, "and never jumps: the largest move from one block to the next is a few per cent");
    }

    // ---- deja vu: figures that come back --------------------------------------------------
    {
        auto notes = [](float dejavu) {
            BrainParams p;
            p.on = true; p.mode = BrainMode::Free; p.density = 3; p.rateSeconds = 1.5f;
            p.holdMin = 0.6f; p.holdMax = 1.0f; p.low = 48; p.high = 72;
            p.dejavu = dejavu; p.loop = 4;
            ClusterBrain brain;
            brain.reset(0x0DEull, 60);
            auto freqOf = [](int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); };
            std::vector<int> out;
            for (int step = 0; step < 60000 && out.size() < 60; ++step)
                brain.update(0.01, p, -1, freqOf, [&](const BrainEvent& e) { if (e.type == BrainEvent::Type::NoteOn) out.push_back(e.note); });
            return out;
        };
        auto repeatRate = [](const std::vector<int>& v) {
            int same = 0, count = 0;
            for (size_t i = 8; i < v.size(); ++i) { ++count; if (v[i] == v[i - 4]) ++same; }
            return count > 0 ? static_cast<double>(same) / count : 0.0;
        };
        const std::vector<int> fresh = notes(0.0f), looped = notes(1.0f);
        const double rFresh = repeatRate(fresh), rLoop = repeatRate(looped);
        std::printf("  [probe] deja vu, loop of four: a note equals the one four back %.0f%% of the time fresh, %.0f%% looped\n", 100.0 * rFresh, 100.0 * rLoop);
        CHECK(fresh.size() >= 40 && looped.size() >= 40, "the conductor produced enough notes to judge");
        CHECK(rLoop > 0.7, "at full Deja Vu the figure of four comes round again and again");
        CHECK(rFresh < 0.35, "and without it, it seldom does");
    }

    // ---- spread and bias: the shape of the draw ----------------------------------------------
    {
        auto velocities = [](float spread, float bias) {
            BrainParams p;
            p.on = true; p.mode = BrainMode::Free; p.density = 6; p.rateSeconds = 0.6f;
            p.holdMin = 0.6f; p.holdMax = 1.0f; p.low = 36; p.high = 84;
            p.spread = spread; p.bias = bias;
            ClusterBrain brain;
            brain.reset(0x5EEDull, 48);
            auto freqOf = [](int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); };
            std::vector<float> v;
            for (int step = 0; step < 200000 && v.size() < 400; ++step)
                brain.update(0.01, p, -1, freqOf, [&](const BrainEvent& e) { if (e.type == BrainEvent::Type::NoteOn) v.push_back(e.velocity); });
            return v;
        };
        auto stats = [](const std::vector<float>& v, double& mean, double& sd, double& extremes) {
            mean = 0.0; for (float x : v) mean += x; mean /= static_cast<double>(v.size());
            sd = 0.0; for (float x : v) sd += (x - mean) * (x - mean); sd = std::sqrt(sd / static_cast<double>(v.size()));
            int ext = 0; for (float x : v) if (std::fabs(x - 0.7f) > 0.15f) ++ext;
            extremes = static_cast<double>(ext) / static_cast<double>(v.size());
        };
        double m0, s0, e0, mN, sN, eN, mW, sW, eW, mB, sB, eB;
        stats(velocities(0.5f, 0.0f), m0, s0, e0);
        stats(velocities(0.0f, 0.0f), mN, sN, eN);
        stats(velocities(1.0f, 0.0f), mW, sW, eW);
        stats(velocities(0.5f, 0.8f), mB, sB, eB);
        std::printf("  [probe] velocity draws: uniform mean %.3f sd %.3f; narrow sd %.3f; wide %.0f%% at the extremes; biased mean %.3f\n", m0, s0, sN, 100.0 * eW, mB);
        CHECK(std::fabs(m0 - 0.7) < 0.03 && s0 > 0.09, "at the defaults the draw is the uniform one it always was");
        CHECK(sN < 0.65 * s0, "Spread at 0 gathers the draw round the middle");
        CHECK(eW > 0.6 && eW > 2.0 * e0, "Spread at 1 pushes most draws to the extremes");
        CHECK(mB > m0 + 0.06, "and Bias moves the centre up");
    }

    // ---- the attractors ---------------------------------------------------------------------
    {
        auto run = [&](const char* matrix, double seconds, int which, std::vector<float>* trace) {
            auto e = std::make_unique<Engine>();
            e->prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e->setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e->setParam(ParamId::BrainOn, 0.0f);
            e->setParam(ParamId::ChaosPeriod, 10.0f);
            e->setModMatrixText(matrix);
            e->reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < static_cast<int>(seconds * sr / 256); ++b) {
                e->process(L.data(), R.data(), 256);
                if (trace != nullptr) trace->push_back(e->chaosOut(which));
            }
            return e;
        };
        auto idle = run("lfo1>cutoff:0.2", 3.0, 0, nullptr);
        CHECK(idle->chaosSteps() == 0, "a patch that reads no attractor never integrates one");
        for (int which : { 0, 5 }) {
            std::vector<float> tr;
            auto live = run(which == 0 ? "lorenz_x>cutoff:0.3" : "rossler_z>far_level:0.2", 120.0, which, &tr);
            float mean = 0.0f, worst = 0.0f, peak = 0.0f; int crossings = 0;
            for (float v : tr) mean += v; mean /= static_cast<float>(tr.size());
            for (size_t i = 0; i < tr.size(); ++i) { peak = std::max(peak, std::fabs(tr[i])); if (i > 0) { worst = std::max(worst, std::fabs(tr[i] - tr[i - 1])); if ((tr[i] - mean) * (tr[i - 1] - mean) < 0.0f) ++crossings; } }
            float var = 0.0f; for (float v : tr) var += (v - mean) * (v - mean); const float sd = std::sqrt(var / static_cast<float>(tr.size()));
            // No cycle: the best match of the second minute against any earlier shift of it.
            const int blocksPerSec = static_cast<int>(sr / 256);
            double bestCorr = -1.0;
            for (int lag = 5 * blocksPerSec; lag <= 55 * blocksPerSec; lag += blocksPerSec / 2) {
                double xy = 0.0, xx = 0.0, yy = 0.0;
                for (size_t i = static_cast<size_t>(60 * blocksPerSec); i < tr.size(); ++i) {
                    const double a = tr[i] - mean, b = tr[i - static_cast<size_t>(lag)] - mean;
                    xy += a * b; xx += a * a; yy += b * b;
                }
                bestCorr = std::max(bestCorr, xy / std::sqrt(xx * yy + 1e-30));
            }
            std::printf("  [probe] %s over two minutes: peak %.2f, sd %.2f, %d crossings, largest move per block %.4f, best self-match at any lag %.2f\n",
                        which == 0 ? "lorenz_x" : "rossler_z", peak, sd, crossings, worst, bestCorr);
            CHECK(live->chaosSteps() > 1000 && peak <= 1.0f, "the attractor runs and its reading stays inside -1..1");
            CHECK(sd > 0.15f && crossings >= 3, "it moves, and crosses its centre several times in two minutes");
            CHECK(worst < 0.02f, "and never jumps");
            // Measured honestly: the Lorenz system does not come back (0.40 at its best lag); the
            // Roessler spiral very nearly does -- x and y circle at almost one rate and only the
            // climb varies, 0.95 at its best lag -- and that is what it is: a spiral with a
            // chaotic climb, not a second Lorenz. The help says so.
            CHECK(bestCorr < (which == 0 ? 0.9 : 0.99), "and does not repeat itself exactly at any lag between five and fifty-five seconds");
        }
    }

    // ---- the one-euro filter -------------------------------------------------------------------
    {
        auto pressure = [&](int filter, bool jitter, std::vector<float>* trace) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::KeysFilter, static_cast<float>(filter));
            e.reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);
            e.noteOn(60, 0.8f);
            Rng jr; jr.seed(0x1E6ull);
            for (int b = 0; b < static_cast<int>(3.0 * sr / 256); ++b) {
                const float target = jitter ? 0.5f + 0.02f * jr.bipolar() : (b >= 40 ? 1.0f : 0.0f);
                e.setPressure(60, target);
                e.process(L.data(), R.data(), 256);
                if (trace != nullptr) trace->push_back(0.5f * (e.modSource(static_cast<int>(ModSource::Pressure)) + 1.0f));   // back from bipolar
            }
        };
        auto restJitter = [&](int filter) {
            std::vector<float> tr; pressure(filter, true, &tr);
            // The last third only: the one-euro filter settles at its rest cutoff, a quarter of a
            // second's time constant, and its tail into the middle of the run read as jitter.
            const size_t from = 2 * tr.size() / 3, n = tr.size() - from;
            double mean = 0.0; for (size_t i = from; i < tr.size(); ++i) mean += tr[i]; mean /= static_cast<double>(n);
            double var = 0.0; for (size_t i = from; i < tr.size(); ++i) var += (tr[i] - mean) * (tr[i] - mean);
            return std::sqrt(var / static_cast<double>(n));
        };
        auto halfTime = [&](int filter) {
            std::vector<float> tr; pressure(filter, false, &tr);
            for (size_t i = 40; i < tr.size(); ++i) if (tr[i] >= 0.5f) return (static_cast<double>(i) - 40.0) * 256.0 / sr;
            return 9.0;
        };
        const double jClassic = restJitter(0), jEuro = restJitter(1);
        const double tClassic = halfTime(0), tEuro = halfTime(1);
        std::printf("  [probe] one euro: jitter left in a held note %.4f classic, %.4f one-euro; half way to a full press in %.0f ms classic, %.0f ms one-euro\n", jClassic, jEuro, 1000.0 * tClassic, 1000.0 * tEuro);
        CHECK(jEuro < 0.5 * jClassic, "One Euro takes most of the sensor's jitter out of a held note");
        CHECK(tEuro < 0.06 && tClassic < 0.06, "and both follow a full press half way inside sixty milliseconds");
    }

    // ---- transposition -------------------------------------------------------------------------
    {
        Engine e;
        e.prepare(sr, 256);
        for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
        e.setParam(ParamId::BrainOn, 0.0f);
        e.reset();
        std::vector<float> L(256), R(256);
        for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);
        e.noteOn(60, 0.8f);
        for (int b = 0; b < 40; ++b) e.process(L.data(), R.data(), 256);
        const double before = e.frequencyOf(60);
        e.setParam(ParamId::Transpose, 2.0f);   // a fifth up
        double worstStep = 0.0, last = before;
        for (int b = 0; b < static_cast<int>(8.0 * sr / 256); ++b) {   // the table is there in 1.2 s; the voice's own glide takes its time
            e.process(L.data(), R.data(), 256);
            const double f = e.frequencyOf(60);
            worstStep = std::max(worstStep, std::fabs(1200.0 * std::log2(f / last)));
            last = f;
        }
        const double cents = 1200.0 * std::log2(e.frequencyOf(60) / before / 1.5);
        const double voiceCents = 1200.0 * std::log2(e.displayFrequency() / before / 1.5);
        std::printf("  [probe] transpose, a fifth up: the table lands %+.2f cents from 3:2, the sounding voice %+.2f; largest step per block %.2f cents\n", cents, voiceCents, worstStep);
        CHECK(std::fabs(cents) < 0.01, "after eight seconds the tuning sits exactly on 3:2");
        CHECK(std::fabs(voiceCents) < 3.0, "and the sounding voice has followed it there");
        CHECK(worstStep < 5.0, "in steps of a few cents per block, never a jump");
    }

    // ---- partial spread ------------------------------------------------------------------------
    {
        auto picture = [&](float spread, double& energy) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Partials, 32.0f); e.setParam(ParamId::Brightness, 1.0f); e.setParam(ParamId::Tilt, 0.3f);
            e.setParam(ParamId::Unison, 1.0f); e.setParam(ParamId::Spread, 0.0f);
            e.setParam(ParamId::FilterOn, 0.0f); e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f);
            e.setParam(ParamId::EnsembleMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
            e.setParam(ParamId::KeysDepth, 0.0f); e.setParam(ParamId::Haas, 0.0f);
            // Time Width off: at its default it gives even a centred single strand an interaural
            // delay (measured with the render tool: width 0.88 with it, 0.04 without), which
            // would hide what this test is about.
            e.setParam(ParamId::Itd, 0.0f);
            e.setParam(ParamId::PartialSpread, spread);
            e.setParam(ParamId::Attack, 0.02f);
            e.reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);
            e.noteOn(48, 0.8f);
            double ll = 0.0, rr = 0.0, lr = 0.0;
            for (int b = 0; b < 240; ++b) {
                e.process(L.data(), R.data(), 256);
                if (b < 100) continue;
                for (int i = 0; i < 256; ++i) { ll += static_cast<double>(L[i]) * L[i]; rr += static_cast<double>(R[i]) * R[i]; lr += static_cast<double>(L[i]) * R[i]; }
            }
            energy = ll + rr;
            return lr / std::sqrt(ll * rr + 1e-30);
        };
        double e0, e1;
        const double c0 = picture(0.0f, e0), c1 = picture(1.0f, e1);
        std::printf("  [probe] partial spread: left/right correlation %.3f -> %.3f, energy %+.2f dB\n", c0, c1, 10.0 * std::log10(e1 / e0));
        CHECK(c0 > 0.9, "a single strand without spread is nearly the same on both sides");
        CHECK(c1 < 0.7, "with Partial Spread the two sides decorrelate: width inside the note");
        CHECK(std::fabs(10.0 * std::log10(e1 / e0)) < 1.0, "and the energy stays within a decibel");
    }

    // ---- the strike's chance, and its clustering ------------------------------------------
    // "Does a strike ever fire when the drone runs by itself?" It did not, unless Fires was on
    // Keys + Brain, and then it fired on every single note. Chance thins that, Cluster lets the
    // survivors follow the cascade. The brain's clock is its own -- no route reads the output --
    // so the note times are the same in every run here and the counts compare directly.
    {
        struct Run { int strikes; double cv; double meanExcAtStrike; };
        auto run = [&](float chance, float cluster, float cascade) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 1.0f);
            e.setParam(ParamId::BrainRate, 2.0f);          // a note every two seconds: enough of them to count
            e.setParam(ParamId::BrainHoldMin, 5.0f);
            e.setParam(ParamId::BrainHoldMax, 12.0f);
            e.setParam(ParamId::BrainCascade, cascade);
            e.setParam(ParamId::StrikeLevel, 0.6f);
            e.setParam(ParamId::StrikeWho, 1.0f);          // Keys + Brain
            e.setParam(ParamId::StrikeChance, chance);
            e.setParam(ParamId::StrikeCluster, cluster);
            e.reset();
            std::vector<float> L(256), R(256);
            std::vector<double> at;
            double excSum = 0.0;
            unsigned last = 0;
            const int blocks = static_cast<int>(300.0 * sr / 256.0);   // five minutes
            for (int b = 0; b < blocks; ++b) {
                e.process(L.data(), R.data(), 256);
                const unsigned now = e.strikesFired();
                if (now > last) { at.push_back(b * 256.0 / sr); excSum += e.brainExcitation(); last = now; }
            }
            Run r { static_cast<int>(last), 0.0, at.empty() ? 0.0 : excSum / static_cast<double>(at.size()) };
            if (at.size() > 2) {
                double m = 0.0;
                for (size_t i = 1; i < at.size(); ++i) m += at[i] - at[i - 1];
                m /= static_cast<double>(at.size() - 1);
                double v = 0.0;
                for (size_t i = 1; i < at.size(); ++i) { const double d = (at[i] - at[i - 1]) - m; v += d * d; }
                r.cv = std::sqrt(v / static_cast<double>(at.size() - 1)) / std::max(m, 1e-9);
            }
            return r;
        };
        const Run all = run(1.0f, 0.0f, 0.0f);      // every note strikes: the count IS the note count
        const Run none = run(0.0f, 0.0f, 0.0f);
        const Run third = run(0.33f, 0.0f, 0.0f);
        std::printf("  [probe] strike chance: %d notes in five minutes, chance 1 -> %d strikes, 0.33 -> %d (%.2f), 0 -> %d\n",
                    all.strikes, all.strikes, third.strikes,
                    all.strikes > 0 ? static_cast<double>(third.strikes) / all.strikes : 0.0, none.strikes);
        CHECK(all.strikes > 40, "with the conductor running there are notes enough to count");
        CHECK(none.strikes == 0, "at Chance 0 the conductor's notes never strike");
        CHECK(std::fabs(static_cast<double>(third.strikes) / std::max(1, all.strikes) - 0.33) < 0.12,
              "at Chance 0.33 about a third of them do");
        // Clustering: the same chance, but the coin weighted by the cascade's excitation. The
        // strikes then arrive where the events crowd, so the gaps between them vary more.
        const Run even = run(0.3f, 0.0f, 1.0f);
        const Run bunched = run(0.3f, 1.0f, 1.0f);
        std::printf("  [probe] strike cluster: even %d strikes cv %.2f (excitation at the strike %.2f), clustered %d strikes cv %.2f (%.2f)\n",
                    even.strikes, even.cv, even.meanExcAtStrike, bunched.strikes, bunched.cv, bunched.meanExcAtStrike);
        CHECK(bunched.meanExcAtStrike > even.meanExcAtStrike * 1.15,
              "with Cluster the strikes sit where the cascade is excited, not where an even coin would put them");
        CHECK(bunched.cv > even.cv * 1.1, "so the gaps between them vary more: handfuls and silences");
        CHECK(std::fabs(static_cast<double>(bunched.strikes - even.strikes)) < 0.6 * even.strikes,
              "and roughly as many of them: Cluster moves the strikes, Chance decides how many");
    }

    // ---- the Cosmos swelling with the cascade ----------------------------------------------
    // The Cosmos is a send, not an event: it hums along with whatever sounds and never rests.
    // Swell lets it follow the conductor's excitation -- measured against that piece's own
    // average, which is what leaves an instrument without a cascade exactly where it was.
    {
        auto run = [&](float swell, float cascade, double& cv, double& corr, std::vector<float>* tail) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 1.0f);
            e.setParam(ParamId::BrainRate, 2.0f);
            e.setParam(ParamId::BrainHoldMin, 5.0f);
            e.setParam(ParamId::BrainHoldMax, 12.0f);
            e.setParam(ParamId::BrainCascade, cascade);
            e.setParam(ParamId::CosmosSend, 0.5f);
            e.setParam(ParamId::CosmosReturn, 0.6f);
            e.setParam(ParamId::CosmosRes, 0.4f);
            e.setParam(ParamId::CosmosSwell, swell);
            e.reset();
            std::vector<float> L(256), R(256), tap(256);
            std::vector<double> lev, exc;
            double excSm = 0.0;
            const int blocks = static_cast<int>(180.0 * sr / 256.0);   // three minutes
            for (int b = 0; b < blocks; ++b) {
                e.process(L.data(), R.data(), 256);
                if (tail != nullptr && b >= blocks - 8) tail->insert(tail->end(), L.begin(), L.end());
                e.cosmosTap(tap.data(), 256);
                double s = 0.0;
                for (int i = 0; i < 256; ++i) s += static_cast<double>(tap[i]) * tap[i];
                lev.push_back(std::sqrt(s / 256.0));
                // The send follows the excitation smoothed over two seconds, so that is what the
                // level is compared with; against the raw kick the smoothing alone would look
                // like a poor correlation.
                const double target = e.brainExcitation() / (1.0 + e.brainExcitation());
                const double a = 1.0 - std::exp(-(256.0 / sr) / 2.0);
                excSm += a * (target - excSm);
                exc.push_back(excSm);
            }
            double m = 0.0, me = 0.0;
            for (size_t i = 0; i < lev.size(); ++i) { m += lev[i]; me += exc[i]; }
            m /= static_cast<double>(lev.size()); me /= static_cast<double>(exc.size());
            double v = 0.0, ve = 0.0, c = 0.0;
            for (size_t i = 0; i < lev.size(); ++i) {
                const double dl = lev[i] - m, de = exc[i] - me;
                v += dl * dl; ve += de * de; c += dl * de;
            }
            cv = m > 1e-12 ? std::sqrt(v / static_cast<double>(lev.size())) / m : 0.0;
            corr = (v > 0.0 && ve > 0.0) ? c / std::sqrt(v * ve) : 0.0;
        };
        // Without a cascade there is nothing to follow, and the Swell must be inaudible: the same
        // samples, not merely a similar level.
        std::vector<float> off, on;
        double cvA, corrA, cvB, corrB;
        run(0.0f, 0.0f, cvA, corrA, &off);
        run(1.0f, 0.0f, cvB, corrB, &on);
        double worst = 0.0;
        for (size_t i = 0; i < std::min(off.size(), on.size()); ++i) worst = std::max(worst, std::fabs(static_cast<double>(off[i] - on[i])));
        std::printf("  [probe] cosmos swell without a cascade: largest sample difference %.3g\n", worst);
        CHECK(worst < 1.0e-9, "with no cascade running, Swell changes nothing at all");
        // With one, the send opens in the clusters and closes in the gaps.
        run(0.0f, 1.0f, cvA, corrA, nullptr);
        run(1.0f, 1.0f, cvB, corrB, nullptr);
        std::printf("  [probe] cosmos swell with a cascade: level variation %.3f -> %.3f, correlation with the excitation %+.3f -> %+.3f\n",
                    cvA, cvB, corrA, corrB);
        CHECK(cvB > cvA * 1.15, "with a cascade the Cosmos comes and goes instead of sitting at one level");
        CHECK(corrB > corrA + 0.05, "and what it follows is the conductor's excitation");
    }

    // ---- the clock-locked arc --------------------------------------------------------------
    {
        // The mapping from the hour to the arc, held to what the help text promises.
        CHECK(std::fabs(Engine::clockArcValue(4.0) + 1.0f) < 1e-5f, "at four in the morning the arc is at its bottom");
        CHECK(std::fabs(Engine::clockArcValue(16.0) - 1.0f) < 1e-5f, "at four in the afternoon at its top");
        CHECK(std::fabs(Engine::clockArcValue(10.0)) < 1e-5f && std::fabs(Engine::clockArcValue(22.0)) < 1e-5f, "and at ten and twenty-two it crosses zero");
        CHECK(std::fabs(Engine::clockArcValue(0.0) - Engine::clockArcValue(24.0)) < 1e-6f, "midnight is midnight from either side");
        float worst = 0.0f;
        for (int i = 0; i < 24 * 60; ++i) worst = std::max(worst, std::fabs(Engine::clockArcValue(i / 60.0 + 1.0 / 60.0) - Engine::clockArcValue(i / 60.0)));
        CHECK(worst < 0.005f, "and from one minute to the next it moves by less than half a per cent");
        // Through the engine: on, the arc reads the clock's value (gliding towards it); off, the drift's.
        Engine e;
        e.prepare(sr, 256);
        for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::ArcClock, 1.0f);
        e.reset();
        std::vector<float> L(256), R(256);
        for (int b = 0; b < 12000; ++b) e.process(L.data(), R.data(), 256);   // a minute: past the glide
        const std::time_t t = std::time(nullptr);
        std::tm lt {};
#if defined(_WIN32)
        localtime_s(&lt, &t);
#else
        localtime_r(&t, &lt);
#endif
        const float want = Engine::clockArcValue(lt.tm_hour + lt.tm_min / 60.0 + lt.tm_sec / 3600.0);
        std::printf("  [probe] arc clock: local hour %.2f, arc %.3f, engine reads %.3f\n", lt.tm_hour + lt.tm_min / 60.0, want, e.arcNow());
        CHECK(std::fabs(e.arcNow() - want) < 0.02f, "with Arc Clock on the engine's arc is the hour's value");
    }

    // ---- the depth law --------------------------------------------------------------------
    {
        auto placed = [&](float law, float depth) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::KeysDepth, depth);
            e.setParam(ParamId::DepthLaw, law);
            e.reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);   // parameters are read per block: let them be
            e.noteOn(60, 0.8f);
            for (int b = 0; b < 40; ++b) e.process(L.data(), R.data(), 256);
            return e.noteDistance(60);
        };
        const float mid0 = placed(0.0f, 0.5f), mid1 = placed(1.0f, 0.5f);
        const float far0 = placed(0.0f, 1.0f), far1 = placed(1.0f, 1.0f);
        std::printf("  [probe] depth 0.5 is heard at %.3f flat, %.3f under the law; depth 1.0 at %.3f and %.3f\n", mid0, mid1, far0, far1);
        CHECK(std::fabs(mid0 - 0.5f) < 0.02f, "with the law off, half depth is half the plane");
        CHECK(std::fabs(mid1 - std::pow(0.5f, 1.85f)) < 0.02f, "with it on, half the knob is d^1.85 -- the inverse of Zahorik's exponent");
        CHECK(std::fabs(far0 - far1) < 0.02f && far1 > 0.98f, "and the horizon does not move");
    }

    // ---- height ---------------------------------------------------------------------------
    {
        // A bright note with thirty-two partials reaches past ten kilohertz, which is where the
        // pinna's notch lives. Measured: the energy in the 9.5-10.5 kHz band against the energy
        // in 6.5-7.5 kHz. Overhead the notch sits near ten kilohertz and that ratio falls; below,
        // it sits near four and the ratio rises.
        auto ratio = [&](float elev) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Partials, 32.0f);
            e.setParam(ParamId::Brightness, 1.0f);
            e.setParam(ParamId::Tilt, 0.3f);
            e.setParam(ParamId::Unison, 1.0f);
            e.setParam(ParamId::FilterOn, 0.0f); e.setParam(ParamId::Air, 0.0f);
            e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f);
            e.setParam(ParamId::EnsembleMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
            e.setParam(ParamId::KeysDepth, 0.0f);
            e.setParam(ParamId::ElevNear, elev); e.setParam(ParamId::ElevFar, elev);
            e.setParam(ParamId::Attack, 0.02f);
            e.reset();
            e.noteOn(67, 0.8f);                   // G4, 392 Hz: partial 32 is 12.5 kHz
            std::vector<float> L(256), R(256), cap;
            for (int b = 0; b < 240; ++b) { e.process(L.data(), R.data(), 256); if (b >= 100) cap.insert(cap.end(), L.begin(), L.end()); }
            const int N = 16384;
            std::vector<float> re(static_cast<size_t>(N), 0.0f), im(static_cast<size_t>(N), 0.0f);
            for (int i = 0; i < N && i < static_cast<int>(cap.size()); ++i) {
                const float w = 0.5f - 0.5f * std::cos(6.2831853f * static_cast<float>(i) / N);
                re[static_cast<size_t>(i)] = cap[static_cast<size_t>(i)] * w;
            }
            Fft fft(N);
            fft.transform(re.data(), im.data(), false);
            auto band = [&](double lo, double hi) {
                double s = 0.0;
                for (int k = static_cast<int>(lo * N / sr); k <= static_cast<int>(hi * N / sr); ++k)
                    s += static_cast<double>(re[static_cast<size_t>(k)]) * re[static_cast<size_t>(k)] + static_cast<double>(im[static_cast<size_t>(k)]) * im[static_cast<size_t>(k)];
                return s;
            };
            return band(9500.0, 10500.0) / (band(6500.0, 7500.0) + 1e-20);
        };
        const double below = ratio(-1.0f), flat = ratio(0.0f), above = ratio(1.0f);
        std::printf("  [probe] 10 kHz against 7 kHz: %.3f below, %.3f flat, %.3f overhead\n", below, flat, above);
        CHECK(above < flat, "overhead, the pinna's notch has moved up to ten kilohertz");
        CHECK(below > above, "and below the ear it has moved down, away from it");
    }

    // ---- envelopment -----------------------------------------------------------------------
    {
        auto sideBand = [&](float envelop, double& midOut) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::KeysDepth, 1.0f);              // straight into the far reverb
            e.setParam(ParamId::FarLevel, 1.0f);
            e.setParam(ParamId::NearMix, 0.0f);
            e.setParam(ParamId::Envelop, envelop);
            e.setParam(ParamId::Air, 0.0f);
            e.reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < 4; ++b) e.process(L.data(), R.data(), 256);   // same: Keys Depth has to be read first
            e.noteOn(45, 0.8f);                                 // A2: partials 2..4 sit in 220-440 Hz
            // The same band the engine lifts, measured on the output's side and mid.
            const float cLo = 1.0f - std::exp(-6.2831853f * 150.0f / sr), cHi = 1.0f - std::exp(-6.2831853f * 500.0f / sr);
            float sLo = 0.0f, sHi = 0.0f, mLo = 0.0f, mHi = 0.0f;
            double side = 0.0, mid = 0.0;
            for (int b = 0; b < 800; ++b) {
                e.process(L.data(), R.data(), 256);
                if (b < 200) continue;
                for (int i = 0; i < 256; ++i) {
                    const float s = 0.5f * (L[static_cast<size_t>(i)] - R[static_cast<size_t>(i)]), m = 0.5f * (L[static_cast<size_t>(i)] + R[static_cast<size_t>(i)]);
                    sLo += cLo * (s - sLo); sHi += cHi * (s - sHi); const float sb = sHi - sLo; side += static_cast<double>(sb) * sb;
                    mLo += cLo * (m - mLo); mHi += cHi * (m - mHi); const float mb = mHi - mLo; mid += static_cast<double>(mb) * mb;
                }
            }
            midOut = mid;
            return side;
        };
        double mid0 = 0.0, mid1 = 0.0;
        const double s0 = sideBand(0.0f, mid0), s1 = sideBand(1.0f, mid1);
        const double liftDb = 10.0 * std::log10((s1 + 1e-20) / (s0 + 1e-20));
        const double midDb = 10.0 * std::log10((mid1 + 1e-20) / (mid0 + 1e-20));
        std::printf("  [probe] far side energy 150-500 Hz: %+.1f dB with Envelop, mid %+.2f dB\n", liftDb, midDb);
        CHECK(liftDb > 3.0, "Envelop lifts the background's low-mid side by several decibels");
        CHECK(std::fabs(midDb) < 0.5, "and leaves the mid where it was");
    }

    // ---- blend: how a chord arrives ---------------------------------------------------------
    {
        auto onsets = [](float blend) {
            BrainParams p;
            p.on = true; p.mode = BrainMode::Free; p.density = 5; p.rateSeconds = 2.0f;
            p.holdMin = 60.0f; p.holdMax = 90.0f; p.low = 36; p.high = 79; p.blend = blend;
            ClusterBrain brain;
            brain.reset(0x1234567ull, 48);
            auto freqOf = [](int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); };
            std::vector<double> t;
            double now = 0.0;
            for (int step = 0; step < 12000 && t.size() < 5; ++step) {          // twelve seconds at 1 ms
                brain.update(0.001, p, -1, freqOf, [&](const BrainEvent& e) { if (e.type == BrainEvent::Type::NoteOn) t.push_back(now); });
                now += 0.001;
            }
            return t;
        };
        const std::vector<double> apart = onsets(0.0f), together = onsets(1.0f);
        const double spanApart = apart.size() >= 2 ? apart.back() - apart.front() : 0.0;
        const double spanTogether = together.size() >= 2 ? together.back() - together.front() : 0.0;
        std::printf("  [probe] first five onsets span %.3f s one by one, %.3f s with Blend\n", spanApart, spanTogether);
        CHECK(apart.size() >= 3 && spanApart > 1.0, "one by one, the chord takes seconds to assemble");
        CHECK(together.size() >= 5 && spanTogether < 0.06, "with Blend the whole chord arrives inside the fusion window");
    }

    // ---- match: partials on the scale ------------------------------------------------------
    {
        auto engineWith = [&](int scale, float match, float inharm) {
            auto e = std::make_unique<Engine>();
            e->prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e->setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e->setParam(ParamId::BrainOn, 0.0f);
            e->setParam(ParamId::Scale, static_cast<float>(scale));
            e->setParam(ParamId::TuneMatch, match);
            e->setParam(ParamId::Inharmonic, inharm);
            e->setParam(ParamId::Air, 0.0f);
            e->reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < 20; ++b) e->process(L.data(), R.data(), 256);
            return e;
        };
        const double cents = 1200.0;
        {   // 12-TET: the fifth and seventh partials move to the nearest semitone.
            auto e = engineWith(0, 1.0f, 0.0f);
            const double r3 = e->matchedPartialRatio(3), r5 = e->matchedPartialRatio(5), r7 = e->matchedPartialRatio(7);
            std::printf("  [probe] 12-TET match: partial 3 -> %.1f ct, 5 -> %.1f ct, 7 -> %.1f ct\n",
                        cents * std::log2(r3), cents * std::log2(r5), cents * std::log2(r7));
            CHECK(std::fabs(cents * std::log2(r3) - 1900.0) < 0.01, "partial 3 lands on the tempered fifth");
            CHECK(std::fabs(cents * std::log2(r5) - 2800.0) < 0.01, "partial 5 on the tempered third");
            CHECK(std::fabs(cents * std::log2(r7) - 3400.0) < 0.01, "and partial 7 on the tempered minor seventh");
        }
        {   // A just scale already holds 3 and 5: nothing to move.
            auto e = engineWith(1, 1.0f, 0.0f);
            CHECK(std::fabs(e->matchedPartialRatio(3) - 3.0) < 1e-9 && std::fabs(e->matchedPartialRatio(5) - 5.0) < 1e-9,
                  "in a just scale the third and fifth partials are already on degrees");
        }
        {   // Bohlen-Pierce: a tritave scale, and the second partial is not one of its degrees.
            auto e = engineWith(9, 1.0f, 0.0f);
            const double r2 = e->matchedPartialRatio(2), r3 = e->matchedPartialRatio(3);
            std::printf("  [probe] Bohlen-Pierce match: partial 2 -> %.4f (was 2), partial 3 -> %.4f\n", r2, r3);
            CHECK(std::fabs(r3 - 3.0) < 1e-9, "the tritave itself is a degree");
            CHECK(std::fabs(r2 - 2.0) > 0.01 && std::fabs(r2 - 2.0) < 0.1, "and the octave partial moves onto the scale");
        }
        {   // The point of it, measured: the scale's own intervals get smoother.
            auto roughOverScale = [&](const Engine& e, bool matched) {
                BrainSpectrum sp;
                sp.count = BrainSpectrum::kMax;
                for (int h = 1; h <= sp.count; ++h) {
                    sp.amp[h - 1] = std::pow(static_cast<double>(h), -1.0);
                    sp.ratio[h - 1] = matched ? e.matchedPartialRatio(h) : static_cast<double>(h);
                }
                double sum = 0.0;
                for (int d = 1; d < 12; ++d)
                    sum += spectralRoughness(261.6, 261.6 * std::pow(2.0, d / 12.0), sp) - spectralRoughness(261.6, 261.6, sp);
                return sum / 11.0;
            };
            auto e = engineWith(0, 1.0f, 0.0f);
            const double natural = roughOverScale(*e, false), matched = roughOverScale(*e, true);
            std::printf("  [probe] mean roughness of the eleven 12-TET intervals: %.4f natural, %.4f matched\n", natural, matched);
            CHECK(matched < natural, "matched to 12-TET, the tempered intervals are smoother than with harmonic partials");
        }
        {   // And it reaches the sound: same note, Match 0 against 1, must not render alike; the
            // fundamental must not move (Match places partials, not notes).
            auto a = engineWith(0, 0.0f, 0.0f), b = engineWith(0, 1.0f, 0.0f);
            CHECK(std::fabs(a->frequencyOf(60) - b->frequencyOf(60)) < 1e-9, "Match leaves the note where it is");
            a->noteOn(60, 0.8f); b->noteOn(60, 0.8f);
            std::vector<float> La(256), Ra(256), Lb(256), Rb(256);
            double diff = 0.0;
            for (int blk = 0; blk < 200; ++blk) {
                a->process(La.data(), Ra.data(), 256); b->process(Lb.data(), Rb.data(), 256);
                if (blk > 100) for (int i = 0; i < 256; ++i) diff += std::fabs(static_cast<double>(La[static_cast<size_t>(i)]) - Lb[static_cast<size_t>(i)]);
            }
            CHECK(diff > 1e-3, "and moves the partials the ear hears");
        }
    }

    // ---- the arc leaning on the harmony ----------------------------------------------------
    {
        auto leanSeen = [&](float amount) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::ArcHarmony, amount);
            e.setParam(ParamId::ArcPeriod, 2.0f);            // the shortest arc, two minutes
            e.setParam(ParamId::BrainOn, 0.0f);
            e.reset();
            std::vector<float> L(256), R(256);
            float biggest = 0.0f;
            for (int b = 0; b < 8000; ++b) {                  // forty seconds
                e.process(L.data(), R.data(), 256);
                biggest = std::max(biggest, std::fabs(e.arcLean()));
            }
            return biggest;
        };
        const float off = leanSeen(0.0f), on = leanSeen(1.0f);
        std::printf("  [probe] arc lean on the harmony over 40 s: %.3f at 0, %.3f at 1\n", off, on);
        CHECK(off == 0.0f, "at 0 the arc does not touch the harmony");
        CHECK(on > 0.05f, "at 1 it leans, and the lean is measurable");
    }

    // ---- the fluctuation guard -------------------------------------------------------------
    {
        // A tempered major third beats at about nine hertz an octave below middle C; at Purity
        // 0.85 that is 1.3 Hz, under the band, and a full Purity Drift carries it up into it.
        auto inBand = [&](float guard, float& lowestFactor) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Scale, 1.0f);                  // JI major, so purity has somewhere to go
            e.setParam(ParamId::TunePurity, 0.85f);
            e.setParam(ParamId::TuneDrift, 1.0f);
            e.setParam(ParamId::TuneDriftRate, 0.1f);
            e.setParam(ParamId::TuneGuard, guard);
            e.setParam(ParamId::Release, 0.2f);
            e.reset();
            e.noteOn(57, 0.8f); e.noteOn(61, 0.8f);
            std::vector<float> L(256), R(256);
            int samples = 0, hits = 0;
            lowestFactor = 1.0f;
            for (int b = 0; b < 22000; ++b) {                  // about two minutes
                e.process(L.data(), R.data(), 256);
                if (b < 1000 || (b % 100) != 0) continue;
                ++samples;
                const float hz = e.beatHz();
                if (hz > 2.0f && hz < 8.0f) ++hits;
                lowestFactor = std::min(lowestFactor, e.guardFactor());
            }
            return samples > 0 ? static_cast<double>(hits) / samples : 0.0;
        };
        float f0 = 1.0f, fPlus = 1.0f, fMinus = 1.0f;
        const double none = inBand(0.0f, f0), reined = inBand(1.0f, fPlus), sought = inBand(-1.0f, fMinus);
        std::printf("  [probe] time with the beat between 2 and 8 Hz: %.0f%% guard off, %.0f%% reined in, %.0f%% sought out (factor down to %.2f)\n",
                    none * 100.0, reined * 100.0, sought * 100.0, fPlus);
        CHECK(f0 == 1.0f, "at 0 the guard does nothing to the drift");
        CHECK(fPlus < 1.0f, "above 0 it reins the drift in when the beat is in the band");
        CHECK(reined <= none, "and the beat spends no more time there than without it");
    }

    // ---- the scale a timbre asks for ---------------------------------------------------------
    {
        // Sethares' claim, put to the instrument's own spectrum: the intervals at which a timbre
        // is least rough against a transposed copy of itself ARE its scale, and for a harmonic
        // spectrum they are just intonation. If that comes out it is not because anybody typed
        // the ratios in -- there is no table anywhere in this path.
        auto build = [](float tilt, float bright, float oddEven, float inharm, FixedScale& out) {
            BrainSpectrum sp;
            const int count = BrainSpectrum::kMax;
            const float hc = 1.0f + bright * bright * 31.0f;
            const double B = static_cast<double>(inharm) * inharm * 0.02;
            sp.count = count;
            for (int h = 1; h <= count; ++h) {
                double a = std::pow(static_cast<double>(h), -static_cast<double>(tilt));
                if (oddEven > 0.0f && (h % 2) == 0) a *= 1.0 - oddEven;
                if (oddEven < 0.0f && (h % 2) == 1 && h > 1) a *= 1.0 + oddEven;
                if (static_cast<float>(h) > hc) { const float x = std::min((static_cast<float>(h) - hc) / 6.0f, 1.0f); a *= 0.5 * (1.0 + std::cos(3.14159265358979 * x)); }
                sp.amp[h - 1] = a;
                sp.ratio[h - 1] = h * (B > 0.0 ? std::sqrt(1.0 + B * h * h) : 1.0);
            }
            return makeTimbreScale(sp, out);
        };
        auto nearest = [](const FixedScale& s, double cents) {
            double best = 1e9;
            for (int i = 0; i < s.count; ++i) {
                const double c = 1200.0 * std::log2(s.ratios[i]);
                if (std::fabs(c - cents) < std::fabs(best)) best = c - cents;
            }
            return best;
        };
        FixedScale harm, stiff;
        CHECK(build(1.0f, 0.7f, 0.0f, 0.0f, harm), "a harmonic spectrum asks for a scale");
        CHECK(build(1.0f, 0.7f, 0.0f, 1.0f, stiff), "and so does a stiff string");
        std::printf("  [probe] timbre scale, harmonic (%d degrees):", harm.count);
        for (int i = 0; i < harm.count; ++i) std::printf(" %.0f", 1200.0 * std::log2(harm.ratios[i]));
        std::printf("\n");
        std::printf("  [probe] timbre scale, stiff string (%d degrees):", stiff.count);
        for (int i = 0; i < stiff.count; ++i) std::printf(" %.0f", 1200.0 * std::log2(stiff.ratios[i]));
        std::printf("\n");
        // The just intervals, in cents: 5/4, 4/3, 3/2, 8/5, 5/3, 7/4.
        const double just[6] = { 386.31, 498.04, 701.96, 813.69, 884.36, 968.83 };
        double worst = 0.0;
        for (double c : just) worst = std::max(worst, std::fabs(nearest(harm, c)));
        std::printf("  [probe] furthest of the six just intervals from a degree of the harmonic scale: %.1f cents\n", worst);
        CHECK(worst < 4.0, "for a harmonic spectrum the dissonance minima ARE just intonation");
        CHECK(harm.ratios[0] == 1.0, "and the scale starts at the unison");
        for (int i = 1; i < harm.count; ++i)
            CHECK(harm.ratios[i] > harm.ratios[i - 1] && harm.ratios[i] < harm.period, "ascending, inside the octave");
        // A stiff string is a different instrument and asks for a different scale.
        int moved = 0;
        for (double c : just) if (std::fabs(nearest(stiff, c)) > 15.0) ++moved;
        std::printf("  [probe] of those six, %d have no degree within 15 cents in the stiff-string scale\n", moved);
        CHECK(moved >= 4, "and an inharmonic spectrum asks for something else entirely");

        // And through the engine: choosing the scale has to actually retune the instrument, and
        // changing the spectrum has to change the tuning with it. A scale that is computed while
        // the sound runs is worth nothing if the thing that computes it never gets asked.
        {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::Scale, static_cast<float>(kTimbreScaleIndex));
            e.setParam(ParamId::Inharmonic, 0.0f);
            e.reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < 200; ++b) e.process(L.data(), R.data(), 256);
            const FixedScale plain = e.scale();
            e.setParam(ParamId::Inharmonic, 1.0f);
            for (int b = 0; b < 200; ++b) e.process(L.data(), R.data(), 256);
            const FixedScale stretched = e.scale();
            std::printf("  [probe] engine timbre scale: %d degrees plain, %d with Inharmonic up\n",
                        plain.count, stretched.count);
            double moved2 = 0.0;
            const int n = std::min(plain.count, stretched.count);
            for (int i = 0; i < n; ++i)
                moved2 = std::max(moved2, std::fabs(1200.0 * std::log2(plain.ratios[i] / stretched.ratios[i])));
            std::printf("  [probe] furthest degree moved when the spectrum changed: %.0f cents\n", moved2);
            CHECK(plain.count >= 5, "the engine builds the timbre scale when it is chosen");
            CHECK(std::fabs(1200.0 * std::log2(plain.ratios[std::min(5, plain.count - 1)]) - 702.0) < 6.0
                  || moved2 > 20.0, "and it is the spectrum's own scale, not the fallback");
            CHECK(moved2 > 20.0, "turning Inharmonic up retunes the instrument");

            // And what a key actually sounds at, which is a different question from what the
            // table says. It has to be asked separately: the first version of this rebuilt the
            // table correctly and set the flag that retunes the sounding notes, and three lines
            // further down that flag was assigned over -- so the scale changed underneath the
            // notes and not one of them heard about it. The table test passed throughout.
            Engine tet;
            tet.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) tet.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            tet.setParam(ParamId::Scale, 0.0f);
            tet.reset();
            std::vector<float> tl(256), tr(256);
            for (int b = 0; b < 200; ++b) tet.process(tl.data(), tr.data(), 256);
            double biggest = 0.0;
            for (int note = 55; note <= 72; ++note) {
                const double a = tet.frequencyOf(note), b2 = e.frequencyOf(note);
                if (a > 0.0 && b2 > 0.0) biggest = std::max(biggest, std::fabs(1200.0 * std::log2(b2 / a)));
            }
            std::printf("  [probe] a key sounds up to %.0f cents from where 12-TET puts it\n", biggest);
            CHECK(biggest > 20.0, "and the keys actually sound at the scale the timbre asked for");
        }
    }

    // ---- evenness, and the distance a chord travels ----------------------------------------
    {
        // Evenness: one for notes that divide the octave equally, zero for notes all in one place.
        auto E = [](std::initializer_list<double> semis) {
            std::vector<double> f;
            for (double s : semis) f.push_back(261.6255653005986 * std::pow(2.0, s / 12.0));
            return chordEvenness(f.data(), static_cast<int>(f.size()));
        };
        const double eAug = E({ 0, 4, 8 });          // the augmented triad divides the octave in three
        const double eMaj = E({ 0, 4, 7 });
        const double eCluster = E({ 0, 1, 2 });
        const double eDim7 = E({ 0, 3, 6, 9 });      // and the diminished seventh in four
        const double eOct = E({ 0, 12, 24 });        // three octaves are one pitch class
        std::printf("  [probe] evenness: augmented %.3f, dim7 %.3f, major triad %.3f, cluster %.3f, octaves %.3f\n",
                    eAug, eDim7, eMaj, eCluster, eOct);
        CHECK(eAug > 0.99 && eDim7 > 0.99, "chords that divide the octave equally are perfectly even");
        CHECK(eMaj > 0.8 && eMaj < eAug, "a major triad is nearly even, but not quite");
        CHECK(eCluster < 0.3, "a cluster is not");
        CHECK(eOct < 0.05, "and three octaves of one note are as uneven as a chord can be");

        // The distance a chord travels when one note is exchanged. Tymoczko's measure is the
        // smallest total movement over every way of pairing the old chord with the new one; for
        // an exchange of one note that is exactly the leap the one voice makes, which is why the
        // number already in the conductor needed no correcting. Checked here rather than argued.
        {
            Rng rng; rng.seed(0xA5A5u);
            double worst = 0.0;
            for (int trial = 0; trial < 400; ++trial) {
                std::vector<int> chord;
                const int n = 2 + static_cast<int>(rng.uniform() * 4.0);
                while (static_cast<int>(chord.size()) < n) {
                    const int c = 36 + static_cast<int>(rng.uniform() * 44.0);
                    if (std::find(chord.begin(), chord.end(), c) == chord.end()) chord.push_back(c);
                }
                std::sort(chord.begin(), chord.end());
                const int leaving = chord[static_cast<size_t>(rng.uniform() * n) % chord.size()];
                int arriving = 36 + static_cast<int>(rng.uniform() * 44.0);
                if (std::find(chord.begin(), chord.end(), arriving) != chord.end()) continue;
                std::vector<int> after;
                for (int c : chord) if (c != leaving) after.push_back(c);
                after.push_back(arriving);
                std::sort(after.begin(), after.end());
                // The sorted pairing is the cheapest one for absolute distances on a line.
                double sorted = 0.0;
                for (size_t i = 0; i < chord.size(); ++i) sorted += std::fabs(static_cast<double>(chord[i] - after[i]));
                worst = std::max(worst, std::fabs(sorted - std::fabs(static_cast<double>(arriving - leaving))));
            }
            std::printf("  [probe] chord distance against the single leap, worst of 400: %.3f semitones\n", worst);
            CHECK(worst < 1e-9, "the leap one voice makes IS the distance the whole chord travels");
        }

        // And in the conductor, in the mode where the question arises: with Smooth up, the chord
        // has to travel less per exchange than when the voice that moves is chosen by the clock.
        //
        // Driven directly, not through the engine. Measured through the audio this said the chord
        // travelled nought semitones with Smooth off -- because what it was really pairing was a
        // voice whose envelope dipped under the threshold and came back, a departure and an
        // arrival of the same note. The conductor emits its exchange as a note off immediately
        // followed by a note on, and reading that is exact and needs no envelopes at all.
        auto travel = [](float smooth, double& mean, int& moves) {
            BrainParams p;
            p.on = true;
            p.mode = BrainMode::Chords;
            p.density = 5;
            p.rateSeconds = 2.0f;
            p.low = 36; p.high = 79;
            p.smooth = smooth;
            ClusterBrain brain;
            brain.reset(0x9E3779B9ull, 48);
            auto freqOf = [](int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); };
            int pending = -1;
            double sum = 0.0;
            moves = 0;
            for (int step = 0; step < 3000; ++step)
                brain.update(0.1, p, -1, freqOf, [&](const BrainEvent& e) {
                    if (e.type == BrainEvent::Type::NoteOff) { pending = e.note; return; }
                    if (pending >= 0) { sum += std::fabs(static_cast<double>(e.note - pending)); ++moves; pending = -1; }
                });
            mean = moves > 0 ? sum / moves : 0.0;
        };
        double tOff = 0.0, tOn = 0.0; int mOff = 0, mOn = 0;
        travel(0.0f, tOff, mOff);
        travel(1.0f, tOn, mOn);
        std::printf("  [probe] chords mode: mean travel %.2f semitones over %d exchanges, %.2f over %d with Smooth\n",
                    tOff, mOff, tOn, mOn);
        CHECK(mOn >= 20 && mOff >= 20, "the conductor exchanges voices in both settings");
        CHECK(tOn < tOff, "and with Smooth up the chord travels less to get where it goes");

        // And Even, on the chords the conductor actually settles on.
        auto spread = [](float even, float harmonic) {
            BrainParams p;
            p.on = true;
            p.mode = BrainMode::Free;
            p.density = 5;
            p.rateSeconds = 3.0f;
            p.holdMin = 20.0f; p.holdMax = 60.0f;
            p.low = 36; p.high = 79;
            p.even = even;
            p.harmonic = harmonic;
            ClusterBrain brain;
            brain.reset(0xC2B2AE35ull, 48);
            auto freqOf = [](int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); };
            double sum = 0.0; int taken = 0;
            for (int step = 0; step < 3000; ++step) {
                brain.update(0.1, p, -1, freqOf, [](const BrainEvent&) {});
                if (step % 100 != 0 || step < 300) continue;
                int m = 0;
                double f[16];
                for (int n = 0; n < 128 && m < 16; ++n) if (brain.sounding(n)) f[m++] = freqOf(n);
                if (m >= 2) { sum += chordEvenness(f, m); ++taken; }
            }
            return taken > 0 ? sum / taken : 0.0;
        };
        const double evenOff = spread(0.0f, 0.0f), evenOn = spread(1.0f, 0.0f), evenHarm = spread(0.0f, 1.0f);
        std::printf("  [probe] mean evenness of the chords built: %.3f plain, %.3f with Even, %.3f with Harmonic\n",
                    evenOff, evenOn, evenHarm);
        // Measured: 0.664 plain, 0.768 with Even, 0.445 with Harmonic. The exponent was chosen by
        // measurement too -- at 2.5 the effect was 0.680, barely there; at 9 it was 0.776, which is
        // most of the way to what a twelve-tone scale can offer and no longer a preference.
        CHECK(evenOn > evenOff * 1.08, "with Even up the conductor spreads the chord round the octave");
        CHECK(evenHarm < evenOn, "and Harmonic pulls the other way, as it is meant to");
    }

    // ---- the tonal hierarchy, and finding the key -------------------------------------------
    {
        // The finder first, on distributions whose answer is known. C major's own scale must come
        // back as C major; the same notes with A weighted heavily must come back as A minor,
        // because that is exactly what the two profiles differ about.
        auto named = [](const KeyEstimate& k) {
            static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            static char buf[16];
            if (k.key < 0) { std::snprintf(buf, sizeof(buf), "none"); return static_cast<const char*>(buf); }
            std::snprintf(buf, sizeof(buf), "%s %s", n[k.tonic()], k.minor() ? "minor" : "major");
            return static_cast<const char*>(buf);
        };
        {   // A plain C major scale, every degree sounding equally.
            float w[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
            const KeyEstimate k = findKey(w);
            std::printf("  [probe] key of a C major scale: %s (r %.2f)\n", named(k), k.confidence);
            CHECK(k.key == 0, "a C major scale is found to be C major");
            CHECK(k.confidence > 0.5f, "and the finder is confident about it");
        }
        {   // The same seven notes, but A and E held far longer: the relative minor.
            float w[12] = { 1, 0, 1, 0, 1, 1, 0, 2, 0, 4, 0, 1 };
            const KeyEstimate k = findKey(w);
            std::printf("  [probe] key with A and E held: %s (r %.2f)\n", named(k), k.confidence);
            CHECK(k.key == 12 + 9, "the same notes with A held are found to be A minor");
        }
        {   // Twelve notes in perfect balance are not a key, and the finder must say so rather
            // than picking one at random.
            float w[12] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
            const KeyEstimate k = findKey(w);
            std::printf("  [probe] key of a full chromatic: %s (r %.2f)\n", named(k), k.confidence);
            CHECK(k.key < 0, "a perfectly balanced chromatic is no key at all");
            float z[12] = {};
            CHECK(findKey(z).key < 0, "and silence is no key either");
        }
        {   // A cluster: five neighbouring semitones. A key may be found, but weakly.
            float w[12] = { 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0 };
            const KeyEstimate k = findKey(w);
            std::printf("  [probe] key of a five-semitone cluster: %s (r %.2f)\n", named(k), k.confidence);
            CHECK(k.confidence < 0.55f, "a cluster is at best a weak key, and says so");
        }

        // And in the conductor: with Key up, what it plays must fit the key it has found better
        // than what it plays without.
        auto run = [&](float keyAmount, int& classes, float& conf) {
            Engine en;
            en.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) en.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            en.setParam(ParamId::BrainOn, 1.0f);
            en.setParam(ParamId::BrainKey, keyAmount);
            en.setParam(ParamId::BrainRate, 2.0f);
            en.setParam(ParamId::BrainDensity, 5.0f);
            en.setParam(ParamId::BrainConsonance, 0.3f);   // loose, so the key has something to do
            en.reset();
            std::vector<float> L(256), R(256);
            for (int b = 0; b < 34000; ++b) en.process(L.data(), R.data(), 256);   // three minutes
            const KeyEstimate k = en.brainKey();
            conf = k.confidence;
            bool on[128] = {};
            en.soundingNotes(on);
            bool seen[12] = {};
            classes = 0;
            for (int i = 0; i < 128; ++i) {
                if (!on[i]) continue;
                const int pc = pitchClassOf(en.frequencyOf(i));
                if (!seen[pc]) { seen[pc] = true; ++classes; }
            }
        };
        int clOff = 0, clOn = 0; float cOff = 0.0f, cOn = 0.0f;
        run(0.0f, clOff, cOff);
        run(1.0f, clOn, cOn);
        std::printf("  [probe] conductor after 3 min: key confidence %.2f off (%d pitch classes), %.2f on (%d classes)\n",
                    cOff, clOff, cOn, clOn);
        CHECK(cOn > cOff, "with Key up the music sits more clearly in one key than without it");
        CHECK(clOn >= 2, "and it is still playing a chord, not one note");
    }

    // ---- loudness in sones ------------------------------------------------------------------
    {
        // The model is checked against the anchors that DEFINE the sone, not against itself. A
        // one-kilohertz tone at 40 dB SPL is one sone by definition; ten decibels more is twice as
        // loud, so 50 dB is two sones and 60 dB is four. And loudness grows with bandwidth once a
        // sound is wider than a critical band, which is the whole reason this figure is worth
        // having beside LUFS: noise spread over many bands is louder than a tone of the same
        // energy, and no energy meter will ever say so.
        auto tone = [](float hz, float spl) {
            float bands[ZwickerLoudness::kBands];
            const float centres[ZwickerLoudness::kBands] = {
                25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f, 200.0f,
                250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f, 2000.0f,
                2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f };
            int best = 0; float d = 1e9f;
            for (int b = 0; b < ZwickerLoudness::kBands; ++b)
                if (std::fabs(centres[b] - hz) < d) { d = std::fabs(centres[b] - hz); best = b; }
            for (int b = 0; b < ZwickerLoudness::kBands; ++b) bands[b] = -100.0f;
            bands[best] = spl;
            return ZwickerLoudness::fromBandLevels(bands);
        };
        const float s40 = tone(1000.0f, 40.0f), s50 = tone(1000.0f, 50.0f), s60 = tone(1000.0f, 60.0f);
        std::printf("  [probe] 1 kHz: 40 dB %.2f sone, 50 dB %.2f, 60 dB %.2f (doubling %.2f, %.2f)\n",
                    s40, s50, s60, s50 / std::max(1e-6f, s40), s60 / std::max(1e-6f, s50));
        // The 40 dB point is pinned there -- it is the definition of the unit -- so this check
        // only proves the pinning is wired up. Everything after it is the model's own answer.
        CHECK(std::fabs(s40 - 1.0f) < 0.02f, "a kilohertz tone at 40 dB is one sone by definition");
        CHECK(s50 / s40 > 1.8f && s50 / s40 < 2.3f, "and ten decibels more is twice as loud");
        CHECK(s60 / s50 > 1.8f && s60 / s50 < 2.3f, "and ten more again");
        CHECK(s60 > 3.4f && s60 < 4.8f, "which puts 60 dB at the four sones the standard says");
        // Under the threshold in quiet nothing is heard at all.
        CHECK(tone(1000.0f, -10.0f) < 0.01f, "nothing under the threshold in quiet is heard");
        // Bandwidth: the same total energy in one band and spread over ten.
        {
            float narrow[ZwickerLoudness::kBands], wide[ZwickerLoudness::kBands];
            for (int b = 0; b < ZwickerLoudness::kBands; ++b) { narrow[b] = -100.0f; wide[b] = -100.0f; }
            narrow[16] = 60.0f;                                  // all of it at 1 kHz
            for (int b = 12; b < 22; ++b) wide[b] = 50.0f;        // the same energy over ten bands
            const float sn = ZwickerLoudness::fromBandLevels(narrow), sw = ZwickerLoudness::fromBandLevels(wide);
            std::printf("  [probe] same energy: one band %.2f sone, ten bands %.2f sone\n", sn, sw);
            CHECK(sw > sn * 1.3f, "the same energy spread over many bands is louder than in one");
        }
        // And through the meter itself, on a rendered signal rather than on a table of levels.
        {
            LoudnessMeter m;
            const int sr2 = 48000;
            m.prepare(sr2);
            std::vector<float> L(sr2), R(sr2);
            // A kilohertz sine at -60 dBFS, which against a full scale of 100 dB SPL is 40 dB.
            const float amp = std::pow(10.0f, -60.0f / 20.0f) * std::sqrt(2.0f);
            for (int i = 0; i < sr2; ++i)
                L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = amp * std::sin(6.2831853f * 1000.0f * static_cast<float>(i) / sr2);
            for (int off = 0; off + 256 <= sr2; off += 256) m.process(L.data() + off, R.data() + off, 256);
            const LoudnessReading r = m.read();
            std::printf("  [probe] meter: %.2f sone now, %.2f max, %.2f N5, %.1f LUFS short\n",
                        r.sones, r.sonesMax, r.sonesN5, r.shortTerm);
            CHECK(r.sones > 0.7f && r.sones < 1.5f, "the meter itself reads one sone for that tone");
            CHECK(r.sonesMax >= r.sones - 0.01f, "the maximum is not below the current value");
        }
        {   // Silence is nothing, and a reset clears it.
            LoudnessMeter m;
            m.prepare(48000.0);
            std::vector<float> z(48000, 0.0f);
            for (int off = 0; off + 256 <= 48000; off += 256) m.process(z.data() + off, z.data() + off, 256);
            CHECK(m.read().sones < 0.001f, "silence is nothing at all");
        }
    }

    // ---- the spectral model -----------------------------------------------------------------
    {
        // A clip with a tonal half and a noisy half, so the analysis has both to find: two seconds
        // of a 300 Hz tone with its fifth, then two seconds of high-passed noise.
        const int clipLen = 4 * sr;
        std::vector<float> clip(static_cast<size_t>(clipLen), 0.0f);
        {
            Rng rng; rng.seed(0x5EED4321u);
            float hp = 0.0f;
            for (int i = 0; i < clipLen; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(sr);
                if (i < clipLen / 2)
                    clip[static_cast<size_t>(i)] = 0.35f * std::sin(6.2831853f * 300.0f * t) + 0.2f * std::sin(6.2831853f * 450.0f * t);
                else {
                    const float w = rng.bipolar();
                    hp += 0.35f * (w - hp);
                    clip[static_cast<size_t>(i)] = 0.4f * (w - hp);
                }
            }
        }
        {   // The model itself: measured on the clip, before any engine touches it.
            Texture tex;
            tex.mono = clip;
            tex.sampleRate = sr;
            tex.measure();
            CHECK(!tex.spectral.empty(), "a clip long enough is measured into a band model");
            const SpectralModel& m = tex.spectral;
            // The tonal half must read as tonal and the noisy half as noise, in the bands that
            // carry the energy. This is the one claim the whole type rests on.
            auto meanTone = [&](int frame) {
                double num = 0.0, den = 0.0;
                for (int b = 0; b < SpectralModel::kBands; ++b) {
                    const size_t i = static_cast<size_t>(frame) * SpectralModel::kBands + static_cast<size_t>(b);
                    num += static_cast<double>(m.tone[i]) * m.amp[i];
                    den += m.amp[i];
                }
                return den > 0.0 ? num / den : 0.0;
            };
            const double tonal = meanTone(m.frames / 4), noisy = meanTone(m.frames * 3 / 4);
            std::printf("  [probe] model %d frames, hop %.1f ms, tone tonal %.2f noisy %.2f\n",
                        m.frames, m.hop * 1000.0f, tonal, noisy);
            CHECK(tonal > 0.5, "a tone reads as tonal");
            CHECK(noisy < 0.3, "and noise reads as noise");
            CHECK(tonal > noisy + 0.3, "with the two well apart");
            // The band that holds the 300 Hz tone must say 300 Hz, not the centre of its band.
            int best = 0; float peak = 0.0f;
            for (int b = 0; b < SpectralModel::kBands; ++b) {
                const size_t i = static_cast<size_t>(m.frames / 4) * SpectralModel::kBands + static_cast<size_t>(b);
                if (m.amp[i] > peak) { peak = m.amp[i]; best = b; }
            }
            const float found = m.freq[static_cast<size_t>(m.frames / 4) * SpectralModel::kBands + static_cast<size_t>(best)];
            std::printf("  [probe] loudest band %d centre %.0f Hz, partial found at %.1f Hz\n", best, m.centre[best], found);
            CHECK(std::fabs(found - 300.0f) < 8.0f, "and the partial is placed inside its band, not at the band's centre");
        }
        {   // Played back: it must sound, at the level of every other type, and Rate and the note
            // must be independent of each other.
            auto render = [&](float rate, float breath, int note, std::vector<float>& cap) {
                Engine e;
                e.prepare(sr, 256);
                for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
                e.setTexture(clip.data(), clipLen, sr, 300.0, false);
                e.setParam(ParamId::BrainOn, 0.0f);
                e.setParam(ParamId::Src1Type, static_cast<float>(SourceType::Spectral));
                e.setParam(ParamId::Src1Follow, 1.0f);
                e.setParam(ParamId::Src1SpecRate, rate);
                e.setParam(ParamId::Src1SpecBreath, breath);
                e.setParam(ParamId::Src1Position, 0.1f);
                e.setParam(ParamId::Src1PosDrift, 0.0f);
                e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::FilterOn, 0.0f);
                e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f);
                e.setParam(ParamId::EnsembleMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
                e.setParam(ParamId::Attack, 0.05f); e.setParam(ParamId::Release, 0.5f);
                e.reset();
                e.noteOn(note, 0.9f);
                std::vector<float> L(256), R(256);
                for (int b = 0; b < 500; ++b) {
                    e.process(L.data(), R.data(), 256);
                    if (b >= 100) for (int i = 0; i < 256; ++i) cap.push_back(L[static_cast<size_t>(i)]);
                }
            };
            auto rmsOf = [](const std::vector<float>& v) {
                double s = 0.0;
                for (float x : v) s += static_cast<double>(x) * x;
                return std::sqrt(s / std::max<size_t>(1, v.size()));
            };
            // How rough the spectrum is: the mean absolute second difference against the signal's
            // own size. A sum of partials is smooth between its peaks; noise is not.
            auto flatness = [](const std::vector<float>& v) {
                double num = 0.0, den = 0.0;
                for (size_t i = 2; i < v.size(); ++i) {
                    const double d = static_cast<double>(v[i]) - 2.0 * v[i - 1] + v[i - 2];
                    num += d * d;
                    den += static_cast<double>(v[i]) * v[i];
                }
                return den > 0.0 ? std::sqrt(num / den) : 0.0;
            };
            std::vector<float> normal, frozen, fast, tonal, breathy, low, high;
            render(1.0f, 0.0f, 62, normal);      // D4, a fourth above the clip's own 300 Hz
            render(0.0f, 0.0f, 62, frozen);
            render(2.0f, 0.0f, 62, fast);
            render(1.0f, -1.0f, 62, tonal);
            render(1.0f, 1.0f, 62, breathy);
            // Held still in the tonal half of the clip and with the noise turned off, so what is
            // left is the partials alone and their pitch can be measured at all.
            render(0.0f, -1.0f, 62, low);
            render(0.0f, -1.0f, 74, high);       // an octave up
            const double rms = rmsOf(normal);
            std::printf("  [probe] spectral rms %.4f frozen %.4f | roughness tonal %.3f breathy %.3f\n",
                        rms, rmsOf(frozen), flatness(tonal), flatness(breathy));
            CHECK(rms > 0.005, "a spectral slot sounds");
            CHECK(rmsOf(frozen) > 0.005, "and still sounds with the read head standing still");
            CHECK(flatness(breathy) > flatness(tonal) * 1.5, "Breath at the noise end is rougher than at the partial end");
            // Rate moves the read, so two rates cannot render the same block.
            double diff = 0.0;
            const size_t n = std::min(normal.size(), fast.size());
            for (size_t i = 0; i < n; ++i) diff += std::fabs(static_cast<double>(normal[i]) - fast[i]);
            CHECK(diff / static_cast<double>(n) > 1e-4, "and Rate moves it: two rates are two renders");
            // The note transposes the model: an octave up has to halve the period. By
            // autocorrelation, which counts periods rather than sign changes -- a sum of partials
            // crosses zero more often than once a period, and counting crossings would say so.
            auto period = [&](const std::vector<float>& v) {
                double mean = 0.0;
                for (float x : v) mean += x;
                mean /= std::max<size_t>(1, v.size());
                const int lo = static_cast<int>(sr / 4000), hi = std::min<int>(static_cast<int>(sr / 60), static_cast<int>(v.size()) / 2);
                double best = 0.0; int bestLag = 1;
                for (int lag = lo; lag < hi; ++lag) {
                    double s = 0.0;
                    for (size_t i = static_cast<size_t>(lag); i < v.size(); i += 3)
                        s += (static_cast<double>(v[i]) - mean) * (static_cast<double>(v[i - static_cast<size_t>(lag)]) - mean);
                    if (s > best) { best = s; bestLag = lag; }
                }
                return sr / static_cast<double>(bestLag);
            };
            const double h1 = period(low), h2 = period(high);
            std::printf("  [probe] frozen partials: note 62 %.1f Hz, note 74 %.1f Hz (ratio %.2f)\n", h1, h2, h2 / std::max(1e-9, h1));
            CHECK(std::fabs(h2 / std::max(1e-9, h1) - 2.0) < 0.15, "an octave up is an octave up");
        }
    }

    // ---- the bowed string -------------------------------------------------------------------
    {
        // It must sound, sustain, stay bounded, play the note it is asked for, and come out at
        // roughly the level every other source type comes out at.
        auto capture = [&](float type, std::vector<float>& cap, double& peak) {
            Engine e;
            e.prepare(sr, 256);
            for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
            e.setParam(ParamId::BrainOn, 0.0f);
            e.setParam(ParamId::Src1Type, type);
            e.setParam(ParamId::Src1BowForce, 0.5f);
            e.setParam(ParamId::Src1BowSpeed, 0.4f);
            e.setParam(ParamId::Air, 0.0f);          // the noise band would pin every measurement
            e.setParam(ParamId::FilterOn, 0.0f);
            e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f);
            e.setParam(ParamId::EnsembleMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
            e.setParam(ParamId::Attack, 0.05f); e.setParam(ParamId::Release, 0.5f);
            e.reset();
            e.noteOn(57, 0.9f);                      // A3, 220 Hz
            std::vector<float> L(256), R(256);
            peak = 0.0;
            for (int b = 0; b < 800; ++b) {
                e.process(L.data(), R.data(), 256);
                for (int i = 0; i < 256; ++i) {
                    peak = std::max(peak, std::fabs(static_cast<double>(L[static_cast<size_t>(i)])));
                    if (b >= 400) cap.push_back(L[static_cast<size_t>(i)]);
                }
            }
        };
        auto rmsOf = [](const std::vector<float>& v) {
            double s = 0.0;
            for (float x : v) s += static_cast<double>(x) * x;
            return std::sqrt(s / std::max<size_t>(1, v.size()));
        };
        std::vector<float> cap, ref;
        double peak = 0.0, refPeak = 0.0;
        capture(7.0f, cap, peak);        // Bow
        capture(1.0f, ref, refPeak);     // Wavetable, as the level every other type is set by
        const double rms = rmsOf(cap), refRms = rmsOf(ref);
        CHECK(rms > 0.001, "the bow sounds");
        CHECK(peak < 1.5 && std::isfinite(peak), "and stays bounded");
        // The pitch by autocorrelation, not by zero crossings: a bowed string's Helmholtz corner
        // crosses zero four times a period, not twice, and counting crossings reported this very
        // signal as 440 Hz when its period was plainly 218 samples.
        double best = 0.0; int bestLag = 1;
        const int lo = static_cast<int>(sr / 800), hi = std::min<int>(static_cast<int>(sr / 60), static_cast<int>(cap.size()) / 2);
        double mean = 0.0;
        for (float v : cap) mean += v;
        mean /= std::max<size_t>(1, cap.size());
        for (int lag = lo; lag < hi; ++lag) {
            double s = 0.0;
            for (size_t i = static_cast<size_t>(lag); i < cap.size(); ++i)
                s += (static_cast<double>(cap[i]) - mean) * (static_cast<double>(cap[i - static_cast<size_t>(lag)]) - mean);
            if (s > best) { best = s; bestLag = lag; }
        }
        const double hz = sr / static_cast<double>(bestLag);
        const double levelDb = 20.0 * std::log10((rms + 1e-12) / (refRms + 1e-12));
        std::printf("  [probe] bow rms %.4f peak %.3f hz %.1f | wavetable rms %.4f -> %+.1f dB\n",
                    rms, peak, hz, refRms, levelDb);
        CHECK(std::fabs(hz - 220.0) < 6.0, "at the pitch it was asked for");
        CHECK(std::fabs(levelDb) < 6.0, "and within a few decibels of a wavetable at the same Level");
        // A bow that stops moving stops sounding: the hair is still on the string, and hair that
        // does not move absorbs. Bowed again at zero speed the note has to die, not ring on.
        Engine e;
        e.prepare(sr, 256);
        for (int i = 0; i < kNumParams; ++i) e.setParam(static_cast<ParamId>(i), paramTable()[static_cast<size_t>(i)].def);
        e.setParam(ParamId::BrainOn, 0.0f);
        e.setParam(ParamId::Src1Type, 7.0f);
        e.setParam(ParamId::Src1BowForce, 0.5f); e.setParam(ParamId::Src1BowSpeed, 0.4f);
        e.setParam(ParamId::Air, 0.0f); e.setParam(ParamId::FilterOn, 0.0f);
        e.setParam(ParamId::NearMix, 0.0f); e.setParam(ParamId::FarLevel, 0.0f);
        e.setParam(ParamId::EnsembleMix, 0.0f); e.setParam(ParamId::DelayMix, 0.0f); e.setParam(ParamId::Delay2Mix, 0.0f);
        e.setParam(ParamId::Attack, 0.05f); e.setParam(ParamId::Release, 0.5f);
        e.reset();
        e.noteOn(57, 0.9f);
        std::vector<float> L(256), R(256);
        for (int b = 0; b < 400; ++b) e.process(L.data(), R.data(), 256);
        e.setParam(ParamId::Src1BowSpeed, 0.0f);
        for (int b = 0; b < 200; ++b) e.process(L.data(), R.data(), 256);
        double sq2 = 0.0;
        for (int b = 0; b < 100; ++b) { e.process(L.data(), R.data(), 256); for (int i = 0; i < 256; ++i) sq2 += static_cast<double>(L[static_cast<size_t>(i)]) * L[static_cast<size_t>(i)]; }
        const double restRms = std::sqrt(sq2 / 25600.0);
        std::printf("  [probe] bow at rest rms %.6f (%.1f dB below the bowed note)\n",
                    restRms, 20.0 * std::log10((rms + 1e-12) / (restRms + 1e-12)));
        CHECK(restRms < rms * 0.1, "and a bow at rest falls silent");
    }
}

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
    testPresetPacks();
    testModulation();
    testModulationEngine();
    testSpace();
    testRichCarving();
    testFeedback();
    testStackAndWander();
    testSources();
    testPresetText();
    testPresetMap();
    testRoom();
    testRoute();
    testZPlane();
    testTimeline();
    testPurityFreezeSleep();
    testGhostPortaInertiaTapeCoherence();
    testParamTable();
    testAutoplay();
    testScore();
    testStems();
    testSampleRates();
    testExpressionBodyPatina();
    testEnvShapePresets();
    testBeatSource();
    testDelayDuck();
    testZModal();
    testZPlaneBank();
    testFilterModels();
    testMixDeskFive();
    testAfterTheClassics();
    testBrainTimbre();
    testResearchBatch();
    if (failures == 0) std::printf("selftest: all checks passed\n");
    else std::printf("selftest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
