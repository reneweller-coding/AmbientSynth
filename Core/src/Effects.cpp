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

// ---------------------------------------------------------------- StereoDelay

void StereoDelay::prepare(double sampleRate)
{
    sr_ = sampleRate;
    const int size = pow2At(static_cast<int>(4.05 * sr_) + 64);
    bufL_.assign(static_cast<size_t>(size), 0.0f);
    bufR_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;
    w_ = 0;
    lpL_ = lpR_ = 0.0f;
    tLcur_ = tRcur_ = 0.0f;
}

void StereoDelay::set(float timeL, float timeR, float feedback, float cross, float damping, float absorb)
{
    const float maxT = static_cast<float>(mask_ - 64);
    tL_ = std::min(clampv(timeL, 0.001f, 4.0f) * static_cast<float>(sr_), maxT);
    tR_ = std::min(clampv(timeR, 0.001f, 4.0f) * static_cast<float>(sr_), maxT);
    if (tLcur_ <= 0.0f) tLcur_ = tL_;
    if (tRcur_ <= 0.0f) tRcur_ = tR_;
    fb_ = clampv(feedback, 0.0f, 0.98f);
    cross_ = clampv(cross, 0.0f, 1.0f);
    lpc_ = 1.0f - 0.9f * clampv(damping, 0.0f, 1.0f);
    absorb_ = clampv(absorb, 0.0f, 1.0f);
    if (absorb_ > 0.0f) {
        // The band narrows with the feedback: at full feedback and full absorb the loop keeps
        // 320 Hz .. 1 kHz, and every repeat passes through it again.
        const float lc = 20.0f * std::pow(2.0f, 4.0f * absorb_ * fb_);
        const float hc = 16000.0f * std::pow(2.0f, -4.0f * absorb_ * fb_);
        hpc_  = 1.0f - std::exp(-kTwoPi * lc / static_cast<float>(sr_));
        lpc2_ = 1.0f - std::exp(-kTwoPi * hc / static_cast<float>(sr_));
    }
}

void StereoDelay::process(const float* inL, const float* inR, float* wetL, float* wetR, int n)
{
    float* bl = bufL_.data();
    float* br = bufR_.data();
    const float modDepth = 0.0003f * static_cast<float>(sr_);   // 0.3 ms of slow wander
    const float glide = 0.0003f;
    for (int i = 0; i < n; ++i) {
        tLcur_ += (tL_ - tLcur_) * glide;
        tRcur_ += (tR_ - tRcur_) * glide;
        modPh_[0] += 0.07 / sr_; if (modPh_[0] >= 1.0) modPh_[0] -= 1.0;
        modPh_[1] += 0.053 / sr_; if (modPh_[1] >= 1.0) modPh_[1] -= 1.0;
        const float dL = tLcur_ + modDepth * sin01(modPh_[0]) + 1.0f;
        const float dR = tRcur_ + modDepth * sin01(modPh_[1]) + 1.0f;
        const float oL = ringRead(bl, mask_, w_, dL);
        const float oR = ringRead(br, mask_, w_, dR);
        lpL_ += lpc_ * (oL - lpL_);
        lpR_ += lpc_ * (oR - lpR_);
        float fdL = lpL_, fdR = lpR_;
        if (absorb_ > 0.0f) {   // the absorption band: low cut, then the sinking high cut
            loL_ += hpc_ * (fdL - loL_); loR_ += hpc_ * (fdR - loR_);
            hiL_ += lpc2_ * ((fdL - loL_) - hiL_); hiR_ += lpc2_ * ((fdR - loR_) - hiR_);
            fdL = hiL_; fdR = hiR_;
        }
        const float fbL = fb_ * ((1.0f - cross_) * fdL + cross_ * fdR);
        const float fbR = fb_ * ((1.0f - cross_) * fdR + cross_ * fdL);
        bl[w_ & mask_] = inL[i] + fbL;
        br[w_ & mask_] = inR[i] + fbR;
        wetL[i] = oL;
        wetR[i] = oR;
        ++w_;
    }
}

// ---------------------------------------------------------------- Reverb

