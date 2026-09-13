// The partial bank's three inner loops (Core/include/ambient/Simd.h), without an engine: every
// one of them against the arithmetic worked out of the definition in double, at lengths that land
// on a lane boundary and at lengths that do not, so the vector body and the scalar tail are both
// run. The selftest runs them, and ambient_banktest runs them once for every vector path the bank
// has (see banktest.cpp) -- AVX2 as the desktop builds it, NEON through the x86 shim, and scalar.
//
// Why a reference rather than "the paths agree with each other": two wrong paths can agree.
//
// And why the reference is re-seeded from the implementation's own state at every sample rather
// than run alongside it: a phasor bank is a RECURRENCE, and the FM loop renormalises with two
// Newton steps. Started together and left to run, a double-precision copy and a single-precision
// one drift apart -- the Newton correction pulls each back onto its own circle -- and after sixty
// samples they disagree by far more than single precision explains. The first draft of this file
// failed all three paths that way, including the scalar one that had not been touched, which is
// the tell: an oracle that fails the code it is meant to bless is measuring itself. So each step
// is checked against the definition applied to the state the implementation actually had, and
// what accumulates is then only what the definition allows.
//
// Include after a CHECK(cond, msg) macro is defined.
#pragma once
#include "ambient/Simd.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bankchecks {

constexpr int kMax = 64;

// A bank in a known state: phasors on the unit circle at spread angles, rotations of a few
// hundredths of a turn, amplitudes falling with the partial, ramps small and of both signs.
struct Bank {
    float pc[kMax], ps[kMax], rc[kMax], rs[kMax], amp[kMax], step[kMax], wL[kMax], wR[kMax], hf[kMax];
    Bank()
    {
        for (int h = 0; h < kMax; ++h) {
            const double a0 = 0.31 * h + 0.11;              // where the phasor starts
            pc[h] = static_cast<float>(std::cos(a0));
            ps[h] = static_cast<float>(std::sin(a0));
            const double w = 0.013 * (h + 1);               // its rotation per sample
            rc[h] = static_cast<float>(std::cos(w));
            rs[h] = static_cast<float>(std::sin(w));
            amp[h] = static_cast<float>(0.9 / (h + 1));
            step[h] = static_cast<float>(((h % 3) - 1) * 1.0e-4);
            const double pan = 0.25 * 3.14159265358979 * (1.0 + 0.7 * std::sin(0.7 * h));
            wL[h] = static_cast<float>(std::cos(pan));
            wR[h] = static_cast<float>(std::sin(pan));
            hf[h] = static_cast<float>(h + 1);
        }
    }
};

// One sample of the definition, in double, FROM the state the implementation is in. `mode`:
// 0 plain, 1 stereo, 2 FM. Writes what the state should become into pcOut/psOut/ampOut.
struct RefStep {
    double sumL = 0.0, sumR = 0.0;
    // The sum of the terms' magnitudes: what a reordered sum's error is bounded BY. A bank whose
    // partials cancel has a sum near zero and an error that is not small against it -- measuring
    // against the sum itself made the FM checks fail on both vector paths for no fault of theirs.
    double magL = 0.0, magR = 0.0;
    double pc[kMax] = {}, ps[kMax] = {}, amp[kMax] = {};
    void run(const Bank& b, const float* pcIn, const float* psIn, const float* ampIn,
             int n, int mode, double theta, double maxStep)
    {
        sumL = sumR = magL = magR = 0.0;
        for (int h = 0; h < n; ++h) {
            const double c = pcIn[h], s = psIn[h], a = ampIn[h];
            const double v = a * s;
            if (mode == 1) {
                sumL += v * b.wL[h]; sumR += v * b.wR[h];
                magL += std::fabs(v * b.wL[h]); magR += std::fabs(v * b.wR[h]);
            } else { sumL += v; magL += std::fabs(v); }
            const double nc = c * b.rc[h] - s * b.rs[h];
            const double ns = s * b.rc[h] + c * b.rs[h];
            if (mode == 2) {
                double t = theta * b.hf[h];
                t = t < -maxStep ? -maxStep : (t > maxStep ? maxStep : t);
                const double c2 = nc - ns * t, s2 = ns + nc * t;
                const double r2 = c2 * c2 + s2 * s2;
                double fix = 1.5 - 0.5 * r2;
                fix *= 1.5 - 0.5 * r2 * fix * fix;
                pc[h] = c2 * fix; ps[h] = s2 * fix;
            } else {
                pc[h] = nc; ps[h] = ns;
            }
            amp[h] = a + b.step[h];
        }
    }
};

