#include "ambient/Voice.h"
#include "ambient/Params.h"   // kStackRatios
#include <cmath>
#include <cstring>

namespace ambient {

namespace {
// Set a phasor from a phase in [0,1).
inline void phasorFromPhase(double phase01, float& c, float& s)
{
    s = sin01(phase01);
    double q = phase01 + 0.25; if (q >= 1.0) q -= 1.0;
    c = sin01(q);
}

// log2 of the harmonic numbers, so the presence bell needs one log2 per strand, not per partial.
struct Log2Harmonics {
    float v[kMaxPartials + 1];
    Log2Harmonics() { v[0] = 0.0f; for (int h = 1; h <= kMaxPartials; ++h) v[h] = std::log2(static_cast<float>(h)); }
};
const Log2Harmonics kLog2H;
constexpr float kPresenceCentreLog2 = 11.64386f;   // log2(3200 Hz): the bell spans about 1.8-5.6 kHz
constexpr float kPresenceHalfWidth  = 0.8f;        // octaves to the bell's zero

// Harmonic numbers as floats: phase modulation of the waveform by θ moves partial h by h·θ.
struct HarmonicNumbers {
    float v[kMaxPartials];
    HarmonicNumbers() { for (int h = 0; h < kMaxPartials; ++h) v[h] = static_cast<float>(h + 1); }
};
const HarmonicNumbers kHf;
constexpr float kFmMaxStep = 0.4f;   // per-sample deviation clamp (tan of the angle): keeps the two-step normalisation exact
}

void Voice::prepare(double sampleRate, uint64_t seed)
{
    sr_ = sampleRate;
    rng_.seed(seed);
    env_.setSampleRate(sr_);
    for (auto& s : strands_) {
        for (int h = 0; h < kMaxPartials; ++h) {
            s.shimmer[h].init(rng_);
            phasorFromPhase(rng_.uniform(), s.pc[h], s.ps[h]);
            s.rc[h] = 1.0f; s.rs[h] = 0.0f;
            s.amp[h] = 0.0f;
            s.ampStep[h] = 0.0f;
        }
        s.pitch.init(rng_);
        s.active = 0;
    }
    filterDrift_.init(rng_);
    airDrift_.init(rng_);
    panCenter_.init(rng_);
    breath_.init(rng_);
    rateWander_.init(rng_);
    filtL_.reset(); filtR_.reset();
    airL_.reset();  airR_.reset();
    std::memset(itdBufL_, 0, sizeof(itdBufL_));
    std::memset(itdBufR_, 0, sizeof(itdBufR_));
    itdW_ = 0;
    itdL_ = itdR_ = itdLTarget_ = itdRTarget_ = 0.0f;
    cachedTilt_ = -1.0f; cachedOddEven_ = -9.0f; cachedPartials_ = -1; cachedB_ = -1.0f;
    env_.kill();
    note_ = -1;
}

void Voice::noteOn(int note, double freqHz, float velocity, int owner, float distance, const VoiceParams& p)
{
    note_ = note;
    freq_ = freqHz;
    velocity_ = 0.3f + 0.7f * clampv(velocity, 0.0f, 1.0f);
    owner_ = owner;
    distance_ = clampv(distance, 0.0f, 1.0f);
    distEff_  = distance_;
    gNear_  = std::cos(distance_ * 0.5f * kPi);
    gFar_   = std::sin(distance_ * 0.5f * kPi);
    gLevel_ = 1.0f - 0.5f * distance_;
    env_.setTimes(p.attack, p.decay, p.sustain, p.release);
    if (!env_.isActive()) {
        // Fresh start: random phases (no two voices share a waveform), silent partials.
        for (auto& s : strands_) {
            for (int h = 0; h < kMaxPartials; ++h) {
                phasorFromPhase(rng_.uniform(), s.pc[h], s.ps[h]);
                s.amp[h] = 0.0f; s.ampStep[h] = 0.0f;
            }
            s.active = 0;
        }
        filtL_.reset(); filtR_.reset();
        airL_.reset();  airR_.reset();
        std::memset(itdBufL_, 0, sizeof(itdBufL_));
        std::memset(itdBufR_, 0, sizeof(itdBufR_));
        bloomT_ = 0.0f;
    }
    env_.noteOn();
}

void Voice::noteOff() { env_.noteOff(); }
void Voice::kill()    { env_.kill(); note_ = -1; }

void Voice::control(int blockLen, const VoiceParams& p)
{
    const float dt = static_cast<float>(blockLen / sr_);
    env_.setTimes(p.attack, p.decay, p.sustain, p.release);

    // Breath: the voice's plane itself wanders slowly (Rich's "the room breathes"): everything
    // that hangs on the distance -- dry/wet balance, level, air absorption, presence -- moves
    // with it, continuously (Drifter = smoothstep curves, no steps).
    // Rate wander (nested modulation): one very slow curve per voice scales every movement
    // rate by up to +-1 octave, so five minutes never look like the five before.
    const float rw = rateWander_.update(dt, 0.01f, rng_);
    const float rateMul = p.rateWander > 0.0f ? std::pow(2.0f, rw * p.rateWander) : 1.0f;
    const float driftRate = p.driftRate * rateMul, shimmerRate = p.shimmerRate * rateMul;

    const float bd = breath_.update(dt, p.breathRate * rateMul, rng_);
    distEff_ = clampv(distance_ + 0.35f * p.breath * bd, 0.0f, 1.0f);
    gNear_  = std::cos(distEff_ * 0.5f * kPi);
    gFar_   = std::sin(distEff_ * 0.5f * kPi);
    gLevel_ = 1.0f - 0.5f * distEff_;
    // Presence: a broad bell in the 2-5 kHz articulation band, only on the near plane. Applied
    // in the additive domain (per partial), so it costs nothing per sample.
    const float presLin = p.presence > 0.0f ? std::pow(10.0f, p.presence * (1.0f - distEff_) / 20.0f) - 1.0f : 0.0f;
    const float lowCut = p.lowCut;

    // Bloom: the spectrum opens over bloomTime seconds (smoothstep, so the start is gentle).
    bloomT_ += dt;
    const float bt = clampv(bloomT_ / std::max(p.bloomTime, 1.0f), 0.0f, 1.0f);
    const float bloomOpen = bt * bt * (3.0f - 2.0f * bt);
    const float brightness = p.brightness * (1.0f - p.bloom * (1.0f - bloomOpen));

    // Base spectrum shared by all strands of this voice. The tilt/odd-even shape is
    // cached (pow is expensive); only the brightness window is applied per block.
    const int partials = clampv(p.partials, 1, kMaxPartials);
    if (p.tilt != cachedTilt_ || p.oddEven != cachedOddEven_ || partials != cachedPartials_) {
        for (int h = 1; h <= partials; ++h) {
            float a = std::pow(static_cast<float>(h), -p.tilt);
            if (p.oddEven > 0.0f && (h % 2) == 0) a *= 1.0f - p.oddEven;
            if (p.oddEven < 0.0f && (h % 2) == 1 && h > 1) a *= 1.0f + p.oddEven;
            tiltCache_[h] = a;
        }
        cachedTilt_ = p.tilt; cachedOddEven_ = p.oddEven; cachedPartials_ = partials;
    }
    const float hc = 1.0f + brightness * brightness * 31.0f;   // brightness -> last full-level harmonic
    float base[kMaxPartials + 1];
    for (int h = 1; h <= partials; ++h) {
        float a = tiltCache_[h];
        if (static_cast<float>(h) > hc) {
            const float x = std::min((static_cast<float>(h) - hc) / 6.0f, 1.0f);
            a *= 0.5f * (1.0f + std::cos(kPi * x));
        }
        base[h] = a;
    }
    const float B = p.inharmonic * p.inharmonic * 0.02f;
    if (B != cachedB_) {
        for (int h = 1; h <= kMaxPartials; ++h) stretchCache_[h - 1] = B > 0.0f ? std::sqrt(1.0 + B * static_cast<double>(h * h)) : 1.0;
        cachedB_ = B;
    }
    const int unison = clampv(p.unison, 1, kMaxStrands);
    const float norm = 1.0f / std::sqrt(static_cast<float>(unison));
    const double nyq = 0.45 * sr_;
    const float invLen = 1.0f / static_cast<float>(blockLen);

    // The voice's centre wanders slowly; strands fan out around it.
    const float centre = panCenter_.update(dt, driftRate * 0.3f, rng_) * p.panDrift;
    const int stack = clampv(p.stack, 0, kNumStacks - 1);

    for (int si = 0; si < unison; ++si) {
        Strand& s = strands_[si];
        const float pos = (unison == 1) ? 0.0f : (2.0f * static_cast<float>(si) / static_cast<float>(unison - 1) - 1.0f);
        const float cents = pos * p.detune + s.pitch.update(dt, driftRate, rng_) * p.drift;
        // Stack: the strand sits at a pure ratio to the note (a just chord from one key);
        // detune and drift still apply on top, so Detune 0 makes it beat-free.
        const double f = freq_ * kStackRatios[stack][si] * std::pow(2.0, cents / 1200.0);
        const float pan = clampv(centre + pos * p.spread, -1.0f, 1.0f);
        const float angle = (pan + 1.0f) * 0.25f * kPi;
        s.gainL = std::cos(angle) * norm;
        s.gainR = std::sin(angle) * norm;

        float target[kMaxPartials];
        float sumSq = 0.0f;
        int H = 0;
        const float lf0 = std::log2(static_cast<float>(f)) - kPresenceCentreLog2;
        for (int h = 1; h <= partials; ++h) {
            const double fh = f * h * stretchCache_[h - 1];
            if (fh >= nyq) break;
            // Rotation per sample for this partial, and keep the phasor on the unit circle.
            const double inc = fh / sr_;
            phasorFromPhase(inc, s.rc[h - 1], s.rs[h - 1]);
            const float r2 = s.pc[h - 1] * s.pc[h - 1] + s.ps[h - 1] * s.ps[h - 1];
            const float fix = 1.5f - 0.5f * r2;
            s.pc[h - 1] *= fix; s.ps[h - 1] *= fix;
            const float d = s.shimmer[h - 1].update(dt, shimmerRate, rng_);
            float a = base[h] * (1.0f + 0.9f * p.shimmer * d);
            if (presLin > 0.0f) {
                const float x = (lf0 + kLog2H.v[h]) / kPresenceHalfWidth;
                if (x > -1.0f && x < 1.0f) a *= 1.0f + presLin * (1.0f - x * x);
            }
            if (lowCut > 0.0f && fh < lowCut) {
                const float r = static_cast<float>(fh) / lowCut;
                a *= r * r;   // 12 dB/oct below the cut
            }
            target[h - 1] = a;
            sumSq += a * a;
            H = h;
        }
        const float scale = sumSq > 0.0f ? 0.5f / std::sqrt(sumSq) : 0.0f;
        for (int h = 0; h < kMaxPartials; ++h) {
            const float tgt = (h < H) ? target[h] * scale : 0.0f;
            s.ampStep[h] = (tgt - s.amp[h]) * invLen;
        }
        int act = H;
        for (int h = H; h < s.active; ++h) if (std::fabs(s.amp[h]) > 1e-6f) act = h + 1;
        s.active = act;
    }
    if (unison < lastUnison_)
        for (int si = unison; si < lastUnison_; ++si)
            for (int h = 0; h < kMaxPartials; ++h) { strands_[si].amp[h] = 0.0f; strands_[si].ampStep[h] = 0.0f; }
    lastUnison_ = unison;

    // Interaural time difference from the centre pan: the far ear hears it later.
    const float maxItd = 0.00065f * static_cast<float>(sr_) * p.itd;
    itdLTarget_ = centre > 0.0f ?  centre * maxItd : 0.0f;
    itdRTarget_ = centre < 0.0f ? -centre * maxItd : 0.0f;
    // Head shadow: the far ear also loses highs (20 kHz -> ~3 kHz at full lateral position).
    const float shadowOct = 2.7f * p.itd;
    const float fcL = 20000.0f * std::pow(2.0f, -shadowOct * std::max(centre, 0.0f));
    const float fcR = 20000.0f * std::pow(2.0f, -shadowOct * std::max(-centre, 0.0f));
    shadowCoefL_ = fcL >= 19000.0f ? 1.0f : 1.0f - std::exp(-kTwoPi * fcL / static_cast<float>(sr_));
    shadowCoefR_ = fcR >= 19000.0f ? 1.0f : 1.0f - std::exp(-kTwoPi * fcR / static_cast<float>(sr_));

    // Filter: cutoff follows key, envelope, a slow drift, and distance (air absorption).
    const float fd = filterDrift_.update(dt, driftRate * 0.5f, rng_);
    const float octaves = p.keyTrack * static_cast<float>(note_ - 60) / 12.0f
                        + p.filterEnv * 4.0f * env_.level()
                        + p.filterDrift * 2.0f * fd
                        - 2.5f * distEff_;
    const float cut = p.cutoff * std::pow(2.0f, octaves);
    filtL_.set(cut, p.resonance, static_cast<float>(sr_));
    filtR_.copyCoefficients(filtL_);

    // Air: band-passed noise around a drifting multiple of the fundamental.
    if (p.air > 0.0f) {
        const float ad = airDrift_.update(dt, driftRate * 0.7f, rng_);
        const float fc = clampv(static_cast<float>(freq_) * p.airColor * std::pow(2.0f, 0.5f * ad), 40.0f, static_cast<float>(nyq));
        const float q = clampv(p.airQ, 1.0f, 40.0f);
        airL_.setQ(fc, q, static_cast<float>(sr_));
        airR_.copyCoefficients(airL_);
        // Normalise the expected band-passed noise level so `air` reads as a level.
        const float expectedRms = std::sqrt((1.0f / 3.0f) * kPi * fc / (q * static_cast<float>(sr_)));
        airGain_ = p.air * std::min(0.05f / std::max(expectedRms, 1e-4f), 40.0f);
    } else {
        airGain_ = 0.0f;
    }
}

void Voice::render(float* nearL, float* nearR, float* farL, float* farR, int n, const VoiceParams& p, const float* fm)
{
    const bool doFm = fm != nullptr && p.fmAmount > 0.0f;
    const float fmScale = p.fmAmount * 3.0f;   // radians at the fundamental per unit of feedback signal
    int pos = 0;
    while (pos < n && env_.isActive()) {
        const int len = std::min(kControlBlock, n - pos);
        control(len, p);
        const int unison = clampv(p.unison, 1, kMaxStrands);
        const bool air = airGain_ > 0.0f;
        for (int i = 0; i < len; ++i) {
            const float e = env_.process();
            const float th = doFm ? fm[pos + i] * fmScale : 0.0f;
            float accL = 0.0f, accR = 0.0f;
            for (int si = 0; si < unison; ++si) {
                Strand& s = strands_[si];
                float sum = 0.0f;
                const int act = s.active;
                float* pc = s.pc; float* ps = s.ps; const float* rc = s.rc; const float* rs = s.rs;
                float* amp = s.amp; const float* step = s.ampStep;
                if (doFm) {
                    // Rotate by the partial's own step, then by the modulation angle h·θ (small-angle
                    // rotation by tan = t, clamped, followed by two Newton steps of 1/sqrt so the
                    // phasor stays on the unit circle within 1e-4 per sample). The clamp makes deep
                    // modulation saturate softly on the high partials instead of tearing.
                    for (int h = 0; h < act; ++h) {
                        sum += amp[h] * ps[h];
                        const float nc = pc[h] * rc[h] - ps[h] * rs[h];
                        const float ns = ps[h] * rc[h] + pc[h] * rs[h];
                        const float t  = clampv(th * kHf.v[h], -kFmMaxStep, kFmMaxStep);
                        const float c2 = nc - ns * t, s2 = ns + nc * t;
                        const float r2 = c2 * c2 + s2 * s2;
                        float fix = 1.5f - 0.5f * r2;
                        fix *= 1.5f - 0.5f * r2 * fix * fix;
                        pc[h] = c2 * fix; ps[h] = s2 * fix;
                        amp[h] += step[h];
                    }
                } else {
                    for (int h = 0; h < act; ++h) {
                        sum += amp[h] * ps[h];
                        const float nc = pc[h] * rc[h] - ps[h] * rs[h];
                        ps[h] = ps[h] * rc[h] + pc[h] * rs[h];
                        pc[h] = nc;
                        amp[h] += step[h];
                    }
                }
                accL += sum * s.gainL;
                accR += sum * s.gainR;
            }
            float outL = filtL_.lp(accL);
            float outR = filtR_.lp(accR);
            if (air) {
                float lp, bp, hp;
                airL_.tick(rng_.bipolar(), lp, bp, hp); outL += airGain_ * bp;
                airR_.tick(rng_.bipolar(), lp, bp, hp); outR += airGain_ * bp;
            }
            const float g = e * velocity_ * gLevel_;
            outL *= g;
            outR *= g;
            // Interaural time difference.
            itdL_ += (itdLTarget_ - itdL_) * 0.002f;
            itdR_ += (itdRTarget_ - itdR_) * 0.002f;
            itdBufL_[itdW_ & (kItdBuffer - 1)] = outL;
            itdBufR_[itdW_ & (kItdBuffer - 1)] = outR;
            {
                const int di = static_cast<int>(itdL_); const float f = itdL_ - static_cast<float>(di);
                const float a = itdBufL_[(itdW_ - di) & (kItdBuffer - 1)], b = itdBufL_[(itdW_ - di - 1) & (kItdBuffer - 1)];
                outL = a + f * (b - a);
            }
            {
                const int di = static_cast<int>(itdR_); const float f = itdR_ - static_cast<float>(di);
                const float a = itdBufR_[(itdW_ - di) & (kItdBuffer - 1)], b = itdBufR_[(itdW_ - di - 1) & (kItdBuffer - 1)];
                outR = a + f * (b - a);
            }
            ++itdW_;
            shadowL_ += shadowCoefL_ * (outL - shadowL_); outL = shadowL_;
            shadowR_ += shadowCoefR_ * (outR - shadowR_); outR = shadowR_;
            nearL[pos + i] += outL * gNear_;
            nearR[pos + i] += outR * gNear_;
            farL[pos + i]  += outL * gFar_;
            farR[pos + i]  += outR * gFar_;
        }
        pos += len;
    }
    if (!env_.isActive()) note_ = -1;
}

} // namespace ambient
