#include "ambient/Loudness.h"
#include <algorithm>
#include <cmath>

namespace ambient {

namespace {
constexpr double kPi = 3.14159265358979323846;

}

void KFilter::prepare(double sr)
{
    // Not the RBJ cookbook. The cookbook's shelf, given the standard's own f0, Q and gain, comes
    // out about two per cent away from the coefficients BS.1770 prints for 48 kHz -- close enough
    // to look right and wrong enough to be wrong. This is the formulation that reproduces them:
    // checked against the published numbers to sixteen digits, and it holds at any sample rate
    // because it is the analogue prototype rather than a copy of one table.
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan(kPi * f0 / sr);
        const double Vh = std::pow(10.0, G / 20.0);
        const double Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf_.b0 = static_cast<float>((Vh + Vb * K / Q + K * K) / a0);
        shelf_.b1 = static_cast<float>(2.0 * (K * K - Vh) / a0);
        shelf_.b2 = static_cast<float>((Vh - Vb * K / Q + K * K) / a0);
        shelf_.a1 = static_cast<float>(2.0 * (K * K - 1.0) / a0);
        shelf_.a2 = static_cast<float>((1.0 - K / Q + K * K) / a0);
    }
    {   // Stage two: the RLB high-pass. Its numerator is exactly 1, -2, 1 -- the standard does not
        // normalise it the way the cookbook would, and that difference is 0.04 dB of gain.
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan(kPi * f0 / sr);
        const double d = 1.0 + K / Q + K * K;
        hp_.b0 = 1.0f; hp_.b1 = -2.0f; hp_.b2 = 1.0f;
        hp_.a1 = static_cast<float>(2.0 * (K * K - 1.0) / d);
        hp_.a2 = static_cast<float>((1.0 - K / Q + K * K) / d);
    }
    reset();
}

void KFilter::reset() { shelf_.reset(); hp_.reset(); }

float KFilter::process(float x) { return hp_.process(shelf_.process(x)); }

// ---------------------------------------------------------------- meter

void LoudnessMeter::prepare(double sampleRate)
{
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    hopLen_   = std::max(1, static_cast<int>(sr_ * 0.1));
    blockLen_ = hopLen_ * kHopsPerBlock;
    kL_.prepare(sr_);
    kR_.prepare(sr_);
    sumL_.assign(static_cast<size_t>(kHopsPerShort), 0.0);
    sumR_.assign(static_cast<size_t>(kHopsPerShort), 0.0);
    blocks_.reserve(4096);
    shortBlocks_.reserve(4096);
    reset();
}

void LoudnessMeter::reset()
{
    kL_.reset(); kR_.reset();
    std::fill(sumL_.begin(), sumL_.end(), 0.0);
    std::fill(sumR_.begin(), sumR_.end(), 0.0);
    ringPos_ = 0; ringFilled_ = 0; hopPos_ = 0;
    hopL_ = hopR_ = 0.0; hopSamples_ = 0;
    blocks_.clear(); shortBlocks_.clear();
    truePeak_ = 0.0; seconds_ = 0.0; lastShort_ = -120.0f;
    for (int i = 0; i < 4; ++i) { tpHistL_[i] = 0.0f; tpHistR_[i] = 0.0f; }
}

namespace {
// Loudness of a mean-square pair, the standard's formula with both channel weights at one.
inline float lufs(double msL, double msR)
{
    const double s = msL + msR;
    return s > 1.0e-12 ? static_cast<float>(-0.691 + 10.0 * std::log10(s)) : -120.0f;
}

// Four-phase interpolation for the inter-sample peak. Not the standard's 48-tap filter -- a
// four-point Lagrange, which is honest about being an estimate and catches the overshoot a
// resampler would produce. It reads within a couple of tenths of a decibel of the long filter on
// material like this, and it is the difference between "0.0 dBFS, fine" and "this will clip in an
// mp3 decoder" that matters here.
inline double interPeak(const float* h)
{
    double peak = std::fabs(static_cast<double>(h[1]));
    for (int k = 1; k < 4; ++k) {
        const double t = k * 0.25;
        const double a = h[0], b = h[1], c = h[2], d = h[3];
        const double v = b + 0.5 * t * (c - a + t * (2.0 * a - 5.0 * b + 4.0 * c - d + t * (3.0 * (b - c) + d - a)));
        peak = std::max(peak, std::fabs(v));
    }
    return peak;
}
}

