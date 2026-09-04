// AmbientSynth -- the Cosmos path: science-fiction / deep-space processing.
//   FreqShifter    single-sideband frequency shifter (Hilbert pair), inharmonic, alien
//   CombResonator  tuned comb filters with feedback (metallic, hull-like resonances)
//   VowelFilter    three formants morphing slowly between vowels (alien choir)
//   Nebula         STFT magnitude smearing with random phases (Paulstretch-like
//                  texture; smear = 1 freezes the spectrum)
//   PitchShifter   granular two-head shifter, used for the shimmer feedback loop
// Buffers are allocated in prepare() only.
#pragma once
#include "Dsp.h"
#include <vector>

namespace ambient {

class FreqShifter {
public:
    void prepare(double sampleRate);
    void set(float shiftHz, float mix) { shift_ = shiftHz; mix_ = clampv(mix, 0.0f, 1.0f); }
    void process(float* L, float* R, int n);
private:
    struct Hilbert {
        float x1[4] = {}, x2[4] = {}, y1[4] = {}, y2[4] = {};
        float x1b[4] = {}, x2b[4] = {}, y1b[4] = {}, y2b[4] = {};
        float delayed = 0.0f;
        void tick(float in, float& re, float& im);
    };
    Hilbert hL_, hR_;
    double sr_ = 48000.0, ph_ = 0.0;
    float  shift_ = 0.0f, mix_ = 0.0f;
};

class CombResonator {
public:
    void prepare(double sampleRate);
    void set(float freqHz, float feedback, float mix);
    void process(float* L, float* R, int n);
private:
    std::vector<float> bufL_, bufR_;
    int    mask_ = 0, w_ = 0;
    double sr_ = 48000.0;
    float  dL_ = 100.0f, dR_ = 100.0f, dLcur_ = 0.0f, dRcur_ = 0.0f;
    float  fb_ = 0.8f, mix_ = 0.0f, lpL_ = 0.0f, lpR_ = 0.0f;
};

class VowelFilter {
public:
    void prepare(double sampleRate, uint64_t seed);
    void set(float mix, float rateHz) { mix_ = clampv(mix, 0.0f, 1.0f); rate_ = rateHz; }
    void process(float* L, float* R, int n);
private:
    Svf     fL_[3], fR_[3];
    Drifter morph_;
    Rng     rng_;
    double  sr_ = 48000.0;
    float   mix_ = 0.0f, rate_ = 0.05f;
    int     counter_ = 0;
};

class PitchShifter {
public:
    void prepare(double sampleRate);
    void setSemitones(float st);
    void process(const float* in, float* out, int n);
private:
    std::vector<float> buf_;
    int    mask_ = 0, w_ = 0;
    double sr_ = 48000.0;
    float  window_ = 3840.0f, rate_ = 1.0f, pos_ = 0.0f;
};

class Fft {
public:
    explicit Fft(int n = 2048);
    void transform(float* re, float* im, bool inverse) const;
    int size() const { return n_; }
private:
    int n_;
    std::vector<float> cos_, sin_;
    std::vector<int>   rev_;
};

class Nebula {
public:
    static constexpr int kN = 2048, kHop = 512;
    void prepare(double sampleRate, uint64_t seed);
    void set(float smear);              // 0 = follows the input, 1 = frozen
    // Wet output only (latency kN samples).
    void process(const float* inL, const float* inR, float* wetL, float* wetR, int n);
private:
    struct Channel {
        std::vector<float> in, out, mag, re, im;
        int inPos = 0, outPos = 0;
        Rng rng;
    };
    void frame(Channel& c);
    Channel ch_[2];
    Fft     fft_{ kN };
    std::vector<float> window_;
    int     hopCounter_ = 0;
    float   alpha_ = 1.0f;
};

} // namespace ambient
