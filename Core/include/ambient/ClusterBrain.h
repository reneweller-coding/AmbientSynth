// AmbientSynth -- Cluster Brain: the generative "sleep concert" conductor.
// Slowly starts and stops notes of the current scale around a wandering root,
// weighted by interval consonance. Emits note events; the engine turns them into
// voices with their long envelopes. Header-only (templated callbacks), no allocation.
#pragma once
#include "Dsp.h"
#include "Tuning.h"
#include <cmath>

namespace ambient {

// How the conductor works. Free is what it has always done: notes start and stop on their own
// timers, so the cluster breathes but never really moves. Chords keeps the cluster full and
// exchanges exactly one voice at a time, choosing the new note for how it sits against the ones
// that stay and for how far that voice has to travel -- which is voice leading, and it is what
// makes a drone shift into a new chord instead of merely churning.
enum class BrainMode : int { Free = 0, Chords, Count };
constexpr int kNumBrainModes = static_cast<int>(BrainMode::Count);
extern const char* const kBrainModeNames[kNumBrainModes];

// The spectrum the conductor judges intervals with when Timbre is up: the partial template of
// the voice as it stands -- the same tilt, odd/even weight, brightness window and inharmonic
// stretch the bank renders with -- as frequency ratios to the fundamental and amplitudes.
struct BrainSpectrum {
    static constexpr int kMax = 12;
    int    count = 0;
    double ratio[kMax] = {};    // f_h / f0, including the stiff-string stretch
    double amp[kMax] = {};
};

// Roughness of two tones with this template at f1 and f2, after Sethares (1993, 2005), which is
// Plomp and Levelt's curve for a pair of pure tones summed over every pair of partials:
//     d(x) = a1 a2 (exp(-b1 s x) - exp(-b2 s x)),  s = d* / (s1 min(f) + s2),
//     b1 = 3.5, b2 = 5.75, d* = 0.24, s1 = 0.021, s2 = 19
// so that the peak of the roughness sits at about a quarter of a critical bandwidth whatever the
// register. Pairs more than a few critical bandwidths apart contribute nothing and are skipped.
inline double spectralRoughness(double f1, double f2, const BrainSpectrum& sp)
{
    double d = 0.0;
    for (int i = 0; i < sp.count; ++i) {
        const double fa = f1 * sp.ratio[i];
        for (int j = 0; j < sp.count; ++j) {
            const double fb = f2 * sp.ratio[j];
            const double lo = fa < fb ? fa : fb;
            const double x = std::fabs(fb - fa);
            const double s = 0.24 / (0.021 * lo + 19.0);
            const double u = s * x;
            if (u > 2.0) continue;                       // exp(-3.5 * 2) is under a thousandth
            d += sp.amp[i] * sp.amp[j] * (std::exp(-3.5 * u) - std::exp(-5.75 * u));
        }
    }
    return d;
}

// The roughness turned into a consonance in 0..1 that sits on the same scale as
// intervalConsonance(): 1 for a tone against itself, about a tenth for a semitone. The semitone
// is the yardstick because it is the roughest interval a scale contains, and measuring against
// it rather than against a fixed number keeps the score meaningful for a soft timbre with few
// partials and for a bright one alike.
inline double spectralConsonance(double f1, double f2, const BrainSpectrum& sp)
{
    if (sp.count <= 0) return 1.0;
    const double d = spectralRoughness(f1, f2, sp) - spectralRoughness(f1, f1, sp);   // less the tone's own roughness
    const double ref = spectralRoughness(f1, f1 * 1.0594630943592953, sp) - spectralRoughness(f1, f1, sp);
    if (ref <= 1.0e-12) return 1.0;
    const double c = std::exp(-2.3 * (d > 0.0 ? d : 0.0) / ref);
    return c < 0.05 ? 0.05 : c;
}

struct BrainParams {
    bool  on = true;
    BrainMode mode = BrainMode::Free;
    float voiceLead = 7.0f;       // Chords: how far the exchanged voice may move, in semitones
    float chordTension = 0.0f;    // 0 = the new note must fit the ones that stay; 1 = anything goes
    float rootMove = 0.0f;        // Chords: how often the exchange moves the root as well
    int   density = 5;            // target number of simultaneous notes
    float rateSeconds = 25.0f;    // mean time between events
    float holdMin = 30.0f, holdMax = 120.0f;
    int   low = 36, high = 79;    // MIDI range for chosen notes
    float consonance = 0.7f;      // 0 = anything goes (clusters), 1 = strictly consonant
    float wander = 0.3f;          // probability weight for root movement
    // Timbre: how much of the consonance is judged from the actual spectrum (Sethares) rather
    // than from the ratio alone. 0 is the ratio score the conductor always had.
    float timbre = 0.0f;
    const BrainSpectrum* spectrum = nullptr;
    // The consonance of two frequencies, as this conductor currently hears it.
    double consonanceOf(double fa, double fb) const
    {
        const double byRatio = intervalConsonance(fa / fb);
        if (timbre <= 0.0f || spectrum == nullptr || spectrum->count <= 0) return byRatio;
        const double bySpectrum = spectralConsonance(fb, fa, *spectrum);
        return byRatio + (bySpectrum - byRatio) * static_cast<double>(timbre);
    }
};

struct BrainEvent {
    enum class Type { NoteOn, NoteOff };
    Type  type;
    int   note;
    float velocity;
};

class ClusterBrain {
public:
    static constexpr int kSlots = 12;

