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

    // Morph: two full parameter snapshots (A = 0, B = 1). While MorphActive is on the
    // engine plays lerp(A, B, position); the position glides toward MorphPos at
    // 1/MorphGlide per second. Meant for a hand in VR: one continuous gesture moves
    // the whole instrument from one world to another.
    void  setMorphSlot(int slot, const float* values);   // kNumParams values, any thread
    void  captureMorphSlot(int slot);                    // copy the live parameters into a slot
    void  morphSlot(int slot, float* out) const;
    float morphPosition() const { return morphCur_.load(std::memory_order_relaxed); }
    float effectiveParam(ParamId id) const;              // what is actually playing

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
    Voice        voices_[kMaxVoices];
    ClusterBrain brain_;
    Ensemble     ensemble_;
    StereoDelay  delay_, delay2_;
    GrainCloud   cloud_;
    float        delay2Mix_ = 0.0f, delay2ToFar_ = 0.5f, cloudSend_ = 0.0f;
    Reverb       nearReverb_, farReverb_;
    MidSide      midSide_;
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
    VoiceParams  vp_;
    BrainParams  bp_;
    Drifter      arc_;
    float        arcAmount_ = 0.0f, arcPeriodMin_ = 40.0f;
    // Foundation sub voice
    double       subPhaseL_ = 0.0, subPhaseR_ = 0.0, subFreqCur_ = 0.0;
    float        subLevel_ = 0.0f, subLevelCur_ = 0.0f, subGlide_ = 8.0f, subBinaural_ = 0.0f, subTone_ = 0.2f;
    int          subOctave_ = 1;
    bool         hold_ = false;
    float        depth_ = 0.7f, keysDepth_ = 0.0f;
    float        delayMix_ = 0.25f, delayToFar_ = 0.4f, farLevel_ = 0.8f;

    FixedScale        scales_[kNumScaleChoices];
    FixedScale        userPending_;
    std::atomic<int>  userVersion_{ 0 };
    std::atomic<bool> userBusy_{ false };
    int               userSeen_ = 0;
    const FixedScale* scale_ = nullptr;
    int               rootNote_ = 62;
    double            refPitch_ = 440.0;
    bool              snapKeys_ = true;

    std::vector<float> nearL_, nearR_, farL_, farR_, wetL_, wetR_;
    double   sr_ = 48000.0;
    int      maxBlock_ = 512;
    uint64_t order_ = 0;
    Rng      rng_;
    int      seed_ = -1;
    int      lastRootPc_ = -1;
    bool     midiHeld_[128] = {};
    Smoother masterSmooth_;

    std::atomic<uint64_t> mask_[2]{ 0, 0 };
    std::atomic<int> activeVoices_{ 0 };
    std::atomic<int> brainRoot_{ 50 };
    std::atomic<float> arcValue_{ 0.0f };
    std::atomic<float> noteDistance_[128];
    std::atomic<float> noteLevel_[128];
};

} // namespace ambient
