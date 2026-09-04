// AmbientSynth -- Z-plane filter (after the Rossum / E-mu Morpheus idea): four filter frames
// sit on the corners of a square; a point (X, Y) inside the square is a filter whose poles are
// interpolated between the corners. Move the point and the whole resonant structure glides.
// Here a frame is three resonators (centre frequency, bandwidth, gain) in parallel; the
// interpolation runs in the log-frequency domain on the pole parameters, never on the
// coefficients, so every point inside the square is a stable filter. Per voice, after (or
// instead of) the state-variable filter; the point wanders on its own with two Drifters.
#pragma once
#include "Dsp.h"

namespace ambient {

constexpr int kZShapes = 6;
constexpr int kZPeaks = 3;
extern const char* const kZShapeNames[kZShapes];
extern const char* const kZModeNames[3];   // Off, Series, Replace

struct ZPeak { float hz, bw, gain; };
struct ZFrame { ZPeak p[kZPeaks]; };
// Four corners per shape: index 0 = (0,0), 1 = (1,0), 2 = (0,1), 3 = (1,1).
extern const ZFrame kZShapes_[kZShapes][4];

// Bilinear interpolation of the corner frames (log frequency, log bandwidth, linear gain).
inline ZFrame zInterpolate(int shape, float x, float y)
{
    shape = clampv(shape, 0, kZShapes - 1);
    x = clampv(x, 0.0f, 1.0f); y = clampv(y, 0.0f, 1.0f);
    const ZFrame& c00 = kZShapes_[shape][0]; const ZFrame& c10 = kZShapes_[shape][1];
    const ZFrame& c01 = kZShapes_[shape][2]; const ZFrame& c11 = kZShapes_[shape][3];
    ZFrame out;
    for (int i = 0; i < kZPeaks; ++i) {
        auto lerp2 = [&](float a, float b, float c, float d) { return (a * (1 - x) + b * x) * (1 - y) + (c * (1 - x) + d * x) * y; };
        out.p[i].hz   = std::exp2(lerp2(std::log2(c00.p[i].hz), std::log2(c10.p[i].hz), std::log2(c01.p[i].hz), std::log2(c11.p[i].hz)));
        out.p[i].bw   = std::exp2(lerp2(std::log2(c00.p[i].bw), std::log2(c10.p[i].bw), std::log2(c01.p[i].bw), std::log2(c11.p[i].bw)));
        out.p[i].gain = lerp2(c00.p[i].gain, c10.p[i].gain, c01.p[i].gain, c11.p[i].gain);
    }
    return out;
}

// One two-pole resonator (constant-peak-gain form), the building block of a frame.
struct Resonator {
    float b0 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
    void set(float hz, float bw, float gain, float sr)
    {
        const float f = clampv(hz, 20.0f, sr * 0.45f);
        const float r = std::exp(-kPi * clampv(bw, 1.0f, sr * 0.2f) / sr);
        const float th = kTwoPi * f / sr;
        a1 = -2.0f * r * std::cos(th);
        a2 = r * r;
        // unity gain at the peak, times the frame gain
        b0 = gain * (1.0f - r) * std::sqrt(std::max(1.0f - 2.0f * r * std::cos(2.0f * th) + r * r, 1e-6f));
    }
    inline float tick(float in)
    {
        const float y = b0 * in - a1 * s1 - a2 * s2;
        s2 = s1; s1 = y;
        return y;
    }
    void reset() { s1 = s2 = 0.0f; }
};

} // namespace ambient
