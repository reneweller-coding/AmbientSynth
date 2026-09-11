// NOT the real <arm_neon.h>. The few AArch64 NEON intrinsics the Room's convolver and the grain
// loop use, written out lane by lane after their ACLE definitions, so that an x86 test build
// (AMBIENT_NEON_SHIM, see Tests/CMakeLists.txt) runs the NEON paths instead of skipping them.
//
// The arithmetic is the same per lane as on the hardware: add, subtract and multiply are single
// IEEE operations, and the fused forms use std::fma, which is what vfmaq_f32 and vfmsq_f32 compute
// on AArch64 (a + b c and a - b c as one fused multiply-add). What this cannot check is whether
// the real header spells something differently -- that is what the NDK build is for.
#pragma once
#include <cmath>

typedef float float32_t;
struct float32x4_t { float32_t lane[4]; };

inline float32x4_t vld1q_f32(const float32_t* p)
{ float32x4_t r; for (int i = 0; i < 4; ++i) r.lane[i] = p[i]; return r; }

inline void vst1q_f32(float32_t* p, float32x4_t a)
{ for (int i = 0; i < 4; ++i) p[i] = a.lane[i]; }

inline float32x4_t vdupq_n_f32(float32_t x)
{ float32x4_t r; for (int i = 0; i < 4; ++i) r.lane[i] = x; return r; }

inline float32x4_t vaddq_f32(float32x4_t a, float32x4_t b)
{ for (int i = 0; i < 4; ++i) a.lane[i] += b.lane[i]; return a; }

inline float32x4_t vsubq_f32(float32x4_t a, float32x4_t b)
{ for (int i = 0; i < 4; ++i) a.lane[i] -= b.lane[i]; return a; }

inline float32x4_t vmulq_f32(float32x4_t a, float32x4_t b)
{ for (int i = 0; i < 4; ++i) a.lane[i] *= b.lane[i]; return a; }

// a + b c, fused
inline float32x4_t vfmaq_f32(float32x4_t a, float32x4_t b, float32x4_t c)
{ for (int i = 0; i < 4; ++i) a.lane[i] = std::fma(b.lane[i], c.lane[i], a.lane[i]); return a; }

// a - b c, fused
inline float32x4_t vfmsq_f32(float32x4_t a, float32x4_t b, float32x4_t c)
{ for (int i = 0; i < 4; ++i) a.lane[i] = std::fma(-b.lane[i], c.lane[i], a.lane[i]); return a; }

// a + b c and a - b c, not fused (32-bit ARM without FMA)
inline float32x4_t vmlaq_f32(float32x4_t a, float32x4_t b, float32x4_t c)
{ for (int i = 0; i < 4; ++i) a.lane[i] += b.lane[i] * c.lane[i]; return a; }

inline float32x4_t vmlsq_f32(float32x4_t a, float32x4_t b, float32x4_t c)
{ for (int i = 0; i < 4; ++i) a.lane[i] -= b.lane[i] * c.lane[i]; return a; }
