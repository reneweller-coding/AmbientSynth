#include <chrono>
#include <thread>
#include "ambient/Convolution.h"
#include "ambient/Dsp.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ambient {

namespace {
constexpr int kN = 2 * Convolver::kBlock;      // FFT size
constexpr int kBins = Convolver::kBlock + 1;   // 0 .. N/2 (real input: the rest is the conjugate mirror)
}

void Convolver::prepare(double sampleRate, float maxSeconds)
{
    sr_ = sampleRate;
    maxParts_ = std::max(1, static_cast<int>(std::ceil(maxSeconds * sr_ / kBlock)));
    for (int c = 0; c < 2; ++c) {
        fdlRe_[c].assign(static_cast<size_t>(maxParts_) * kBins, 0.0f);
        fdlIm_[c].assign(static_cast<size_t>(maxParts_) * kBins, 0.0f);
        inBuf_[c].assign(kBlock, 0.0f);
        tail_[c].assign(kBlock, 0.0f);
        outBuf_[c].assign(kBlock, 0.0f);
    }
    workRe_.assign(kN, 0.0f); workIm_.assign(kN, 0.0f);
    accRe_.assign(kN, 0.0f);  accIm_.assign(kN, 0.0f);
    for (auto& s : sets_) { s.parts = 0; s.seconds = 0.0; for (int c = 0; c < 2; ++c) { s.re[c].clear(); s.im[c].clear(); } }
    active_.store(-1, std::memory_order_release);
    inUse_.store(-1, std::memory_order_release);
    reset();
}

void Convolver::reset()
{
    for (int c = 0; c < 2; ++c) {
        std::fill(fdlRe_[c].begin(), fdlRe_[c].end(), 0.0f);
        std::fill(fdlIm_[c].begin(), fdlIm_[c].end(), 0.0f);
        std::fill(inBuf_[c].begin(), inBuf_[c].end(), 0.0f);
        std::fill(tail_[c].begin(), tail_[c].end(), 0.0f);
        std::fill(outBuf_[c].begin(), outBuf_[c].end(), 0.0f);
    }
    fdlHead_ = 0; fdlFilled_ = 0; inFill_ = 0; outRead_ = 0;
}

float Convolver::impulseSeconds() const
{
    const int a = active_.load(std::memory_order_acquire);
    return a < 0 ? 0.0f : static_cast<float>(sets_[a].seconds);
}

void Convolver::analyse(Set& s, const std::vector<float>& L, const std::vector<float>& R, bool stereo)
{
    const int n = static_cast<int>(L.size());
    s.parts = std::min(maxParts_, (n + kBlock - 1) / kBlock);
    s.seconds = static_cast<double>(s.parts * kBlock) / sr_;
    s.stereo = stereo;
    std::vector<float> re(kN), im(kN);
    for (int c = 0; c < (stereo ? 2 : 1); ++c) {
        const std::vector<float>& src = (c == 0) ? L : R;
        s.re[c].assign(static_cast<size_t>(s.parts) * kBins, 0.0f);
        s.im[c].assign(static_cast<size_t>(s.parts) * kBins, 0.0f);
        for (int p = 0; p < s.parts; ++p) {
            std::fill(re.begin(), re.end(), 0.0f); std::fill(im.begin(), im.end(), 0.0f);
            for (int i = 0; i < kBlock; ++i) { const int k = p * kBlock + i; if (k < n) re[static_cast<size_t>(i)] = src[static_cast<size_t>(k)]; }
            fft_.transform(re.data(), im.data(), false);
            std::memcpy(s.re[c].data() + static_cast<size_t>(p) * kBins, re.data(), sizeof(float) * kBins);
            std::memcpy(s.im[c].data() + static_cast<size_t>(p) * kBins, im.data(), sizeof(float) * kBins);
        }
    }
}

