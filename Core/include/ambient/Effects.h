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
    // Chorus (0) is the three modulated taps this has always been. Microshift (1) is the other
    // way a mix engineer widens a drone: the two channels are detuned by a few cents in opposite
    // directions and delayed by different amounts, with nothing modulated. It survives a mono
    // sum -- two channels a few cents apart are never at a fixed phase, so there is no comb to
    // cancel into -- where a deep chorus at 13 to 22 ms is exactly a comb filter waiting to be
    // summed. Depth becomes the detune in cents, Rate a very slow wander of it.
    void setMode(int mode) { mode_ = mode; }
    void process(float* L, float* R, int n);
private:
    void processChorus(float* L, float* R, int n);
    void processShift(float* L, float* R, int n);
    std::vector<float> bufL_, bufR_;
    int    mask_ = 0, w_ = 0;
    double sr_ = 48000.0;
    double ph_[3] = { 0.0, 0.33, 0.66 };
    float  mix_ = 0.4f, depth_ = 0.4f, rate_ = 0.2f;
    int    mode_ = 0;
    // Microshift state: the two shifters' window phases and the slow wander of the detune.
    double shPh_[2] = { 0.0, 0.5 };
    double wanderPh_ = 0.0;
};

class StereoDelay {
public:
    void prepare(double sampleRate);
    // absorb: with it up, the loop also loses its low end and its high cut sinks as the feedback
    // rises, so long echoes drown into a fog instead of merely getting quieter.
    void set(float timeL, float timeR, float feedback, float cross, float damping, float absorb = 0.0f);
    // Duck: how far the loop's high cut is pulled down while the input is loud. The idea is the
    // one the far reverb's Unmask already uses -- get out of the way of the thing being played --
    // applied to the echoes: a fresh attack should not have to fight the brightness of the last
    // one's tail. At 0 the loop behaves exactly as it did.
    void setDuck(float amount) { duck_ = clampv(amount, 0.0f, 1.0f); }
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
    float  duck_ = 0.0f, duckEnv_ = 0.0f, duckFast_ = 0.0f;   // how much, and the two envelopes driving it
    double modPh_[2] = { 0.0, 0.5 };
};

class Reverb {
public:
    void prepare(double sampleRate);
    void set(float size, float decaySeconds, float damping, float preDelayMs, bool freeze, float mix);
    // asymmetry 0..1: right-hand lines lengthened and the right output delayed by up to 10 ms
    // highcutHz: one-pole low-pass on the wet output (20000 = off)
    // lowcutHz:  two one-pole high-passes on the wet output, 12 dB/oct (20 = off). The other half
    //            of the filter funnel a mixing engineer puts on a reverb return: without it the
    //            tail piles up in the low mids, where it turns the whole picture to mud rather
    //            than sitting behind it.
    void setSpace(float asymmetry, float highcutHz, float lowcutHz = 20.0f);
    // Classic (0) is the network as it always was. Scattering (1) puts a Schroeder all-pass
    // inside every delay line's loop, after Schlecht and Habets (2020): each pass round the
    // network then scatters every echo into many, so the echo density grows much faster
    // (measured: 0.60 -> 0.76 of Gaussian after 50 ms) while the late tail stays at least as
    // smooth. Same decay, same level, same lines; a denser texture of tail.
    void setMode(int mode) { mode_ = mode; }
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
    float  lcCoef_ = 0.0f;                                        // 0 = off
    float  lcL1_ = 0.0f, lcR1_ = 0.0f, lcL2_ = 0.0f, lcR2_ = 0.0f;
    bool   freeze_ = false;
    int    mode_ = 0;
    std::vector<float> sc_[kLines];   // the scattering all-passes, one per line
    int    scLen_[kLines] = {};
};

// The background steps aside for the foreground, band by band. A mixing engineer rides the
// reverb return down while a line is sounding and lets it back up in the gaps; done per band it
// is what keeps a dense pad from swallowing its own notes. Three bands (below 300 Hz, 300 Hz to
// 2.5 kHz, above), the near bus as the side chain, fast to duck and slow to return -- the return
// is the part the ear hears as the room breathing back in.
class Unmask {
public:
    void prepare(double sampleRate);
    // spread: how far a loud low band also ducks the bands above it. Masking in the ear is
    // asymmetric -- a low tone masks the frequencies above it far more than those below
    // (Zwicker and Fastl 1999, the upward spread of masking) -- and at 0 the three bands
    // are independent, as they always were.
    void set(float amount, float spread = 0.0f);   // 0 = off (and then not computed at all)
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
    float  amount_ = 0.0f, spread_ = 0.0f;
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
    // Tilt: a see-saw around the pivot -- one first-order low pass and its complement, weighted
    // against each other. Flat at 0 dB and then not computed at all.
    void setTilt(float dB, float pivotHz);
    void process(float* L, float* R, int n);
    // Mono safety. Everything upstream is built to widen -- all-pass phase width, asymmetric
    // delays, a reverb whose two sides are deliberately different -- and a drone that is
    // gigantic in stereo can vanish when it is summed to mono. The side channel is measured
    // against the mid over a long window and the width is eased back only when the side actually
    // dominates: a slow, small correction that a listener cannot hear and a mono sum can.
    void setMonoGuard(bool on) { guardOn_ = on; }
    float widthTrim() const { return guard_; }   // 1 = untouched, for the meter

private:
    Svf    hp_, air_;
    float  airGain_ = 0.0f, width_ = 1.0f;
    float  midPow_ = 0.0f, sidePow_ = 0.0f, guard_ = 1.0f;
    bool   guardOn_ = true;
    float  tiltLo_ = 1.0f, tiltHi_ = 1.0f, tiltC_ = 0.1f, tiltState_[2] = {};
    bool   tiltOn_ = false;
    double sr_ = 48000.0;
};