    void reset(uint64_t seed, int rootNote)
    {
        rng_.seed(seed);
        stepRequested_ = false;
        for (int& r : recent_) r = -1;
        recentHead_ = 0;
        for (auto& s : slots_) { s.note = -1; s.remaining = 0.0; }
        root_ = rootNote;
        timer_ = 1.0;
        wasOn_ = false;
    }

    int  root() const { return root_; }
    void setRoot(int note) { root_ = clampv(note, 0, 127); }
    int  activeCount() const { int c = 0; for (auto& s : slots_) if (s.note >= 0) ++c; return c; }
    bool sounding(int note) const { for (auto& s : slots_) if (s.note == note) return true; return false; }

    // Ask for one exchange at the next opportunity, whatever the timer says: the button on the
    // panel, a mapped controller, a footswitch. Read and cleared inside update().
    void requestStep() { stepRequested_ = true; }
    bool stepPending() const { return stepRequested_; }

    // Advance `dt` seconds. `freqOf(int note) -> double`, `emit(const BrainEvent&)`.
    // `anchorNote` (>= 0) pins the root to a note the player holds on the keyboard.
    template <class FreqFn, class EmitFn>
    void update(double dt, const BrainParams& p, int anchorNote, FreqFn&& freqOf, EmitFn&& emit)
    {
        if (!p.on) {
            if (wasOn_) {
                for (auto& s : slots_) if (s.note >= 0) { emit(BrainEvent{ BrainEvent::Type::NoteOff, s.note, 0.0f }); s.note = -1; }
                wasOn_ = false;
            }
            return;
        }
        if (!wasOn_) { wasOn_ = true; timer_ = 0.5; }
        if (anchorNote >= 0) root_ = anchorNote;
        if (p.mode == BrainMode::Chords) { updateChords(dt, p, freqOf, emit); return; }

        for (auto& s : slots_) {
            if (s.note < 0) continue;
            s.remaining -= dt;
            if (s.remaining <= 0.0) { emit(BrainEvent{ BrainEvent::Type::NoteOff, s.note, 0.0f }); s.note = -1; }
        }

        timer_ -= dt;
        if (timer_ > 0.0) return;
        const double mean = std::max(0.5, static_cast<double>(p.rateSeconds));
        timer_ = clampv(-std::log(1.0 - static_cast<double>(rng_.uniform()) + 1e-9) * mean, 0.5, mean * 4.0);

        const int low = std::min(p.low, p.high), high = std::max(p.low, p.high);
        const int density = clampv(p.density, 1, kSlots);

        if (activeCount() >= density) {
            // Room is full: half of the time retire the note that would end soonest, else wait.
            if (rng_.uniform() >= 0.5f) return;
            int best = -1; double rem = 1e12;
            for (int i = 0; i < kSlots; ++i) if (slots_[i].note >= 0 && slots_[i].remaining < rem) { rem = slots_[i].remaining; best = i; }
            if (best < 0) return;
            emit(BrainEvent{ BrainEvent::Type::NoteOff, slots_[best].note, 0.0f });
            slots_[best].note = -1;
        }

        if (anchorNote < 0 && rng_.uniform() < p.wander * 0.35f) wanderRoot(low, high, freqOf);

        // Weighted choice of the next note.
        const double rootFreq = freqOf(root_);
        bool rootSounding = false;
        for (auto& s : slots_) if (s.note >= 0 && pitchClassEqual(freqOf(s.note), rootFreq)) rootSounding = true;
        const double mid = 0.5 * (low + high), half = std::max(1.0, 0.5 * (high - low));
        float weights[128] = {};
        float total = 0.0f;
        for (int c = low; c <= high && c < 128; ++c) {
            if (sounding(c)) continue;
            const double fc = freqOf(c);
            bool duplicate = false;   // with snapped keys two keys can share one pitch
            for (auto& s : slots_) if (s.note >= 0 && std::fabs(std::log2(freqOf(s.note) / fc)) * 1200.0 < 1.0) duplicate = true;
            if (duplicate) continue;
            const double cons = p.consonanceOf(fc, rootFreq);
            float w = static_cast<float>(std::pow(cons, p.consonance * 3.0f));
            w *= 0.6f + 0.4f * static_cast<float>(1.0 - std::fabs(c - mid) / half);
            for (auto& s : slots_) if (s.note >= 0 && pitchClassEqual(freqOf(s.note), fc)) w *= 0.15f;  // octave doubling is rare
            if (pitchClassEqual(fc, rootFreq)) w *= rootSounding ? 0.25f : 3.0f;                       // keep a foundation
            weights[c] = w;
            total += w;
        }
        if (total <= 0.0f) return;
        float r = rng_.uniform() * total;
        int chosen = -1;
        for (int c = low; c <= high && c < 128; ++c) { r -= weights[c]; if (r <= 0.0f && weights[c] > 0.0f) { chosen = c; break; } }
        if (chosen < 0) for (int c = high; c >= low; --c) if (weights[c] > 0.0f) { chosen = c; break; }
        if (chosen < 0) return;

        for (auto& s : slots_) {
            if (s.note >= 0) continue;
            const float hmin = std::min(p.holdMin, p.holdMax), hmax = std::max(p.holdMin, p.holdMax);
            s.note = chosen;
            s.remaining = hmin + rng_.uniform() * (hmax - hmin);
            emit(BrainEvent{ BrainEvent::Type::NoteOn, chosen, 0.5f + 0.4f * rng_.uniform() });
            break;
        }
    }

private:
    struct Slot { int note = -1; double remaining = 0.0; };

