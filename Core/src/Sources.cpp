#include "ambient/Sources.h"
#include "ambient/Voice.h"     // kControlBlock
#include "ambient/Cosmos.h"    // Fft for wavetable analysis
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ambient {

const char* const kSourceTypeNames[kNumSourceTypes] = { "Off", "Wavetable", "FM", "Texture", "Noise", "Additive" };
const char* const kNoiseKindNames[kNumNoiseKinds] = {
    "White", "Pink", "Brown", "Blue", "Violet", "Grey", "Band", "Wind", "Crackle", "Digital",
};
const char* const kTableNames[kNumTables] = { "Classic", "Organ", "Vocal", "Glass", "Metal", "User" };
const char* const kSlotRatioNames[kNumSlotRatios] = { "1/1", "9/8", "6/5", "5/4", "4/3", "3/2", "8/5", "5/3", "7/4", "2/1" };
const double      kSlotRatios[kNumSlotRatios] = { 1.0, 9.0 / 8.0, 6.0 / 5.0, 5.0 / 4.0, 4.0 / 3.0, 3.0 / 2.0, 8.0 / 5.0, 5.0 / 3.0, 7.0 / 4.0, 2.0 };
const char* const kFollowNames[2] = { "Free", "Note" };

// ---------------------------------------------------------------- wavetables

namespace {

void normaliseFrame(float* a)
{
    float sq = 0.0f;
    for (int h = 0; h < kTablePartials; ++h) sq += a[h] * a[h];
    if (sq <= 0.0f) return;
    const float s = 1.0f / std::sqrt(sq);
    for (int h = 0; h < kTablePartials; ++h) a[h] *= s;
}

struct BuiltinTables {
    Wavetable t[kNumTables - 1];
    BuiltinTables()
    {
        auto frame = [&](Wavetable& w, int fi) -> float* { w.frames = std::max(w.frames, fi + 1); return w.amp[fi]; };
        // Classic: sine -> triangle -> saw -> square -> narrow pulse.
        {
            Wavetable& w = t[0];
            float* f0 = frame(w, 0); f0[0] = 1.0f;
            float* f1 = frame(w, 1); for (int h = 1; h <= 32; h += 2) f1[h - 1] = 1.0f / static_cast<float>(h * h);
            float* f2 = frame(w, 2); for (int h = 1; h <= 32; ++h) f2[h - 1] = 1.0f / static_cast<float>(h);
            float* f3 = frame(w, 3); for (int h = 1; h <= 32; h += 2) f3[h - 1] = 1.0f / static_cast<float>(h);
            float* f4 = frame(w, 4); for (int h = 1; h <= 32; ++h) f4[h - 1] = std::fabs(std::sin(kPi * 0.2f * static_cast<float>(h))) / static_cast<float>(h);
        }
        // Organ: 16'+8' -> flutes 8'4'2' -> full drawbars -> mixture with fifths.
        {
            Wavetable& w = t[1];
            float* f0 = frame(w, 0); f0[0] = 1.0f; f0[1] = 0.8f;
            float* f1 = frame(w, 1); f1[0] = 1.0f; f1[1] = 0.7f; f1[3] = 0.5f; f1[7] = 0.3f;
            float* f2 = frame(w, 2); { const int hs[] = { 1, 2, 3, 4, 5, 6, 8 }; const float g[] = { 1.0f, 0.8f, 0.6f, 0.5f, 0.4f, 0.35f, 0.3f }; for (int i = 0; i < 7; ++i) f2[hs[i] - 1] = g[i]; }
            float* f3 = frame(w, 3); { const int hs[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16, 24 }; for (int i = 0; i < 10; ++i) f3[hs[i] - 1] = 1.0f / std::sqrt(static_cast<float>(i + 1)); }
        }
        // Vocal: formant envelopes a e i o u evaluated at a 130 Hz fundamental, with a 1/h tilt.
        {
            Wavetable& w = t[2];
            const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
            const float B[3] = { 90.0f, 110.0f, 160.0f };
            const float G[3] = { 1.0f, 0.5f, 0.25f };
            for (int v = 0; v < 5; ++v) {
                float* f = frame(w, v);
                for (int h = 1; h <= 32; ++h) {
                    const float fh = 130.0f * static_cast<float>(h);
                    float a = 0.02f;
                    for (int k = 0; k < 3; ++k) { const float x = (fh - F[v][k]) / B[k]; a += G[k] * std::exp(-x * x); }
                    f[h - 1] = a / std::sqrt(static_cast<float>(h));
                }
            }
        }
        // Glass: sparse, high, thinning out toward the last frame.
        {
            Wavetable& w = t[3];
            float* f0 = frame(w, 0); f0[0] = 1.0f;
            float* f1 = frame(w, 1); f1[0] = 1.0f; f1[2] = 0.6f; f1[7] = 0.3f;
            float* f2 = frame(w, 2); f2[0] = 1.0f; f2[3] = 0.5f; f2[8] = 0.35f; f2[15] = 0.25f;
            float* f3 = frame(w, 3); f3[0] = 1.0f; f3[2] = 0.6f; f3[6] = 0.45f; f3[11] = 0.35f; f3[18] = 0.3f; f3[26] = 0.25f;
        }
        // Metal: dense, odd-heavy, comb-dipped.
        {
            Wavetable& w = t[4];
            float* f0 = frame(w, 0); for (int h = 1; h <= 32; h += 2) f0[h - 1] = 1.0f / std::sqrt(static_cast<float>(h));
            float* f1 = frame(w, 1); for (int h = 1; h <= 32; ++h) f1[h - 1] = std::fabs(std::cos(0.7f * static_cast<float>(h))) / std::sqrt(static_cast<float>(h));
            float* f2 = frame(w, 2); for (int h = 1; h <= 32; ++h) f2[h - 1] = ((h % 3) == 0 ? 1.0f : 0.3f) / std::sqrt(static_cast<float>(h));
            float* f3 = frame(w, 3); for (int h = 1; h <= 32; ++h) f3[h - 1] = (0.5f + 0.5f * std::sin(2.3f * static_cast<float>(h) + 1.0f)) / std::pow(static_cast<float>(h), 0.35f);
        }
        for (auto& w : t) for (int fi = 0; fi < w.frames; ++fi) normaliseFrame(w.amp[fi]);
    }
};

const BuiltinTables& builtins() { static const BuiltinTables b; return b; }

} // namespace

const Wavetable& builtinTable(int index)
{
    return builtins().t[clampv(index, 0, kNumTables - 2)];
}

double baseHzFromName(const char* fileName)
{
    if (fileName == nullptr) return 0.0;
    // Strip directories and the extension.
    const char* base = fileName;
    for (const char* p = fileName; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    int len = static_cast<int>(std::strlen(base));
    for (int i = len - 1; i > 0; --i) if (base[i] == '.') { len = i; break; }
    // The last token after '_', '-' or ' ': letter, optional #/b, octave digit(s).
    int start = len;
    while (start > 0 && base[start - 1] != '_' && base[start - 1] != '-' && base[start - 1] != ' ') --start;
    const char* t = base + start;
    const int tlen = len - start;
    if (tlen < 2 || tlen > 4) return 0.0;
    int pc = -1;
    switch (t[0]) { case 'C': pc = 0; break; case 'D': pc = 2; break; case 'E': pc = 4; break; case 'F': pc = 5; break;
                    case 'G': pc = 7; break; case 'A': pc = 9; break; case 'B': pc = 11; break; default: return 0.0; }
    int i = 1;
    if (t[i] == '#') { pc += 1; ++i; }
    else if (t[i] == 'b') { pc -= 1; ++i; }
    if (i >= tlen) return 0.0;
    int octave = 0; bool neg = false;
    if (t[i] == '-') { neg = true; ++i; }
    if (i >= tlen) return 0.0;
    for (; i < tlen; ++i) { if (t[i] < '0' || t[i] > '9') return 0.0; octave = octave * 10 + (t[i] - '0'); }
    if (neg) octave = -octave;
    const int midi = (octave + 1) * 12 + pc;
    if (midi < 0 || midi > 127) return 0.0;
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

void Wavetable::spectrumAt(float pos, float* out) const
{
    if (frames <= 0) { std::memset(out, 0, sizeof(float) * kTablePartials); return; }
    if (frames == 1) { std::memcpy(out, amp[0], sizeof(float) * kTablePartials); return; }
    const float x = clampv(pos, 0.0f, 1.0f) * static_cast<float>(frames - 1);
    const int i = std::min(static_cast<int>(x), frames - 2);
    const float f = x - static_cast<float>(i);
    for (int h = 0; h < kTablePartials; ++h) out[h] = amp[i][h] + f * (amp[i + 1][h] - amp[i][h]);
}

bool Wavetable::analyse(const float* mono, int n, int frameLen)
{
    frames = 0;
    if (mono == nullptr || frameLen < 64 || n < frameLen) return false;
    const int total = n / frameLen;
    const int keep = std::min(total, kTableFrames);
    Fft fft(frameLen);
    std::vector<float> re(static_cast<size_t>(frameLen)), im(static_cast<size_t>(frameLen));
    for (int k = 0; k < keep; ++k) {
        const int src = (keep == total) ? k : static_cast<int>(static_cast<long long>(k) * (total - 1) / std::max(keep - 1, 1));
        std::memcpy(re.data(), mono + static_cast<size_t>(src) * static_cast<size_t>(frameLen), sizeof(float) * static_cast<size_t>(frameLen));
        std::fill(im.begin(), im.end(), 0.0f);
        fft.transform(re.data(), im.data(), false);
        for (int h = 1; h <= kTablePartials; ++h)
            amp[k][h - 1] = h < frameLen / 2 ? std::sqrt(re[static_cast<size_t>(h)] * re[static_cast<size_t>(h)] + im[static_cast<size_t>(h)] * im[static_cast<size_t>(h)]) : 0.0f;
        normaliseFrame(amp[k]);
    }
    frames = keep;
    return true;
}

// ---------------------------------------------------------------- slot

namespace {
inline double wrap01(double x) { return x - std::floor(x); }
}

void SourceSlot::prepare(double sampleRate, uint64_t seed)
{
    sr_ = sampleRate;
    rng_.seed(seed);
    posDrift_.init(rng_);
    idxDrift_.init(rng_);
    for (int h = 0; h < kTablePartials; ++h) { phasorFrom(rng_.uniform(), pc_[h], ps_[h]); rc_[h] = 1.0f; rs_[h] = 0.0f; amp_[h] = ampStep_[h] = 0.0f; }
    {   // the shimmer drifters seed from a side stream, so the slot's own stream is what it always was
        Rng aux; aux.seed(seed ^ 0xD1B54A32D192ED03ull);
        for (auto& d : shim_) d.init(aux);
        pitchDrift_.init(aux);
    }
    active_ = 0;
    cTilt_ = -1.0f; cOdd_ = -9.0f; cPartials_ = -1;
    phC_ = phM_ = 0.0;
    for (auto& g : grains_) g.on = false;
    spawnIn_ = 0.0;
    gL_ = gR_ = 0.0f;
}

void SourceSlot::noteOn(bool fresh)
{
    if (!fresh) return;
    for (int h = 0; h < kTablePartials; ++h) { phasorFrom(rng_.uniform(), pc_[h], ps_[h]); amp_[h] = ampStep_[h] = 0.0f; }
    active_ = 0;
    phC_ = rng_.uniform(); phM_ = rng_.uniform();
    hpX_ = hpY_ = 0.0f;
    for (auto& g : grains_) g.on = false;
    spawnIn_ = 0.0;
}

void SourceSlot::render(float* outL, float* outR, int n, double noteHz, const SlotParams& p,
                        const Wavetable* table, const Texture* texture, float driftRate)
{
    if (p.type == SourceType::Off || n <= 0) { lastType_ = p.type; return; }   // (Source 1 in Additive mode also lands here with n = 0)
    n = std::min(n, kControlBlock);
    const float dt = static_cast<float>(n / sr_);
    if (p.type != lastType_) {   // switching type: start clean, no leftover phasor amplitudes or grains
        for (int h = 0; h < kTablePartials; ++h) { amp_[h] = ampStep_[h] = 0.0f; }
        active_ = 0;
        for (auto& g : grains_) g.on = false;
        lastType_ = p.type;
    }
    double hz = noteHz * kSlotRatios[clampv(p.ratio, 0, kNumSlotRatios - 1)] * std::pow(2.0, clampv(p.octave, -2, 2));
    // Slow independent pitch drift: each source on its own curve, so three sources on pure ratios
    // beat like an ensemble in a room whose temperature moves, never in lockstep.
    if (p.drift > 0.0f) hz *= std::pow(2.0, static_cast<double>(p.drift * pitchDrift_.update(dt, driftRate > 0.0f ? driftRate : 0.05f, rng_)) / 1200.0);

    // Level and pan ramp across the block (equal power).
    const float angle = (clampv(p.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    const float tL = p.level * std::cos(angle), tR = p.level * std::sin(angle);
    const float sL = (tL - gL_) / static_cast<float>(n), sR = (tR - gR_) / static_cast<float>(n);

    if (p.type == SourceType::Noise) {
        float bufL[kControlBlock];
        std::memset(bufL, 0, sizeof(float) * static_cast<size_t>(n));
        renderNoise(bufL, n, hz, p, dt);        // left into bufL, right into scratch_
        for (int i = 0; i < n; ++i) { outL[i] += bufL[i]; outR[i] += scratch_[i]; }
        gL_ = tL; gR_ = tR;
        return;
    }
    if (p.type == SourceType::Texture) {
        // Grains carry their own level/pan (fixed at spawn), written straight to L/R.
        float bufL[kControlBlock];
        std::memset(bufL, 0, sizeof(float) * static_cast<size_t>(n));
        renderTexture(bufL, n, hz, hz / std::max(noteHz, 1.0), p, texture, dt);   // left into bufL, right into scratch_
        for (int i = 0; i < n; ++i) { outL[i] += bufL[i]; outR[i] += scratch_[i]; }
        gL_ = tL; gR_ = tR;
        return;
    }

    std::memset(scratch_, 0, sizeof(float) * static_cast<size_t>(n));
    if (p.type == SourceType::Wavetable) renderWavetable(scratch_, n, hz, p, table, dt);
    else if (p.type == SourceType::Additive) renderAdditive(scratch_, n, hz, p, dt);
    else renderFm(scratch_, n, hz, p, dt);
    for (int i = 0; i < n; ++i) {
        gL_ += sL; gR_ += sR;
        outL[i] += scratch_[i] * gL_;
        outR[i] += scratch_[i] * gR_;
    }
    gL_ = tL; gR_ = tR;
}

void SourceSlot::renderWavetable(float* out, int n, double hz, const SlotParams& p, const Wavetable* table, float dt)
{
    // Control: spectrum at the (wandering) position, targets normalised, rotations refreshed.
    const float wander = posDrift_.update(dt, 0.02f, rng_) * 0.5f * p.positionDrift;
    float spec[kTablePartials];
    if (table != nullptr) table->spectrumAt(p.position + wander, spec);
    else std::memset(spec, 0, sizeof(spec));
    const double nyq = 0.45 * sr_;
    float sumSq = 0.0f; int H = 0;
    for (int h = 1; h <= kTablePartials; ++h) {
        const double fh = hz * h;
        if (fh >= nyq) break;
        phasorFrom(fh / sr_, rc_[h - 1], rs_[h - 1]);
        const float r2 = pc_[h - 1] * pc_[h - 1] + ps_[h - 1] * ps_[h - 1];
        const float fix = 1.5f - 0.5f * r2;
        pc_[h - 1] *= fix; ps_[h - 1] *= fix;
        sumSq += spec[h - 1] * spec[h - 1];
        H = h;
    }
    renderBank(spec, H, n, out);
}

void SourceSlot::renderBank(const float* spec, int H, int n, float* out)
{
    float sumSq = 0.0f;
    for (int h = 0; h < H; ++h) sumSq += spec[h] * spec[h];
    const float scale = sumSq > 0.0f ? 0.5f / std::sqrt(sumSq) : 0.0f;
    const float invLen = 1.0f / static_cast<float>(n);
    for (int h = 0; h < kTablePartials; ++h) {
        const float tgt = (h < H) ? spec[h] * scale : 0.0f;
        ampStep_[h] = (tgt - amp_[h]) * invLen;
    }
    int act = H;
    for (int h = H; h < active_; ++h) if (std::fabs(amp_[h]) > 1e-6f) act = h + 1;
    active_ = act;

    for (int i = 0; i < n; ++i) {
        float sum = 0.0f;
        for (int h = 0; h < active_; ++h) {
            sum += amp_[h] * ps_[h];
            const float nc = pc_[h] * rc_[h] - ps_[h] * rs_[h];
            ps_[h] = ps_[h] * rc_[h] + pc_[h] * rs_[h];
            pc_[h] = nc;
            amp_[h] += ampStep_[h];
        }
        out[i] = sum;
    }
}

// Additive in a slot: the voice's spectrum formula (tilt, brightness window, odd/even, inharmonic
// stretch, per-partial shimmer) on the slot's own bank -- one strand, so a second or third
// additive source costs what a wavetable slot costs.
void SourceSlot::renderAdditive(float* out, int n, double hz, const SlotParams& p, float dt)
{
    const int partials = clampv(p.partials, 1, kTablePartials);
    if (p.tilt != cTilt_ || p.oddEven != cOdd_ || partials != cPartials_) {
        for (int h = 1; h <= partials; ++h) {
            float a = std::pow(static_cast<float>(h), -p.tilt);
            if (p.oddEven > 0.0f && (h % 2) == 0) a *= 1.0f - p.oddEven;
            if (p.oddEven < 0.0f && (h % 2) == 1 && h > 1) a *= 1.0f + p.oddEven;
            tiltCache_[h - 1] = a;
        }
        cTilt_ = p.tilt; cOdd_ = p.oddEven; cPartials_ = partials;
    }
    const float hc = 1.0f + p.bright * p.bright * 31.0f;
    const float B = p.inharm * p.inharm * 0.02f;
    const double nyq = 0.45 * sr_;
    float spec[kTablePartials];
    int H = 0;
    for (int h = 1; h <= partials; ++h) {
        const double stretch = B > 0.0f ? std::sqrt(1.0 + B * static_cast<double>(h * h)) : 1.0;
        const double fh = hz * h * stretch;
        if (fh >= nyq) break;
        phasorFrom(fh / sr_, rc_[h - 1], rs_[h - 1]);
        const float r2 = pc_[h - 1] * pc_[h - 1] + ps_[h - 1] * ps_[h - 1];
        const float fix = 1.5f - 0.5f * r2;
        pc_[h - 1] *= fix; ps_[h - 1] *= fix;
        float a = tiltCache_[h - 1];
        if (static_cast<float>(h) > hc) {
            const float x = std::min((static_cast<float>(h) - hc) / 6.0f, 1.0f);
            a *= 0.5f * (1.0f + std::cos(kPi * x));
        }
        a *= 1.0f + 0.9f * p.shimmer * shim_[h - 1].update(dt, p.shimmerRate, rng_);
        spec[h - 1] = a;
        H = h;
    }
    renderBank(spec, H, n, out);
}

int SourceSlot::displayGrains(GrainInfo* out, int maxCount, int clipLen) const
{
    int n = 0;
    for (int c = 0; c < kSlotGrains && n < maxCount; ++c) {
        const Grain& g = grains_[c];
        if (!g.on || g.len <= 0) continue;
        GrainInfo& o = out[n++];
        o.pos  = clipLen > 0 ? static_cast<float>(g.pos / static_cast<double>(clipLen)) : 0.0f;
        o.age  = static_cast<float>(g.age) / static_cast<float>(g.len);
        o.gain = g.gain;
        const float sum = g.gl + g.gr;
        o.pan  = sum > 1e-6f ? (g.gr - g.gl) / sum : 0.0f;
    }
    return n;
}

void SourceSlot::renderFm(float* out, int n, double hz, const SlotParams& p, float dt)
{
    // Two operators; the index shrinks above 3 kHz so high notes do not alias, and wanders
    // slowly by up to a factor two with Pos Drift.
    const float d = idxDrift_.update(dt, 0.03f, rng_);
    const float idx = p.fmIndex * std::min(1.0f, 3000.0f / static_cast<float>(std::max(hz, 20.0))) * std::pow(2.0f, 0.5f * d * p.positionDrift);
    const double incC = hz / sr_, incM = hz * static_cast<double>(clampv(p.fmRatio, 0.25f, 16.0f)) / sr_;
    const double k = idx / kTwoPi;
    // DC blocker (10 Hz): with an integer ratio the phase-modulated wave carries a DC term
    // (J1(index) for ratio 1), which would sit in the reverbs and pump the clipper.
    const float hpc = 1.0f - kTwoPi * 10.0f / static_cast<float>(sr_);
    for (int i = 0; i < n; ++i) {
        const float m = sin01(phM_);
        const float x = 0.5f * sin01(wrap01(phC_ + k * m));
        const float y = x - hpX_ + hpc * hpY_;
        hpX_ = x; hpY_ = y;
        out[i] = y;
        phC_ += incC; if (phC_ >= 1.0) phC_ -= 1.0;
        phM_ += incM; if (phM_ >= 1.0) phM_ -= 1.0;
    }
}

void Texture::measure()
{
    // The wavetable and FM slots normalise themselves to unity, so a Texture slot has to be
    // brought to the same reference or Level means something different in each slot: a field
    // recording at -25 dBFS RMS entered the mix twenty decibels below its neighbours.
    double sq = 0.0;
    for (float v : mono) sq += static_cast<double>(v) * v;
    const double rms = mono.empty() ? 0.0 : std::sqrt(sq / static_cast<double>(mono.size()));
    gain = rms > 1e-6 ? static_cast<float>(clampv(0.3 / rms, 0.5, 16.0)) : 1.0f;
}

void SourceSlot::renderTexture(float* outL, int n, double hz, double speed, const SlotParams& p, const Texture* tex, float dt)
{
    // outL receives the left channel, scratch_ the right (the caller adds both).
    std::memset(scratch_, 0, sizeof(float) * static_cast<size_t>(n));
    if (tex == nullptr || tex->empty()) return;
    const float* s = tex->mono.data();
    const int len = static_cast<int>(tex->mono.size());
    const float wander = posDrift_.update(dt, 0.02f, rng_) * 0.15f * p.positionDrift;
    const int maxGrains = clampv(p.grains, 1, kSlotGrains);

    // Spawn grains at Density per second (jittered), around Position.
    spawnIn_ -= dt;
    while (spawnIn_ <= 0.0) {
        spawnIn_ += (1.0 / std::max(p.density, 0.1f)) * (0.7 + 0.6 * rng_.uniform());
        Grain* g = nullptr;
        for (int c = 0; c < maxGrains; ++c) if (!grains_[c].on) { g = &grains_[c]; break; }
        if (g == nullptr) continue;   // the slot is full: this grain is dropped
        const double resample = tex->sampleRate / sr_;
        // Note: the sample is pitched to the note (recorded at baseHz). Free: original speed,
        // with octave and ratio acting as a playback-speed multiplier.
        const double rate = (p.follow ? hz / std::max(tex->baseHz, 20.0) : speed) * resample;
        const int glen = std::max(64, static_cast<int>(p.grainMs * 0.001f * static_cast<float>(sr_)));
        const double span = static_cast<double>(len) - static_cast<double>(glen) * rate - 2.0;
        if (span <= 0.0) continue;
        const double centre = clampv(static_cast<double>(p.position) + wander, 0.0, 1.0) * span;
        // Spread scatters the start point around Position; at 1 a grain may come from anywhere.
        double start = centre + (rng_.bipolar() * clampv(p.spread, 0.0f, 1.0f)) * len;
        start = clampv(start, 0.0, span);
        // Overlap normalisation, capped by the grains actually available: asking for more overlap
        // than the slot can hold used to make the sound quieter instead of denser.
        const float overlap = std::min(std::max(1.0f, p.density * p.grainMs * 0.001f),
                                       static_cast<float>(maxGrains));
        // For a Hann-windowed stream at overlap N the sum has RMS sqrt(N)*0.612*source, so the
        // constant that returns the source's own level is 1/0.612 = 1.63, not the 0.7 that stood
        // here -- which is where the missing 7.4 dB were.
        const float norm = 1.63f / std::sqrt(overlap);
        const float pan = clampv(p.pan + 0.3f * rng_.bipolar(), -1.0f, 1.0f);
        const float angle = (pan + 1.0f) * 0.25f * kPi;
        g->pos = start; g->rate = rate; g->len = glen; g->age = 0; g->on = true;
        g->gain = p.level * norm * tex->gain;
        g->gl = g->gain * std::cos(angle); g->gr = g->gain * std::sin(angle);
        // Hann window as a rotating phasor: w = 0.5 - 0.5*wc, advanced by one grain-length step.
        g->wc = 1.0f; g->ws = 0.0f;
        phasorFrom(1.0 / static_cast<double>(glen), g->rc, g->rs);
    }
    for (int c = 0; c < maxGrains; ++c) {
        Grain& g = grains_[c];
        if (!g.on) continue;
        {   // keep the window phasor on the unit circle (a grain can run for 48000 samples)
            const float r2 = g.wc * g.wc + g.ws * g.ws, fix = 1.5f - 0.5f * r2;
            g.wc *= fix; g.ws *= fix;
        }
        for (int i = 0; i < n; ++i) {
            if (g.age >= g.len || g.pos >= len - 2 || g.pos < 0.0) { g.on = false; break; }
            const int ip = static_cast<int>(g.pos); const float f = static_cast<float>(g.pos - ip);
            const float v = s[ip] + f * (s[ip + 1] - s[ip]);
            const float w = 0.5f - 0.5f * g.wc;
            outL[i] += v * w * g.gl;
            scratch_[i] += v * w * g.gr;
            const float nc = g.wc * g.rc - g.ws * g.rs;
            g.ws = g.ws * g.rc + g.wc * g.rs;
            g.wc = nc;
            g.pos += g.rate; ++g.age;
        }
    }
}

namespace {
// Paul Kellet's pink filter, all seven terms. The three-pole short form is 1.7 dB per octave too
// steep -- measured, which is why it is not used here.
inline float pinkStep(SourceSlot::NoiseState& st, float w)
{
    st.pink[0] = 0.99886f * st.pink[0] + w * 0.0555179f;
    st.pink[1] = 0.99332f * st.pink[1] + w * 0.0750759f;
    st.pink[2] = 0.96900f * st.pink[2] + w * 0.1538520f;
    st.pink[3] = 0.86650f * st.pink[3] + w * 0.3104856f;
    st.pink[4] = 0.55000f * st.pink[4] + w * 0.5329522f;
    st.pink[5] = -0.7616f * st.pink[5] - w * 0.0168980f;
    const float out = st.pink[0] + st.pink[1] + st.pink[2] + st.pink[3] + st.pink[4] + st.pink[5]
                    + st.pink[6] + w * 0.5362f;
    st.pink[6] = w * 0.115926f;
    return out * 0.18f;
}
} // namespace

// ---------------------------------------------------------------- noise
//
// Ten colours. The three textbook slopes (pink, brown, blue/violet) plus grey, a resonant band
// that can track the note, a wandering band that is wind, sparse crackle and sample-and-hold
// digital noise. Each is normalised so that Level means roughly the same loudness across the
// lot -- the same lesson the Texture slot taught: a source whose Level means something different
// from its neighbour's is a source nobody uses.
void SourceSlot::renderNoise(float* outL, int n, double hz, const SlotParams& p, float dt)
{
    std::memset(scratch_, 0, sizeof(float) * static_cast<size_t>(n));
    const NoiseKind kind = static_cast<NoiseKind>(clampv(static_cast<int>(p.noise), 0, kNumNoiseKinds - 1));

    // Position picks the band centre (or the colour), and Pos Drift lets it wander. With Pitch =
    // Note the centre follows the played note instead, which turns Band into a formant.
    const float wander = noiseDrift_.update(dt, 0.03f, rng_) * p.positionDrift;
    const float posN = clampv(p.position + 0.35f * wander, 0.0f, 1.0f);
    double centre = 40.0 * std::pow(300.0, static_cast<double>(posN));      // 40 Hz .. 12 kHz
    if (p.follow) centre = clampv(hz * (0.5 + 8.0 * posN), 30.0, 0.45 * sr_);
    centre = clampv(centre, 20.0, 0.45 * sr_);

    // State-variable band pass; q from Noise Q, wider for Wind so it breathes rather than whistles.
    const float f = 2.0f * std::sin(static_cast<float>(kPi * centre / sr_));
    const float qAmount = clampv(p.noiseQ, 0.0f, 1.0f);
    const float damp = kind == NoiseKind::Wind ? (0.6f - 0.5f * qAmount) : (0.7f - 0.66f * qAmount);

    const double rate = kind == NoiseKind::Digital
        ? clampv(200.0 * std::pow(100.0, static_cast<double>(posN)), 100.0, 0.5 * sr_)   // 200 Hz .. 20 kHz
        : 0.0;
    const double crackleRate = clampv(static_cast<double>(p.density) * 8.0, 1.0, 4000.0);

    // Level per colour, measured so a slot at Level 1 lands near the wavetable slot's output.
    // Measured against a Wavetable slot at the same Level (-22.8 dBFS) and corrected, one colour
    // at a time. Without this a violet slot was thirty decibels below a pink one at the same
    // setting, which is the same trap the Texture slot fell into.
    // Measured with the voice filter open against a Wavetable slot at the same Level (-22.0
    // dBFS) and corrected one colour at a time. Without this a violet slot sat eleven decibels
    // above a pink one at the same setting -- the trap the Texture slot fell into.
    static const float kGain[kNumNoiseKinds] = {
        0.85f,   // White
        1.29f,   // Pink
        1.74f,   // Brown
        3.19f,   // Blue
        0.99f,   // Violet
        0.86f,   // Grey
        3.20f,   // Band
        3.07f,   // Wind
        1.80f,   // Crackle
        0.64f,   // Digital
    };
    const float gain = kGain[static_cast<int>(kind)] * p.level;

    for (int c = 0; c < 2; ++c) {
        NoiseState& st = noise_[c];
        float* dst = (c == 0) ? outL : scratch_;
        for (int i = 0; i < n; ++i) {
            const float w = rng_.bipolar();
            float v = 0.0f;
            switch (kind) {
            case NoiseKind::White: v = w; break;
            case NoiseKind::Pink: {
                // Paul Kellet's economy filter: three poles, about 1 dB from a true 1/f slope.
                v = pinkStep(st, w);
                break;
            }
            case NoiseKind::Brown: {
                st.brown = clampv(st.brown + 0.02f * w, -1.0f, 1.0f);
                v = st.brown;
                break;
            }
            case NoiseKind::Blue: {   // differentiated pink: +3 dB per octave
                const float pink = pinkStep(st, w);
                v = pink - st.prev;
                st.prev = pink;
                break;
            }
            case NoiseKind::Violet:    // differentiated white: +6 dB per octave
                v = w - st.prev;
                st.prev = w;
                break;
            case NoiseKind::Grey: {
                // White with the ear's most sensitive region taken out, so it sounds flat rather
                // than measuring flat: a broad dip around 3 kHz through the same band pass.
                st.bp2 += f * st.bp1;
                const float hp = w - st.bp2 - 0.4f * st.bp1;
                st.bp1 += f * hp;
                v = w - 0.75f * st.bp1;
                break;
            }
            case NoiseKind::Band:
            case NoiseKind::Wind: {
                st.bp2 += f * st.bp1;
                const float hp = w - st.bp2 - damp * st.bp1;
                st.bp1 += f * hp;
                v = st.bp1;
                break;
            }
            case NoiseKind::Crackle: {
                // Sparse impulses, each a short decaying blip: vinyl, embers, rain on a roof.
                st.nextGrain -= 1.0;
                if (st.nextGrain <= 0.0) {
                    st.nextGrain = -std::log(1.0 - static_cast<double>(rng_.uniform()) + 1e-9) * sr_ / crackleRate;
                    st.crackle = rng_.bipolar();
                    st.crackleDecay = std::exp(-1.0f / (0.0015f * (1.0f + 6.0f * posN) * static_cast<float>(sr_)));
                }
                v = st.crackle;
                st.crackle *= st.crackleDecay;
                break;
            }
            case NoiseKind::Digital: {
                st.holdLeft -= rate;
                if (st.holdLeft <= 0.0) { st.holdLeft += sr_; st.hold = w; }
                v = st.hold;
                break;
            }
            default: break;
            }
            dst[i] = v;
        }
    }

    // Pan and level, ramped across the block like the other types.
    const float angle = (clampv(p.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    const float gl = gain * std::cos(angle), gr = gain * std::sin(angle);
    for (int i = 0; i < n; ++i) {
        const float l = outL[i], r = scratch_[i];
        outL[i] = l * gl;
        scratch_[i] = r * gr;
    }
}

} // namespace ambient
