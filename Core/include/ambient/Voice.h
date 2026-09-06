// AmbientSynth -- one cluster voice: `unison` strands, each an additive bank of
// up to 32 harmonic partials with individually drifting amplitudes and a slowly
// drifting pitch, plus a filtered-noise "air" layer. Alias-free by construction.
//
// Spatial model (after Robert Rich): every voice sits on a plane between the
// listener's ear (distance 0: dry, bright, close) and the infinite background
// (distance 1: darker, softer, sent to the far reverb). The voice's centre pan
// is rendered with a true interaural time difference, not only with gain.
#pragma once
#include "Dsp.h"
#include "Sources.h"
#include "Filter.h"
#include "ZPlane.h"
#include <cstdint>

namespace ambient {

constexpr int kMaxPartials  = 32;
constexpr int kMaxStrands   = 6;
constexpr int kControlBlock = 64;   // samples between control-rate updates (1.3 ms at 48 kHz)
constexpr int kItdBuffer    = 256;  // >= 0.7 ms at 192 kHz

struct VoiceParams {
    float level = 1.0f;         // level of the partial bank (Source 1)
    int   partials = 16;
    float tilt = 1.2f, brightness = 0.7f, oddEven = 0.0f, inharmonic = 0.0f;
    float shimmer = 0.4f, shimmerRate = 0.15f;
    int   unison = 3;
    float detune = 8.0f, drift = 4.0f, driftRate = 0.08f, spread = 0.7f;
    float bloom = 0.0f, bloomTime = 60.0f;   // spectrum opens from brightness*(1-bloom) to brightness over bloomTime
    int   stack = 0;            // index into kStackRatios: strands at pure ratios (0 = classic detuned unison)
    float rateWander = 0.3f;    // drift/shimmer/breath rates wander by up to +-1 octave on a 100 s curve
    float air = 0.15f, airColor = 3.0f, airQ = 10.0f;
    float attack = 6.0f, decay = 4.0f, sustain = 0.8f, release = 12.0f;
    float cutoff = 2500.0f, resonance = 0.15f, filterEnv = 0.3f, filterDrift = 0.3f, keyTrack = 0.5f;
    int   filterModel = 1;      // FilterModel (Filter.h); 1 = the 12 dB state-variable low pass
    float filterDrive = 0.0f;
    float fold = 0.0f;          // wavefolder after the filters, 0 = off (see Voice.cpp)
    // Rich refinements: the binaural phase field, the breathing doppler, the pitch tide (from the
    // engine, already a multiplier), and the strike layer
    float phaseWidth = 0.0f, phaseRate = 0.03f, doppler = 0.0f;
    // Expression: how far this voice's own pressure, slide and bend reach
    float pressDistance = 0.0f, pressBright = 0.0f, pressLevel = 0.0f;
    float slideCutoff = 0.0f, slideZ = 0.0f;
    float externalise = 0.0f;   // pinna notch + shoulder reflection, for headphones
    // Headphones binaural mode: the pan becomes an azimuth, the head's yaw turns the field the
    // other way, the interaural delay follows Woodworth's head and the shadow is at full
    // strength whatever Time Width says. Off, everything below is exactly as it was.
    bool  binaural = false;
    float headYawDeg = 0.0f;
    float sympathy = 0.0f;      // how much of the other voices this one hears, through its own filter
    float pitchMul = 1.0f;
    float strikeLevel = 0.0f, strikeDecay = 0.4f, strikeDamp = 0.5f;
    int   strikeType = 0;       // String, Wood, Metal
    bool  strikeBrain = false;  // the brain's notes strike too
    bool  filterOn = true;      // the voice filter can be switched out; z-plane Replace also bypasses it
    bool  filterParallel = false;   // both filters on: z-plane after the filter (false) or beside it (true)
    // Z-plane filter (ZPlane.h): 0 off, 1 in series after the SVF, 2 instead of it
    int   zMode = 0, zShape = 0;
    float zDecay = 2.5f, zDamp = 0.6f;   // Modal: T60 of the lowest mode, and how much shorter the high ones
    float zX = 0.5f, zY = 0.5f, zZ = 0.0f, zRate = 0.05f, zDepth = 0.5f, zRes = 0.5f, zKeyTrack = 0.0f, zMix = 0.7f;
    float panDrift = 0.4f, itd = 0.6f;
    // Rich's foreground/background carving inside the voice (see concept.md):
    float presence = 0.0f;      // dB bell at 2-5 kHz, full on the near plane, gone on the far plane
    float breath = 0.0f, breathRate = 0.03f;   // slow wandering of the distance itself (+-0.35 at 1)
    float lowCut = 0.0f;        // Hz; partials below fall 12 dB/oct, keeping the pads out of the sub's register
    float fmAmount = 0.0f;      // phase modulation of every partial (h times the deviation) by the `fm` signal given to render()
    bool  freeze = false;       // hold the spectrum still: shimmer, pitch drift, breath and bloom stop moving
    bool  airGhost = false;     // Air through six resonators on the note's harmonics 1 2 3 5 7 9 instead of one band
    double rootHz = 130.81;     // the brain's root, for the portamento's consonance gravity
    // Coherence offsets (from the engine's Kuramoto bank), added on top of the parameters
    float cohBrightness = 0.0f, cohPan = 0.0f, cohZ = 0.0f;
    // The four source slots (slot[0] = Source 1: Additive means the strand bank above, any other
    // type mutes the bank and renders in the slot) and the data they may need; pointers stay
    // valid for the block. Each slot has its own clip.
    SlotParams       slot[kSlots];
    const Wavetable* userTable = nullptr;
    const Texture*   texture[kSlots] = {};
};

class Voice {
public:
    void prepare(double sampleRate, uint64_t seed);
    // distance 0..1 = plane (see above); fixed for the life of the note.
    void noteOn(int note, double freqHz, float velocity, int owner, float distance, const VoiceParams& p);
    void noteOff();
    void kill();

