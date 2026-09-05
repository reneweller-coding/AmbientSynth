// AmbientSynth -- the engine: voices, cluster brain, spatial routing, effects, parameters.
// Framework-free. `process()` never allocates; parameters are plain atomics so the
// host layer can write them from any thread.
//
// Signal flow:
//   voices -> near bus (dry plane) -> ensemble -> stereo delay -> near reverb ----+
//          -> far bus (reverb send) + delay "to far" -> far reverb (dark, wide) --+-> mid/side -> master
#pragma once
#include "Params.h"
#include "Tuning.h"
#include "Voice.h"
#include "Effects.h"
#include "Cosmos.h"
#include "Convolution.h"
#include "Route.h"
#include "ClusterBrain.h"
#include "Presets.h"
#include <atomic>
#include <vector>
#include <cstdint>

namespace ambient {

class Engine {
public:
    static constexpr int kMaxVoices = 16;

    Engine();
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void  setParam(ParamId id, float v) { params_[static_cast<int>(id)].store(v, std::memory_order_relaxed); }
    float getParam(ParamId id) const    { return params_[static_cast<int>(id)].load(std::memory_order_relaxed); }
    bool  applyPreset(int index);       // any thread; sets every parameter (full preset)
    bool  applySoundPreset(int index);  // sound layer only: everything except the Cosmos section
    bool  applyCosmosPreset(int index); // Cosmos layer only, from the Cosmos preset bank

    // MIDI, audio thread only.
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

    // Renders `n` stereo samples (replaces L/R). Any n; larger than maxBlockSize is chunked.
    void process(float* L, float* R, int n);

    // Any thread. Applied at the start of the next block.
    void setUserScale(const FixedScale& s);
    // User wavetable (Table = User in a source slot) and texture (Type = Texture), message
    // thread. The texture is double-buffered: the call waits (at most one block) until the
    // audio thread has left the buffer it is about to overwrite, then swaps.
    void setUserWavetable(const Wavetable& t);
    bool loadUserWavetable(const float* mono, int n, int frameLen = 2048);   // analyses frames, then sets
    void setTexture(const float* mono, int n, double sampleRate, double baseHz = 261.6256);
    bool hasTexture() const { return textureActive_.load(std::memory_order_relaxed) >= 0; }
    // Convolution room: a stereo (R may be null) impulse response, message thread.
    void  setImpulse(const float* L, const float* R, int n, double sampleRate) { room_.setImpulse(L, R, n, sampleRate); userImpulse_ = true; }
    float impulseSeconds() const { return room_.impulseSeconds(); }
    bool  hasUserImpulse() const { return userImpulse_; }
    // Longest impulse the Room keeps (memory and CPU grow with it); call before prepare().
    // 8 s on the desktop; the Quest app uses 4 s (a 4 s hall costs about a third of one of its cores).
    void  setRoomMaxSeconds(float s) { roomMaxSeconds_ = clampv(s, 0.5f, 12.0f); }
    int  userWavetableFrames() const { return userTableFrames_.load(std::memory_order_relaxed); }

    // Morph: two full parameter snapshots (A = 0, B = 1). While MorphActive is on the
    // engine plays lerp(A, B, position); the position glides toward MorphPos at
    // 1/MorphGlide per second. Meant for a hand in VR: one continuous gesture moves
    // the whole instrument from one world to another.
    void  setMorphSlot(int slot, const float* values);   // kNumParams values, any thread
    void  captureMorphSlot(int slot);                    // copy the live parameters into a slot
    void  morphSlot(int slot, float* out) const;
    float morphPosition() const { return morphCur_.load(std::memory_order_relaxed); }
    float effectiveParam(ParamId id) const;              // what is actually playing
    // Preset map (MapActive / MapX / MapY / MapRadius): the engine blends the presets around
    // the cursor at control rate and glides every parameter toward the blend with the Morph
    // Glide time constant. While active it overrides the live parameters and the morph.
    // blendValue() is the gliding value the host should copy back into its parameters when
    // the map is switched off, so the sound stays where the map left it.
    float blendValue(ParamId id) const { return blendCur_[static_cast<int>(id)].load(std::memory_order_relaxed); }
    bool  mapActive() const { return blendActive_.load(std::memory_order_relaxed); }
    // Route over the map. The host calls routeStep() once per block (dt in seconds, before its
    // own speed scaling): while RouteActive the route moves the cursor and the engine plays the
    // map blend; the new cursor is returned so the host can mirror it into its parameters.
    // The route text is performance state (plugin state / OSC / Quest config), not a preset.
    Route& route() { return route_; }
    bool routeStep(double dt, float& x, float& y, float& radius);
    bool routeRunning() const { return route_.running(); }
    bool asleep() const { return asleep_; }   // no voice and no tail for two seconds: effects skipped
    float coherencePhase(int i) const { return kuraPhase_[i & 3]; }   // Kuramoto oscillator phases, for pictures

