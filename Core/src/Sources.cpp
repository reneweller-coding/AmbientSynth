#include "ambient/Sources.h"
#include "ambient/Simd.h"
#include "ambient/Voice.h"     // kControlBlock
#include "ambient/Cosmos.h"    // Fft for wavetable analysis
#include <cmath>
#include <cstring>
#include <algorithm>
#include <memory>      // the shared transforms of the Stretch type

namespace ambient {

const char* const kSourceTypeNames[kNumSourceTypes] = { "Off", "Wavetable", "FM", "Texture", "Noise", "Additive", "Stretch", "Bow", "Spectral" };
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

bool loopFromName(const char* fileName)
{
    if (fileName == nullptr) return false;
    const char* base = fileName;
    for (const char* p = fileName; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    // Lower-case copy without the extension, then look for the token.
    char buf[256];
    int n = 0;
    for (const char* p = base; *p && n < 255; ++p) buf[n++] = static_cast<char>((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p);
    buf[n] = 0;
    for (int i = n - 1; i > 0; --i) if (buf[i] == '.') { buf[i] = 0; break; }
    return std::strstr(buf, "_loop") != nullptr || std::strstr(buf, "-loop") != nullptr;
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

void Wavetable::spectrumAt(float pos, float* out, float transport) const
{
    if (frames <= 0) { std::memset(out, 0, sizeof(float) * kTablePartials); return; }
    if (frames == 1) { std::memcpy(out, amp[0], sizeof(float) * kTablePartials); return; }
    const float x = clampv(pos, 0.0f, 1.0f) * static_cast<float>(frames - 1);
    const int i = std::min(static_cast<int>(x), frames - 2);
    const float f = x - static_cast<float>(i);
    for (int h = 0; h < kTablePartials; ++h) out[h] = amp[i][h] + f * (amp[i + 1][h] - amp[i][h]);
    if (transport <= 0.0f || f <= 0.0f || f >= 1.0f) return;
    // Optimal transport along the partial axis. Mass is what a partial's amplitude is read as
    // (negative entries count as nothing); the two frames' distributions are normalised, walked
    // together by cumulative mass, and every slice put down at (1 - f) * from + f * to, split
    // between the two nearest partials when that lands between them. What comes out carries the
    // blended total, and the plain blend is faded into it by the amount.
    const float* A = amp[i];
    const float* B = amp[i + 1];
    float mA = 0.0f, mB = 0.0f;
    for (int h = 0; h < kTablePartials; ++h) { mA += std::max(A[h], 0.0f); mB += std::max(B[h], 0.0f); }
    if (mA <= 1.0e-9f || mB <= 1.0e-9f) return;
    float ot[kTablePartials] = {};
    const float total = (1.0f - f) * mA + f * mB;
    int ia = 0, ib = 0;
    float ra = std::max(A[0], 0.0f) / mA, rb = std::max(B[0], 0.0f) / mB;
    for (int guard = 0; guard < 4 * kTablePartials && ia < kTablePartials && ib < kTablePartials; ++guard) {
        if (ra <= 1.0e-7f) { if (++ia < kTablePartials) ra = std::max(A[ia], 0.0f) / mA; continue; }
        if (rb <= 1.0e-7f) { if (++ib < kTablePartials) rb = std::max(B[ib], 0.0f) / mB; continue; }
        const float m = std::min(ra, rb);
        const float xPos = (1.0f - f) * static_cast<float>(ia) + f * static_cast<float>(ib);
        const int x0 = std::min(static_cast<int>(xPos), kTablePartials - 1);
        const float fr = xPos - static_cast<float>(x0);
        ot[x0] += m * total * (1.0f - fr);
        if (x0 + 1 < kTablePartials) ot[x0 + 1] += m * total * fr;
        ra -= m; rb -= m;
    }
    const float t = clampv(transport, 0.0f, 1.0f);
    for (int h = 0; h < kTablePartials; ++h) out[h] += t * (ot[h] - out[h]);
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
    // Stretch: the buffers, sized once for the longest window; the shared transforms, built here
    // on the message thread so render() only ever reads them.
    st_.out.assign(static_cast<size_t>(2 * kStretchMaxN), 0.0f);
    st_.re.assign(static_cast<size_t>(kStretchMaxN), 0.0f);
    st_.im.assign(static_cast<size_t>(kStretchMaxN), 0.0f);
    st_.win.assign(static_cast<size_t>(kStretchMaxN), 0.0f);
    st_.n = 0; st_.outPos = 0; st_.hopLeft = 0; st_.advance = 0.0;
    for (int n = kStretchMinN; n <= kStretchMaxN; n <<= 1) (void)stretchFft(n);
    // Bow: the string itself, allocated here rather than carried in the object.
    bowNut_.assign(static_cast<size_t>(kBowMax), 0.0f);
    bowBridge_.assign(static_cast<size_t>(kBowMax), 0.0f);
    bowW_ = 0; bowLp_ = 0.0f; bowReady_ = false;
    specAdvance_ = 0.0;
}

const Fft& SourceSlot::stretchFft(int n)
{
    // One transform per size, shared by every slot of every voice: the tables are read-only once
    // built, and building them in prepare() keeps the allocation off the audio thread.
    static std::vector<std::unique_ptr<Fft>> table;
    static std::vector<int> sizes;
    for (size_t i = 0; i < sizes.size(); ++i) if (sizes[i] == n) return *table[i];
    table.push_back(std::make_unique<Fft>(n));
    sizes.push_back(n);
    return *table.back();
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
    st_.advance = 0.0;   // a fresh note reads from Position again
    specAdvance_ = 0.0;
    for (int b = 0; b < kTablePartials; ++b) {
        specA_[b] = specAStep_[b] = specN_[b] = specNStep_[b] = 0.0f;
        specBp_[b] = specLp_[b] = 0.0f;
    }
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
        if (!st_.out.empty()) std::fill(st_.out.begin(), st_.out.end(), 0.0f);
        st_.n = 0; st_.hopLeft = 0;
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
    else if (p.type == SourceType::Stretch) renderStretch(scratch_, n, hz, hz / std::max(noteHz, 1.0), p, texture, dt);
    else if (p.type == SourceType::Bow) renderBow(scratch_, n, hz, p, dt);
    else if (p.type == SourceType::Spectral)
        renderSpectral(scratch_, n, p.follow ? hz / std::max(1.0, texture != nullptr ? texture->baseHz : 1.0)
                                             : hz / std::max(noteHz, 1.0), p, texture, dt);
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
    if (table != nullptr) table->spectrumAt(p.position + wander, spec, p.transport);
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

    for (int i = 0; i < n; ++i)
        out[i] = phasorBankStep(pc_, ps_, rc_, rs_, amp_, ampStep_, active_);
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
    analyse();
}

// Measure the clip into the band model. Runs where measure() runs -- the loader thread, once per
// clip -- and never in render(): a six-second clip is under three hundred transforms.
void Texture::analyse()
{
    spectral = SpectralModel();
    const int total = static_cast<int>(mono.size());
    const int N = 4096;
    if (total < 2 * N) return;
    // The hop is a quarter of the window, stretched if it would take more frames than the model
    // holds, so a long recording is described more coarsely rather than cut short.
    constexpr int kMaxFrames = 4096;
    int hop = N / 4;
    if ((total - N) / hop + 1 > kMaxFrames) hop = (total - N) / (kMaxFrames - 1) + 1;
    const int frames = (total - N) / hop + 1;
    if (frames < 2) return;

    SpectralModel& m = spectral;
    m.frames = frames;
    m.hop = static_cast<float>(hop / sampleRate);
    m.amp.assign(static_cast<size_t>(frames) * SpectralModel::kBands, 0.0f);
    m.freq.assign(static_cast<size_t>(frames) * SpectralModel::kBands, 0.0f);
    m.tone.assign(static_cast<size_t>(frames) * SpectralModel::kBands, 0.0f);

    // The bands sit on the ear's own scale, one equal step of the ERB rate apart (Glasberg and
    // Moore 1990), so a band is about as wide as the ear's own resolution at that frequency: a
    // sixth of an octave down low, a whole octave up high. Thirty-two of them span 45 Hz to 16 kHz.
    const double top = std::min(16000.0, 0.45 * sampleRate);
    auto erbRate = [](double f) { return 21.4 * std::log10(0.00437 * f + 1.0); };
    auto erbInv  = [](double e) { return (std::pow(10.0, e / 21.4) - 1.0) / 0.00437; };
    const double e0 = erbRate(45.0), e1 = erbRate(top);
    double edge[SpectralModel::kBands + 1];
    for (int b = 0; b <= SpectralModel::kBands; ++b)
        edge[b] = erbInv(e0 + (e1 - e0) * b / SpectralModel::kBands);
    const double binHz = sampleRate / N;
    int lo[SpectralModel::kBands], hi[SpectralModel::kBands];
    for (int b = 0; b < SpectralModel::kBands; ++b) {
        lo[b] = std::max(1, static_cast<int>(edge[b] / binHz));
        hi[b] = std::max(lo[b] + 1, std::min(N / 2 - 1, static_cast<int>(edge[b + 1] / binHz)));
        m.centre[b] = static_cast<float>(std::sqrt(edge[b] * edge[b + 1]));
        m.bandQ[b] = static_cast<float>(clampv(m.centre[b] / std::max(1.0, edge[b + 1] - edge[b]), 0.7, 12.0));
    }

    const Fft fft(N);
    std::vector<float> re(static_cast<size_t>(N)), im(static_cast<size_t>(N));
    std::vector<float> win(static_cast<size_t>(N)), mag(static_cast<size_t>(N / 2 + 1));
    std::vector<double> logSum(static_cast<size_t>(N / 2 + 2));
    for (int i = 0; i < N; ++i)
        win[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(kTwoPi * static_cast<float>(i) / static_cast<float>(N));
    // A Hann window of this length puts a sinusoid's energy across about three bins and halves
    // its amplitude; both are taken out here so a band's stored amplitude is the amplitude of the
    // sound and not of the analysis.
    const float norm = 2.0f / static_cast<float>(N) * 2.0f;

    for (int f = 0; f < frames; ++f) {
        const int off = f * hop;
        for (int i = 0; i < N; ++i) { re[static_cast<size_t>(i)] = mono[static_cast<size_t>(off + i)] * win[static_cast<size_t>(i)]; im[static_cast<size_t>(i)] = 0.0f; }
        fft.transform(re.data(), im.data(), false);
        for (int k = 0; k <= N / 2; ++k)
            mag[static_cast<size_t>(k)] = std::sqrt(re[static_cast<size_t>(k)] * re[static_cast<size_t>(k)] + im[static_cast<size_t>(k)] * im[static_cast<size_t>(k)]) * norm;
        // The local noise floor: the geometric mean of the magnitudes over twenty-one bins either
        // side. A partial stands well above it; noise sits on it. This is what decides how tonal a
        // band is, and it needs no fundamental and no pitch tracker to say so.
        logSum[0] = 0.0;
        for (int k = 0; k <= N / 2; ++k)
            logSum[static_cast<size_t>(k + 1)] = logSum[static_cast<size_t>(k)] + std::log(static_cast<double>(mag[static_cast<size_t>(k)]) + 1e-12);
        for (int b = 0; b < SpectralModel::kBands; ++b) {
            double power = 0.0, toneSum = 0.0;
            float peak = 0.0f; int peakBin = lo[b];
            for (int k = lo[b]; k <= hi[b]; ++k) {
                const float v = mag[static_cast<size_t>(k)];
                power += static_cast<double>(v) * v;
                if (v > peak) { peak = v; peakBin = k; }
                const int a = std::max(0, k - 10), c = std::min(N / 2, k + 10);
                const double floorMag = std::exp((logSum[static_cast<size_t>(c + 1)] - logSum[static_cast<size_t>(a)]) / (c - a + 1));
                const double above = static_cast<double>(v) / (floorMag + 1e-12);
                toneSum += clampv((above - 1.2) / 3.0, 0.0, 1.0) * static_cast<double>(v) * v;
            }
            // Where in the band the strongest partial sits, to a fraction of a bin: three
            // magnitudes around the peak fit a parabola, which is the standard refinement and is
            // worth having, because a band is wide and a partial at its edge is not its centre.
            double refined = peakBin;
            if (peakBin > 0 && peakBin < N / 2) {
                const double a = std::log(static_cast<double>(mag[static_cast<size_t>(peakBin - 1)]) + 1e-12);
                const double c = std::log(static_cast<double>(mag[static_cast<size_t>(peakBin + 1)]) + 1e-12);
                const double bb = std::log(static_cast<double>(mag[static_cast<size_t>(peakBin)]) + 1e-12);
                const double den = a - 2.0 * bb + c;
                if (std::fabs(den) > 1e-9) refined += clampv(0.5 * (a - c) / den, -0.5, 0.5);
            }
            const size_t idx = static_cast<size_t>(f) * SpectralModel::kBands + static_cast<size_t>(b);
            m.amp[idx] = static_cast<float>(std::sqrt(power));
            m.freq[idx] = static_cast<float>(refined * binHz);
            m.tone[idx] = power > 1e-18 ? static_cast<float>(toneSum / power) : 0.0f;
        }
    }
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

// ---------------------------------------------------------------- stretch
//
// Paulstretch, in a voice. A window of the clip is transformed, its magnitudes are kept and its
// phases thrown away and drawn afresh, and the result is overlap-added at a quarter of the
// window -- the same machinery as the Cosmos's Nebula, pointed at a recording instead of at the
// mix. Every frame is a plausible piece of the clip's spectrum with no memory of where its
// transients were, so the analysis position can crawl through the recording at a thousandth of
// its speed and what comes out is a continuum: twenty seconds of rain becoming an evening of it.
//
// Pitch is applied when the window is READ, as a resampling step through the clip, and the
// stretch is applied to how far the read position moves between frames. The two do not know
// about each other, which is the point: Follow = Note plays a chromatic sample across the
// keyboard without a high note ending sooner than a low one.

void SourceSlot::stretchFrame(const SlotParams& p, const Texture* tex, double rate, int N)
{
    const float* s = tex->mono.data();
    const int len = static_cast<int>(tex->mono.size());
    if (st_.n != N) {   // a new window size: its Hann, and a clean start for the overlap
        st_.n = N;
        for (int i = 0; i < N; ++i) st_.win[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(kTwoPi * i / N);
    }
    // The seam. A clip marked seamless wraps straight round; any other loops over [zone, len)
    // and fades its last `zone` samples into its first `zone`, so the wrap lands on the sample
    // the fade has been arriving at. Half of Xfade of the clip, at most a quarter of it.
    const double zone = tex->seamless ? 0.0 : clampv(0.5 * static_cast<double>(p.xfade), 0.0, 0.25) * len;
    const double loopLen = static_cast<double>(len) - zone;
    auto wrap = [&](double q) {
        if (loopLen <= 1.0) return 0.0;
        while (q >= len) q -= loopLen;
        while (q < zone) q += loopLen;
        return q;
    };
    auto read = [&](double q) {
        q = wrap(q);
        const int ip = static_cast<int>(q);
        const float f = static_cast<float>(q - ip);
        const int ip1 = ip + 1 < len ? ip + 1 : ip;
        float v = s[ip] + f * (s[ip1] - s[ip]);
        if (zone > 0.0 && q > len - zone) {
            const double a = (q - (len - zone)) / zone;          // 0 at the start of the fade, 1 at the end
            double q2 = q - loopLen;                              // the matching point near the clip's start
            if (q2 < 0.0) q2 = 0.0;
            const int jp = static_cast<int>(q2);
            const float g = static_cast<float>(q2 - jp);
            const int jp1 = jp + 1 < len ? jp + 1 : jp;
            const float w = s[jp] + g * (s[jp1] - s[jp]);
            v = static_cast<float>((1.0 - a) * v + a * w);
        }
        return v;
    };
    // The window, read around the analysis position at the pitch's rate.
    const double centre = clampv(static_cast<double>(p.position), 0.0, 1.0) * loopLen + zone + st_.advance;
    const double start = centre - 0.5 * N * rate;
    for (int i = 0; i < N; ++i) {
        st_.re[static_cast<size_t>(i)] = read(start + i * rate) * st_.win[static_cast<size_t>(i)];
        st_.im[static_cast<size_t>(i)] = 0.0f;
    }
    const Fft& fft = stretchFft(N);
    fft.transform(st_.re.data(), st_.im.data(), false);
    // Keep the magnitudes, draw the phases: what makes it a continuum rather than a loop.
    for (int k = 0; k <= N / 2; ++k) {
        const float m = std::sqrt(st_.re[static_cast<size_t>(k)] * st_.re[static_cast<size_t>(k)] + st_.im[static_cast<size_t>(k)] * st_.im[static_cast<size_t>(k)]);
        const float ph = rng_.uniform();
        const float r = m * sin01(ph + 0.25 >= 1.0 ? ph - 0.75 : ph + 0.25), q = m * sin01(ph);
        st_.re[static_cast<size_t>(k)] = r; st_.im[static_cast<size_t>(k)] = q;
        if (k > 0 && k < N / 2) { st_.re[static_cast<size_t>(N - k)] = r; st_.im[static_cast<size_t>(N - k)] = -q; }
    }
    st_.im[0] = 0.0f; st_.im[static_cast<size_t>(N / 2)] = 0.0f;
    fft.transform(st_.re.data(), st_.im.data(), true);
    // Overlap-add at a quarter of the window. 1.3 is the Nebula's measured unity constant for
    // random-phase resynthesis at this hop with Hann in and out; the clip's own gain brings a
    // quiet recording to the level the other types normalise themselves to.
    const int outMask = 2 * kStretchMaxN - 1;
    const float g = 1.3f * tex->gain;
    for (int i = 0; i < N; ++i)
        st_.out[static_cast<size_t>((st_.outPos + i) & outMask)] += st_.re[static_cast<size_t>(i)] * st_.win[static_cast<size_t>(i)] * g;
    // The read moves on by a hop, slowed by the stretch. At 1000 that is four samples of clip per
    // frame of a third of a second.
    st_.advance += (0.25 * N) * rate / std::max(static_cast<double>(p.stretch), 1.0);
    if (loopLen > 1.0) while (st_.advance >= loopLen) st_.advance -= loopLen;
    st_.hopLeft += N / 4;
}

void SourceSlot::renderStretch(float* out, int n, double hz, double speed, const SlotParams& p, const Texture* tex, float dt)
{
    if (tex == nullptr || tex->empty() || st_.out.empty()) { std::memset(out, 0, sizeof(float) * static_cast<size_t>(n)); return; }
    // Position wanders the way it does for the grains, and the window comes from Grain.
    (void)posDrift_.update(dt, 0.02f, rng_);
    int N = kStretchMinN;
    while (N < static_cast<int>(p.grainMs * 0.001f * static_cast<float>(sr_)) && N < kStretchMaxN) N <<= 1;
    const double resample = tex->sampleRate / sr_;
    const double rate = (p.follow ? hz / std::max(tex->baseHz, 20.0) : speed) * resample;
    const int outMask = 2 * kStretchMaxN - 1;
    for (int i = 0; i < n; ++i) {
        while (st_.hopLeft <= 0) stretchFrame(p, tex, rate, N);
        const size_t o = static_cast<size_t>(st_.outPos & outMask);
        out[i] = st_.out[o];
        st_.out[o] = 0.0f;
        ++st_.outPos;
        --st_.hopLeft;
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


// ---------------------------------------------------------------- Bow
//
// A bowed string, after McIntyre, Schumacher and Woodhouse (1983) and the waveguide form of Smith
// (2010). The instrument had a struck source (Strike) but nothing continuously excited, and the
// two are not the same thing: a struck body rings and dies, a bowed one is driven for as long as
// the bow moves and settles into a stick-slip oscillation of its own -- which is exactly what a
// drone wants, a note that sustains because it is being fed rather than because its release is long.
//
// The string is two waveguides meeting at the bow -- one to the nut, one to the bridge -- so that
// the bow's position along the string decides which partials it favours, as it does on a real
// instrument. The nut reflects and inverts; the bridge reflects, inverts and loses the highs,
// which is what makes the upper partials die first. Every sample the friction between bow and string is evaluated:
//
//     dv   = v_bow - v_string           the relative velocity at the bow
//     rho  = min(1, (F / (|dv| + eps))^0.8)   the Stribeck curve: sticking while dv is small,
//                                             slipping once it is not
//     out  = dv * rho
//
// so a slow bow with a heavy hand sticks for most of the period and releases suddenly -- the
// Helmholtz motion -- and a fast bow with a light hand slips more and gives the thinner, airier
// tone. The friction force is fed back into the string, and the string's own motion changes the
// relative velocity on the next sample, which is the loop that makes it oscillate at all.
//
// It is guarded rather than trusted: the loop gain is below one by construction, the injected
// force is clamped, and a non-finite state resets the string. A physical model that runs away is
// a burst of full-scale noise, and this instrument's whole point is that it never does that.
// Playing a recording back from its band model rather than from its samples.
//
// Two numbers that a sampler has to share are separate here. `transpose` scales every band's
// frequency, so the note decides the pitch; Rate scales how fast the model is read, so it decides
// the speed. At Rate 0 the read head stands still and the clip becomes one endless chord -- a
// frozen moment of a recording, held at whatever pitch is played, which no amount of granular
// overlap can do without a texture of its own.
//
// Each band is rebuilt from two things: an oscillator at the band's strongest partial, weighted by
// how tonal the band measured, and a band of noise at the same place, weighted by the rest. Breath
// tilts that balance -- all the way to the partials on its own, or all the way to the wind.
void SourceSlot::renderSpectral(float* out, int n, double transpose, const SlotParams& p, const Texture* tex, float dt)
{
    if (tex == nullptr || tex->spectral.empty()) { active_ = 0; return; }
    const SpectralModel& m = tex->spectral;
    const int B = SpectralModel::kBands;

    // Where in the model to read: Position sets the point, Rate carries the head on from it, and
    // the drift wanders around both. All three in frames, wrapped, never stepped.
    const float wander = posDrift_.update(dt, 0.02f, rng_) * 0.15f * p.positionDrift;
    const double span = static_cast<double>(m.frames - 1);
    specAdvance_ += static_cast<double>(p.specRate) * (dt / std::max(1.0e-4f, m.hop));
    if (specAdvance_ > 1.0e9 || specAdvance_ < -1.0e9) specAdvance_ = 0.0;
    double fr = static_cast<double>(clampv(p.position + wander, 0.0f, 1.0f)) * span + specAdvance_;
    fr = std::fmod(fr, span);
    if (fr < 0.0) fr += span;
    const int i0 = clampv(static_cast<int>(fr), 0, m.frames - 1);
    const int i1 = (i0 + 1) % m.frames;
    const float frac = static_cast<float>(fr - i0);
    const float* a0 = m.amp.data() + static_cast<size_t>(i0) * B;
    const float* a1 = m.amp.data() + static_cast<size_t>(i1) * B;
    const float* f0 = m.freq.data() + static_cast<size_t>(i0) * B;
    const float* f1 = m.freq.data() + static_cast<size_t>(i1) * B;
    const float* t0 = m.tone.data() + static_cast<size_t>(i0) * B;
    const float* t1 = m.tone.data() + static_cast<size_t>(i1) * B;

    const float breath = clampv(p.specBreath, -1.0f, 1.0f);
    // Bright tilts the whole model around a kilohertz, the same gesture the additive types make.
    const float tilt = clampv(p.bright, 0.0f, 1.0f) - 0.5f;
    const float gain = tex->gain;
    const double nyq = 0.45 * sr_;
    const float invLen = 1.0f / static_cast<float>(n);
    const float invSr = static_cast<float>(1.0 / sr_);
    int act = 0;
    for (int b = 0; b < B; ++b) {
        const float amp = (a0[b] + frac * (a1[b] - a0[b])) * gain;
        double hzB = static_cast<double>(f0[b] + frac * (f1[b] - f0[b])) * transpose;
        const float tone = clampv(t0[b] + frac * (t1[b] - t0[b]) - breath, 0.0f, 1.0f);
        float g = amp;
        if (g > 0.0f) {
            const float oct = std::log2(static_cast<float>(std::max(20.0, hzB)) / 1000.0f);
            g *= std::pow(2.0f, tilt * oct);          // +-6 dB per octave at the ends of Bright
        }
        if (hzB >= nyq || hzB < 15.0 || !(g > 0.0f)) { g = 0.0f; hzB = 1000.0; }
        specAStep_[b] = (g * tone - specA_[b]) * invLen;
        specNStep_[b] = (g * (1.0f - tone) - specN_[b]) * invLen;
        // The oscillator's rotation, and the noise band's own filter, both at the same frequency.
        phasorFrom(hzB / sr_, rc_[b], rs_[b]);
        const float r2 = pc_[b] * pc_[b] + ps_[b] * ps_[b];
        const float fix = 1.5f - 0.5f * r2;           // keep the phasor on the unit circle
        pc_[b] *= fix; ps_[b] *= fix;
        specF_[b] = std::min(1.4f, static_cast<float>(kTwoPi * hzB) * invSr);
        specQ_[b] = 1.0f / m.bandQ[b];
        if (g > 1.0e-6f) act = b + 1;
    }
    active_ = act;

    // A state-variable band pass per band, all fed from one white noise: the bands overlap the way
    // the ear's own do, so one source is right -- six-and-thirty independent noises would be six
    // and thirty times the wind.
    for (int i = 0; i < n; ++i) {
        const float white = rng_.bipolar();
        float sum = 0.0f;
        for (int b = 0; b < B; ++b) {
            specA_[b] += specAStep_[b];
            specN_[b] += specNStep_[b];
            const float c = pc_[b] * rc_[b] - ps_[b] * rs_[b];
            const float s = pc_[b] * rs_[b] + ps_[b] * rc_[b];
            pc_[b] = c; ps_[b] = s;
            const float hp = white - specLp_[b] - specQ_[b] * specBp_[b];
            specBp_[b] += specF_[b] * hp;
            specLp_[b] += specF_[b] * specBp_[b];
            // The band pass peaks at 1/q, so the noise is divided by it: what comes out is the
            // band's own amplitude, not the filter's.
            sum += specA_[b] * s + specN_[b] * specBp_[b] * specQ_[b] * 1.6f;
        }
        out[i] += sum;
    }
    for (int b = 0; b < B; ++b) amp_[b] = specA_[b] + specN_[b];   // so the scope shows the bands
}

void SourceSlot::renderBow(float* out, int n, double hz, const SlotParams& p, float dt)
{
    (void)dt;
    const double f0 = hz > 20.0 ? hz : 20.0;
    const int len = static_cast<int>(sr_ / f0);
    if (len < 4 || len >= kBowMax) { std::memset(out, 0, sizeof(float) * static_cast<size_t>(n)); return; }
    if (!bowReady_) {
        std::fill(bowNut_.begin(), bowNut_.end(), 0.0f);
        std::fill(bowBridge_.begin(), bowBridge_.end(), 0.0f);
        bowW_ = 0; bowLp_ = 0.0f; bowReady_ = true;
        // A breath of noise to start it: a string exactly at rest is a fixed point, and the
        // friction curve alone would never leave it.
        for (int i = 0; i < kBowMax; ++i) { bowNut_[i] = 0.002f * rng_.bipolar(); bowBridge_[i] = 0.002f * rng_.bipolar(); }
    }
    // The friction characteristic is the whole model. It has to FALL as the slipping gets
    // faster -- more slip, less force -- because that negative resistance is what feeds the
    // oscillation; a curve that merely saturates has a stable fixed point, and the string sticks
    // to the bow and stays there. The first version of this did exactly that, and measured as a
    // constant with no pitch at all. The shape below is the one the waveguide literature uses
    // (Smith 2010; the bow table of the Synthesis ToolKit): a steep inverse power of the
    // relative velocity, whose width is set by how hard the bow presses.
    const float slope = 5.0f - 4.0f * clampv(p.bowForce, 0.0f, 1.0f);   // heavier hand, wider stick
    const float speed = clampv(p.bowSpeed, 0.0f, 1.0f);
    const float vBow = 0.6f * speed;                                    // a bow at rest bows nothing
    // A bow that has stopped is still lying on the string, and hair that does not move absorbs:
    // stop bowing and the note dies in a tenth of a second, it does not ring on like a plucked
    // string. Without this the junction is a lossless termination and the string keeps its energy
    // for seconds, which is what the test measured. The loss is confined to the bottom sixth of
    // the Speed range, so nothing that is being played is touched by it.
    const float rest = std::max(0.0f, 1.0f - 6.0f * speed);
    const float contact = 1.0f - 0.02f * rest * clampv(p.bowForce, 0.0f, 1.0f);
    // Where the bow sits, as a fraction of the string, kept away from the ends.
    const float rel = 0.06f + 0.34f * clampv(p.position, 0.0f, 1.0f);
    // The string is two waveguides that meet at the bow: from the bow to the nut and back is 2a
    // samples, to the bridge and back 2b, and the two together are one period.
    const int a = std::max(1, static_cast<int>(rel * len * 0.5f));
    const int b = std::max(1, len / 2 - a);
    // The bridge's reflection loses the highs, which is what makes a string's upper partials die
    // first. Bright is the slot's own brightness knob, used here for the same thing.
    const float damp = 0.55f - 0.45f * clampv(p.bright, 0.0f, 1.0f);
    for (int i = 0; i < n; ++i) {
        // What arrives at the bow from each side, having been reflected at its end: the nut
        // inverts, the bridge inverts and damps.
        const float vl = -bowNut_[(bowW_ - 2 * a + kBowMax) & (kBowMax - 1)];
        bowLp_ += (1.0f - damp) * (bowBridge_[(bowW_ - 2 * b + kBowMax) & (kBowMax - 1)] - bowLp_);
        const float vr = -0.995f * bowLp_;
        const float dv = vBow - (vl + vr);
        float rho = std::pow(std::fabs((dv - 0.001f) * slope) + 0.75f, -4.0f);
        if (rho > 1.0f) rho = 1.0f;
        float f = dv * rho;
        if (f > 1.0f) f = 1.0f; else if (f < -1.0f) f = -1.0f;
        float toNut = (vr + f) * contact, toBridge = (vl + f) * contact;
        if (!(toNut > -8.0f && toNut < 8.0f) || !(toBridge > -8.0f && toBridge < 8.0f)) {
            std::fill(bowNut_.begin(), bowNut_.end(), 0.0f);
            std::fill(bowBridge_.begin(), bowBridge_.end(), 0.0f);
            bowLp_ = 0.0f; toNut = toBridge = 0.0f;
        }
        bowNut_[bowW_ & (kBowMax - 1)] = toNut;
        bowBridge_[bowW_ & (kBowMax - 1)] = toBridge;
        ++bowW_;
        // What the bridge radiates. The factor is measured, not guessed: with the same note, the
        // same Level and everything after the sources switched off, a wavetable slot renders at
        // an RMS of 0.103, and this brings the string to within a decibel of it. The selftest
        // measures both and fails if they drift more than six decibels apart.
        out[i] += vr * 0.78f;
    }
}

} // namespace ambient
