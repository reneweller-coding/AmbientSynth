#include "ambient/ZPlane.h"
#include <algorithm>

namespace ambient {

const char* const kZModeNames[3] = { "Off", "Series", "Replace" };

#include "ZPlaneBank.inc"

ZFrame zFrameFromSpec(const ZCornerSpec& c)
{
    ZFrame f;
    for (int i = 0; i < kZSections; ++i) {
        if (c.hz[i] <= 0.0f) break;
        ZSection& s = f.s[i];
        s.poleHz = c.hz[i];
        s.poleBw = std::max(c.hz[i] * c.bwRatio, 0.5f);
        s.zeroHz = c.zeroRatio > 0.0f ? c.hz[i] * c.zeroRatio : 0.0f;
        s.zeroBw = s.zeroHz * c.zeroBwRatio;
        s.gain   = std::pow(c.tilt, static_cast<float>(i));
        f.used = i + 1;
    }
    return f;
}

ZFrame zInterpolate(int shape, float x, float y, float z)
{
    shape = clampv(shape, 0, kZShapes - 1);
    x = clampv(x, 0.0f, 1.0f); y = clampv(y, 0.0f, 1.0f); z = clampv(z, 0.0f, 1.0f);
    ZFrame c[8];
    for (int k = 0; k < 8; ++k) c[k] = zFrameFromSpec(kZCorners[shape][k]);
    ZFrame out;
    out.used = c[0].used;
    for (int k = 1; k < 8; ++k) out.used = std::min(out.used, c[k].used);
    // Trilinear. At z = 0 only the first four terms have any weight, so this is exactly the
    // bilinear interpolation it replaces -- which is what makes the third axis free to add.
    auto lerp3 = [&](const float* v) {
        const float f0 = (v[0] * (1 - x) + v[1] * x) * (1 - y) + (v[2] * (1 - x) + v[3] * x) * y;
        const float f1 = (v[4] * (1 - x) + v[5] * x) * (1 - y) + (v[6] * (1 - x) + v[7] * x) * y;
        return f0 * (1 - z) + f1 * z;
    };
    auto logLerp = [&](const float* v) {
        float l[8];
        for (int k = 0; k < 8; ++k) l[k] = std::log2(std::max(v[k], 1e-3f));
        return std::exp2(lerp3(l));
    };
    for (int i = 0; i < out.used; ++i) {
        ZSection& s = out.s[i];
        float pole[8], bw[8], zh[8], zb[8], gn[8];
        bool zeros = true;
        for (int k = 0; k < 8; ++k) {
            pole[k] = c[k].s[i].poleHz; bw[k] = c[k].s[i].poleBw;
            zh[k] = c[k].s[i].zeroHz;   zb[k] = c[k].s[i].zeroBw; gn[k] = c[k].s[i].gain;
            if (zh[k] <= 0.0f) zeros = false;
        }
        s.poleHz = logLerp(pole);
        s.poleBw = logLerp(bw);
        // A zero that exists in every corner is interpolated; if any corner has none, there is none.
        s.zeroHz = zeros ? logLerp(zh) : 0.0f;
        s.zeroBw = zeros ? logLerp(zb) : 0.0f;
        s.gain   = lerp3(gn);
    }
    return out;
}

namespace {
// The normalisation grid: 16 logarithmically spaced points in normalised frequency (about
// 24 Hz to 21 kHz at 48 kHz), with cos/sin of w and 2w precomputed once.
constexpr int kGrid = 16;
struct NormGrid {
    float c1[kGrid], s1[kGrid], c2[kGrid], s2[kGrid];
    NormGrid()
    {
        for (int k = 0; k < kGrid; ++k) {
            const float w = kPi * 0.001f * std::pow(900.0f, static_cast<float>(k) / (kGrid - 1));
            c1[k] = std::cos(w); s1[k] = std::sin(w);
            c2[k] = std::cos(2.0f * w); s2[k] = std::sin(2.0f * w);
        }
    }
};
const NormGrid& grid() { static const NormGrid g; return g; }
}

float zBuildCascade(const ZFrame& f, ZBiquad* ch, float sr)
{
    const NormGrid& g = grid();
    const int n = clampv(f.used, 0, kZSections);
    if (n == 0) return 1.0f;
    // The probe points: the fixed grid plus every pole and zero angle of this frame. A needle-
    // sharp resonance has its maximum at its own pole angle, which no fixed grid would catch.
    constexpr int kMaxPoints = kGrid + 2 * kZSections;
    float c1[kMaxPoints], s1[kMaxPoints], c2[kMaxPoints], s2[kMaxPoints];
    for (int k = 0; k < kGrid; ++k) { c1[k] = g.c1[k]; s1[k] = g.s1[k]; c2[k] = g.c2[k]; s2[k] = g.s2[k]; }
    int points = kGrid;
    auto addAngle = [&](float hz) {
        if (hz <= 0.0f || points >= kMaxPoints) return;
        const float w = kTwoPi * clampv(hz, 10.0f, sr * 0.48f) / sr;
        c1[points] = std::cos(w); s1[points] = std::sin(w);
        c2[points] = std::cos(2.0f * w); s2[points] = std::sin(2.0f * w);
        ++points;
    };
    for (int i = 0; i < n; ++i) { addAngle(f.s[i].poleHz); addAngle(f.s[i].zeroHz); }
    // One pass gives both normalisations: the row maximum per section, and the product down
    // each column for the finished cascade.
    float mag[kZSections][kMaxPoints];
    for (int i = 0; i < n; ++i) {
        ch[i].set(f.s[i], sr);
        float rowMax = 1e-20f, logSum = 0.0f;
        for (int k = 0; k < points; ++k) {
            mag[i][k] = std::max(ch[i].magSqAt(c1[k], s1[k], c2[k], s2[k]), 1e-20f);
            rowMax = std::max(rowMax, mag[i][k]);
            logSum += std::log(mag[i][k]);
        }
        // Normalise the section to its geometric mean over the grid, not to its peak: a bell
        // (zero on the pole, wider) then keeps unity background and boosts only its peak, so
        // six of them in series shape a spectrum instead of cancelling each other out. The
        // boost is capped at 30 dB per section so the cascade cannot overflow.
        const float gmean = std::exp(0.5f * logSum / static_cast<float>(points));
        const float scale = std::min(f.s[i].gain / gmean, 32.0f / std::sqrt(rowMax));
        ch[i].b0 *= scale; ch[i].b1 *= scale; ch[i].b2 *= scale;
        const float sq = scale * scale;
        for (int k = 0; k < points; ++k) mag[i][k] *= sq;
    }
    float peakSq = 1e-20f;
    for (int k = 0; k < points; ++k) {
        float prod = 1.0f;
        for (int i = 0; i < n; ++i) prod *= mag[i][k];
        peakSq = std::max(peakSq, prod);
    }
    return clampv(1.0f / std::sqrt(peakSq), 1e-4f, 16.0f);
}

void ZBiquad::set(const ZSection& s, float sr)
{
    // Poles: radius from the bandwidth, angle from the centre frequency. The radius is capped
    // well inside the unit circle (about 0.4 s of ringing), so no interpolated point can blow up.
    const float f = clampv(s.poleHz, 20.0f, sr * 0.45f);
    const float r = std::min(std::exp(-kPi * std::max(s.poleBw, 0.5f) / sr), 0.99995f);
    const float th = kTwoPi * f / sr;
    a1 = -2.0f * r * std::cos(th);
    a2 = r * r;
    if (s.zeroHz > 0.0f) {
        const float fz = clampv(s.zeroHz, 20.0f, sr * 0.48f);
        const float rz = std::min(std::exp(-kPi * std::max(s.zeroBw, 0.5f) / sr), 0.9999f);
        const float tz = kTwoPi * fz / sr;
        b0 = 1.0f; b1 = -2.0f * rz * std::cos(tz); b2 = rz * rz;
    } else {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
    }
    // The gain is applied by zBuildCascade, which normalises the section over the whole grid.
}

float ZBiquad::magnitudeAt(float w) const
{
    const float c1 = std::cos(w), s1 = std::sin(w);
    const float c2 = std::cos(2.0f * w), s2 = std::sin(2.0f * w);
    const float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    const float dr = 1.0f + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    const float den = dr * dr + di * di;
    return std::sqrt((nr * nr + ni * ni) / std::max(den, 1e-20f));
}

} // namespace ambient