void Reverb::prepare(double sampleRate)
{
    sr_ = sampleRate;
    const int need = static_cast<int>(std::max(0.08 * 3.0 * 1.1 * sr_, 0.5 * sr_)) + 64;
    const int size = pow2At(need);
    mask_ = size - 1;
    w_ = 0;
    for (auto& l : line_) l.assign(static_cast<size_t>(size), 0.0f);
    for (auto& a : ap_)   a.assign(static_cast<size_t>(size), 0.0f);
    pre_.assign(static_cast<size_t>(size), 0.0f);
    const int outSize = pow2At(static_cast<int>(0.012 * sr_) + 8);
    outR_.assign(static_cast<size_t>(outSize), 0.0f);
    outMask_ = outSize - 1;

    static const float kApMs[kAllpasses] = { 5.1f, 7.3f, 11.3f, 13.7f };
    for (int k = 0; k < kAllpasses; ++k) apLen_[k] = std::max(1, static_cast<int>(kApMs[k] * sr_ / 1000.0));
    static const float kModHz[kLines] = { 0.11f, 0.13f, 0.17f, 0.19f, 0.23f, 0.29f, 0.31f, 0.37f };
    for (int l = 0; l < kLines; ++l) { modRate_[l] = kModHz[l]; modPh_[l] = l / static_cast<double>(kLines); lp_[l] = 0.0f; lenCur_[l] = 0.0f; }
    preCur_ = 0.0f;
    outDelayCur_ = 0.0f;
    hcL_ = hcR_ = 0.0f;
    setSpace(asym_, 20000.0f);
    set(size_, decay_, damp_, 40.0f, false, mix_);
}

void Reverb::setSpace(float asymmetry, float highcutHz)
{
    asym_ = clampv(asymmetry, 0.0f, 1.0f);
    const float fc = clampv(highcutHz, 200.0f, 20000.0f);
    hcCoef_ = (fc >= 19000.0f) ? 1.0f : (1.0f - std::exp(-kTwoPi * fc / static_cast<float>(sr_)));
    outDelayTarget_ = asym_ * 0.010f * static_cast<float>(sr_);
    set(size_, decay_, damp_, preTarget_ * 1000.0f / static_cast<float>(sr_), freeze_, mix_);
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
        const float stretch = (l >= kLines / 2) ? (1.0f + 0.08f * asym_) : 1.0f;   // right-hand group runs longer
        lenTarget_[l] = std::min(kBaseMs[l] * size_ * stretch * static_cast<float>(sr_ / 1000.0), maxLen);
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
    float* outR = outR_.data();
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

        float wetL = 0.3f * (o[0] - o[1] + o[2] - o[3]);
        float wetR = 0.3f * (o[4] - o[5] + o[6] - o[7]);
        // Interaural disparity: the right output arrives a little later.
        outR[w_ & outMask_] = wetR;
        outDelayCur_ += (outDelayTarget_ - outDelayCur_) * glide;
        wetR = ringRead(outR, outMask_, w_, outDelayCur_ + 1.0f);
        // Tail darkening.
        hcL_ += hcCoef_ * (wetL - hcL_);
        hcR_ += hcCoef_ * (wetR - hcR_);
        L[i] = L[i] * (1.0f - mix) + hcL_ * mix;
        R[i] = R[i] * (1.0f - mix) + hcR_ * mix;
        ++w_;
    }
}

// ---------------------------------------------------------------- GrainCloud

void GrainCloud::prepare(double sampleRate, uint64_t seed)
{
    sr_ = sampleRate;
    rng_.seed(seed);
    const int size = pow2At(static_cast<int>(4.0 * sr_) + 64);
    buf_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;
    w_ = 0;
    nextGrain_ = 0.0;
    for (auto& g : grains_) g.active = false;
}

void GrainCloud::set(float densityPerSec, float sizeMs, float pitch, float spraySec, float level)
{
    density_ = clampv(densityPerSec, 0.1f, 200.0f);
    size_ = clampv(sizeMs, 5.0f, 2000.0f);
    pitch_ = clampv(pitch, 0.0f, 1.0f);
    spray_ = clampv(spraySec, 0.0f, 3.5f);
    level_ = clampv(level, 0.0f, 2.0f);
}

