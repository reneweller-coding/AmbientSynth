// AmbientSynth -- one cluster voice: `unison` strands, each an additive bank of
// up to 32 harmonic partials with individually drifting amplitudes and a slowly
// drifting pitch, plus a filtered-noise "air" layer. Alias-free by construction.
//
// Spatial model (after Robert Rich): every voice sits on a plane between the
// listener's ear (distance 0: dry, bright, close) and the infinite background
// (distance 1: darker, softer, sent to the far reverb). The voice's centre pan
// is rendered with a true interaural time difference, not only with gain.
#pragma once
#include "Dsp.h"
#include "Sources.h"
#include "ZPlane.h"
#include <cstdint>

namespace ambient {

constexpr int kMaxPartials  = 32;
constexpr int kMaxStrands   = 6;
constexpr int kControlBlock = 64;   // samples between control-rate updates (1.3 ms at 48 kHz)
constexpr int kItdBuffer    = 256;  // >= 0.7 ms at 192 kHz

struct VoiceParams {
    float level = 1.0f;         // level of the partial bank (Source 1)
    int   partials = 16;
    float tilt = 1.2f, brightness = 0.7f, oddEven = 0.0f, inharmonic = 0.0f;
    float shimmer = 0.4f, shimmerRate = 0.15f;
    int   unison = 3;
    float detune = 8.0f, drift = 4.0f, driftRate = 0.08f, spread = 0.7f;
    float bloom = 0.0f, bloomTime = 60.0f;   // spectrum opens from brightness*(1-bloom) to brightness over bloomTime
    int   stack = 0;            // index into kStackRatios: strands at pure ratios (0 = classic detuned unison)
    float rateWander = 0.3f;    // drift/shimmer/breath rates wander by up to +-1 octave on a 100 s curve
    float air = 0.15f, airColor = 3.0f, airQ = 10.0f;
    float attack = 6.0f, decay = 4.0f, sustain = 0.8f, release = 12.0f;
    float cutoff = 2500.0f, resonance = 0.15f, filterEnv = 0.3f, filterDrift = 0.3f, keyTrack = 0.5f;
    // Z-plane filter (ZPlane.h): 0 off, 1 in series after the SVF, 2 instead of it
    int   zMode = 0, zShape = 0;
    float zX = 0.5f, zY = 0.5f, zRate = 0.05f, zDepth = 0.5f, zRes = 0.5f, zKeyTrack = 0.0f, zMix = 0.7f;
    float panDrift = 0.4f, itd = 0.6f;
    // Rich's foreground/background carving inside the voice (see concept.md):
    float presence = 0.0f;      // dB bell at 2-5 kHz, full on the near plane, gone on the far plane
    float breath = 0.0f, breathRate = 0.03f;   // slow wandering of the distance itself (+-0.35 at 1)
    float lowCut = 0.0f;        // Hz; partials below fall 12 dB/oct, keeping the pads out of the sub's register
    float fmAmount = 0.0f;      // phase modulation of every partial (h times the deviation) by the `fm` signal given to render()
    // Extra sources (Source 2 / 3) and the data they may need; pointers stay valid for the block.
    SlotParams       slot[kSlots];
    const Wavetable* userTable = nullptr;
    const Texture*   texture = nullptr;
};

class Voice {
public:
    void prepare(double sampleRate, uint64_t seed);
    // distance 0..1 = plane (see above); fixed for the life of the note.
    void noteOn(int note, double freqHz, float velocity, int owner, float distance, const VoiceParams& p);
    void noteOff();
    void kill();

    bool isActive() const    { return env_.isActive(); }
    bool isReleasing() const { return env_.isReleasing(); }
    float level() const      { return env_.level(); }
    int  note() const        { return note_; }
    int  owner() const       { return owner_; }
    float distance() const   { return distEff_; }   // where the voice is right now (breath included)
    double frequency() const { return freq_; }
    uint64_t order = 0;      // allocation order for voice stealing

    // Adds `n` samples into the near (dry plane) and far (reverb send) buses. `fm` (n samples,
    // may be null) phase-modulates the partials when p.fmAmount > 0 (the feedback loop).
    void render(float* nearL, float* nearR, float* farL, float* farR, int n, const VoiceParams& p, const float* fm = nullptr);

private:
    struct Strand {
        // Every partial is a rotating phasor (cos, sin) turned by its own per-sample rotation;
        // independent across partials, so the loop pipelines and vectorises. Phasors are
        // renormalised once per control block.
        float  pc[kMaxPartials] = {}, ps[kMaxPartials] = {};
        float  rc[kMaxPartials] = {}, rs[kMaxPartials] = {};
        float  amp[kMaxPartials] = {};
        float  ampStep[kMaxPartials] = {};
        Drifter shimmer[kMaxPartials];
        Drifter pitch;
        float  gainL = 0.7f, gainR = 0.7f;
        int    active = 0;
    };
    void control(int blockLen, const VoiceParams& p);

    Strand   strands_[kMaxStrands];
    SourceSlot slots_[kSlots];
    float    rateMul_ = 1.0f;
    Envelope env_;
    Svf      filtL_, filtR_;
    Svf      airL_, airR_;
    Drifter  filterDrift_, airDrift_, panCenter_, breath_, rateWander_;
    Drifter  zDriftX_, zDriftY_;
    Resonator zL_[kZPeaks], zR_[kZPeaks];
    float    zWet_ = 0.0f, zDry_ = 1.0f, zGain_ = 0.5f;
    int      zModeCur_ = 0;
    Rng      rng_;
    double   sr_ = 48000.0;
    double   freq_ = 220.0;
    float    velocity_ = 1.0f;
    float    distance_ = 0.0f;   // the plane the note was placed on
    float    distEff_ = 0.0f;    // distance after breathing, refreshed at control rate
    float    gNear_ = 1.0f, gFar_ = 0.0f, gLevel_ = 1.0f;
    float    airGain_ = 0.0f;
    float    bloomT_ = 0.0f;     // seconds since note start, for Bloom
    // Control-rate caches: the spectral shape only changes when its parameters do.
    float    tiltCache_[kMaxPartials + 1] = {};
    float    cachedTilt_ = -1.0f, cachedOddEven_ = -9.0f;
    int      cachedPartials_ = -1;
    double   stretchCache_[kMaxPartials] = {};
    float    cachedB_ = -1.0f;
    float    itdBufL_[kItdBuffer] = {}, itdBufR_[kItdBuffer] = {};
    int      itdW_ = 0;
    float    itdL_ = 0.0f, itdR_ = 0.0f, itdLTarget_ = 0.0f, itdRTarget_ = 0.0f;
    float    shadowL_ = 0.0f, shadowR_ = 0.0f, shadowCoefL_ = 1.0f, shadowCoefR_ = 1.0f;   // head shadow on the far ear
    int      note_ = -1;
    int      owner_ = 0;
    int      lastUnison_ = 0;
};

} // namespace ambient
