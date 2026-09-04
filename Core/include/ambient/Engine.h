// AmbientSynth -- the engine: voices, cluster brain, effects, parameters.
// Framework-free. `process()` never allocates; parameters are plain atomics so the
// host layer can write them from any thread.
#pragma once
#include "Params.h"
#include "Tuning.h"
#include "Voice.h"
#include "Effects.h"
#include "ClusterBrain.h"
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

    // MIDI, audio thread only.
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

    // Renders `n` stereo samples (replaces L/R). Any n; larger than maxBlockSize is chunked.
    void process(float* L, float* R, int n);

    // Any thread. Applied at the start of the next block.
    void setUserScale(const FixedScale& s);

    const FixedScale& scale() const { return *scale_; }
    double frequencyOf(int note) const;
    double sampleRate() const { return sr_; }

    // Observers for the GUI (approximate, lock-free).
    int  activeVoices() const { return activeVoices_.load(std::memory_order_relaxed); }
    int  brainRoot() const    { return brainRoot_.load(std::memory_order_relaxed); }
    void soundingNotes(bool (&out)[128]) const;

private:
    enum Owner { OwnerMidi = 0, OwnerBrain = 1 };
    Voice* allocate(int note, int owner);
    void   startNote(int note, float velocity, int owner);
    void   stopNote(int note, int owner);
    void   readParams();
    void   renderChunk(float* L, float* R, int n);

    std::atomic<float> params_[kNumParams];
    Voice        voices_[kMaxVoices];
    ClusterBrain brain_;
    Ensemble     ensemble_;
    Reverb       reverb_;
    VoiceParams  vp_;
    BrainParams  bp_;

    FixedScale        scales_[kNumScaleChoices];
    FixedScale        userPending_;
    std::atomic<int>  userVersion_{ 0 };
    std::atomic<bool> userBusy_{ false };
    int               userSeen_ = 0;
    const FixedScale* scale_ = nullptr;
    int               rootNote_ = 62;
    double            refPitch_ = 440.0;
    bool              snapKeys_ = true;

    std::vector<float> busL_, busR_;
    double   sr_ = 48000.0;
    int      maxBlock_ = 512;
    uint64_t order_ = 0;
    Rng      rng_;
    int      seed_ = -1;
    int      lastRootPc_ = -1;
    bool     midiHeld_[128] = {};
    Smoother masterSmooth_;
    float    width_ = 1.0f;

    std::atomic<uint64_t> mask_[2]{ 0, 0 };
    std::atomic<int> activeVoices_{ 0 };
    std::atomic<int> brainRoot_{ 50 };
};

} // namespace ambient
