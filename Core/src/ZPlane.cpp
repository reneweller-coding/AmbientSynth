#include "ambient/ZPlane.h"
#include <algorithm>

namespace ambient {

const char* const kZModeNames[3] = { "Off", "Series", "Replace" };

// Sixteen shapes in six families, in the spirit of the Morpheus taxonomy (vowels, sweeps,
// combs and phasers, instrument bodies, peak clusters, extremes) but designed here, not copied.
const char* const kZShapeNames[kZShapes] = {
    "Vowel Morph", "Choir", "Nasal",                 // vowels and voices
    "Low Sweep", "High Sweep", "Band Sweep",         // classic filter sweeps
    "Phaser", "Comb", "Flanger", "Notch Cluster",    // combs, phasers, notches
    "Strings", "Metal Bars", "Wood", "Glass",        // instrument bodies
    "Peaks", "Infinite",                             // peak cluster and near self-oscillation
};

// Corners: (0,0) (1,0) (0,1) (1,1).  { frequencies... }, bandwidth ratio, zero ratio, zero bw ratio, tilt
const ZCornerSpec kZCorners[kZShapes][4] = {
    {   // Vowel Morph: a -> e across X, o -> i up Y; pure resonators, so the vowel takes the sound over
        { { 700, 1150, 2900 }, 0.12f, 0.0f, 0.0f, 0.70f },
        { { 400, 1600, 2700 }, 0.12f, 0.0f, 0.0f, 0.70f },
        { { 450,  800, 2830 }, 0.12f, 0.0f, 0.0f, 0.70f },
        { { 270, 2300, 3000 }, 0.12f, 0.0f, 0.0f, 0.70f },
    },
    {   // Choir: four voiced formants as bell sections -- peaks on a flat background, not a hollow tube
        { { 320,  800, 2200, 3400 }, 0.10f, 1.00f, 0.80f, 0.80f },
        { { 400, 1100, 2600, 3800 }, 0.10f, 1.00f, 0.80f, 0.80f },
        { { 250,  700, 1900, 3000 }, 0.10f, 1.00f, 0.80f, 0.80f },
        { { 500, 1400, 3000, 4200 }, 0.10f, 1.00f, 0.80f, 0.80f },
    },
    {   // Nasal: formants with a deep notch just above each
        { { 300, 1000, 2400 }, 0.09f, 1.35f, 0.15f, 0.85f },
        { { 380, 1500, 2900 }, 0.09f, 1.35f, 0.15f, 0.85f },
        { { 260,  850, 2000 }, 0.09f, 1.35f, 0.15f, 0.85f },
        { { 480, 1900, 3600 }, 0.09f, 1.35f, 0.15f, 0.85f },
    },
    {   // Low Sweep: X = cutoff (120 Hz .. 1.8 kHz), Y = resonance; a zero far above tips it down
        { {  120,  180 }, 0.60f, 3.0f, 0.80f, 0.90f },
        { { 1800, 2700 }, 0.60f, 3.0f, 0.80f, 0.90f },
        { {  120,  180 }, 0.12f, 3.0f, 0.80f, 0.90f },
        { { 1800, 2700 }, 0.12f, 3.0f, 0.80f, 0.90f },
    },
    {   // High Sweep: the zero sits below the pole, so the low end is cut away
        { {  150,  220 }, 0.60f, 0.35f, 0.60f, 0.90f },
        { { 2500, 3700 }, 0.60f, 0.35f, 0.60f, 0.90f },
        { {  150,  220 }, 0.15f, 0.35f, 0.60f, 0.90f },
        { { 2500, 3700 }, 0.15f, 0.35f, 0.60f, 0.90f },
    },
    {   // Band Sweep: one band, X = position, Y = narrowness
        { {  200 }, 0.25f, 0.0f, 0.0f, 1.0f },
        { { 3000 }, 0.25f, 0.0f, 0.0f, 1.0f },
        { {  200 }, 0.06f, 0.0f, 0.0f, 1.0f },
        { { 3000 }, 0.06f, 0.0f, 0.0f, 1.0f },
    },
    {   // Phaser: six octave-spaced pole/zero pairs, the whole comb sliding up
        { { 200, 400,  800, 1600, 3200,  6400 }, 0.35f, 1.25f, 0.30f, 1.0f },
        { { 300, 600, 1200, 2400, 4800,  9600 }, 0.35f, 1.25f, 0.30f, 1.0f },
        { { 150, 300,  600, 1200, 2400,  4800 }, 0.35f, 1.25f, 0.30f, 1.0f },
        { { 450, 900, 1800, 3600, 7200, 12000 }, 0.35f, 1.25f, 0.30f, 1.0f },
    },
    {   // Comb: harmonic teeth, each a sharp bell (zero on the pole, ten times wider)
        { { 110, 220, 330, 440,  550,  660 }, 0.06f, 1.00f, 0.60f, 0.95f },
        { { 165, 330, 495, 660,  825,  990 }, 0.06f, 1.00f, 0.60f, 0.95f },
        { {  82, 164, 246, 328,  410,  492 }, 0.06f, 1.00f, 0.60f, 0.95f },
        { { 220, 440, 660, 880, 1100, 1320 }, 0.06f, 1.00f, 0.60f, 0.95f },
    },
    {   // Flanger: a wider comb of bells whose spacing opens and closes
        { { 300,  600,  900, 1200, 1500, 1800 }, 0.20f, 1.00f, 1.20f, 1.0f },
        { { 500, 1000, 1500, 2000, 2500, 3000 }, 0.20f, 1.00f, 1.20f, 1.0f },
        { { 200,  400,  600,  800, 1000, 1200 }, 0.20f, 1.00f, 1.20f, 1.0f },
        { { 800, 1600, 2400, 3200, 4000, 4800 }, 0.20f, 1.00f, 1.20f, 1.0f },
    },
    {   // Notch Cluster: four deep notches (zero on the pole, far narrower) in a wide filter
        { { 400,  900, 1800, 3600 }, 0.80f, 1.0f, 0.06f, 1.0f },
        { { 600, 1300, 2600, 5200 }, 0.80f, 1.0f, 0.06f, 1.0f },
        { { 300,  700, 1400, 2800 }, 0.80f, 1.0f, 0.06f, 1.0f },
        { { 900, 2000, 4000, 8000 }, 0.80f, 1.0f, 0.06f, 1.0f },
    },
    {   // Strings: the inharmonic body resonances of a plucked box
        { { 190, 330, 510, 780, 1250 }, 0.08f, 1.00f, 0.80f, 0.80f },
        { { 240, 420, 650, 990, 1580 }, 0.08f, 1.00f, 0.80f, 0.80f },
        { { 150, 260, 400, 610,  980 }, 0.08f, 1.00f, 0.80f, 0.80f },
        { { 310, 540, 830, 1270, 2030 }, 0.08f, 1.00f, 0.80f, 0.80f },
    },
    {   // Metal Bars: sharp inharmonic bells, the partials of a struck bar
        { { 300,  760, 1490, 2470, 3700 }, 0.03f, 1.00f, 0.45f, 0.85f },
        { { 420, 1060, 2080, 3450, 5170 }, 0.03f, 1.00f, 0.45f, 0.85f },
        { { 210,  530, 1040, 1730, 2590 }, 0.03f, 1.00f, 0.45f, 0.85f },
        { { 600, 1520, 2980, 4940, 7400 }, 0.03f, 1.00f, 0.45f, 0.85f },
    },
    {   // Wood: broad low-mid resonances, the inside of a hollow body
        { { 180, 420,  900, 1600 }, 0.22f, 1.00f, 1.50f, 0.75f },
        { { 240, 560, 1200, 2100 }, 0.22f, 1.00f, 1.50f, 0.75f },
        { { 140, 330,  700, 1250 }, 0.22f, 1.00f, 1.50f, 0.75f },
        { { 320, 750, 1600, 2800 }, 0.22f, 1.00f, 1.50f, 0.75f },
    },
    {   // Glass: high, sparse, very sharp
        { {  900, 2100, 3900 }, 0.020f, 1.00f, 0.30f, 0.70f },
        { { 1300, 3000, 5600 }, 0.020f, 1.00f, 0.30f, 0.70f },
        { {  700, 1600, 3000 }, 0.020f, 1.00f, 0.30f, 0.70f },
        { { 1800, 4200, 7800 }, 0.020f, 1.00f, 0.30f, 0.70f },
    },
    {   // Peaks: six harmonic needles -- the harmonic series pressed onto anything that goes through
        { { 220, 440,  660,  880, 1100, 1320 }, 0.025f, 1.00f, 0.375f, 0.90f },
        { { 330, 660,  990, 1320, 1650, 1980 }, 0.025f, 1.00f, 0.375f, 0.90f },
        { { 165, 330,  495,  660,  825,  990 }, 0.025f, 1.00f, 0.375f, 0.90f },
        { { 440, 880, 1320, 1760, 2200, 2640 }, 0.025f, 1.00f, 0.375f, 0.90f },
    },
    {   // Infinite: two almost identical poles at the stability limit -- a ringing that beats
        { {  150,  151 }, 0.004f, 0.0f, 0.0f, 1.0f },
        { { 1200, 1210 }, 0.004f, 0.0f, 0.0f, 1.0f },
        { {  150,  153 }, 0.012f, 0.0f, 0.0f, 1.0f },
        { { 1200, 1230 }, 0.012f, 0.0f, 0.0f, 1.0f },
    },
};

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

ZFrame zInterpolate(int shape, float x, float y)
{
    shape = clampv(shape, 0, kZShapes - 1);
    x = clampv(x, 0.0f, 1.0f); y = clampv(y, 0.0f, 1.0f);
    const ZFrame c[4] = { zFrameFromSpec(kZCorners[shape][0]), zFrameFromSpec(kZCorners[shape][1]),
                          zFrameFromSpec(kZCorners[shape][2]), zFrameFromSpec(kZCorners[shape][3]) };
    ZFrame out;
    out.used = std::min(std::min(c[0].used, c[1].used), std::min(c[2].used, c[3].used));
    auto lerp2 = [&](float a, float b, float cc, float d) { return (a * (1 - x) + b * x) * (1 - y) + (cc * (1 - x) + d * x) * y; };
    auto logLerp = [&](float a, float b, float cc, float d) {
        return std::exp2(lerp2(std::log2(std::max(a, 1e-3f)), std::log2(std::max(b, 1e-3f)),
                               std::log2(std::max(cc, 1e-3f)), std::log2(std::max(d, 1e-3f))));
    };
    for (int i = 0; i < out.used; ++i) {
        ZSection& s = out.s[i];
        s.poleHz = logLerp(c[0].s[i].poleHz, c[1].s[i].poleHz, c[2].s[i].poleHz, c[3].s[i].poleHz);
        s.poleBw = logLerp(c[0].s[i].poleBw, c[1].s[i].poleBw, c[2].s[i].poleBw, c[3].s[i].poleBw);
        // A zero that exists in every corner is interpolated; if any corner has none, there is none.
        const bool zeros = c[0].s[i].zeroHz > 0.0f && c[1].s[i].zeroHz > 0.0f && c[2].s[i].zeroHz > 0.0f && c[3].s[i].zeroHz > 0.0f;
        s.zeroHz = zeros ? logLerp(c[0].s[i].zeroHz, c[1].s[i].zeroHz, c[2].s[i].zeroHz, c[3].s[i].zeroHz) : 0.0f;
        s.zeroBw = zeros ? logLerp(c[0].s[i].zeroBw, c[1].s[i].zeroBw, c[2].s[i].zeroBw, c[3].s[i].zeroBw) : 0.0f;
        s.gain   = lerp2(c[0].s[i].gain, c[1].s[i].gain, c[2].s[i].gain, c[3].s[i].gain);
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