    // ---------------------------------------------------------------- chords
    //
    // The cluster is kept at `density` notes. Every `rateSeconds` -- or the moment someone asks --
    // one voice is exchanged: the one that has been sounding longest goes, and the note that takes
    // its place is scored for three things at once. How well it sits against the voices that stay
    // (the mean consonance with each of them, which is what makes it a chord rather than a heap),
    // how far it has to travel from the note it replaces (near is better: that is voice leading,
    // and it is why the change is heard as a shift and not as a cut), and a little dislike of
    // doubling a pitch class that is already there. Tension loosens the first of the three, so a
    // progression can be made to lean without becoming random.
    template <class FreqFn, class EmitFn>
    void updateChords(double dt, const BrainParams& p, FreqFn&& freqOf, EmitFn&& emit)
    {
        const int low = std::min(p.low, p.high), high = std::max(p.low, p.high);
        const int density = clampv(p.density, 1, kSlots);
        for (auto& s : slots_) if (s.note >= 0) s.remaining += dt;   // in this mode it counts age, not time left

        // Fill an empty chord one note per tick, so the first bars are an entrance and not a chord.
        int sounding = activeCount();
        timer_ -= dt;
        const bool asked = stepRequested_;
        if (sounding < density) {
            if (timer_ > 0.0 && !asked && sounding > 0) return;
            stepRequested_ = false;
            timer_ = std::max(0.5, static_cast<double>(p.rateSeconds));
            const int add = chooseNote(-1, low, high, p, freqOf);
            if (add >= 0) startIn(add, emit);
            return;
        }
        if (timer_ > 0.0 && !asked) return;
        stepRequested_ = false;
        timer_ = std::max(0.5, static_cast<double>(p.rateSeconds));

        // The voice that has been sounding longest is the one that goes.
        int oldest = -1; double age = -1.0;
        for (int i = 0; i < kSlots; ++i) if (slots_[i].note >= 0 && slots_[i].remaining > age) { age = slots_[i].remaining; oldest = i; }
        if (oldest < 0) return;
        const int leaving = slots_[oldest].note;
        // The root may travel with the chord, which is what turns a voicing change into a
        // progression; without it the harmony circles one centre for ever.
        if (p.rootMove > 0.0f && rng_.uniform() < p.rootMove * 0.5f) wanderRoot(low, high, freqOf);
        slots_[oldest].note = -1;                                   // it must not vote on its own replacement
        const int arriving = chooseNote(leaving, low, high, p, freqOf);
        slots_[oldest].note = leaving;
        if (arriving < 0 || arriving == leaving) return;
        emit(BrainEvent{ BrainEvent::Type::NoteOff, leaving, 0.0f });
        recent_[recentHead_] = leaving;                             // it has had its turn
        recentHead_ = (recentHead_ + 1) % kRecent;
        slots_[oldest].note = arriving;
        slots_[oldest].remaining = 0.0;
        emit(BrainEvent{ BrainEvent::Type::NoteOn, arriving, 0.5f + 0.4f * rng_.uniform() });
    }

