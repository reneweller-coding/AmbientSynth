#include "ambient/Voice.h"
#include <cmath>

namespace ambient {

void Voice::prepare(double sampleRate, uint64_t seed)
{
    sr_ = sampleRate;
    rng_.seed(seed);
    env_.setSampleRate(sr_);
    for (auto& s : strands_) {
        for (int h = 0; h < kMaxPartials; ++h) {
            s.shimmer[h].init(rng_);
            s.phase[h] = rng_.uniform();
            s.amp[h] = 0.0f;
            s.ampStep[h] = 0.0f;
        }
        s.pitch.init(rng_);
        s.active = 0;
    }
    filterDrift_.init(rng_);
    filtL_.reset();
    filtR_.reset();
    env_.kill();
    note_ = -1;
}

void Voice::noteOn(int note, double freqHz, float velocity, int owner, const VoiceParams& p)
{
    note_ = note;
    freq_ = freqHz;
    velocity_ = 0.3f + 0.7f * clampv(velocity, 0.0f, 1.0f);
    owner_ = owner;
    env_.setTimes(p.attack, p.decay, p.sustain, p.release);
    if (!env_.isActive()) {
        // Fresh start: random phases (no two voices share a waveform), silent partials.
        for (auto& s : strands_) {
            for (int h = 0; h < kMaxPartials; ++h) { s.phase[h] = rng_.uniform(); s.amp[h] = 0.0f; s.ampStep[h] = 0.0f; }
            s.active = 0;
        }
        filtL_.reset();
        filtR_.reset();
    }
    env_.noteOn();
}

void Voice::noteOff() { env_.noteOff(); }
void Voice::kill()    { env_.kill(); note_ = -1; }

void Voice::control(int blockLen, const VoiceParams& p)
{
    const float dt = static_cast<float>(blockLen / sr_);
    env_.setTimes(p.attack, p.decay, p.sustain, p.release);

    // Base spectrum shared by all strands of this voice.
    const int partials = clampv(p.partials, 1, kMaxPartials);
    const float hc = 1.0f + p.brightness * p.brightness * 31.0f;   // brightness -> last full-level harmonic
    float base[kMaxPartials + 1];
    for (int h = 1; h <= partials; ++h) {
        float a = std::pow(static_cast<float>(h), -p.tilt);
        if (p.oddEven > 0.0f && (h % 2) == 0) a *= 1.0f - p.oddEven;
        if (p.oddEven < 0.0f && (h % 2) == 1 && h > 1) a *= 1.0f + p.oddEven;
        if (static_cast<float>(h) > hc) {
            const float x = std::min((static_cast<float>(h) - hc) / 6.0f, 1.0f);
            a *= 0.5f * (1.0f + std::cos(kPi * x));
        }
        base[h] = a;
    }
    const float B = p.inharmonic * p.inharmonic * 0.02f;
    const int unison = clampv(p.unison, 1, kMaxStrands);
    const float norm = 1.0f / std::sqrt(static_cast<float>(unison));
    const double nyq = 0.45 * sr_;
    const float invLen = 1.0f / static_cast<float>(blockLen);

    for (int si = 0; si < unison; ++si) {
        Strand& s = strands_[si];
        const float pos = (unison == 1) ? 0.0f : (2.0f * static_cast<float>(si) / static_cast<float>(unison - 1) - 1.0f);
        const float cents = pos * p.detune + s.pitch.update(dt, p.driftRate, rng_) * p.drift;
        const double f = freq_ * std::pow(2.0, cents / 1200.0);
        const float pan = pos * p.spread;
        const float angle = (pan + 1.0f) * 0.25f * kPi;
        s.gainL = std::cos(angle) * norm;
        s.gainR = std::sin(angle) * norm;

        float target[kMaxPartials];
        float sumSq = 0.0f;
        int H = 0;
        for (int h = 1; h <= partials; ++h) {
            const double fh = f * h * std::sqrt(1.0 + B * static_cast<double>(h * h));
            if (fh >= nyq) break;
            s.inc[h - 1] = fh / sr_;
            const float d = s.shimmer[h - 1].update(dt, p.shimmerRate, rng_);
            const float a = base[h] * (1.0f + 0.9f * p.shimmer * d);
            target[h - 1] = a;
            sumSq += a * a;
            H = h;
        }
        const float scale = sumSq > 0.0f ? 0.5f / std::sqrt(sumSq) : 0.0f;
        for (int h = 0; h < kMaxPartials; ++h) {
            const float tgt = (h < H) ? target[h] * scale : 0.0f;
            s.ampStep[h] = (tgt - s.amp[h]) * invLen;
        }
        // Keep rendering partials that are fading out after a drop in H.
        int act = H;
        for (int h = H; h < s.active; ++h) if (std::fabs(s.amp[h]) > 1e-6f) act = h + 1;
        s.active = act;
    }
    // Strands that were switched off keep their state but are not rendered; when
    // switched back on they fade in from zero because amp was left untouched only
    // if they had been silent -- so silence them explicitly.
    if (unison < lastUnison_)
        for (int si = unison; si < lastUnison_; ++si)
            for (int h = 0; h < kMaxPartials; ++h) { strands_[si].amp[h] = 0.0f; strands_[si].ampStep[h] = 0.0f; }
    lastUnison_ = unison;

    // Filter: cutoff follows key, envelope and a slow drift.
    const float fd = filterDrift_.update(dt, p.driftRate * 0.5f, rng_);
    const float octaves = p.keyTrack * static_cast<float>(note_ - 60) / 12.0f
                        + p.filterEnv * 4.0f * env_.level()
                        + p.filterDrift * 2.0f * fd;
    const float cut = p.cutoff * std::pow(2.0f, octaves);
    filtL_.set(cut, p.resonance, static_cast<float>(sr_));
    filtR_.set(cut, p.resonance, static_cast<float>(sr_));
}

void Voice::render(float* L, float* R, int n, const VoiceParams& p)
{
    int pos = 0;
    while (pos < n && env_.isActive()) {
        const int len = std::min(kControlBlock, n - pos);
        control(len, p);
        const int unison = clampv(p.unison, 1, kMaxStrands);
        for (int i = 0; i < len; ++i) {
            const float e = env_.process();
            float accL = 0.0f, accR = 0.0f;
            for (int si = 0; si < unison; ++si) {
                Strand& s = strands_[si];
                float sum = 0.0f;
                const int act = s.active;
                for (int h = 0; h < act; ++h) {
                    double ph = s.phase[h] + s.inc[h];
                    if (ph >= 1.0) ph -= 1.0;
                    s.phase[h] = ph;
                    sum += s.amp[h] * sin01(ph);
                    s.amp[h] += s.ampStep[h];
                }
                accL += sum * s.gainL;
                accR += sum * s.gainR;
            }
            const float g = e * velocity_;
            L[pos + i] += filtL_.lp(accL) * g;
            R[pos + i] += filtR_.lp(accR) * g;
        }
        pos += len;
    }
    if (!env_.isActive()) note_ = -1;
}

} // namespace ambient
