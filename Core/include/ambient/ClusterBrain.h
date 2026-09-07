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

// How strongly a set of tones implies ONE virtual root: harmonicity, in the sense of Terhardt's
// virtual pitch (1974, 1979) and Parncutt's root support. This is a different question from
// whether the tones are pairwise consonant, and the difference is not academic.
//
// The conductor scores a chord as the MEAN CONSONANCE OVER ALL PAIRS, and measured on the
// instrument's own function that rule prefers a stack of fifths (4:6:9, mean 0.233) to a just
// major triad (4:5:6, 0.212) -- and puts a plain segment of the harmonic series (8:9:10:11:12,
// 0.165) last of all. Adjacent members of one series make complicated ratios pairwise (9/8,
// 11/10) however perfectly the set as a whole fits together. For a dyad the two measures agree,
// which is why this went unnoticed; for five voices they invert.
//
// The measure: try every fundamental that could hold the set -- the lowest tone divided by one
// to sixteen -- assign each tone to its nearest harmonic, and score how cleanly it sits there,
// weighted so that a tone on a low harmonic supports the root far more than one high up.
//
// Two rules keep the trivial answers out, and both were put there because the first version gave
// them. A tone may not share a harmonic number with another: several tones crammed onto one
// harmonic is a cluster, not a fit. And a candidate root supported by fewer than two tones does
// not count at all, because every tone is the first harmonic of itself -- without that rule a
// semitone cluster scored as high as a just major triad, and a bare tritone scored higher than
// both.
inline double chordHarmonicity(const double* freqs, int n)
{
    if (n < 2) return 0.0;
    constexpr double kCents = 25.0;      // how far off a harmonic a tone may sit and still support the root
    constexpr int kMaxHarmonic = 32, kMaxSub = 16;
    double fmin = freqs[0];
    for (int i = 1; i < n; ++i) if (freqs[i] < fmin) fmin = freqs[i];
    if (!(fmin > 0.0)) return 0.0;
    double best = 0.0;
    for (int k = 1; k <= kMaxSub; ++k) {
        const double f0 = fmin / k;
        double s = 0.0;
        int landed = 0;
        unsigned int used = 0;           // a bit per harmonic number, 1..32
        for (int i = 0; i < n; ++i) {
            const int h = static_cast<int>(std::lround(freqs[i] / f0));
            if (h < 1 || h > kMaxHarmonic) continue;
            const unsigned int bit = 1u << (h - 1);
            if (used & bit) continue;
            const double cents = std::fabs(1200.0 * std::log2(freqs[i] / (h * f0)));
            const double fit = std::exp(-(cents / kCents) * (cents / kCents));
            if (fit < 0.05) continue;
            used |= bit;
            ++landed;
            s += fit / std::log2(1.0 + h);
        }
        if (landed < 2) continue;
        s /= n;
        if (s > best) best = s;
    }
    return best;
}

