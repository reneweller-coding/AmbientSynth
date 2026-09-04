#include "ambient/Effects.h"
#include <cmath>
#include <algorithm>

namespace ambient {

namespace {
int pow2At(int n) { int p = 1; while (p < n) p <<= 1; return p; }
}

// ---------------------------------------------------------------- Ensemble

void Ensemble::prepare(double sampleRate)
{
    sr_ = sampleRate;
    const int size = pow2At(static_cast<int>(0.08 * sr_) + 8);
    bufL_.assign(static_cast<size_t>(size), 0.0f);
    bufR_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;
    w_ = 0;
}

void Ensemble::process(float* L, float* R, int n)
{
    static const float kBaseMs[3]  = { 13.0f, 17.0f, 22.0f };
    static const float kRateMul[3] = { 1.0f, 1.27f, 0.81f };
    const float msToSamples = static_cast<float>(sr_ / 1000.0);
    const float depthSamples = depth_ * 6.0f * msToSamples;
    const float mix = clampv(mix_, 0.0f, 1.0f);
    float* bl = bufL_.data();
    float* br = bufR_.data();
    for (int i = 0; i < n; ++i) {
        bl[w_ & mask_] = L[i];
        br[w_ & mask_] = R[i];
        float wetL = 0.0f, wetR = 0.0f;
        for (int k = 0; k < 3; ++k) {
            ph_[k] += rate_ * kRateMul[k] / sr_;
            if (ph_[k] >= 1.0) ph_[k] -= 1.0;
            double phR = ph_[k] + 0.25; if (phR >= 1.0) phR -= 1.0;
            const float dL = kBaseMs[k] * msToSamples + depthSamples * sin01(ph_[k]) + 1.0f;
            const float dR = kBaseMs[k] * msToSamples + depthSamples * sin01(phR) + 1.0f;
            wetL += ringRead(bl, mask_, w_, dL);
            wetR += ringRead(br, mask_, w_, dR);
        }
        wetL *= (1.0f / 3.0f);
        wetR *= (1.0f / 3.0f);
        L[i] = L[i] * (1.0f - mix) + wetL * mix;
        R[i] = R[i] * (1.0f - mix) + wetR * mix;
        ++w_;
    }
}

// ---------------------------------------------------------------- Reverb

void Reverb::prepare(double sampleRate)
{
    sr_ = sampleRate;
    const int need = static_cast<int>(std::max(0.08 * 3.0 * sr_, 0.5 * sr_)) + 64;
    const int size = pow2At(need);
    mask_ = size - 1;
    w_ = 0;
    for (auto& l : line_) l.assign(static_cast<size_t>(size), 0.0f);
    for (auto& a : ap_)   a.assign(static_cast<size_t>(size), 0.0f);
    pre_.assign(static_cast<size_t>(size), 0.0f);

    static const float kApMs[kAllpasses] = { 5.1f, 7.3f, 11.3f, 13.7f };
    for (int k = 0; k < kAllpasses; ++k) apLen_[k] = std::max(1, static_cast<int>(kApMs[k] * sr_ / 1000.0));
    static const float kModHz[kLines] = { 0.11f, 0.13f, 0.17f, 0.19f, 0.23f, 0.29f, 0.31f, 0.37f };
    for (int l = 0; l < kLines; ++l) { modRate_[l] = kModHz[l]; modPh_[l] = l / static_cast<double>(kLines); lp_[l] = 0.0f; lenCur_[l] = 0.0f; }
    preCur_ = 0.0f;
    set(size_, decay_, damp_, 40.0f, false, mix_);
}

void Reverb::set(float size, float decaySeconds, float damping, float preDelayMs, bool freeze, float mix)
{
    static const float kBaseMs[kLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 61.9f, 71.3f, 79.9f };
    size_ = clampv(size, 0.5f, 3.0f);
    decay_ = std::max(decaySeconds, 0.1f);
    damp_ = clampv(damping, 0.0f, 1.0f);
    mix_ = clampv(mix, 0.0f, 1.0f);
    freeze_ = freeze;
    const float maxLen = static_cast<float>(mask_) - 8.0f;
    for (int l = 0; l < kLines; ++l) {
        lenTarget_[l] = std::min(kBaseMs[l] * size_ * static_cast<float>(sr_ / 1000.0), maxLen);
        if (lenCur_[l] <= 0.0f) lenCur_[l] = lenTarget_[l];
        gain_[l] = freeze_ ? 1.0f : std::pow(10.0f, -3.0f * lenTarget_[l] / (decay_ * static_cast<float>(sr_)));
    }
    preTarget_ = std::min(clampv(preDelayMs, 0.0f, 500.0f) * static_cast<float>(sr_ / 1000.0), maxLen);
    if (preCur_ <= 0.0f) preCur_ = preTarget_;
}

void Reverb::process(float* L, float* R, int n)
{
    const float lpc = freeze_ ? 1.0f : (1.0f - 0.92f * damp_);
    const float inGain = freeze_ ? 0.0f : 0.5f;
    const float mix = mix_;
    const float glide = 0.0005f;
    for (int i = 0; i < n; ++i) {
        const float in = 0.5f * (L[i] + R[i]);
        pre_[static_cast<size_t>(w_ & mask_)] = in;
        preCur_ += (preTarget_ - preCur_) * glide;
        float x = ringRead(pre_.data(), mask_, w_, preCur_ + 1.0f);

        for (int k = 0; k < kAllpasses; ++k) {
            float* a = ap_[k].data();
            const float d = a[(w_ - apLen_[k]) & mask_];
            const float y = d - 0.6f * x;
            a[w_ & mask_] = x + 0.6f * y;
            x = y;
        }

        float o[kLines];
        float sum = 0.0f;
        for (int l = 0; l < kLines; ++l) {
            lenCur_[l] += (lenTarget_[l] - lenCur_[l]) * glide;
            modPh_[l] += modRate_[l] / sr_;
            if (modPh_[l] >= 1.0) modPh_[l] -= 1.0;
            const float d = lenCur_[l] + 1.5f * sin01(modPh_[l]) + 2.0f;
            const float v = ringRead(line_[l].data(), mask_, w_, d);
            lp_[l] += lpc * (v - lp_[l]);
            o[l] = lp_[l];
            sum += o[l];
        }
        const float hh = sum * (2.0f / static_cast<float>(kLines));   // Householder reflection
        for (int l = 0; l < kLines; ++l)
            line_[l][static_cast<size_t>(w_ & mask_)] = gain_[l] * (o[l] - hh) + ((l & 1) ? -inGain : inGain) * x;

        const float wetL = 0.3f * (o[0] - o[1] + o[2] - o[3]);
        const float wetR = 0.3f * (o[4] - o[5] + o[6] - o[7]);
        L[i] = L[i] * (1.0f - mix) + wetL * mix;
        R[i] = R[i] * (1.0f - mix) + wetR * mix;
        ++w_;
    }
}

} // namespace ambient