    bool isActive() const    { return env_.isActive(); }
    bool isReleasing() const { return env_.isReleasing(); }
    float level() const      { return env_.level(); }
    int  note() const        { return note_; }
    int  owner() const       { return owner_; }
    float distance() const   { return distEff_; }   // where the voice is right now (breath included)
    double frequency() const { return freq_; }
    // Retune a sounding voice (tuning purity/drift): glides in the log domain, ~1 s time constant.
    void setTargetFrequency(double hz) { freqTarget_ = hz; }
    // Per-note expression. Pressure and slide are 0..1, bend is in semitones; all three are
    // smoothed inside the voice, so a controller sending steps never steps the sound.
    void setPressure(float v) { pressTarget_ = clampv(v, 0.0f, 1.0f); }
    void setSlide(float v)    { slideTarget_ = clampv(v, 0.0f, 1.0f); }
    void setBend(float semitones) { bendTarget_ = semitones; }
    float pressure() const { return press_; }
    float slide() const    { return slide_; }
    // Portamento: start at fromHz and slide to the note's frequency over `seconds`, slowing near
    // consonant ratios to the root by `gravity` (0 = an even log-domain glide).
    void glideFrom(double fromHz, float seconds, float gravity);
    bool gliding() const { return portaLeft_ > 0.0f; }
    uint64_t order = 0;      // allocation order for voice stealing

    // For pictures only: the current amplitude of each partial of the first strand, exactly the
    // numbers the oscillator is summing right now -- so a display drawn from them breathes with
    // the shimmer and the drift instead of being a static illustration. Read without
    // synchronisation from the message thread; a torn float costs one wrong pixel.
    int displayPartials(float* out, int maxCount) const
    {
        const int n = maxCount < strands_[0].active ? maxCount : strands_[0].active;
        for (int i = 0; i < n; ++i) out[i] = strands_[0].amp[i];
        return n < 0 ? 0 : n;
    }
    // The same for a slot's own bank (Additive / Wavetable in Source 1..3), and its grains.
    int displaySlotPartials(int slot, float* out, int maxCount) const { return slots_[slot < 0 ? 0 : (slot >= kSlots ? kSlots - 1 : slot)].displayAmps(out, maxCount); }
    int displayGrains(int slot, SourceSlot::GrainInfo* out, int maxCount, int clipLen) const { return slots_[slot < 0 ? 0 : (slot >= kSlots ? kSlots - 1 : slot)].displayGrains(out, maxCount, clipLen); }
    float pan() const { return centre_; }   // where the voice's centre sits right now, -1..1

    // Adds `n` samples into the near (dry plane) and far (reverb send) buses. `fm` (n samples,
    // may be null) phase-modulates the partials when p.fmAmount > 0 (the feedback loop).
    // `couple` (n samples, may be null) is the previous block's foreground, mixed into this
    // voice's own filter input when p.sympathy > 0 -- strings on a shared soundboard.
    void render(float* nearL, float* nearR, float* farL, float* farR, int n, const VoiceParams& p,
                const float* fm = nullptr, const float* couple = nullptr);

private:
    struct Strand {
        // Every partial is a rotating phasor (cos, sin) turned by its own per-sample rotation;
        // independent across partials, so the loop pipelines and vectorises. Phasors are
        // renormalised once per control block.
        float  pc[kMaxPartials] = {}, ps[kMaxPartials] = {};
        float  rc[kMaxPartials] = {}, rs[kMaxPartials] = {};
        float  amp[kMaxPartials] = {};
        float  ampStep[kMaxPartials] = {};
        Drifter shimmer[kMaxPartials];
        Drifter pitch;
        float  gainL = 0.7f, gainR = 0.7f;
        int    active = 0;
    };
    void control(int blockLen, const VoiceParams& p);