void Convolver::setImpulse(const float* L, const float* R, int n, double impulseSampleRate)
{
    if (L == nullptr || n <= 0) return;
    // Resample to the engine rate (linear), cut to the maximum, normalise energy to 1 so a
    // room never changes the level of what it reverberates.
    const double ratio = impulseSampleRate > 0.0 ? sr_ / impulseSampleRate : 1.0;
    const int out = std::min(static_cast<int>(n * ratio), maxParts_ * kBlock);
    std::vector<float> l(static_cast<size_t>(out)), r(static_cast<size_t>(out));
    for (int i = 0; i < out; ++i) {
        const double pos = i / ratio;
        const int i0 = std::min(static_cast<int>(pos), n - 1), i1 = std::min(i0 + 1, n - 1);
        const float f = static_cast<float>(pos - i0);
        l[static_cast<size_t>(i)] = L[i0] + f * (L[i1] - L[i0]);
        r[static_cast<size_t>(i)] = R ? (R[i0] + f * (R[i1] - R[i0])) : l[static_cast<size_t>(i)];
    }
    double e = 0.0;
    for (int i = 0; i < out; ++i) e += 0.5 * (l[static_cast<size_t>(i)] * l[static_cast<size_t>(i)] + r[static_cast<size_t>(i)] * r[static_cast<size_t>(i)]);
    const float g = e > 1e-12 ? static_cast<float>(1.0 / std::sqrt(e)) : 1.0f;
    for (int i = 0; i < out; ++i) { l[static_cast<size_t>(i)] *= g; r[static_cast<size_t>(i)] *= g; }

    const int activeNow = active_.load(std::memory_order_acquire);
    const int target = activeNow < 0 ? 0 : 1 - activeNow;
    // Sleeps, and up to 200 ms: see Engine::setTexture.
    for (int spin = 0; spin < 4000 && inUse_.load(std::memory_order_acquire) == target; ++spin)
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    analyse(sets_[target], l, r, R != nullptr);
    active_.store(target, std::memory_order_release);
}

void Convolver::generateDefault(uint64_t seed, float seconds)
{
    // Dark hall: three bands of decorrelated noise with their own decay (low 5 s, mid 3 s,
    // high 1.2 s at RT60), a sparse early-reflection cluster in the first 60 ms, stereo from
    // independent noise per channel. Measured against Rich's rooms in spirit, not copied.
    Rng rng; rng.seed(seed + 77);
    const int n = static_cast<int>(seconds * sr_);
    std::vector<float> ch[2];
    const float rt[3] = { 5.0f, 3.0f, 1.2f };
    for (int c = 0; c < 2; ++c) {
        ch[c].assign(static_cast<size_t>(n), 0.0f);
        float lp1 = 0.0f, lp2 = 0.0f;
        const float a1 = 1.0f - std::exp(-kTwoPi * 300.0f / static_cast<float>(sr_));
        const float a2 = 1.0f - std::exp(-kTwoPi * 3000.0f / static_cast<float>(sr_));
        for (int i = 0; i < n; ++i) {
            const float w = rng.bipolar();
            lp1 += a1 * (w - lp1);           // low band
            lp2 += a2 * (w - lp2);           // low + mid
            const float low = lp1, mid = lp2 - lp1, high = w - lp2;
            const float t = static_cast<float>(i) / static_cast<float>(sr_);
            const float env0 = std::pow(10.0f, -3.0f * t / rt[0]), env1 = std::pow(10.0f, -3.0f * t / rt[1]), env2 = std::pow(10.0f, -3.0f * t / rt[2]);
            const float onset = std::min(1.0f, t / 0.02f);   // the diffuse tail builds over 20 ms
            ch[c][static_cast<size_t>(i)] = onset * (low * env0 * 1.2f + mid * env1 + high * env2 * 0.7f);
        }
        // early reflections: 8 taps in 8..60 ms, alternating sign, fading
        for (int k = 0; k < 8; ++k) {
            const int at = static_cast<int>((0.008f + 0.052f * rng.uniform()) * static_cast<float>(sr_));
            if (at < n) ch[c][static_cast<size_t>(at)] += (k % 2 ? -1.0f : 1.0f) * (0.6f - 0.06f * k);
        }
    }
    setImpulse(ch[0].data(), ch[1].data(), n, sr_);
}