// A diffusion field: four modulated all-passes that turn an impulse into a swell before the
// reverb ever sees it. A feedback network answers immediately by construction; this is what
// gives a tail the slow arrival a large room has.
class Diffuser {
public:
    void prepare(double sampleRate);
    void set(float amount);
    void process(float* L, float* R, int n);
    void reset();
private:
    static constexpr int kStages = 4;
    std::vector<float> buf_[2][kStages];
    int    len_[kStages] = {};
    int    mask_ = 0, w_ = 0;
    double modPh_[kStages] = { 0.0, 0.3, 0.6, 0.85 };
    float  amount_ = 0.0f;
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

// The frequency-selective Haas effect. Delaying a whole channel by ten to thirty milliseconds
// widens it and destroys it in mono: the two channels comb, and the comb's first notch lands in
// the bass. Done to one band only -- roughly 1.2 to 4 kHz, where the ear takes its direction from
// level rather than from time -- and cross-fed, so each side hears the other's delayed band six
// decibels down, the width appears at the edges and the low end and the top stay exactly where
// they were. Off at amount 0, and then not computed.
class HaasBand {
public:
    void prepare(double sampleRate)
    {
        sr_ = sampleRate;
        int size = 1; while (size < static_cast<int>(0.05 * sr_) + 8) size <<= 1;
        bufL_.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1; w_ = 0;
        smDelay_ = timeMs_ * static_cast<float>(sr_ / 1000.0) + 1.0f;
        // Two poles at the ends of the band: a high-pass at 1.2 kHz and a low-pass at 4 kHz, which
        // between them is the band the paper isolates.
        hp_[0].setQ(1200.0f, 0.7f, static_cast<float>(sr_));
        lp_[0].setQ(4000.0f, 0.7f, static_cast<float>(sr_));
    }
    void set(float amount, float timeMs) { amount_ = clampv(amount, 0.0f, 1.0f); timeMs_ = clampv(timeMs, 1.0f, 45.0f); }
    void process(float* L, float* R, int n)
    {
        if (amount_ <= 0.0f && smAmount_ <= 1.0e-5f) { w_ = (w_ + n) & 0x3FFFFFFF; return; }
        // The delay follows the knob at the same pace as the amount: read straight from the knob
        // it would jump the read pointer, which is a click, not a change of width.
        const float delayTarget = timeMs_ * static_cast<float>(sr_ / 1000.0) + 1.0f;
        const float c = 1.0f - std::exp(-1.0f / (0.05f * static_cast<float>(sr_)));   // 50 ms
        float* bl = bufL_.data();
        for (int i = 0; i < n; ++i) {
            smAmount_ += c * (amount_ - smAmount_);
            smDelay_ += c * (delayTarget - smDelay_);
            const float delay = smDelay_;
            // The band of what is in the middle -- the part of the picture that has no width yet.
            float lp, bp, hp;
            const float mid = 0.5f * (L[i] + R[i]);
            hp_[0].tick(mid, lp, bp, hp); float band = hp;
            lp_[0].tick(band, lp, bp, hp); band = lp;
            bl[w_ & mask_] = band;
            // Into the side channel: added on the left, taken off on the right. That is what "hard
            // to the opposite side" comes to once it is made symmetrical, and it is the only form
            // of it that is exactly mono-safe -- summed to mono the two cancel to the sample, so
            // the centre of the mix is the same picture it was before the widening.
            const float d = smAmount_ * 0.5f * ringRead(bl, mask_, w_, delay);   // -6 dB at full amount
            L[i] += d;
            R[i] -= d;
            ++w_;
        }
    }
private:
    std::vector<float> bufL_;                       // one band, one line: it goes to the sides
    int    mask_ = 0, w_ = 0;
    double sr_ = 48000.0;
    Svf    hp_[1], lp_[1];
    float  amount_ = 0.0f, smAmount_ = 0.0f, timeMs_ = 15.0f, smDelay_ = 0.0f;
};

} // namespace ambient