    template <class EmitFn>
    void startIn(int note, EmitFn&& emit)
    {
        for (auto& s : slots_) if (s.note < 0) {
            s.note = note; s.remaining = 0.0;
            emit(BrainEvent{ BrainEvent::Type::NoteOn, note, 0.5f + 0.4f * rng_.uniform() });
            return;
        }
    }

    // The best note to bring in, given the ones that stay. `from` is the note being replaced, or
    // -1 when the chord is still filling up.
    template <class FreqFn>
    int chooseNote(int from, int low, int high, const BrainParams& p, FreqFn&& freqOf) const
    {
        const double rootFreq = freqOf(root_);
        const float lead = std::max(p.voiceLead, 0.5f);
        int best = -1; double bestScore = -1e9;
        for (int c = low; c <= high && c < 128; ++c) {
            if (sounding(c)) continue;
            // The note being replaced cannot replace itself. Its slot is emptied before this runs
            // so that it does not vote on its own successor -- which also left it in the running,
            // and being at zero distance it always won: the chord never moved once until this
            // line existed. The self test found it, the descriptors never would have.
            if (from >= 0 && c == from) continue;
            const double fc = freqOf(c);
            bool duplicate = false;
            for (const auto& s : slots_) if (s.note >= 0 && std::fabs(std::log2(freqOf(s.note) / fc)) * 1200.0 < 1.0) duplicate = true;
            if (duplicate) continue;
            // How it sits against the voices that stay, and against the root.
            double fit = p.consonanceOf(fc, rootFreq);
            int n = 1;
            for (const auto& s : slots_) if (s.note >= 0) { fit += p.consonanceOf(fc, freqOf(s.note)); ++n; }
            fit /= n;
            double score = std::pow(fit, 1.0 + 2.5 * (1.0 - clampv(p.chordTension, 0.0f, 1.0f)));
            // Voice leading: the further this voice has to travel, the worse, and beyond the
            // allowance it is not considered at all.
            if (from >= 0) {
                const double steps = std::fabs(static_cast<double>(c - from));
                if (steps > lead) continue;
                score *= 1.0 - 0.75 * (steps / lead);
            }
            // What left recently is worth less. Without this the harmony keeps picking up the note
            // it has just put down -- it is the nearest candidate and it fitted a moment ago, so
            // it wins again -- and a run with little room to move (a pinned root and a narrow
            // allowance) circles a handful of chords for ever instead of going somewhere. The
            // penalty fades with age, so nothing is banned, only postponed.
            for (int i = 0; i < kRecent; ++i) if (recent_[i] == c) {
                const int age = (recentHead_ - 1 - i + 2 * kRecent) % kRecent;   // 0 = just left
                score *= 0.12 + 0.11 * static_cast<double>(age);
            }
            // An octave of something already sounding is a doubling, not a new colour.
            for (const auto& s : slots_) if (s.note >= 0 && pitchClassEqual(freqOf(s.note), fc)) score *= 0.2;
            if (pitchClassEqual(fc, rootFreq)) score *= 0.5;
            score *= 0.85 + 0.3 * rng_.uniform();          // a little life, so it is not a machine
            if (score > bestScore) { bestScore = score; best = c; }
        }
        return best;
    }

    static bool pitchClassEqual(double fa, double fb)
    {
        double r = fa / fb;
        while (r >= 2.0) r *= 0.5;
        while (r < 1.0) r *= 2.0;
        return std::fabs(std::log2(r)) * 1200.0 < 10.0 || std::fabs(std::log2(r) - 1.0) * 1200.0 < 10.0;
    }

    template <class FreqFn>
    void wanderRoot(int low, int high, FreqFn&& freqOf)
    {
        static const double kTargets[] = { 1.5, 4.0 / 3.0, 1.25, 1.2, 5.0 / 3.0, 1.6 };
        const double target = kTargets[rng_.below(6)];
        const double rootFreq = freqOf(root_);
        int best = root_; double bestScore = 1e9;
        for (int c = low; c <= high; ++c) {
            if (c == root_) continue;
            double r = freqOf(c) / rootFreq;
            while (r >= 2.0) r *= 0.5;
            while (r < 1.0) r *= 2.0;
            const double score = std::fabs(std::log2(r / target)) * 12.0 + std::fabs(c - root_) / 12.0;
            if (score < bestScore) { bestScore = score; best = c; }
        }
        if (bestScore < 0.5 + 3.0) root_ = best;
    }

    Slot   slots_[kSlots];
    mutable Rng rng_;
    double timer_ = 1.0;
    int    root_ = 48;
    bool   wasOn_ = false;
    bool   stepRequested_ = false;
    static constexpr int kRecent = 8;   // Chords: the notes that left, most recent first-ish
    int    recent_[kRecent] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    int    recentHead_ = 0;
};

} // namespace ambient