void Convolver::processBlock()
{
    const int a = active_.load(std::memory_order_acquire);
    inUse_.store(a, std::memory_order_release);
    if (a < 0) { for (int c = 0; c < 2; ++c) std::fill(outBuf_[c].begin(), outBuf_[c].end(), 0.0f); return; }
    const Set& s = sets_[a];
    // 1. spectra of the new input blocks into the delay line
    for (int c = 0; c < 2; ++c) {
        std::memcpy(workRe_.data(), inBuf_[c].data(), sizeof(float) * kBlock);
        std::memset(workRe_.data() + kBlock, 0, sizeof(float) * kBlock);
        std::fill(workIm_.begin(), workIm_.end(), 0.0f);
        fft_.transform(workRe_.data(), workIm_.data(), false);
        std::memcpy(fdlRe_[c].data() + static_cast<size_t>(fdlHead_) * kBins, workRe_.data(), sizeof(float) * kBins);
        std::memcpy(fdlIm_[c].data() + static_cast<size_t>(fdlHead_) * kBins, workIm_.data(), sizeof(float) * kBins);
    }
    fdlFilled_ = std::min(fdlFilled_ + 1, maxParts_);
    // 2. per channel: sum over partitions, inverse FFT, overlap-add
    const int parts = std::min(s.parts, fdlFilled_);
    for (int c = 0; c < 2; ++c) {
        const int ic = s.stereo ? c : 0;
        std::fill(accRe_.begin(), accRe_.begin() + kBins, 0.0f);
        std::fill(accIm_.begin(), accIm_.begin() + kBins, 0.0f);
        for (int p = 0; p < parts; ++p) {
            int slot = fdlHead_ - p; if (slot < 0) slot += maxParts_;
            const float* xr = fdlRe_[c].data() + static_cast<size_t>(slot) * kBins;
            const float* xi = fdlIm_[c].data() + static_cast<size_t>(slot) * kBins;
            const float* hr = s.re[ic].data() + static_cast<size_t>(p) * kBins;
            const float* hi = s.im[ic].data() + static_cast<size_t>(p) * kBins;
            float* ar = accRe_.data(); float* ai = accIm_.data();
            for (int k = 0; k < kBins; ++k) {
                ar[k] += xr[k] * hr[k] - xi[k] * hi[k];
                ai[k] += xr[k] * hi[k] + xi[k] * hr[k];
            }
        }
        for (int k = 1; k < kBlock; ++k) { accRe_[static_cast<size_t>(kN - k)] = accRe_[static_cast<size_t>(k)]; accIm_[static_cast<size_t>(kN - k)] = -accIm_[static_cast<size_t>(k)]; }
        fft_.transform(accRe_.data(), accIm_.data(), true);
        for (int i = 0; i < kBlock; ++i) {
            outBuf_[c][static_cast<size_t>(i)] = accRe_[static_cast<size_t>(i)] + tail_[c][static_cast<size_t>(i)];
            tail_[c][static_cast<size_t>(i)] = accRe_[static_cast<size_t>(kBlock + i)];
        }
    }
    fdlHead_ = (fdlHead_ + 1) % maxParts_;
    // See Engine::process: the flag says "in use", so it is given back here.
    inUse_.store(-1, std::memory_order_release);
}

void Convolver::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    for (int i = 0; i < n; ++i) {
        inBuf_[0][static_cast<size_t>(inFill_)] = inL[i];
        inBuf_[1][static_cast<size_t>(inFill_)] = inR[i];
        outL[i] = outBuf_[0][static_cast<size_t>(inFill_)];   // one block of latency
        outR[i] = outBuf_[1][static_cast<size_t>(inFill_)];
        if (++inFill_ >= kBlock) { processBlock(); inFill_ = 0; }
    }
}

} // namespace ambient
