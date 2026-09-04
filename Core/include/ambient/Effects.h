// AmbientSynth -- stereo effects: ensemble (modulated multi-tap chorus) and an
// 8-line feedback-delay-network reverb with diffusion, damping, freeze.
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

class Reverb {
public:
    void prepare(double sampleRate);
    void set(float size, float decaySeconds, float damping, float preDelayMs, bool freeze, float mix);
    void process(float* L, float* R, int n);
private:
    static constexpr int kLines = 8;
    static constexpr int kAllpasses = 4;
    std::vector<float> line_[kLines];
    std::vector<float> ap_[kAllpasses];
    std::vector<float> pre_;
    int    mask_ = 0, w_ = 0;
    double sr_ = 48000.0;
    float  lenTarget_[kLines] = {}, lenCur_[kLines] = {};
    float  gain_[kLines] = {};
    float  lp_[kLines] = {};
    double modPh_[kLines] = {};
    float  modRate_[kLines] = {};
    int    apLen_[kAllpasses] = {};
    float  preTarget_ = 0.0f, preCur_ = 0.0f;
    float  damp_ = 0.4f, mix_ = 0.45f, decay_ = 12.0f, size_ = 1.6f;
    bool   freeze_ = false;
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
