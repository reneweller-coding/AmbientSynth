// AmbientSynth -- one cluster voice: `unison` strands, each an additive bank of
// up to 32 harmonic partials with individually drifting amplitudes and a slowly
// drifting pitch. Alias-free by construction (partials above Nyquist are dropped).
#pragma once
#include "Dsp.h"
#include <cstdint>

namespace ambient {

constexpr int kMaxPartials  = 32;
constexpr int kMaxStrands   = 6;
constexpr int kControlBlock = 32;   // samples between control-rate updates

struct VoiceParams {
    int   partials = 16;
    float tilt = 1.2f, brightness = 0.7f, oddEven = 0.0f, inharmonic = 0.0f;
    float shimmer = 0.4f, shimmerRate = 0.15f;
    int   unison = 3;
    float detune = 8.0f, drift = 4.0f, driftRate = 0.08f, spread = 0.7f;
    float attack = 6.0f, decay = 4.0f, sustain = 0.8f, release = 12.0f;
    float cutoff = 2500.0f, resonance = 0.15f, filterEnv = 0.3f, filterDrift = 0.3f, keyTrack = 0.5f;
};

class Voice {
public:
    void prepare(double sampleRate, uint64_t seed);
    void noteOn(int note, double freqHz, float velocity, int owner, const VoiceParams& p);
    void noteOff();
    void kill();

    bool isActive() const    { return env_.isActive(); }
    bool isReleasing() const { return env_.isReleasing(); }
    float level() const      { return env_.level(); }
    int  note() const        { return note_; }
    int  owner() const       { return owner_; }
    uint64_t order = 0;      // allocation order for voice stealing

    // Adds `n` samples into L/R. n should be <= kControlBlock for one control update per call,
    // larger n is split internally.
    void render(float* L, float* R, int n, const VoiceParams& p);

private:
    struct Strand {
        double phase[kMaxPartials] = {};
        double inc[kMaxPartials] = {};
        float  amp[kMaxPartials] = {};
        float  ampStep[kMaxPartials] = {};
        Drifter shimmer[kMaxPartials];
        Drifter pitch;
        float  gainL = 0.7f, gainR = 0.7f;
        int    active = 0;
    };
    void control(int blockLen, const VoiceParams& p);

    Strand   strands_[kMaxStrands];
    Envelope env_;
    Svf      filtL_, filtR_;
    Drifter  filterDrift_;
    Rng      rng_;
    double   sr_ = 48000.0;
    double   freq_ = 220.0;
    float    velocity_ = 1.0f;
    int      note_ = -1;
    int      owner_ = 0;
    int      lastUnison_ = 0;
};

} // namespace ambient