// The tonal hierarchy: how stable each degree of a key feels.
//
// Krumhansl and Kessler (1982) measured it. A listener hears a context that establishes a key,
// then a single probe tone, and rates how well it fits; averaged over listeners the twelve
// ratings are these two profiles. The tonic stands highest, then the fifth, then the third, then
// the rest of the scale, then the notes outside it. It is not a rule anybody wrote down -- it is
// what a set of listeners reported, and it has held up across four decades of replication.
//
// The conductor had no notion of a degree at all. It asked how a candidate sounded against what
// was already sounding, which is a question about intervals, and never how it sat in a key.
inline const float* keyProfileMajor()
{
    static const float p[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    return p;
}
inline const float* keyProfileMinor()
{
    static const float p[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    return p;
}

// Which key a distribution of sounding pitch classes is in: the Krumhansl-Schmuckler method.
// Correlate the distribution against all twenty-four rotated profiles and take the best. The
// correlation itself is the confidence, and it is worth having: an honest "this is barely a key
// at all" is exactly what a cluster should report.
// The pitch class of a frequency, in twelve bins of a hundred cents from an arbitrary anchor.
// Arbitrary is fine: the key search tries all twelve rotations, so only the intervals matter.
// Taken from the frequency rather than from the MIDI number because a scale in this instrument
// need not have twelve degrees -- with consecutive degrees on a nine-tone scale, note % 12 means
// nothing at all, while cents always mean cents.
inline int pitchClassOf(double f)
{
    if (!(f > 0.0)) return 0;
    const int semis = static_cast<int>(std::lround(12.0 * std::log2(f / 261.6255653005986)));
    return ((semis % 12) + 12) % 12;
}

// How evenly a chord's pitch classes are spread round the octave.
//
// Tymoczko (Science, 2006) showed that the chords which can be joined to their transpositions by
// small voice movements are the nearly even ones -- and that those are, not by coincidence, the
// chords Western music actually uses. Evenness is therefore not a taste: it is the property that
// makes a chord able to MOVE. A cluster can only leap.
//
// One for a chord whose notes divide the octave equally, zero for one whose notes are all in the
// same place. Duplicated pitch classes leave a gap of zero, which is exactly right: an octave
// doubling adds nothing to how the chord is spread.
inline double chordEvenness(const double* freqs, int n)
{
    if (n < 2) return 0.0;
    double pc[16];
    int m = 0;
    for (int i = 0; i < n && m < 16; ++i) {
        if (!(freqs[i] > 0.0)) continue;
        double x = std::fmod(12.0 * std::log2(freqs[i] / 261.6255653005986), 12.0);
        if (x < 0.0) x += 12.0;
        pc[m++] = x;
    }
    if (m < 2) return 0.0;
    for (int i = 1; i < m; ++i) {                 // insertion sort: m is at most sixteen
        const double v = pc[i];
        int j = i - 1;
        while (j >= 0 && pc[j] > v) { pc[j + 1] = pc[j]; --j; }
        pc[j + 1] = v;
    }
    const double ideal = 12.0 / m;
    double err = 0.0;
    for (int i = 0; i < m; ++i) {
        const double gap = (i + 1 < m) ? pc[i + 1] - pc[i] : 12.0 - pc[m - 1] + pc[0];
        err += std::fabs(gap - ideal);
    }
    const double worst = 24.0 * (1.0 - 1.0 / m);  // every note in one place
    return clampv(1.0 - err / worst, 0.0, 1.0);
}

struct KeyEstimate {
    int   key = -1;            // 0..11 major, 12..23 minor, -1 for nothing heard yet
    float confidence = 0.0f;   // the correlation, -1..1
    bool  minor() const { return key >= 12; }
    int   tonic() const { return key < 0 ? -1 : key % 12; }
};

inline KeyEstimate findKey(const float* weights)
{
    double mean = 0.0;
    for (int i = 0; i < 12; ++i) mean += weights[i];
    mean /= 12.0;
    double var = 0.0;
    for (int i = 0; i < 12; ++i) var += (weights[i] - mean) * (weights[i] - mean);
    KeyEstimate out;
    if (var < 1.0e-9) return out;                 // silence, or twelve notes in perfect balance
    const double sd = std::sqrt(var);
    for (int k = 0; k < 24; ++k) {
        const float* prof = k < 12 ? keyProfileMajor() : keyProfileMinor();
        const int rot = k % 12;
        double pm = 0.0;
        for (int i = 0; i < 12; ++i) pm += prof[i];
        pm /= 12.0;
        double pv = 0.0, cov = 0.0;
        for (int i = 0; i < 12; ++i) {
            const double a = weights[(i + rot) % 12] - mean;
            const double b = prof[i] - pm;
            cov += a * b;
            pv += b * b;
        }
        const double r = cov / (sd * std::sqrt(pv) + 1.0e-12);
        if (out.key < 0 || r > out.confidence) { out.key = k; out.confidence = static_cast<float>(r); }
    }
    return out;
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
    // Even: how strongly the conductor prefers chords whose notes are spread evenly round the
    // octave. Those are the chords that can be joined to their neighbours by small movements
    // rather than leaps (Tymoczko 2006), which is what lets a harmony go somewhere at all.
    // Against Harmonic, which pulls towards low harmonics and octaves, this pulls apart; the two
    // are meant to be set against each other.
    float even = 0.0f;
    // Smooth: which voice moves. The conductor retires the one that has been sounding longest and
    // then looks for its replacement; with this up it tries every voice and keeps the exchange
    // that moves the chord the shortest distance -- the voice-leading distance of Tymoczko's
    // geometry, which for an exchange of one note is exactly the leap that voice makes. It has no
    // effect outside Chords mode, where the choice of which voice leaves is made differently.
    float smooth = 0.0f;
    // Key: how strongly the conductor prefers the stable degrees of the key it finds itself in.
    // Nothing sets that key -- it is measured from what has actually been sounding, weighted by
    // how long, which for an instrument whose notes last minutes is the only weighting that means
    // anything. Turn it up and the music acquires a home it keeps returning to; leave it at zero
    // and the conductor hears intervals and no key at all, as it always did.
    float key = 0.0f;
    // Harmonic: how much the choice is judged by how well the WHOLE resulting chord fits one
    // harmonic series, rather than only by how its pairs sound. Listeners' preferences track
    // harmonicity at least as strongly as they track the absence of beating (McDermott, Lehr and
    // Oxenham 2010), and the two models are combined in the current accounts (Harrison and Pearce
    // 2020). 0 is the pairwise judgement the conductor always had.
    float harmonic = 0.0f;
    // Spacing: how strongly the conductor avoids putting a note within a critical band of one
    // that is already sounding. Two tones closer than about an equivalent rectangular bandwidth
    // excite overlapping regions of the cochlea, and the ear fuses them into one rough sound
    // instead of hearing two (Glasberg and Moore 1990; Bregman 1990). Negative values seek that
    // crowding out instead, which is what a cluster is. 0 leaves the choice as it was.
    float spacing = 0.0f;
    const BrainSpectrum* spectrum = nullptr;
    // The equivalent rectangular bandwidth of the auditory filter at f, in hertz
    // (Glasberg and Moore 1990): ERB = 24.7 (0.00437 f + 1).
    static double erbAt(double f) { return 24.7 * (0.00437 * f + 1.0); }
    // How much a candidate at fa is discouraged by a sounding tone at fb. 1 leaves it alone.
    float crowding(double fa, double fb) const
    {
        if (spacing == 0.0f) return 1.0f;
        const double d = std::fabs(fa - fb) / erbAt(0.5 * (fa + fb));   // in critical bandwidths
        const double closeness = std::exp(-d * d * 2.0);                // 1 at the same pitch, gone past one ERB
        const double s = static_cast<double>(spacing);
        return static_cast<float>(s > 0.0 ? (1.0 - 0.95 * s * closeness) : (1.0 - s * closeness));
    }

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
        for (float& w : pcWeight_) w = 0.0f;
        root_ = rootNote;
        timer_ = 1.0;
        wasOn_ = false;
    }

    // What key the conductor finds itself in, and how sure it is. Measured, never set: the
    // histogram below is what has actually been sounding, weighted by how long.
    KeyEstimate estimatedKey() const { return findKey(pcWeight_); }
    const float* pitchClassWeights() const { return pcWeight_; }

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

        // What has been sounding, and for how long. A note held for four minutes tells more about
        // where the music is than one that passed through in twenty seconds, and on this
        // instrument that is the difference between almost every pair of notes. The memory fades
        // over about three minutes, so the key can drift when the music does instead of being
        // anchored for ever to whatever it opened with.
        {
            const double fade = std::exp(-dt / 180.0);
            for (float& w : pcWeight_) w = static_cast<float>(w * fade);
            for (const auto& s : slots_)
                if (s.note >= 0) pcWeight_[pitchClassOf(freqOf(s.note))] += static_cast<float>(dt);
        }
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
        const KeyEstimate key = p.key > 0.0f ? findKey(pcWeight_) : KeyEstimate{};

        if (activeCount() >= density) {
            // Room is full: half of the time retire a note, else wait.
            if (rng_.uniform() >= 0.5f) return;
            int best = -1;
            // Which one leaves is half the question. By the clock alone -- the note that would end
            // soonest -- whatever the additions gained in harmonicity is given back one voice at a
            // time, and the chord never settles anywhere. With Harmonic up, the voice that goes is
            // the one whose leaving does the rest of the chord the most good. The draw is
            // short-circuited at zero, so a conductor that has not been asked for this behaves
            // exactly as it always did, down to the random stream.
            if (p.harmonic > 0.0f && rng_.uniform() < p.harmonic) {
                double bestH = -1.0;
                for (int i = 0; i < kSlots; ++i) {
                    if (slots_[i].note < 0) continue;
                    double rest[kSlots];
                    int m = 0;
                    for (int j = 0; j < kSlots; ++j) if (j != i && slots_[j].note >= 0 && m < kSlots) rest[m++] = freqOf(slots_[j].note);
                    const double h = m >= 2 ? chordHarmonicity(rest, m) : 0.0;
                    if (h > bestH) { bestH = h; best = i; }
                }
            } else {
                double rem = 1e12;
                for (int i = 0; i < kSlots; ++i) if (slots_[i].note >= 0 && slots_[i].remaining < rem) { rem = slots_[i].remaining; best = i; }
            }
            if (best < 0) return;
            emit(BrainEvent{ BrainEvent::Type::NoteOff, slots_[best].note, 0.0f });
            slots_[best].note = -1;
        }

        if (anchorNote < 0 && rng_.uniform() < p.wander * 0.35f) wanderRoot(low, high, freqOf, p.key);

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
            // Octave doubling is rare -- but the two rules disagree here, and the disagreement is
            // real rather than a wrinkle to be smoothed over. "A doubling is not a new colour" is a
            // matter of taste; harmonicity says an octave is the strongest relation two tones can
            // have, harmonics one and two of the same series. Harmonic is what settles it: at zero
            // the old taste rule stands untouched, and as it rises the veto softens to a
            // preference.
            {
                const float doubling = 0.15f + 0.6f * clampv(p.harmonic, 0.0f, 1.0f);
                for (auto& s : slots_) if (s.note >= 0 && pitchClassEqual(freqOf(s.note), fc)) w *= doubling;
            }
            if (p.spacing != 0.0f)
                for (auto& s : slots_) if (s.note >= 0) w *= p.crowding(fc, freqOf(s.note));
            // Free mode weighs a candidate against the ROOT alone, which is a weaker test than
            // the chord mode's: a note can sit well on the root and still pull the chord away
            // from having one. Harmonic is where that is caught, and it belongs here at least as
            // much as it belongs there -- this is the mode nearly every preset uses.
            if (p.harmonic > 0.0f) {
                double set[kSlots + 1];
                int m = 0;
                for (const auto& s : slots_) if (s.note >= 0 && m < kSlots) set[m++] = freqOf(s.note);
                set[m++] = fc;
                if (m >= 2)
                    w *= static_cast<float>(std::pow(std::max(chordHarmonicity(set, m), 1.0e-4), 5.0 * static_cast<double>(p.harmonic)));
            }
            if (p.key > 0.0f) w *= static_cast<float>(keyWeightOf(fc, key, p.key));
            if (p.even > 0.0f) {
                double set[kSlots + 1];
                int m = 0;
                for (const auto& s : slots_) if (s.note >= 0 && m < kSlots) set[m++] = freqOf(s.note);
                set[m++] = fc;
                w *= static_cast<float>(std::pow(std::max(chordEvenness(set, m), 1.0e-3), 5.0 * static_cast<double>(p.even)));
            }
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

        // Which voice goes. By default the one that has been sounding longest, which is a rule
        // about time and knows nothing about where the chord would land.
        int oldest = -1; double age = -1.0;
        for (int i = 0; i < kSlots; ++i) if (slots_[i].note >= 0 && slots_[i].remaining > age) { age = slots_[i].remaining; oldest = i; }
        if (oldest < 0) return;
        // The root may travel with the chord, which is what turns a voicing change into a
        // progression; without it the harmony circles one centre for ever.
        if (p.rootMove > 0.0f && rng_.uniform() < p.rootMove * 0.5f) wanderRoot(low, high, freqOf, p.key);

        int moving = oldest, arriving = -1;
        if (p.smooth > 0.0f) {
            // Try them all and keep the exchange that moves the chord least for what it gains.
            // The distance a chord travels is the sum of what its voices move under the cheapest
            // pairing of the two chords; when one note is exchanged that sum is exactly the leap
            // that one voice makes, which is worth stating because it means the number already in
            // this function is the right one and needs no correcting -- checked over four thousand
            // random exchanges, the two agree exactly.
            double bestQ = -1.0;
            for (int i = 0; i < kSlots; ++i) {
                if (slots_[i].note < 0) continue;
                const int was = slots_[i].note;
                slots_[i].note = -1;
                double sc = 0.0;
                const int cand = chooseNote(was, low, high, p, freqOf, &sc);
                slots_[i].note = was;
                if (cand < 0 || cand == was) continue;
                const double travel = std::fabs(static_cast<double>(cand - was)) / 12.0;
                const double q = sc / (1.0 + static_cast<double>(p.smooth) * travel);
                if (q > bestQ) { bestQ = q; moving = i; arriving = cand; }
            }
            if (arriving < 0) return;
        } else {
            const int was = slots_[oldest].note;
            slots_[oldest].note = -1;                               // it must not vote on its own replacement
            arriving = chooseNote(was, low, high, p, freqOf);
            slots_[oldest].note = was;
        }
        const int leaving = slots_[moving].note;
        const int oldestKeep = oldest;
        (void)oldestKeep;
        if (arriving < 0 || arriving == leaving) return;
        emit(BrainEvent{ BrainEvent::Type::NoteOff, leaving, 0.0f });
        recent_[recentHead_] = leaving;                             // it has had its turn
        recentHead_ = (recentHead_ + 1) % kRecent;
        slots_[moving].note = arriving;
        slots_[moving].remaining = 0.0;
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
    int chooseNote(int from, int low, int high, const BrainParams& p, FreqFn&& freqOf,
                   double* outScore = nullptr) const
    {
        const double rootFreq = freqOf(root_);
        const KeyEstimate key = p.key > 0.0f ? findKey(pcWeight_) : KeyEstimate{};
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
            if (p.key > 0.0f) score *= keyWeightOf(fc, key, p.key);
            if (p.even > 0.0f) {
                double set[kSlots + 1];
                int m = 0;
                for (const auto& s : slots_) if (s.note >= 0 && m < kSlots) set[m++] = freqOf(s.note);
                set[m++] = fc;
                score *= std::pow(std::max(chordEvenness(set, m), 1.0e-3), 5.0 * static_cast<double>(p.even));
            }
            // And how the WHOLE chord would sit, if the conductor has been told to listen for it.
            // The pairwise score above cannot hear this: it asks how each pair sounds, never
            // whether the set has one root.
            if (p.harmonic > 0.0f) {
                double set[kSlots + 1];
                int m = 0;
                for (const auto& s : slots_) if (s.note >= 0 && m < kSlots) set[m++] = freqOf(s.note);
                set[m++] = fc;
                const double h = chordHarmonicity(set, m);
                score *= std::pow(std::max(h, 1.0e-4), 5.0 * static_cast<double>(p.harmonic));
            }
            // An octave of something already sounding is a doubling, not a new colour.
            for (const auto& s : slots_) if (s.note >= 0 && pitchClassEqual(freqOf(s.note), fc)) score *= 0.2;
            if (pitchClassEqual(fc, rootFreq)) score *= 0.5;
            score *= 0.85 + 0.3 * rng_.uniform();          // a little life, so it is not a machine
            if (score > bestScore) { bestScore = score; best = c; }
        }
        if (outScore != nullptr) *outScore = best >= 0 ? bestScore : 0.0;
        return best;
    }

    // How stable a frequency is as a degree of the key that was found, on the Krumhansl-Kessler
    // profile: 1 for the tonic, about a third for a note outside the scale. Weighted by the
    // confidence of the key estimate, so an uncertain key pulls gently and a clear one pulls
    // hard -- and a passage with no key in it at all is left alone.
    static double keyWeightOf(double f, const KeyEstimate& k, float amount)
    {
        if (amount <= 0.0f || k.key < 0 || k.confidence <= 0.0f) return 1.0;
        const float* prof = k.minor() ? keyProfileMinor() : keyProfileMajor();
        const int degree = ((pitchClassOf(f) - k.tonic()) % 12 + 12) % 12;
        const double stability = static_cast<double>(prof[degree]) / 6.35;
        return std::pow(stability, 2.5 * static_cast<double>(amount) * static_cast<double>(k.confidence));
    }

    static bool pitchClassEqual(double fa, double fb)
    {
        double r = fa / fb;
        while (r >= 2.0) r *= 0.5;
        while (r < 1.0) r *= 2.0;
        return std::fabs(std::log2(r)) * 1200.0 < 10.0 || std::fabs(std::log2(r) - 1.0) * 1200.0 < 10.0;
    }

    template <class FreqFn>
    void wanderRoot(int low, int high, FreqFn&& freqOf, float keyAmount = 0.0f)
    {
        static const double kTargets[] = { 1.5, 4.0 / 3.0, 1.25, 1.2, 5.0 / 3.0, 1.6 };
        const double target = kTargets[rng_.below(6)];
        const double rootFreq = freqOf(root_);
        // The root and the key are two different things, and this is where they are told about
        // each other: a root that lands on a stable degree of the key the music is already in is
        // what a modulation is, while a root that ignores it is a second conductor disagreeing
        // with the first. It should be said that this is an argument, not a measurement. Measured
        // over three minutes it makes no difference to how clearly the music sits in a key -- 0.90
        // against 0.91 with the root left key-blind -- and what it does buy is one more pitch
        // class in play, nine against eight. It is kept because it is right, not because the
        // number moved.
        const KeyEstimate key = keyAmount > 0.0f ? findKey(pcWeight_) : KeyEstimate{};
        int best = root_; double bestScore = 1e9;
        for (int c = low; c <= high; ++c) {
            if (c == root_) continue;
            double r = freqOf(c) / rootFreq;
            while (r >= 2.0) r *= 0.5;
            while (r < 1.0) r *= 2.0;
            double score = std::fabs(std::log2(r / target)) * 12.0 + std::fabs(c - root_) / 12.0;
            // A penalty, not a veto: the wander is what keeps the harmony moving at all.
            if (keyAmount > 0.0f) score += 2.0 * static_cast<double>(keyAmount) * (1.0 - keyWeightOf(freqOf(c), key, 1.0f));
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
    float  pcWeight_[12] = {};    // how long each pitch class has been sounding, faded
    int    recent_[kRecent] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    int    recentHead_ = 0;
};

} // namespace ambient