void LoudnessMeter::pushBlock()
{
    // One hop finished: it joins the ring, and the last four hops make a 400 ms block.
    sumL_[static_cast<size_t>(ringPos_)] = hopSamples_ > 0 ? hopL_ / static_cast<double>(hopSamples_) : 0.0;
    sumR_[static_cast<size_t>(ringPos_)] = hopSamples_ > 0 ? hopR_ / static_cast<double>(hopSamples_) : 0.0;
    ringPos_ = (ringPos_ + 1) % kHopsPerShort;
    if (ringFilled_ < kHopsPerShort) ++ringFilled_;
    hopL_ = hopR_ = 0.0; hopSamples_ = 0;

    auto meanOver = [this](int hops, double& l, double& r) {
        const int n = std::min(hops, ringFilled_);
        l = r = 0.0;
        for (int k = 1; k <= n; ++k) {
            const int idx = (ringPos_ - k + kHopsPerShort) % kHopsPerShort;
            l += sumL_[static_cast<size_t>(idx)];
            r += sumR_[static_cast<size_t>(idx)];
        }
        if (n > 0) { l /= n; r /= n; }
        return n;
    };

    double bl = 0.0, br = 0.0;
    if (meanOver(kHopsPerBlock, bl, br) == kHopsPerBlock) blocks_.push_back(lufs(bl, br));
    double sl = 0.0, sr = 0.0;
    if (meanOver(kHopsPerShort, sl, sr) == kHopsPerShort) {
        lastShort_ = lufs(sl, sr);
        shortBlocks_.push_back(lastShort_);
    }
}

void LoudnessMeter::process(const float* L, const float* R, int n)
{
    for (int i = 0; i < n; ++i) {
        const float l = L[i], r = R[i];
        // True peak first, on the unweighted signal: it is a peak, not a loudness.
        tpHistL_[0] = tpHistL_[1]; tpHistL_[1] = tpHistL_[2]; tpHistL_[2] = tpHistL_[3]; tpHistL_[3] = l;
        tpHistR_[0] = tpHistR_[1]; tpHistR_[1] = tpHistR_[2]; tpHistR_[2] = tpHistR_[3]; tpHistR_[3] = r;
        truePeak_ = std::max(truePeak_, std::max(interPeak(tpHistL_), interPeak(tpHistR_)));

        const float kl = kL_.process(l), kr = kR_.process(r);
        hopL_ += static_cast<double>(kl) * kl;
        hopR_ += static_cast<double>(kr) * kr;
        ++hopSamples_;
        if (++hopPos_ >= hopLen_) { hopPos_ = 0; pushBlock(); }
    }
    seconds_ += static_cast<double>(n) / sr_;
}

LoudnessReading LoudnessMeter::read() const
{
    LoudnessReading out;
    out.seconds = static_cast<float>(seconds_);
    out.shortTerm = lastShort_;

    {   // Momentary: the last four hops, which is the last 400 ms.
        double l = 0.0, r = 0.0;
        const int n = std::min(kHopsPerBlock, ringFilled_);
        for (int k = 1; k <= n; ++k) {
            const int idx = (ringPos_ - k + kHopsPerShort) % kHopsPerShort;
            l += sumL_[static_cast<size_t>(idx)];
            r += sumR_[static_cast<size_t>(idx)];
        }
        if (n > 0) out.momentary = lufs(l / n, r / n);
    }

    // Integrated, gated twice: everything below -70 LUFS is not programme at all, and then
    // everything more than 10 LU under the mean of what is left is a gap between the loud parts.
    // Without the second gate a piece that is mostly silence measures as mostly silence.
    if (!blocks_.empty()) {
        double sum = 0.0; int count = 0;
        for (float b : blocks_) if (b > -70.0f) { sum += std::pow(10.0, (b + 0.691) / 10.0); ++count; }
        if (count > 0) {
            const float absMean = static_cast<float>(-0.691 + 10.0 * std::log10(sum / count));
            const float gate = absMean - 10.0f;
            double s2 = 0.0; int c2 = 0;
            for (float b : blocks_) if (b > -70.0f && b > gate) { s2 += std::pow(10.0, (b + 0.691) / 10.0); ++c2; }
            if (c2 > 0) out.integrated = static_cast<float>(-0.691 + 10.0 * std::log10(s2 / c2));
        }
    }

    // Loudness range: the spread of the short-term values between the 10th and 95th centile, over
    // those above the usual relative gate. It is the number that says whether a piece still has
    // its dynamics -- ambient wants it wide, and a limiter is what makes it narrow.
    if (shortBlocks_.size() >= 10) {
        std::vector<float> v;
        v.reserve(shortBlocks_.size());
        double sum = 0.0; int count = 0;
        for (float b : shortBlocks_) if (b > -70.0f) { sum += std::pow(10.0, (b + 0.691) / 10.0); ++count; }
        if (count > 0) {
            const float gate = static_cast<float>(-0.691 + 10.0 * std::log10(sum / count)) - 20.0f;
            for (float b : shortBlocks_) if (b > gate) v.push_back(b);
            if (v.size() >= 10) {
                std::sort(v.begin(), v.end());
                const size_t lo = static_cast<size_t>(0.10 * (v.size() - 1));
                const size_t hi = static_cast<size_t>(0.95 * (v.size() - 1));
                out.range = v[hi] - v[lo];
            }
        }
    }

    out.truePeak = truePeak_ > 1.0e-9 ? static_cast<float>(20.0 * std::log10(truePeak_)) : -120.0f;
    // Crest: how far the loudest instant stands above the loudness of the last three seconds.
    // Under a limiter this collapses; it is the single number the ambient literature asks for.
    if (out.shortTerm > -119.0f && out.truePeak > -119.0f) out.crest = out.truePeak - out.shortTerm;
    return out;
}

}   // namespace ambient
