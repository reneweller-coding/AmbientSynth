// AmbientSynth self test: parameter table, tuning, envelope, engine render,
// cluster brain, determinism. Exit code 0 = all passed.
#include "ambient/Engine.h"
#include "ambient/Params.h"
#include "ambient/Tuning.h"
#include "ambient/Dsp.h"
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
    e.setParam(ParamId::ReverbDecay, 1.0f);
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

} // namespace

int main()
{
    testParams();
    testTuning();
    testEnvelope();
    testEngineMidi();
    testBrain();
    testDeterminism();
    testUserScale();
    if (failures == 0) std::printf("selftest: all checks passed\n");
    else std::printf("selftest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