void GrainCloud::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    float* b = buf_.data();
    static const double kRates[4] = { 2.0, 0.5, 1.5, 4.0 };
    const float gain = level_ * 0.6f / std::sqrt(std::max(density_ * size_ * 0.001f, 1.0f));   // roughly constant loudness
    for (int i = 0; i < n; ++i) {
        b[w_ & mask_] = 0.5f * (inL[i] + inR[i]);
        nextGrain_ -= 1.0;
        if (nextGrain_ <= 0.0) {
            nextGrain_ = -std::log(1.0 - static_cast<double>(rng_.uniform()) + 1e-9) * sr_ / density_;
            for (auto& g : grains_) {
                if (g.active) continue;
                const float len = size_ * (0.7f + 0.6f * rng_.uniform()) * static_cast<float>(sr_ / 1000.0);
                double rate = 1.0 + 0.02 * rng_.bipolar();
                if (rng_.uniform() < pitch_) rate = kRates[rng_.below(rng_.uniform() < 0.7f ? 2 : 4)];
                const double behind = len * std::max(rate, 1.0) + spray_ * rng_.uniform() * sr_ + 2.0;
                if (behind >= static_cast<double>(mask_) - 8.0) continue;
                const float pan = rng_.bipolar();
                const float angle = (pan + 1.0f) * 0.25f * kPi;
                g.active = true; g.pos = static_cast<double>(w_) - behind; g.rate = rate; g.len = len; g.phase = 0.0f;
                g.gainL = std::cos(angle) * gain; g.gainR = std::sin(angle) * gain;
                g.wc = 1.0f; g.ws = 0.0f;
                phasorFrom(1.0 / static_cast<double>(std::max(len, 1.0f)), g.rc, g.rs);
                break;
            }
        }
        float sl = 0.0f, sr = 0.0f;
        for (auto& g : grains_) {
            if (!g.active) continue;
            const double delay = static_cast<double>(w_) - g.pos;
            if (delay < 1.0 || g.phase >= g.len) { g.active = false; continue; }
            const float win = 0.5f - 0.5f * g.wc;
            const float s = ringRead(b, mask_, static_cast<int>(w_ & mask_), static_cast<float>(delay)) * win;
            sl += s * g.gainL; sr += s * g.gainR;
            const float r2 = g.wc * g.wc + g.ws * g.ws, fix = 1.5f - 0.5f * r2;   // keep it on the unit circle
            const float nc = (g.wc * g.rc - g.ws * g.rs) * fix;
            g.ws = (g.ws * g.rc + g.wc * g.rs) * fix;
            g.wc = nc;
            g.pos += g.rate; g.phase += 1.0f;
        }
        outL[i] += sl; outR[i] += sr;
        ++w_;
    }
}

// ---------------------------------------------------------------- MidSide

void MidSide::prepare(double sampleRate)
{
    sr_ = sampleRate;
    hp_.reset();
    air_.reset();
    set(150.0f, 2.0f, 1.2f);
}

void MidSide::set(float bassMonoHz, float sideAirDb, float width)
{
    hp_.setQ(clampv(bassMonoHz, 20.0f, 400.0f), 0.707f, static_cast<float>(sr_));
    air_.setQ(3000.0f, 0.6f, static_cast<float>(sr_));           // broad, gentle upper-mid bell
    airGain_ = std::pow(10.0f, clampv(sideAirDb, 0.0f, 12.0f) / 20.0f) - 1.0f;
    width_ = clampv(width, 0.0f, 2.0f);
}

void MidSide::process(float* L, float* R, int n)
{
    for (int i = 0; i < n; ++i) {
        const float m = 0.5f * (L[i] + R[i]);
        float s = 0.5f * (L[i] - R[i]);
        float lp, bp, hp;
        hp_.tick(s, lp, bp, hp);          // everything below the crossover collapses to the centre
        s = hp;
        air_.tick(s, lp, bp, hp);
        s += airGain_ * bp;               // lift the side's upper mids
        s *= width_;
        L[i] = m + s;
        R[i] = m - s;
    }
}

} // namespace ambient
