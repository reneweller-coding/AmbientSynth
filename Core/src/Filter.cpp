#include "ambient/Filter.h"
#include <complex>
#include <cstring>

namespace ambient {

const char* const kFilterModelNames[kNumFilterModels] = {
    "LP 6", "LP 12", "LP 24", "HP 12", "BP 12", "Notch", "Peak", "Ladder", "Comb",
};

void VoiceFilter::reset()
{
    svfL_.reset(); svfR_.reset(); svf2L_.reset(); svf2R_.reset();
    std::memset(lad_, 0, sizeof(lad_));
    onePole_[0] = onePole_[1] = 0.0f;
    std::memset(combBuf_, 0, sizeof(combBuf_));
    combW_ = 0;
    combDamp_[0] = combDamp_[1] = 0.0f;
}

void VoiceFilter::set(FilterModel model, float cutoffHz, float resonance, float drive)
{
    if (model != model_) {   // a model change starts from rest; the states mean different things
        model_ = model;
        reset();
    }
    const float fc = clampv(cutoffHz, 10.0f, sr_ * 0.45f);
    const float res = clampv(resonance, 0.0f, 1.0f);
    // Drive: a soft saturation ahead of the filter, level-compensated so the knob adds harmonics
    // rather than loudness. Off at zero (and then not even computed).
    const float d = clampv(drive, 0.0f, 1.0f);
    driveIn_  = d > 0.0f ? 1.0f + 6.0f * d : 1.0f;
    driveOut_ = d > 0.0f ? (1.0f + 0.5f * d) / driveIn_ : 1.0f;
    switch (model) {
    case FilterModel::Lp12:
        svfL_.set(fc, res, sr_); svfR_.copyCoefficients(svfL_);
        break;
    case FilterModel::Lp24:
        // two identical stages; the resonance is shared so the peak is not squared into a spike
        svfL_.set(fc, res * 0.75f, sr_); svfR_.copyCoefficients(svfL_);
        svf2L_.copyCoefficients(svfL_); svf2R_.copyCoefficients(svfL_);
        break;
    case FilterModel::Hp12:
    case FilterModel::Bp12:
    case FilterModel::Notch:
        svfL_.set(fc, res, sr_); svfR_.copyCoefficients(svfL_);
        break;
    case FilterModel::Peak:
        svfL_.set(fc, 0.5f + 0.5f * res, sr_); svfR_.copyCoefficients(svfL_);   // narrower bell with more resonance
        peakGain_ = 4.0f * res;                                                  // up to +14 dB
        break;
    case FilterModel::Lp6:
        g1_ = 1.0f - std::exp(-kTwoPi * fc / sr_);
        break;
    case FilterModel::Ladder: {
        // The four-pole cascade is 12 dB down at its one-pole corner, so the corner is placed
        // above the knob's value to bring the -3 dB point back near Cutoff.
        g1_ = 1.0f - std::exp(-kTwoPi * std::min(fc * 1.55f, sr_ * 0.45f) / sr_);
        kLad_ = 4.0f * res * res * 0.98f;   // squared: the interesting range is near the top
        ladComp_ = 1.0f + kLad_ * 0.5f;     // passband loss under feedback, partly restored
        break;
    }
    case FilterModel::Comb:
        combDelay_ = clampv(sr_ / fc, 2.0f, static_cast<float>(kCombMax - 4));
        combFb_ = 0.98f * res;
        break;
    default: break;
    }
}

float VoiceFilter::magnitude(FilterModel model, float cutoffHz, float resonance, float hz, float sr)
{
    using C = std::complex<float>;
    const float fc = clampv(cutoffHz, 10.0f, sr * 0.45f);
    const float res = clampv(resonance, 0.0f, 1.0f);
    const float w = kTwoPi * hz / sr;
    // The state-variable stages in the analogue prototype (s normalised to the warped cutoff).
    auto svf = [&](float r) {
        const float gc = std::tan(kPi * fc / sr);
        const float k = 2.0f - 1.9f * r;
        const C s(0.0f, std::tan(0.5f * w) / gc);
        const C den = s * s + k * s + 1.0f;
        struct R { C lp, bp, hp; float k; } out{ 1.0f / den, s / den, (s * s) / den, k };
        return out;
    };
    switch (model) {
    case FilterModel::Lp12: return std::abs(svf(res).lp);
    case FilterModel::Lp24: { const C h = svf(res * 0.75f).lp; return std::abs(h * h); }
    case FilterModel::Hp12: return std::abs(svf(res).hp);
    case FilterModel::Bp12: { auto r = svf(res); return std::abs(r.bp * r.k); }
    case FilterModel::Notch: { auto r = svf(res); return std::abs(r.lp + r.hp); }
    case FilterModel::Peak: { auto r = svf(0.5f + 0.5f * res); return std::abs(1.0f + 4.0f * res * r.k * r.bp); }
    case FilterModel::Lp6: {
        const float g = 1.0f - std::exp(-kTwoPi * fc / sr);
        const C z = std::polar(1.0f, -w);
        return std::abs(g / (1.0f - (1.0f - g) * z));
    }
    case FilterModel::Ladder: {
        const float g = 1.0f - std::exp(-kTwoPi * std::min(fc * 1.55f, sr * 0.45f) / sr);
        const float k = 4.0f * res * res * 0.98f;
        const C z = std::polar(1.0f, -w);
        const C G = g / (1.0f - (1.0f - g) * z);
        const C G4 = G * G * G * G;
        return std::abs(G4 / (1.0f + k * G4)) * (1.0f + k * 0.5f);
    }
    case FilterModel::Comb: {
        const float D = clampv(sr / fc, 2.0f, static_cast<float>(kCombMax - 4));
        const float fb = 0.98f * res;
        const C z = std::polar(1.0f, -w);
        const C zD = std::polar(1.0f, -w * D);
        const C damp = 0.35f / (1.0f - 0.65f * z);
        return std::abs((1.0f - fb) / (1.0f - fb * damp * zD));
    }
    default: return 1.0f;
    }
}

} // namespace ambient
