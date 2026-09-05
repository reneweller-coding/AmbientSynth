// AmbientSynth -- stereo effects.
//   Ensemble    modulated three-tap chorus
//   StereoDelay asymmetric L/R delay with cross-feed and damping (time-based width)
//   Reverb      8-line feedback delay network with diffusion, damping, freeze,
//               L/R asymmetry and a tail high-cut (the dark "infinite background")
//   MidSide     mono bass below a crossover, gentle side upper-mid lift, width
// Buffers are allocated in prepare() only.
#pragma once
#include "Dsp.h"
#include <vector>

namespace ambient {

class Ensemble {
public:
    void prepare(double sampleRate);
    void set(float mix, float depth, float rateHz) { mix_ = mix; depth_ = depth; rate_ = rateHz; }
    void process(float* L, float* R, int n);
private:
    std::vector<float> bufL_, bufR_;
    int    mask_ = 0, w_ = 0;
    double sr_ = 48000.0;
    double ph_[3] = { 0.0, 0.33, 0.66 };
    float  mix_ = 0.4f, depth_ = 0.4f, rate_ = 0.2f;
};

class StereoDelay {
public:
    void prepare(double sampleRate);
    // absorb: with it up, the loop also loses its low end and its high cut sinks as the feedback
    // rises, so long echoes drown into a fog instead of merely getting quieter.
    void set(float timeL, float timeR, float feedback, float cross, float damping, float absorb = 0.0f);
    // Writes the wet signal only; the caller mixes it.
    void process(const float* inL, const float* inR, float* wetL, float* wetR, int n);
private:
    std::vector<float> bufL_, bufR_;
    int    mask_ = 0, w_ = 0;
    double sr_ = 48000.0;
    float  tL_ = 0, tR_ = 0, tLcur_ = 0, tRcur_ = 0;
    float  fb_ = 0.5f, cross_ = 0.3f, lpc_ = 0.5f;
    float  lpL_ = 0, lpR_ = 0;
    float  absorb_ = 0.0f, hpc_ = 0.0f, lpc2_ = 1.0f;   // the absorption band, from feedback and absorb
    float  loL_ = 0, loR_ = 0, hiL_ = 0, hiR_ = 0;
    double modPh_[2] = { 0.0, 0.5 };
};

class Reverb {
public:
    void prepare(double sampleRate);
    void set(float size, float decaySeconds, float damping, float preDelayMs, bool freeze, float mix);
    // asymmetry 0..1: right-hand lines lengthened and the right output delayed by up to 10 ms
    // highcutHz: one-pole low-pass on the wet output (20000 = off)
    void setSpace(float asymmetry, float highcutHz);
    void process(float* L, float* R, int n);
private:
    static constexpr int kLines = 8;
    static constexpr int kAllpasses = 4;
    std::vector<float> line_[kLines];
    std::vector<float> ap_[kAllpasses];
    std::vector<float> pre_, outR_;
    int    mask_ = 0, w_ = 0, outMask_ = 0;
    double sr_ = 48000.0;
    float  lenTarget_[kLines] = {}, lenCur_[kLines] = {};
    float  gain_[kLines] = {};
    float  lp_[kLines] = {};
    double modPh_[kLines] = {};
    float  modRate_[kLines] = {};
    int    apLen_[kAllpasses] = {};
    float  preTarget_ = 0.0f, preCur_ = 0.0f;
    float  outDelayTarget_ = 0.0f, outDelayCur_ = 0.0f;
    float  damp_ = 0.4f, mix_ = 0.45f, decay_ = 12.0f, size_ = 1.6f, asym_ = 0.0f;
    float  hcCoef_ = 1.0f, hcL_ = 0.0f, hcR_ = 0.0f;
    bool   freeze_ = false;
};

// The background steps aside for the foreground, band by band. A mixing engineer rides the
// reverb return down while a line is sounding and lets it back up in the gaps; done per band it
// is what keeps a dense pad from swallowing its own notes. Three bands (below 300 Hz, 300 Hz to
// 2.5 kHz, above), the near bus as the side chain, fast to duck and slow to return -- the return
// is the part the ear hears as the room breathing back in.
class Unmask {
public:
    void prepare(double sampleRate);
    void set(float amount);   // 0 = off (and then not computed at all)
    // Ducks far[] where near[] has energy, in place.
    void process(const float* nearL, const float* nearR, float* farL, float* farR, int n);
    void reset();
private:
    struct Split { float lo = 0.0f, mid = 0.0f; };   // one-pole state per crossover per channel
    Split  sNear_[2], sFar_[2];
    float  env_[3] = {};                              // the near bus's band envelopes
    float  gain_[3] = { 1.0f, 1.0f, 1.0f };           // what the far bus is multiplied by, smoothed
    float  aCoef_ = 0.01f, rCoef_ = 0.0005f;
    float  c1_ = 0.02f, c2_ = 0.2f;                   // crossover coefficients (300 Hz, 2.5 kHz)
    float  amount_ = 0.0f;
    double sr_ = 48000.0;
};

// The master's age: tape wow, the highs a worn machine loses, a noise floor that lives under the
// music, and a gentle saturation. Every one of them is a defect, and together they are most of
// what separates a recording from a render. Off at amount 0, and then bypassed entirely.
class Patina {
public:
    void prepare(double sampleRate, uint64_t seed);
    void set(float amount, float wow, float hiss, float age);
    void process(float* L, float* R, int n);
    void reset();
private:
    std::vector<float> bufL_, bufR_;
    int     mask_ = 0, w_ = 0;
    double  sr_ = 48000.0;
    float   amount_ = 0.0f, wow_ = 0.3f, hiss_ = 0.2f, age_ = 0.3f;
    float   lpC_ = 1.0f, lpL_ = 0.0f, lpR_ = 0.0f;
    float   env_ = 0.0f;
    Drifter wowDrift_;
    double  flutterPh_ = 0.0;
    Rng     rng_;
};

class MidSide {
public:
    void prepare(double sampleRate);
    void set(float bassMonoHz, float sideAirDb, float width);
    void process(float* L, float* R, int n);
private:
    Svf    hp_, air_;
    float  airGain_ = 0.0f, width_ = 1.0f;
    double sr_ = 48000.0;
};

// Granular cloud: grains of the recent past, randomly placed in time, optionally
// transposed by octaves/fifths, Hann-windowed, scattered across the stereo field.
// Meant for the far plane: the cloud is fed into the far reverb and smears there.
class GrainCloud {
public:
    void prepare(double sampleRate, uint64_t seed);
    void set(float densityPerSec, float sizeMs, float pitch, float spraySec, float level);
    // Feeds the history with (inL+inR)/2 and ADDS the cloud to outL/outR.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);
private:
    static constexpr int kMaxGrains = 32;
    // The Hann window runs as a rotating phasor (wc/ws advanced by rc/rs): a std::cos per sample
    // per grain, with up to 32 sounding, was the most expensive line in the cloud.
    struct Grain { bool active = false; double pos = 0.0; double rate = 1.0; float len = 1.0f, phase = 0.0f,
                   gainL = 0.0f, gainR = 0.0f, wc = 1.0f, ws = 0.0f, rc = 1.0f, rs = 0.0f; };
    std::vector<float> buf_;
    Grain  grains_[kMaxGrains];
    Rng    rng_;
    int    mask_ = 0;
    long long w_ = 0;
    double sr_ = 48000.0, nextGrain_ = 0.0;
    float  density_ = 12.0f, size_ = 250.0f, pitch_ = 0.3f, spray_ = 0.8f, level_ = 0.7f;
};

// Read `delay` samples (>= 1, fractional) behind write index `w` from a power-of-two ring.
inline float ringRead(const float* buf, int mask, int w, float delay)
{
    const int di = static_cast<int>(delay);
    const float f = delay - static_cast<float>(di);
    const float a = buf[(w - di) & mask];
    const float b = buf[(w - di - 1) & mask];
    return a + f * (b - a);
}

} // namespace ambient