    const FixedScale& scale() const { return *scale_; }
    double frequencyOf(int note) const;
    double sampleRate() const { return sr_; }

    // Observers for the GUI (approximate, lock-free).
    int  activeVoices() const { return activeVoices_.load(std::memory_order_relaxed); }
    int  brainRoot() const    { return brainRoot_.load(std::memory_order_relaxed); }
    float arcValue() const    { return arcValue_.load(std::memory_order_relaxed); }
    void soundingNotes(bool (&out)[128]) const;
    // Distance (0 near .. 1 far) of the voice sounding `note`, or -1 if none.
    float noteDistance(int note) const;
    // Envelope level (0..1) of the loudest voice on `note`, 0 if none.
    float noteLevel(int note) const { return (note >= 0 && note < 128) ? noteLevel_[note].load(std::memory_order_relaxed) : 0.0f; }

private:
    enum Owner { OwnerMidi = 0, OwnerBrain = 1 };
    Voice* allocate(int note, int owner);
    void   startNote(int note, float velocity, int owner, float distance);
    void   stopNote(int note, int owner);
    void   readParams();
    void   renderChunk(float* L, float* R, int n);

    std::atomic<float> params_[kNumParams];
    std::atomic<float> slotA_[kNumParams], slotB_[kNumParams];
    std::atomic<float> morphCur_{ 0.0f };
    // Preset map blend (audio thread writes, host reads)
    std::atomic<float> blendCur_[kNumParams];
    std::atomic<bool>  blendActive_{ false };
    float              blendTarget_[kNumParams] = {};
    void updateBlend(int n);
    Voice        voices_[kMaxVoices];
    ClusterBrain brain_;
    Ensemble     ensemble_;
    StereoDelay  delay_, delay2_;
    GrainCloud   cloud_;
    float        delay2Mix_ = 0.0f, delay2ToFar_ = 0.5f, cloudSend_ = 0.0f;
    Reverb       nearReverb_, farReverb_;
    MidSide      midSide_;
    // Room (convolution) on the far plane: level, source, pre-delay ring, tail low-pass
    Convolver    room_;
    bool         userImpulse_ = false;
    float        roomMaxSeconds_ = 8.0f;
    Route        route_;
    bool         routeWasActive_ = false;
    float        roomLevel_ = 0.0f, roomLevelCur_ = 0.0f, roomHighcut_ = 5000.0f;
    int          roomSource_ = 0, roomPreDelay_ = 0;
    long         roomTailLeft_ = 0;
    std::vector<float> roomInL_, roomInR_, roomOutL_, roomOutR_, roomDelayL_, roomDelayR_;
    int          roomDelayW_ = 0, roomDelayMask_ = 0;
    float        roomLpL_ = 0.0f, roomLpR_ = 0.0f;
    // Cosmos path (parallel send from the near bus) and the shimmer loop around the far reverb.
    FreqShifter   shifter_;
    CombResonator resonator_;
    VowelFilter   vowel_;
    Nebula        nebula_;
    PitchShifter  shimmerL_, shimmerR_;
    Drifter       shiftDrift_;
    float         cosmosSend_ = 0.0f, cosmosReturn_ = 0.5f, cosmosToFar_ = 0.5f, cosmosNebula_ = 0.0f;
    float         cosmosShimmer_ = 0.0f, shimmerLpL_ = 0.0f, shimmerLpR_ = 0.0f, shimmerEnv_ = 0.0f;
    std::vector<float> cosL_, cosR_, nebL_, nebR_, shimL_, shimR_;
    // Feedback loop: the previous chunk's output mix, low-passed, saturated and throttled,
    // kept in a ring so any chunk length reads back exactly the samples just written.
    std::vector<float> fbRingL_, fbRingR_, fbInL_, fbInR_, fbMono_;
    int           fbW_ = 0, fbMask_ = 0;
    float         fbBus_ = 0.0f, fbFm_ = 0.0f, fbTone_ = 1500.0f, fbDrive_ = 0.5f;
    float         fbLpL_ = 0.0f, fbLpR_ = 0.0f, fbEnv_ = 0.0f, fbReg_ = 1.0f;
    float         fbHpXL_ = 0.0f, fbHpXR_ = 0.0f, fbHpYL_ = 0.0f, fbHpYR_ = 0.0f;   // loop DC blocker
    VoiceParams  vp_;
    BrainParams  bp_;
    Drifter      arc_;
    float        arcAmount_ = 0.0f, arcPeriodMin_ = 40.0f;
    // Foundation sub voice
    double       subPhaseL_ = 0.0, subPhaseR_ = 0.0, subFreqCur_ = 0.0;
    float        subLevel_ = 0.0f, subLevelCur_ = 0.0f, subGlide_ = 8.0f, subBinaural_ = 0.0f, subTone_ = 0.2f;
    int          subOctave_ = 1;
    bool         subGhost_ = false;   // Source = Difference: follow the ghost tone of the two lowest voices
    bool         hold_ = false;
    float        depth_ = 0.7f, keysDepth_ = 0.0f;
    float        delayMix_ = 0.25f, delayToFar_ = 0.4f, farLevel_ = 0.8f;