inline void bankChecks()
{
    // Lengths that end on a lane boundary and lengths that do not: eight lanes on AVX2, four on
    // NEON, so 1, 3, 7, 11 and 33 leave a tail on one or both and 8, 16, 32 leave none.
    const int lengths[] = { 0, 1, 3, 7, 8, 11, 16, 32, 33 };
    const int samples = 64;
    // One step from the same state: what may differ is the ORDER the n terms were added in. The
    // bound on that is the machine epsilon times the number of terms times the sum of their
    // MAGNITUDES -- not times the sum, which can be anywhere near zero when the partials cancel.
    // Single precision, at most 33 terms: 33 * 1.2e-7 is 4e-6, and this leaves a factor of five.
    auto sumOk = [](double got, double want, double mag) { return std::fabs(got - want) <= 2.0e-5 * (mag + 1.0e-6); };

    Bank b;
    for (int n : lengths) {
        float pc[kMax], ps[kMax], amp[kMax];
        RefStep r;

        {   // the plain bank
            for (int h = 0; h < kMax; ++h) { pc[h] = b.pc[h]; ps[h] = b.ps[h]; amp[h] = b.amp[h]; }
            double worstSum = 0.0, worstState = 0.0, drift = 0.0;
            for (int i = 0; i < samples; ++i) {
                r.run(b, pc, ps, amp, n, 0, 0.0, 0.0);
                const float got = ambient::phasorBankStep(pc, ps, b.rc, b.rs, amp, b.step, n);
                if (!sumOk(got, r.sumL, r.magL)) worstSum = std::max(worstSum, std::fabs(got - r.sumL) / (r.magL + 1.0e-6));
                for (int h = 0; h < n; ++h) {
                    worstState = std::max(worstState, std::fabs(pc[h] - r.pc[h]) + std::fabs(ps[h] - r.ps[h]));
                    drift = std::max(drift, std::fabs(std::sqrt(static_cast<double>(pc[h]) * pc[h]
                                                              + static_cast<double>(ps[h]) * ps[h]) - 1.0));
                }
            }
            CHECK(worstSum == 0.0, "the plain partial bank sums what the definition says");
            CHECK(worstState < 1.0e-5, "and turns every phasor where the definition turns it");
            CHECK(n == 0 || drift < 1.0e-3, "and its phasors stay on the unit circle");
        }
        {   // the stereo bank: two sums, one weight pair per partial
            for (int h = 0; h < kMax; ++h) { pc[h] = b.pc[h]; ps[h] = b.ps[h]; amp[h] = b.amp[h]; }
            double worst = 0.0;
            for (int i = 0; i < samples; ++i) {
                r.run(b, pc, ps, amp, n, 1, 0.0, 0.0);
                float gotL = 0.0f, gotR = 0.0f;
                ambient::phasorBankStepStereo(pc, ps, b.rc, b.rs, amp, b.step, n, b.wL, b.wR, gotL, gotR);
                if (!sumOk(gotL, r.sumL, r.magL)) worst = std::max(worst, std::fabs(gotL - r.sumL) / (r.magL + 1.0e-6));
                if (!sumOk(gotR, r.sumR, r.magR)) worst = std::max(worst, std::fabs(gotR - r.sumR) / (r.magR + 1.0e-6));
            }
            CHECK(worst == 0.0, "the stereo partial bank sums both sides as the definition says");
        }
        {   // the FM bank: no modulation, a little, and enough to reach the clamp on the high partials
            for (double theta : { 0.0, 0.004, 0.05 }) {
                for (int h = 0; h < kMax; ++h) { pc[h] = b.pc[h]; ps[h] = b.ps[h]; amp[h] = b.amp[h]; }
                double worst = 0.0, drift = 0.0, worstState = 0.0;
                for (int i = 0; i < samples; ++i) {
                    r.run(b, pc, ps, amp, n, 2, theta, 0.4);
                    const float got = ambient::phasorBankStepFm(pc, ps, b.rc, b.rs, amp, b.step, n, b.hf,
                                                                static_cast<float>(theta), 0.4f);
                    if (!sumOk(got, r.sumL, r.magL)) worst = std::max(worst, std::fabs(got - r.sumL) / (r.magL + 1.0e-6));
                    for (int h = 0; h < n; ++h) {
                        worstState = std::max(worstState, std::fabs(pc[h] - r.pc[h]) + std::fabs(ps[h] - r.ps[h]));
                        drift = std::max(drift, std::fabs(std::sqrt(static_cast<double>(pc[h]) * pc[h]
                                                                  + static_cast<double>(ps[h]) * ps[h]) - 1.0));
                    }
                }
                CHECK(worst == 0.0, "the FM partial bank sums what the definition says, clamp and all");
                CHECK(worstState < 1.0e-5, "and turns every phasor by its own modulation as the definition does");
                CHECK(n == 0 || drift < 1.0e-3, "and its two Newton steps keep the phasors on the unit circle");
            }
        }
    }

    {   // The three agree with one another where they must: weights of one make the stereo bank
        // the plain one twice over, and no modulation makes the FM bank the plain one. Run side by
        // side from one state, which is fair here because all three are single precision.
        const int n = 19;
        Bank one;
        for (int h = 0; h < kMax; ++h) { one.wL[h] = 1.0f; one.wR[h] = 1.0f; }
        float pcA[kMax], psA[kMax], ampA[kMax], pcB[kMax], psB[kMax], ampB[kMax], pcC[kMax], psC[kMax], ampC[kMax];
        for (int h = 0; h < kMax; ++h) {
            pcA[h] = pcB[h] = pcC[h] = one.pc[h];
            psA[h] = psB[h] = psC[h] = one.ps[h];
            ampA[h] = ampB[h] = ampC[h] = one.amp[h];
        }
        double worstStereo = 0.0, worstFm = 0.0;
        for (int i = 0; i < 32; ++i) {
            const float mono = ambient::phasorBankStep(pcA, psA, one.rc, one.rs, ampA, one.step, n);
            float l = 0.0f, rr = 0.0f;
            ambient::phasorBankStepStereo(pcB, psB, one.rc, one.rs, ampB, one.step, n, one.wL, one.wR, l, rr);
            const float fm = ambient::phasorBankStepFm(pcC, psC, one.rc, one.rs, ampC, one.step, n, one.hf, 0.0f, 0.4f);
            const double scale = std::fabs(mono) + 1.0e-3;
            worstStereo = std::max(worstStereo, (std::fabs(l - mono) + std::fabs(rr - mono)) / scale);
            worstFm = std::max(worstFm, std::fabs(fm - mono) / scale);
        }
        CHECK(worstStereo < 1.0e-4, "weights of one make the stereo bank the plain one on both sides");
        CHECK(worstFm < 1.0e-4, "no modulation makes the FM bank the plain one");
    }
}

}   // namespace bankchecks

inline void bankChecks() { bankchecks::bankChecks(); }
