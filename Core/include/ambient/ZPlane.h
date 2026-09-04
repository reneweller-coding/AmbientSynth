// AmbientSynth -- Z-plane filter, after the idea Dave Rossum built into the E-mu Morpheus
// (US 5,170,369, expired): four filter "frames" sit on the corners of a square, and a point
// (X, Y) inside it is a filter whose POLES AND ZEROS are interpolated between the corners.
// Move the point and the whole resonant structure glides, always through stable filters.
//
// The coefficient tables of the original are proprietary firmware data; this bank is our own,
// built in the same architecture: up to six cascaded two-pole/two-zero sections (a 12-pole
// filter, the order the Morpheus used), interpolation in the polar z-plane domain (log centre
// frequency, log bandwidth), never on the coefficients themselves. Every section is normalised
// to unity at its own peak, and the cascade is normalised again over the section frequencies,
// so a shape can be extremely resonant without becoming loud.
#pragma once
#include "Dsp.h"

namespace ambient {

constexpr int kZSections = 6;
constexpr int kZShapes   = 16;
extern const char* const kZShapeNames[kZShapes];
extern const char* const kZModeNames[3];   // Off, Series, Replace

// One two-pole / two-zero section. A zero frequency of 0 means "no zeros" (a plain resonator).
struct ZSection { float poleHz = 1000.0f, poleBw = 100.0f, zeroHz = 0.0f, zeroBw = 0.0f, gain = 1.0f; };
struct ZFrame   { ZSection s[kZSections]; int used = 0; };

// Corner descriptor: up to six centre frequencies plus the rules that shape them. Compact on
// purpose -- one readable line per corner instead of a wall of coefficients.
struct ZCornerSpec {
    float hz[kZSections];   // 0 ends the list
    float bwRatio;          // pole bandwidth as a fraction of the centre frequency (small = resonant)
    float zeroRatio;        // zero frequency / pole frequency (0 = no zeros; 1 = notch at the pole)
    float zeroBwRatio;      // zero bandwidth / zero frequency (small = deep notch)
    float tilt;             // gain factor per section, applied as tilt^i
};
// Corners in the order (0,0) (1,0) (0,1) (1,1).
extern const ZCornerSpec kZCorners[kZShapes][4];

ZFrame zFrameFromSpec(const ZCornerSpec& c);
// Bilinear interpolation of the four corner frames in the log-frequency / log-bandwidth domain.
ZFrame zInterpolate(int shape, float x, float y);

// A cascaded two-pole/two-zero section, transposed direct form II.
struct ZBiquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
    // Poles and zeros from centre frequencies and bandwidths; the section is normalised to
    // unity gain at its own pole frequency, then scaled by `gain`.
    void set(const ZSection& s, float sr);
    // |H| at the given angular frequency (radians per sample), for the cascade normalisation.
    float magnitudeAt(float w) const;
    // |H|^2 from precomputed cos/sin of w and 2w (the normalisation grid).
    float magSqAt(float c1, float s1, float c2, float s2) const
    {
        const float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const float dr = 1.0f + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return (nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-20f);
    }
    inline float tick(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void copyCoefficients(const ZBiquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
    void reset() { z1 = z2 = 0.0f; }
};

// Builds the cascade of a frame into `ch` (f.used sections) and returns the gain that brings its
// loudest point back to unity. Every section is first normalised to its own maximum over a
// 16-point logarithmic grid -- normalising at the pole frequency alone fails for a section whose
// zero sits on its pole (a notch), and the cascade would then be either silent or very loud.
float zBuildCascade(const ZFrame& f, ZBiquad* ch, float sr);

// One two-pole resonator (constant-peak-gain form), used by the Air "Ghost" mode.
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