    FixedScale        scales_[kNumScaleChoices];
    FixedScale        userPending_;
    std::atomic<int>  userVersion_{ 0 };
    std::atomic<bool> userBusy_{ false };
    int               userSeen_ = 0;
    // User wavetable: pending copy + version (8 KB, copied by the audio thread).
    Wavetable         userTable_, userTablePending_;
    std::atomic<int>  tableVersion_{ 0 };
    std::atomic<bool> tableBusy_{ false };
    int               tableSeen_ = 0;
    std::atomic<int>  userTableFrames_{ 0 };
    // Texture: two buffers, the audio thread reads the active one and publishes which.
    Texture           textures_[2];
    std::atomic<int>  textureActive_{ -1 }, textureInUse_{ -1 };
    const FixedScale* scale_ = nullptr;
    int               rootNote_ = 62;
    double            refPitch_ = 440.0;
    bool              snapKeys_ = true;
    float             inertiaCur_[kNumParams] = {};
    float             lastBlockSeconds_ = 0.005f;
    float             kuraPhase_[4] = { 0.0f, 1.3f, 2.9f, 4.4f };   // Kuramoto bank phases
    float             portamento_ = 0.0f, portaGravity_ = 0.5f;
    double            lastKeyHz_ = 0.0;   // frequency of the last key pressed, for portamento
    float             fbTape_ = 0.0f;
    Drifter           tapeWow_;
    double            tapeFlutterPhase_ = 0.0;
    double            purityCur_ = 1.0;   // Purity plus its drift, evaluated per block
    Drifter           purityDrift_;
    bool              retune_ = false;    // purity below 1 or drifting: sounding voices follow
    // Sleep: after two seconds of silence (no voice, output below -90 dBFS) the effects sleep
    long              silentSamples_ = 0;
    bool              asleep_ = false;

    std::vector<float> nearL_, nearR_, farL_, farR_, wetL_, wetR_;
    double   sr_ = 48000.0;
    int      maxBlock_ = 512;
    uint64_t order_ = 0;
    Rng      rng_;
    int      seed_ = -1;
    int      lastRootPc_ = -1;
    bool     midiHeld_[128] = {};
    Smoother masterSmooth_;
    float    dcXL_ = 0.0f, dcXR_ = 0.0f, dcYL_ = 0.0f, dcYR_ = 0.0f;   // output DC blocker
    // Per-sample smoothing of the level-type parameters in the effect chain (20 ms), so
    // automation, gestures, morph and map blend never step a gain by a whole block.
    Smoother smDelayMix_, smDelayToFar_, smDelay2Mix_, smDelay2ToFar_, smCloudSend_, smCosmosSend_, smCosmosReturn_, smCosmosToFar_, smFarLevel_;

    std::atomic<uint64_t> mask_[2]{ 0, 0 };
    std::atomic<int> activeVoices_{ 0 };
    std::atomic<int> brainRoot_{ 50 };
    std::atomic<float> arcValue_{ 0.0f };
    std::atomic<float> noteDistance_[128];
    std::atomic<float> noteLevel_[128];
};

} // namespace ambient