    Strand   strands_[kMaxStrands];
    SourceSlot slots_[kSlots];
    float    rateMul_ = 1.0f;
    Envelope env_;
    VoiceFilter filt_;
    bool     lastFilterOn_ = true;
    // Binaural phase field: two first-order all-passes per ear, corners drifting apart
    Drifter  phaseDrift_;
    bool     phaseOn_ = false;
    float    apC_[2][2] = {}, apX_[2][2] = {}, apY_[2][2] = {};
    // Doppler from the breathing distance
    float    prevDist_ = 0.0f;
    double   dopplerMul_ = 1.0;
    // Strike: a Karplus-Strong loop excited at note-on, on the near plane
    static constexpr int kStrikeMax = 4096;
    float    ks_[kStrikeMax] = {};
    int      ksLen_ = 0, ksPos_ = 0, ksLeft_ = 0, ksT_ = 0, ksTotal_ = 0;
    float    ksG_ = 0.0f, ksLpC_ = 0.5f, ksLp_ = 0.0f, ksApK_ = 0.0f, ksApX_ = 0.0f, ksApY_ = 0.0f, ksAmp_ = 0.0f;
    float    ksGainL_ = 0.7f, ksGainR_ = 0.7f;
    bool     ksOn_ = false;
    void     strikeStart(double hz, const VoiceParams& p);
    inline float strikeTick();
    Svf      airL_, airR_;
    Drifter  filterDrift_, airDrift_, panCenter_, breath_, rateWander_;
    Drifter  zDriftX_, zDriftY_;
    ZBiquad  zbL_[kZSections], zbR_[kZSections];
    ZModal   zModal_;                  // the same frame read as a bank of ringing modes
    // Series of biquads, or a parallel bank of resonators: one call so the two paths cannot
    // drift apart at the two places the filter is applied.
    inline void zRun(float& l, float& r)
    {
        if (zModeCur_ == 3) { l = zModal_.tick(0, l); r = zModal_.tick(1, r); return; }
        for (int k = 0; k < zUsed_; ++k) { l = zbL_[k].tick(l); r = zbR_[k].tick(r); }
    }
    int      zUsed_ = 0;
    float    zNorm_ = 1.0f;
    float    fmHpXL_ = 0.0f, fmHpXR_ = 0.0f, fmHpYL_ = 0.0f, fmHpYR_ = 0.0f, fmHpCoef_ = 0.9987f;   // DC blocker for feedback FM
    float    zWet_ = 0.0f, zDry_ = 1.0f;
    int      zModeCur_ = 0;
    Rng      rng_;
    double   sr_ = 48000.0;
    double   freq_ = 220.0, freqTarget_ = 220.0;
    double   portaFrom_ = 0.0; float portaLeft_ = 0.0f, portaSeconds_ = 0.0f, portaGravity_ = 0.0f;
    Resonator ghostL_[6], ghostR_[6];
    float    ghostGain_ = 0.0f;
    float    velocity_ = 1.0f;
    float    press_ = 0.0f, pressTarget_ = 0.0f;     // per-note expression, smoothed at control rate
    float    slide_ = 0.0f, slideTarget_ = 0.0f;
    float    bend_ = 0.0f, bendTarget_ = 0.0f;
    float    centre_ = 0.0f;     // pan centre after drift and Source 1's Pan, for the stage picture
    float    distance_ = 0.0f;   // the plane the note was placed on
    float    distEff_ = 0.0f;    // distance after breathing, refreshed at control rate
    float    gNear_ = 1.0f, gFar_ = 0.0f, gLevel_ = 1.0f;
    float    airGain_ = 0.0f;
    float    bloomT_ = 0.0f;     // seconds since note start, for Bloom
    // Control-rate caches: the spectral shape only changes when its parameters do.
    float    tiltCache_[kMaxPartials + 1] = {};
    float    cachedTilt_ = -1.0f, cachedOddEven_ = -9.0f;
    int      cachedPartials_ = -1;
    double   stretchCache_[kMaxPartials] = {};
    float    cachedB_ = -1.0f;
    float    itdBufL_[kItdBuffer] = {}, itdBufR_[kItdBuffer] = {};
    int      itdW_ = 0;
    float    itdL_ = 0.0f, itdR_ = 0.0f, itdLTarget_ = 0.0f, itdRTarget_ = 0.0f;
    float    shadowL_ = 0.0f, shadowR_ = 0.0f, shadowCoefL_ = 1.0f, shadowCoefR_ = 1.0f;   // head shadow on the far ear
    // The binaural mode's head shadow is a one-pole/one-zero rather than a one-pole: the
    // spherical-head model (Brown and Duda 1998), whose zero moves with the angle.
    bool     sphereShadow_ = false;
    float    sphB0_[2] = { 1.0f, 1.0f }, sphB1_[2] = {}, sphA1_[2] = {};
    float    sphX1_[2] = {}, sphY1_[2] = {};
    // Externalisation (Brown and Duda's structural model, the parts that need no measured data):
    // the pinna's notch, whose frequency moves with the source's angle, and the shoulder echo.
    Svf      pinnaL_, pinnaR_;
    float    extAmt_ = 0.0f;
    int      shoulder_ = 0;
    int      note_ = -1;
    int      owner_ = 0;
    int      lastUnison_ = 0;
};

} // namespace ambient
