// AmbientSynth self test: parameter table, tuning, envelope, engine render,
// cluster brain, determinism. Exit code 0 = all passed.
#include "ambient/Engine.h"
#include "ambient/Params.h"
#include "ambient/Tuning.h"
#include "ambient/Dsp.h"
#include "ambient/Effects.h"
#include "ambient/Cosmos.h"
#include "ambient/Presets.h"
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
    CHECK(numPresets() == 128, "exactly 128 presets");
    for (int p = 0; p < numPresets(); ++p)
        for (int q = 0; q < p; ++q) CHECK(std::strcmp(preset(p).name, preset(q).name) != 0, "preset names unique");
    for (int p = 0; p < numPresets(); ++p) {
        int count = 0;
        const bool ok = applyPreset(preset(p), [&](ParamId, float) { ++count; });
        CHECK(ok, "preset settings all refer to known parameters");
        CHECK(count >= kNumParams, "preset sets every parameter");
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
    int seen = 0, far = 0;
    for (int n = 0; n < 128; ++n) if (notes[n]) { const float d = e.noteDistance(n); CHECK(d >= 0.0f && d <= 1.0f, "distance in range"); ++seen; if (d > 0.5f) ++far; }
    CHECK(seen >= 2, "several notes sounding");
    CHECK(far >= 1, "at least one note in the background plane");
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

} // namespace

int main()
{
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
    if (failures == 0) std::printf("selftest: all checks passed\n");
    else std::printf("selftest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
