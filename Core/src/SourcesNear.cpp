// AmbientSynth -- the near sources (13.09.2026): what the instrument plays close to the ear.
//
// Every source the instrument had was a plane -- a bank, a table, a recording, a string, all of
// them made to be sustained and to be sent back into the far reverb. Rich's foreground is
// something else: a flute blown once, water falling into a bowl, a voice on a radio, a rim rubbed
// until it sings. Four models, each a physical caricature small enough to run in every voice:
//
//   Flute   a blown pipe after the jet-drive waveguide of Cook (STK), with the loop closed the
//           way an open pipe closes it -- the jet's travel time sets the register, so the same
//           pipe overblows to its octave when the embouchure shortens the jet.
//   Murmur  a voice that never says anything: a glottal pulse through three formants that walk
//           between vowels at a syllable's pace, consonants as bursts of noise, phrases and pauses,
//           and behind it a radio -- a band, a saturation, the hiss of the carrier, the squelch
//           that closes after every transmission, the Quindar tones Apollo keyed its air with.
//   Bowl    a set of modes rubbed by a stick, the bow's own friction curve (Sources.cpp) driving
//           a modal body instead of a string. Every mode is a doublet a hair apart, which is
//           where a real bowl's beating comes from (P1: the instrument wants beats).
//   Ice     the same friction on a low, dense, short-lived set of modes, and a slip clock in
//           front of it: ice or old wood creaking under a slow load, not struck, stretched.
//   Drops   water falling into a vessel, after van den Doel (2005): a drop is a bubble, a sine
//           whose pitch RISES as it decays, and the vessel it falls into rings after the click.
//
// Each is calibrated against the wavetable slot at the same Level, as the Bow is, and the selftest
// holds them to it.
#include "ambient/Sources.h"
#include "ambient/Voice.h"     // kControlBlock
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ambient {

namespace {

// A one-pole delay's share of a loop, in samples at the fundamental: (1 - g) / g.
inline double onePoleDelay(float g) { return (1.0 - static_cast<double>(g)) / std::max(static_cast<double>(g), 1.0e-3); }

// The bow's friction, as one function (Sources.cpp keeps its own copy inside the string loop):
// the Stribeck curve of the waveguide literature, sticking while the relative velocity is small,
// slipping once it is not.
inline float friction(float dv, float slope)
{
    float rho = std::pow(std::fabs((dv - 0.001f) * slope) + 0.75f, -4.0f);
    if (rho > 1.0f) rho = 1.0f;
    return clampv(dv * rho, -1.0f, 1.0f);
}

// The vowels the murmur walks between: F1, F2, F3 in hertz (Peterson and Barney's men, rounded),
// and a schwa in the middle where every unstressed syllable goes.
constexpr float kVowels[6][3] = {
    { 730.0f, 1090.0f, 2440.0f },   // a
    { 530.0f, 1840.0f, 2480.0f },   // e
    { 270.0f, 2290.0f, 3010.0f },   // i
    { 570.0f,  840.0f, 2410.0f },   // o
    { 300.0f,  870.0f, 2240.0f },   // u
    { 500.0f, 1500.0f, 2500.0f },   // schwa
};

// The modes of the two rubbed bodies, as ratios to the lowest, with each mode's weight at the
// rim and how long it rings against the lowest. A thin-walled bowl's (n,0) modes go roughly as
// (n^2 - 1), which is the 1 : 2.7 : 5 : 7.8 ladder every singing bowl has; ice and old wood are
// plate-like, dense and inharmonic, and their modes die in a second.
struct RubBody { float ratio[SourceSlot::kRubModes]; float weight[SourceSlot::kRubModes]; float ring[SourceSlot::kRubModes]; float t60; };
constexpr RubBody kBowlBody = { { 1.0f, 2.71f, 4.98f, 7.78f, 11.0f, 14.6f }, { 1.0f, 0.55f, 0.32f, 0.18f, 0.10f, 0.06f }, { 1.0f, 0.7f, 0.5f, 0.35f, 0.25f, 0.18f }, 14.0f };
constexpr RubBody kIceBody  = { { 1.0f, 1.58f, 2.24f, 3.02f, 3.98f, 5.1f },  { 1.0f, 0.8f, 0.6f, 0.45f, 0.3f, 0.2f },     { 1.0f, 0.8f, 0.6f, 0.45f, 0.35f, 0.25f }, 1.4f };

} // namespace

// ---------------------------------------------------------------- Flute
//
// The pipe is a loop of one period: a delay for the round trip, a one-pole at the far end for the
// losses (radiation and the walls take the highs first), and the air jet at the embouchure. The
// jet is deflected by the acoustic velocity at the hole, travels to the edge in a time of its own
// -- the jet delay -- and there it is switched into or out of the pipe by a soft cubic (Cook's
// jet table, x - x^3). The sign is what makes it a flute: the jet works AGAINST the wave that
// deflected it, and with a travel time of half a period that inversion arrives back in phase, so
// the pipe speaks its fundamental. Shorten the travel to a quarter period and the octave is in
// phase instead: that is overblowing, and Position is the embouchure that does it. Breath
// pressure (Force) sets how hard the jet is driven, and the loop's small-signal gain with it,
// so a light breath does not speak at all and a heavy one saturates towards the cubic's limit;
// Speed is the air's own noise, the turbulence a real jet always carries, part of it into the
// pipe (where it is filtered into breathiness) and part straight out.
//
// The loop's length is the period less what the filter and the feedback sample already delay,
// as the bow's is, and the selftest measures the pitch across the register against the
// instrument's own tuning.
void SourceSlot::renderFlute(float* out, int n, double hz, const SlotParams& p, float dt)
{
    (void)dt;
    // A flute has a bottom: asked for a note below it, it plays the note an octave up, as a
    // player would. (Measured: at 110 Hz the loop, with the jet's low pass down at 165 Hz, no
    // longer found its fundamental at all.)
    double f0 = hz > 25.0 ? hz : 25.0;
    while (f0 < 180.0) f0 *= 2.0;
    const double period = sr_ / f0;
    if (period < 6.0 || period >= static_cast<double>(kBowMax - 4)) { std::memset(out, 0, sizeof(float) * static_cast<size_t>(n)); return; }
    if (!fluteReady_) {
        std::fill(bowNut_.begin(), bowNut_.end(), 0.0f);      // the bore
        std::fill(bowBridge_.begin(), bowBridge_.end(), 0.0f); // the jet's travel
        bowW_ = 0; fluteRefl_ = 0.0f; fluteBreath_ = 0.0f; fluteHpX_ = fluteHpY_ = 0.0f; fluteJetLp_ = 0.0f;
        fluteNoiseBp1_ = fluteNoiseBp2_ = 0.0f;
        flutePhase_ = rng_.uniform();
        fluteVibHz_ = 4.4 + 1.4 * static_cast<double>(rng_.uniform());   // a player's vibrato, one per note
        fluteReady_ = true;
    }
    const float force = clampv(p.bowForce, 0.0f, 1.0f);
    const float air   = clampv(p.bowSpeed, 0.0f, 1.0f);
    // The breath rises over a quarter of a second, and the air comes before the tone: below the
    // pressure at which the jet's gain beats the pipe's losses only the turbulence is heard, the
    // pipe filtering it into breath, and the note grows out of that once the breath is there.
    // Rich's flutes begin exactly so. (A one-pole per SAMPLE: the first version took the block's
    // seconds for a per-sample coefficient and was at full pressure in four milliseconds.)
    const float pressure = 0.22f + 0.78f * force;
    const float rise = 1.0f - std::exp(-1.0f / (0.1f * static_cast<float>(sr_)));
    // Losses: Bright is the pipe's end filter, as it is the string's loop filter; and the pipe
    // loses seven per cent of its wave per round trip whatever Bright says -- a flute's Q is
    // about thirty. That loss is what the jet's saturation is balanced against: with a nearly
    // lossless pipe (0.985) and the same jet the amplitude had no limit at all, grew to the
    // guard, reset, and the pitch that was measured was the pitch of a loop restarting itself.
    const float lpCoef = 0.35f + 0.6f * clampv(p.bright, 0.0f, 1.0f);
    const float loopGain = 0.93f;
    // The jet's travel as a fraction of the period: half a period speaks the fundamental, a quarter
    // the octave. Position is the embouchure: the lower half of the knob the one, the upper half
    // the other. Not a slide between them -- a jet standing anywhere between pulls the pitch
    // (simulated: +5 cents at 0.475, +17 at 0.4), which is what a player's embouchure does and
    // what a source that is asked for a note must not.
    const double jetRatio = p.position < 0.5f ? 0.5 : 0.25;
    // The jet is a slow thing: its deflection cannot follow the pipe's upper modes, and without
    // that the loop at A3 chose its twelfth over its fundamental (both are in phase for a jet at
    // half a period). A one-pole at one and a half times the register's frequency on what the jet
    // reads; its own phase delay at that frequency is taken off the jet's line, so the register
    // stays exactly where it was.
    const double freg = 0.5 * f0 / jetRatio;
    const double fc = 1.5 * freg;
    const float  gj = 1.0f - std::exp(static_cast<float>(-kTwoPi * fc / sr_));
    // The one-pole's phase delay at the register's frequency, in samples, for the DIGITAL
    // one-pole and not its continuous cousin: the two differ by four tenths of a sample at C6,
    // which the loop answered with a constant two cents sharp across the whole register.
    const double wr = kTwoPi * freg / sr_;
    const double jetLpDelay = std::atan2((1.0 - gj) * std::sin(wr), 1.0 - (1.0 - gj) * std::cos(wr)) / wr;
    // Vibrato: Pos Drift is its depth, a third of a percent of pitch at 1 -- a breath, not a wobble.
    const double vibDepth = 0.0035 * static_cast<double>(clampv(p.positionDrift, 0.0f, 1.0f));
    const double vibInc = fluteVibHz_ / sr_;
    // The noise the air carries: a band around 3 kHz (a Chamberlin band pass, well below its limit).
    const float nf = 2.0f * std::sin(static_cast<float>(kPi * std::min(3000.0, 0.15 * sr_) / sr_));
    const float hpCoef = 1.0f - static_cast<float>(kTwoPi * 20.0 / sr_);
    for (int i = 0; i < n; ++i) {
        fluteBreath_ += (pressure - fluteBreath_) * rise;
        flutePhase_ += vibInc; if (flutePhase_ >= 1.0) flutePhase_ -= 1.0;
        const double vib = 1.0 + vibDepth * static_cast<double>(sin01(flutePhase_));
        // The bore's line plus the end filter's own delay is one period: what is written at the
        // embouchure at time n is read back, filtered, exactly a period later. The jet line holds
        // the FILTERED wave, so the filter's delay is already inside what it reads, and its line
        // is the jet's share of the period and nothing less. Measured: a spare sample in the bore
        // line made C6 +54 cents; the filter's delay subtracted a second time from the jet made it
        // +18; with both right the register is within a few cents.
        const double lpD   = onePoleDelay(lpCoef);
        const double dBore = std::max(2.0, period / vib - lpD);
        const double dJet  = std::max(1.0, period / vib * jetRatio - jetLpDelay);
        const int bI = static_cast<int>(dBore), jI = static_cast<int>(dJet);
        const float bF = static_cast<float>(dBore - bI), jF = static_cast<float>(dJet - jI);
        // What returns from the far end of the pipe, filtered and inverted at the open end.
        const float b0 = bowNut_[(bowW_ - bI + kBowMax) & (kBowMax - 1)];
        const float b1 = bowNut_[(bowW_ - bI - 1 + kBowMax) & (kBowMax - 1)];
        fluteRefl_ += lpCoef * ((b0 + bF * (b1 - b0)) - fluteRefl_);
        const float v = fluteRefl_ * loopGain;
        // The air's noise, coloured, part of it into the jet.
        const float w = rng_.bipolar();
        fluteNoiseBp2_ += nf * fluteNoiseBp1_;
        const float hp = w - fluteNoiseBp2_ - 0.5f * fluteNoiseBp1_;
        fluteNoiseBp1_ += nf * hp;
        const float turb = fluteNoiseBp1_ * (0.02f + 0.12f * air) * fluteBreath_;
        // The jet: deflected by the velocity at the hole, delayed by its travel, then switched
        // by the cubic, and working against what deflected it.
        const float j0 = bowBridge_[(bowW_ - jI + kBowMax) & (kBowMax - 1)];
        const float j1 = bowBridge_[(bowW_ - jI - 1 + kBowMax) & (kBowMax - 1)];
        // The jet: a thin thing fully deflected by a small velocity (the gain of three into the
        // cubic) whose saturated push is small against the wave in the pipe (a tenth) -- so the
        // loop's small-signal gain through it is well above the pipe's losses and the note
        // speaks, and its saturated gain is what the losses balance, at an amplitude of about a
        // half. Simulated before it was built: within two cents across the register, no reset.
        fluteJetLp_ += gj * ((j0 + jF * (j1 - j0)) - fluteJetLp_);
        float x = 3.0f * fluteJetLp_ + turb;
        if (x > 1.0f) x = 1.0f; else if (x < -1.0f) x = -1.0f;
        const float jet = -0.1f * (x - x * x * x * (1.0f / 3.0f)) * fluteBreath_;
        float in = v + jet;
        if (!(in > -4.0f && in < 4.0f)) {   // guarded, not trusted: a pipe that runs away is reset
            std::fill(bowNut_.begin(), bowNut_.end(), 0.0f);
            std::fill(bowBridge_.begin(), bowBridge_.end(), 0.0f);
            fluteRefl_ = 0.0f; in = 0.0f;
        }
        bowNut_[bowW_ & (kBowMax - 1)] = in;
        bowBridge_[bowW_ & (kBowMax - 1)] = v;
        ++bowW_;
        // What leaves the pipe: the wave at the open end, with the breath's own noise beside it,
        // through a DC blocker (the cubic is odd, but the breath's rise is not).
        const float raw = in * 0.55f + fluteNoiseBp1_ * 0.05f * air * fluteBreath_;
        const float y = raw - fluteHpX_ + hpCoef * fluteHpY_;
        fluteHpX_ = raw; fluteHpY_ = y;
        out[i] += y;
    }
}

// ---------------------------------------------------------------- Bowl and Ice
//
// A modal body under a stick. Each mode is a two-pole resonator on the mode's velocity, driven
// by the friction between the stick and the sum of the modes at the contact point -- the bow's
// curve (friction() above), with the bow's Force as the pressing and its Speed as the rubbing.
// A slow stick with a heavy hand sticks and slips once per period of the lowest mode, which is
// the Helmholtz motion of a rubbed rim, and because the modes' decay is long the tone takes
// seconds to build, as a bowl does. Every mode is two resonators a hair apart, split by Pos
// Drift: the doublet an asymmetric bowl always has, and its beating -- a real bowl warbles,
// and so does this one. Position is where the stick sits, from the rim (every mode) to the
// belly (the lowest alone). Below a twentieth of Speed the stick is lifted and the body only
// rings, so a note that lets go of the stick decays on its own physics.
//
// Ice is the same body with low, dense, short modes and a slip clock in front of the friction:
// under a slow load the stick does not glide, it creeps -- holds, gives, holds -- and every
// give is a pulse of stick velocity into the modes. The rate of the creeping follows Speed.
void SourceSlot::renderRub(float* out, int n, double hz, const SlotParams& p, float dt, bool ice)
{
    (void)dt;
    const RubBody& body = ice ? kIceBody : kBowlBody;
    const double f0 = clampv(hz, 20.0, 0.2 * sr_);
    if (!rubReady_) {
        std::memset(rubY1_, 0, sizeof(rubY1_)); std::memset(rubY2_, 0, sizeof(rubY2_));
        rubSlipLeft_ = 0.0; rubSlipOn_ = 0.0; rubF1_ = rubF2_ = 0.0f;
        rubContact_ = static_cast<double>(rng_.uniform());
        rubReady_ = true;
    }
    const float force = clampv(p.bowForce, 0.0f, 1.0f);
    const float speed = clampv(p.bowSpeed, 0.0f, 1.0f);
    const float slope = 5.0f - 4.0f * force;
    const bool lifted = speed < 0.05f;
    const float vStick = 0.6f * speed;
    const float split = 0.0008f + 0.012f * clampv(p.positionDrift, 0.0f, 1.0f);   // the doublet's width
    const float bright = clampv(p.bright, 0.0f, 1.0f);
    const float contact = clampv(p.position, 0.0f, 1.0f);
    // Per mode: the two poles' angle and radius, and the mode's weight at the contact point.
    float a1[kRubModes][2], a2[kRubModes][2], w[kRubModes], drive[kRubModes];
    int used = 0;
    for (int m = 0; m < kRubModes; ++m) {
        const double fm = f0 * static_cast<double>(body.ratio[m]);
        if (fm >= 0.45 * sr_) break;
        // How long this mode rings: the body's own T60, scaled by Bright for the upper modes.
        const float t60 = body.t60 * (m == 0 ? 1.0f : body.ring[m] * (0.3f + 1.4f * bright));
        const float r = std::exp(-6.908f / (t60 * static_cast<float>(sr_)));   // -60 dB after t60
        for (int d = 0; d < 2; ++d) {
            const double fd = fm * (1.0 + (d == 0 ? -0.5 : 0.5) * static_cast<double>(split) * (m + 1));
            const float th = static_cast<float>(kTwoPi * std::min(fd, 0.45 * sr_) / sr_);
            a1[m][d] = 2.0f * r * std::cos(th);
            a2[m][d] = -r * r;
        }
        // The stick at the rim touches every mode, at the belly mostly the lowest.
        w[m] = body.weight[m] * (m == 0 ? 1.0f : (1.0f - 0.85f * contact));
        // The mode is a VELOCITY resonator: two poles and a zero at zero frequency (the numerator
        // 1 - z^-2), so that at its own frequency the velocity is in phase with the force, as a
        // mass on a spring has it at resonance. The plain two-pole was tried first and measured
        // as silence: its response at resonance stands a quarter turn behind the force, so the
        // friction's negative slope only detuned the mode instead of feeding it. With the phase
        // right, the slope (about -0.2 at the stick's speed) times the mode's gain -- 1/(1-r),
        // scaled here to sixty -- is a loop that grows, and the friction's own saturation is what
        // stops it, at the amplitude where the body's swing reaches the stick's speed. The ice,
        // with its short modes, needs a tenth of the gain.
        drive[m] = (1.0f - r) * 60.0f;
        used = m + 1;
    }
    // The stick travels round the rim as it rubs -- a turn every few seconds at the rubbing
    // speed -- and the two halves of every doublet stand at right angles on the rim, so the
    // stick drives first the one and then the other. Driven together and held still, the two
    // lock to one frequency under the friction's nonlinearity and the beating is gone
    // (measured: an envelope flat to the third decimal); driven in turn they keep their own
    // pitches, and the bowl warbles the way a rubbed bowl does.
    const double turn = (0.04 + 0.15 * static_cast<double>(speed)) / sr_;   // revolutions per sample
    // The creep: pulses of stick velocity, 4 to 40 a second with Speed, each ten to forty
    // milliseconds -- long enough for the lowest mode to swing a few times in each.
    const double slipRate = 4.0 + 36.0 * static_cast<double>(speed);
    for (int i = 0; i < n; ++i) {
        if (!lifted) { rubContact_ += turn; if (rubContact_ >= 1.0) rubContact_ -= 1.0; }
        // The doublet's two halves under the stick: |cos| and |sin| of the contact angle, so the
        // two weights' squares always sum to one and the drive's power does not dip between them
        // (with 1 +- cos it fell to a half at forty-five degrees, an eighteen-decibel wah).
        const float wa = std::fabs(sin01(rubContact_ + 0.25)), wb = std::fabs(sin01(rubContact_));
        float vBody = 0.0f;
        for (int m = 0; m < used; ++m) vBody += w[m] * (wa * rubY1_[m][0] + wb * rubY1_[m][1]);
        float f = 0.0f;
        if (!lifted) {
            float vs = vStick;
            if (ice) {
                rubSlipLeft_ -= 1.0;
                if (rubSlipLeft_ <= 0.0) {
                    rubSlipLeft_ = -std::log(1.0 - static_cast<double>(rng_.uniform()) + 1e-9) * sr_ / slipRate;
                    rubSlipOn_ = (0.01 + 0.03 * static_cast<double>(rng_.uniform())) * sr_;
                }
                if (rubSlipOn_ > 0.0) { rubSlipOn_ -= 1.0; vs = vStick * 3.0f; } else vs = vStick * 0.15f;
            }
            f = friction(vs - vBody, slope) * (0.3f + 0.7f * force);
        }
        const float fd = f - rubF2_;          // the numerator's 1 - z^-2, shared by every mode
        rubF2_ = rubF1_; rubF1_ = f;
        float y = 0.0f;
        for (int m = 0; m < used; ++m) {
            const float in = fd * w[m] * drive[m];
            for (int d = 0; d < 2; ++d) {
                const float v = in * (d == 0 ? wa : wb) + a1[m][d] * rubY1_[m][d] + a2[m][d] * rubY2_[m][d];
                rubY2_[m][d] = rubY1_[m][d]; rubY1_[m][d] = v;
                y += v * body.weight[m];
            }
        }
        if (!(y > -50.0f && y < 50.0f)) {   // a body that runs away is a body reset
            std::memset(rubY1_, 0, sizeof(rubY1_)); std::memset(rubY2_, 0, sizeof(rubY2_));
            y = 0.0f;
        }
        // Calibrated at a middling Force and Speed against the table (the amplitude of a rubbed
        // body follows both knobs over twenty decibels, measured), and softly held from above,
        // so the heavy-and-fast corner is loud rather than a wall.
        const float o = y * (ice ? 12.0f : 5.0f);
        out[i] += o / (1.0f + 1.2f * std::fabs(o));
    }
}

// ---------------------------------------------------------------- Murmur
//
// Speech without words. The source is a glottal pulse train at the slot's pitch -- an impulse
// per period through two one-poles, which is the -12 dB per octave of a glottal flow -- with a
// jitter per period so it is a voice and not an oscillator, and a pitch that moves the way
// speech moves: it falls over a phrase (declination) and rises and falls a little on every
// syllable. Three formant filters in parallel walk between the vowels of the table above at a
// syllable's pace (Speed: three to six a second), some syllables are fricatives (noise through
// the formants and above them), some begin with a stop (a gap, then a burst), and the syllables
// come in phrases of a few seconds with pauses between -- so it is heard as someone talking, and
// never as what they say.
//
// Position is the medium. At 0 the voice is in the room, close; towards 1 it goes through a
// radio: a band from 300 to 3000 Hz, a saturation, the hiss of the carrier under every
// transmission, the burst of squelch noise that closes the channel after each phrase, and from
// 0.7 up the Quindar tones -- 2525 Hz to key the transmitter, 2475 Hz to release it, a quarter of
// a second each, which is the sound every Apollo air-to-ground loop opened and closed with.
// Force is effort: level, openness (the first formant rises), and the tilt of the pulse.
void SourceSlot::renderMurmur(float* out, int n, double hz, const SlotParams& p, float dt)
{
    const float sr = static_cast<float>(sr_);
    if (!murReady_) {
        murPhase_ = 0.0; murSylLeft_ = 0.0; murPauseLeft_ = 0.3 * sr_; murInPhrase_ = false; murSyllables_ = 0;
        murBeepLeft_ = 0.0; murSquelchLeft_ = 0.0; murBeepPhase_ = 0.0;
        for (int k = 0; k < 3; ++k) { murF_[k] = murTo_[k] = kVowels[5][k]; murForm_[k].reset(); }
        murGlot1_ = murGlot2_ = 0.0f; murPitchSt_ = murPitchTo_ = 0.0f; murDecl_ = 0.0f;
        murVoice_ = murVoiceTo_ = 1.0f; murGap_ = murGapTo_ = 1.0f; murJitter_ = 1.0f;
        murHpX_ = murHpY_ = 0.0f; murHiss_ = 0.0f;
        murRadioHp_.reset(); murRadioLp_.reset(); murFric_.reset();
        murRadioHp_.setQ(300.0f, 0.7f, sr);
        murRadioLp_.setQ(std::min(3000.0f, 0.4f * sr), 0.7f, sr);
        murFric_.setQ(std::min(4500.0f, 0.4f * sr), 1.2f, sr);
        murReady_ = true;
    }
    const float effort = clampv(p.bowForce, 0.0f, 1.0f);
    const float rate   = 2.5f + 4.0f * clampv(p.bowSpeed, 0.0f, 1.0f);     // syllables a second
    const float medium = clampv(p.position, 0.0f, 1.0f);
    const float range  = 1.0f + 4.0f * clampv(p.positionDrift, 0.0f, 1.0f); // semitones of intonation
    const float clarity = clampv(p.bright, 0.0f, 1.0f);
    // The syllable and phrase machine, stepped once per block: it moves at a few hertz.
    const double block = static_cast<double>(n);
    if (murInPhrase_) {
        murSylLeft_ -= block;
        if (murSylLeft_ <= 0.0) {
            if (murSyllables_ <= 0) {
                // The phrase ends: a pause, and the radio closes its channel.
                murInPhrase_ = false;
                murPauseLeft_ = (0.5 + 2.2 * static_cast<double>(rng_.uniform())) * sr_ * (4.0 / rate);
                murVoiceTo_ = 0.0f; murGapTo_ = 0.0f;
                if (medium > 0.15f) murSquelchLeft_ = (0.04 + 0.05 * static_cast<double>(rng_.uniform())) * sr_;
                if (medium > 0.7f) { murBeepLeft_ = 0.25 * sr_; murBeepHz_ = 2475.0f; murBeepPhase_ = 0.0; }
            } else {
                // The next syllable: a vowel (the schwa when unstressed), voiced or a fricative,
                // sometimes after a stop; its length, its accent.
                --murSyllables_;
                const bool stressed = rng_.uniform() < 0.35f;
                const int vowel = stressed ? rng_.below(5) : (rng_.uniform() < 0.6f ? 5 : rng_.below(5));
                for (int k = 0; k < 3; ++k) murTo_[k] = kVowels[vowel][k];
                murTo_[0] *= 1.0f + 0.35f * effort;                     // effort opens the mouth
                const bool fric = rng_.uniform() < 0.22f;
                murVoiceTo_ = fric ? 0.0f : (stressed ? 1.0f : 0.7f);
                murGapTo_ = 1.0f;
                if (rng_.uniform() < 0.18f) murGap_ = 0.0f;            // a stop: silence, then the burst
                murSylLeft_ = (0.6 + 0.8 * static_cast<double>(rng_.uniform())) * sr_ / rate;
                murPitchTo_ = (stressed ? 1.0f : -0.3f) * range * (0.5f + 0.5f * rng_.uniform()) - murDecl_;
                murDecl_ += 0.35f * range / 8.0f;                       // the phrase falls as it goes
                murJitter_ = 1.0f;
            }
        }
    } else {
        murPauseLeft_ -= block;
        if (murPauseLeft_ <= 0.0) {
            murInPhrase_ = true;
            murSyllables_ = 3 + rng_.below(9);
            murSylLeft_ = 0.0;
            murDecl_ = 0.0f;
            if (medium > 0.7f) { murBeepLeft_ = 0.25 * sr_; murBeepHz_ = 2525.0f; murBeepPhase_ = 0.0; }
        }
    }
    // Glides: formants in sixty milliseconds, voicing in twenty, pitch in eighty, the stop's gap in ten.
    const float cF = 1.0f - std::exp(-dt / 0.06f), cV = 1.0f - std::exp(-dt / 0.02f);
    const float cP = 1.0f - std::exp(-dt / 0.08f), cG = 1.0f - std::exp(-dt / 0.01f);
    for (int k = 0; k < 3; ++k) {
        murF_[k] += (murTo_[k] - murF_[k]) * cF;
        murForm_[k].setQ(clampv(murF_[k], 100.0f, 0.4f * sr), k == 0 ? 9.0f : 11.0f, sr);
    }
    murVoice_ += (murVoiceTo_ - murVoice_) * cV;
    murPitchSt_ += (murPitchTo_ - murPitchSt_) * cP;
    murGap_ += (murGapTo_ - murGap_) * cG;
    const double f0 = clampv(hz, 40.0, 1000.0) * std::pow(2.0, static_cast<double>(murPitchSt_) / 12.0);
    const double inc = f0 / sr_;
    // The glottal pulse's two poles: effort and Bright open the tilt (a brighter, harder voice).
    const float tilt = 1.0f - std::exp(-kTwoPi * (900.0f + 2500.0f * (0.5f * effort + 0.5f * clarity)) / sr);
    const float hpCoef = 1.0f - static_cast<float>(kTwoPi * 40.0 / sr_);
    const float level = 0.45f + 0.55f * effort;
    const float hissLevel = 0.012f * medium;
    const float drive = 1.0f + 3.0f * medium;
    const float bandMix = medium;                       // how much of the voice goes through the radio
    const float fricGain = 0.35f + 0.3f * clarity;
    const double beepInc = static_cast<double>(murBeepHz_) / sr_;
    for (int i = 0; i < n; ++i) {
        // The pulse train, jittered per period.
        murPhase_ += inc * static_cast<double>(murJitter_);
        float pulse = 0.0f;
        if (murPhase_ >= 1.0) { murPhase_ -= 1.0; pulse = 1.0f; murJitter_ = 1.0f + 0.012f * rng_.bipolar(); }
        murGlot1_ += tilt * (pulse - murGlot1_);
        murGlot2_ += tilt * (murGlot1_ - murGlot2_);
        const float w = rng_.bipolar();
        // Voiced: the pulse; unvoiced: noise, through the same formants and a band above them.
        const float src = (murGlot2_ * 18.0f * murVoice_ + w * 0.5f * (1.0f - murVoice_)) * murGap_;
        float lp, bp, hp, voice = 0.0f;
        for (int k = 0; k < 3; ++k) { murForm_[k].tick(src, lp, bp, hp); voice += bp * (k == 0 ? 1.0f : (k == 1 ? 0.7f : 0.35f)); }
        murFric_.tick(w, lp, bp, hp);
        voice += bp * fricGain * (1.0f - murVoice_) * murGap_ * (murInPhrase_ ? 1.0f : 0.0f);
        voice *= level;
        // The radio: the band, the saturation, the carrier's hiss while the channel is open,
        // the squelch when it closes, the beeps that key it.
        float radio = voice;
        if (bandMix > 0.0f) {
            murRadioHp_.tick(radio, lp, bp, hp); radio = hp;
            murRadioLp_.tick(radio, lp, bp, hp); radio = lp;
            radio = softClip(radio * drive) / drive * 1.6f;
            murHiss_ += 0.002f * ((murInPhrase_ ? 1.0f : 0.0f) - murHiss_);
            radio += w * hissLevel * murHiss_;
            if (murSquelchLeft_ > 0.0) { murSquelchLeft_ -= 1.0; radio += w * 0.08f * medium; }
            if (murBeepLeft_ > 0.0) {
                murBeepLeft_ -= 1.0;
                murBeepPhase_ += beepInc; if (murBeepPhase_ >= 1.0) murBeepPhase_ -= 1.0;
                radio += 0.06f * sin01(murBeepPhase_);
            }
        }
        const float mixed = voice + (radio - voice) * bandMix;
        const float y = mixed - murHpX_ + hpCoef * murHpY_;
        murHpX_ = mixed; murHpY_ = y;
        out[i] += y;
    }
    for (int k = 0; k < 3; ++k) if (!std::isfinite(murForm_[k].ic1) || !std::isfinite(murForm_[k].ic2)) murForm_[k].reset();
}

// ---------------------------------------------------------------- Drops
//
// A drop of water falling into a vessel makes two sounds: the click of the impact, and the
// bubble the impact pulls under the surface, which rings like a bell whose pitch RISES as the
// bubble rises towards the surface. Van den Doel (2005) gives the bubble as a sine at f0 with a
// damping d = 0.043 f0 + 0.0014 f0^1.5 and a frequency f(t) = f0 (1 + s d t): the "bloop" of
// every drip. The bubble's size sets f0 -- a millimetre is three kilohertz, and a drop's bubbles
// are one to seven -- and Bright is that size: small and high, or large and low. The click goes
// into the vessel, two resonators whose pitch is Position (a cup at the top, a cistern at the
// bottom), and the bubble is heard dry beside it. Drops fall at Density a second, on a Poisson
// clock, so the pattern never repeats; with Pitch = Note the bubbles sit on the note's own
// partials instead of on a random size, which is the wet marimba of a cave.
void SourceSlot::renderDrops(float* out, int n, double hz, const SlotParams& p, float dt)
{
    (void)dt;
    const double density = clampv(static_cast<double>(p.density), 0.05, 60.0);
    const float bright = clampv(p.bright, 0.0f, 1.0f);
    const float wander = clampv(p.positionDrift, 0.0f, 1.0f);
    // The vessel: two modes, the second a little over twice the first, from Position. Each mode's
    // input is scaled by (1 - r), so its gain at its own frequency is the number written here
    // and not 1/(1 - r) -- unscaled, a two-millisecond click rang the vessel to full scale and
    // beyond (measured: peak 1.0, the clipper, and a reset every few drops).
    const double vessel = 150.0 * std::pow(10.0, static_cast<double>(clampv(p.position, 0.0f, 1.0f)));   // 150 .. 1500 Hz
    float va1[2], va2[2], vdrive[2];
    for (int m = 0; m < 2; ++m) {
        const double fm = std::min(vessel * (m == 0 ? 1.0 : 2.32), 0.4 * sr_);
        const float r = std::exp(-6.908f / ((m == 0 ? 0.9f : 0.5f) * static_cast<float>(sr_)));
        const float th = static_cast<float>(kTwoPi * fm / sr_);
        va1[m] = 2.0f * r * std::cos(th); va2[m] = -r * r;
        vdrive[m] = (1.0f - r) * 300.0f;
    }
    for (int i = 0; i < n; ++i) {
        dropNext_ -= 1.0;
        if (dropNext_ <= 0.0) {
            dropNext_ = -std::log(1.0 - static_cast<double>(rng_.uniform()) + 1e-9) * sr_ / density;
            for (auto& d : drops_) {
                if (d.on) continue;
                // The bubble: its size from Bright with a spread from Pos Drift, or on the note.
                double f0;
                if (p.follow) {
                    f0 = hz;
                    while (f0 < 400.0) f0 *= 2.0;
                    while (f0 > 3200.0) f0 *= 0.5;
                    f0 *= std::pow(2.0, 0.08 * static_cast<double>(wander * rng_.bipolar()));
                } else {
                    const double lo = 2600.0 - 2000.0 * static_cast<double>(bright) * 0.5;   // dull: large bubbles
                    const double centre = 500.0 + 2200.0 * static_cast<double>(bright);
                    f0 = centre * std::pow(2.0, (0.3 + 0.9 * static_cast<double>(wander)) * static_cast<double>(rng_.bipolar()));
                    f0 = clampv(f0, 300.0, std::max(lo, 3400.0));
                }
                f0 = std::min(f0, 0.3 * sr_);
                const double damp = 0.043 * f0 + 0.0014 * std::pow(f0, 1.5);   // van den Doel's damping, per second
                d.hz = f0;
                d.rise = 0.1 * damp;                                            // s d: the pitch's rise per second
                d.decay = std::exp(-static_cast<float>(damp / sr_));
                d.amp = (0.35f + 0.65f * rng_.uniform()) * 0.4f;
                d.phase = 0.0;
                d.left = static_cast<int>(std::min(8.0 / damp, 1.5) * sr_);
                d.on = true;
                dropClick_ = 0.5f + 0.5f * rng_.uniform();
                dropClickLeft_ = static_cast<int>(0.0015 * sr_);
                break;
            }
        }
        float y = 0.0f;
        for (auto& d : drops_) {
            if (!d.on) continue;
            const double t = 1.0 - static_cast<double>(d.left) / std::max(1.0, 1.5 * sr_);
            (void)t;
            d.phase += d.hz / sr_; if (d.phase >= 1.0) d.phase -= 1.0;
            d.hz = std::min(d.hz + d.rise * d.hz / sr_, 0.4 * sr_);
            y += d.amp * sin01(d.phase);
            d.amp *= d.decay;
            if (--d.left <= 0 || d.amp < 1.0e-4f) d.on = false;
        }
        // The click into the vessel.
        float click = 0.0f;
        if (dropClickLeft_ > 0) { --dropClickLeft_; click = dropClick_ * rng_.bipolar(); }
        for (int m = 0; m < 2; ++m) {
            const float v = click * vdrive[m] + va1[m] * dropVesselY1_[m] + va2[m] * dropVesselY2_[m];
            dropVesselY2_[m] = dropVesselY1_[m]; dropVesselY1_[m] = v;
            y += v * (m == 0 ? 0.5f : 0.25f);
        }
        if (!(y > -50.0f && y < 50.0f)) { dropVesselY1_[0] = dropVesselY1_[1] = dropVesselY2_[0] = dropVesselY2_[1] = 0.0f; y = 0.0f; }
        out[i] += y * 0.3f;
    }
}

// ---------------------------------------------------------------- Clip
//
// The recording as it is. Every other way this instrument has of playing a clip takes it apart --
// grains, a spectral stretch, a band model -- because a drone wants a texture and not a document.
// The foreground wants the document: "Houston, we've had a problem" is a sentence, and a sentence
// in grains is not one. So: the clip from Position, once, at its own speed (Pitch = Free, with
// the slot's octave and ratio as a speed) or pitched to the note (Pitch = Note, against the pitch
// its name carries), the last twenty milliseconds faded, and silence after -- unless the file's
// name marks it seamless, in which case it wraps. A new note starts it again.
void SourceSlot::renderClip(float* outL, int n, double hz, double speed, const SlotParams& p, const Texture* tex, float dt)
{
    (void)dt;
    std::memset(scratch_, 0, sizeof(float) * static_cast<size_t>(n));
    if (tex == nullptr || tex->empty() || clipDone_) return;
    const int len = static_cast<int>(tex->mono.size());
    const bool wide = tex->stereo();
    if (clipPos_ < 0.0) clipPos_ = static_cast<double>(clampv(p.position, 0.0f, 1.0f)) * static_cast<double>(len - 2);
    const double rate = (tex->sampleRate / sr_) * (p.follow ? hz / std::max(tex->baseHz, 1.0) : speed);
    const float angle = (clampv(p.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    const float gain = tex->gain * clampv(p.level, 0.0f, 2.0f);
    const float gl = gain * std::cos(angle), gr = gain * std::sin(angle);
    const double fadeLen = 0.02 * sr_;
    const float* s = wide ? tex->lr.data() : tex->mono.data();
    for (int i = 0; i < n; ++i) {
        if (clipPos_ >= static_cast<double>(len - 2)) {
            if (tex->seamless) clipPos_ -= static_cast<double>(len - 2);
            else { clipDone_ = true; break; }
        }
        const int    i0 = static_cast<int>(clipPos_);
        const float  fr = static_cast<float>(clipPos_ - i0);
        float l, r;
        if (wide) {
            const float* a = s + 2 * i0;
            l = a[0] + fr * (a[2] - a[0]);
            r = a[1] + fr * (a[3] - a[1]);
        } else {
            l = r = s[i0] + fr * (s[i0 + 1] - s[i0]);
        }
        const double left = static_cast<double>(len - 2) - clipPos_;
        const float fade = left < fadeLen ? static_cast<float>(left / fadeLen) : 1.0f;
        outL[i] += l * gl * fade;
        scratch_[i] += r * gr * fade;
        clipPos_ += rate;
    }
}

} // namespace ambient
