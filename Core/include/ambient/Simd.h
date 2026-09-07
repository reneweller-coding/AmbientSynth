// AmbientSynth -- the one loop worth vectorising by hand.
//
// Every voice is a bank of rotating phasors: per sample and per partial, one complex multiply to
// turn the phasor, one multiply-add into the sum, one add to ramp the amplitude. With up to six
// strands of thirty-two partials in sixteen voices, plus three source slots that run the same
// bank, this loop is most of the instrument's arithmetic. The compiler vectorises parts of it,
// but not the reduction, because floating-point addition is not associative and it may not
// reorder the sum on its own. Doing it here is the one place where saying so explicitly pays.
//
// The scalar path stays, and is what every platform without AVX (the Quest's arm64 among them)
// compiles: they are the same arithmetic in a different order, and the difference between them
// is the last bit or two of the sum.
#pragma once
#include "Dsp.h"

#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX__)) || (defined(_MSC_VER) && defined(_M_X64) && defined(AMBIENT_AVX))
  #define AMBIENT_HAS_AVX 1
  #include <immintrin.h>
#else
  #define AMBIENT_HAS_AVX 0
#endif

namespace ambient {

// The same step with a left and a right weight per partial: two sums instead of one, for the
// bank whose partials are spread across the field one by one. Scalar; the spread is a choice,
// and the vectorised path above stays exactly as it was for everything that does not make it.
inline void phasorBankStepStereo(float* pc, float* ps, const float* rc, const float* rs,
                                 float* amp, const float* step, int n,
                                 const float* wL, const float* wR, float& outL, float& outR)
{
    float sumL = 0.0f, sumR = 0.0f;
    for (int h = 0; h < n; ++h) {
        const float v = amp[h] * ps[h];
        sumL += v * wL[h];
        sumR += v * wR[h];
        const float nc = pc[h] * rc[h] - ps[h] * rs[h];
        ps[h] = ps[h] * rc[h] + pc[h] * rs[h];
        pc[h] = nc;
        amp[h] += step[h];
    }
    outL = sumL; outR = sumR;
}

// One sample of a phasor bank: turns every phasor by its own rotation, sums amp*sin, and steps
// the amplitudes. Returns the sum. `n` may be anything from 0 to the array length.
inline float phasorBankStep(float* pc, float* ps, const float* rc, const float* rs,
                            float* amp, const float* step, int n)
{
#if AMBIENT_HAS_AVX
    __m256 acc = _mm256_setzero_ps();
    int h = 0;
    for (; h + 8 <= n; h += 8) {
        const __m256 c = _mm256_loadu_ps(pc + h), s = _mm256_loadu_ps(ps + h);
        const __m256 kc = _mm256_loadu_ps(rc + h), ks = _mm256_loadu_ps(rs + h);
        const __m256 a = _mm256_loadu_ps(amp + h);
        acc = _mm256_add_ps(acc, _mm256_mul_ps(a, s));
        // (c, s) turned by (kc, ks): the complex product, which is two multiplies and a subtract
        // for the real part and the same for the imaginary one.
        _mm256_storeu_ps(pc + h, _mm256_sub_ps(_mm256_mul_ps(c, kc), _mm256_mul_ps(s, ks)));
        _mm256_storeu_ps(ps + h, _mm256_add_ps(_mm256_mul_ps(s, kc), _mm256_mul_ps(c, ks)));
        _mm256_storeu_ps(amp + h, _mm256_add_ps(a, _mm256_loadu_ps(step + h)));
    }
    // Horizontal sum of the eight lanes, then whatever partials are left over.
    __m128 lo = _mm256_castps256_ps128(acc), hi = _mm256_extractf128_ps(acc, 1);
    lo = _mm_add_ps(lo, hi);
    lo = _mm_add_ps(lo, _mm_movehl_ps(lo, lo));
    lo = _mm_add_ss(lo, _mm_shuffle_ps(lo, lo, 1));
    float sum = _mm_cvtss_f32(lo);
    for (; h < n; ++h) {
        sum += amp[h] * ps[h];
        const float nc = pc[h] * rc[h] - ps[h] * rs[h];
        ps[h] = ps[h] * rc[h] + pc[h] * rs[h];
        pc[h] = nc;
        amp[h] += step[h];
    }
    return sum;
#else
    float sum = 0.0f;
    for (int h = 0; h < n; ++h) {
        sum += amp[h] * ps[h];
        const float nc = pc[h] * rc[h] - ps[h] * rs[h];
        ps[h] = ps[h] * rc[h] + pc[h] * rs[h];
        pc[h] = nc;
        amp[h] += step[h];
    }
    return sum;
#endif
}

} // namespace ambient
