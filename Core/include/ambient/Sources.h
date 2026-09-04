// AmbientSynth -- extra sound sources per voice ("Source 2" and "Source 3").
//
// Source 1 is the additive partial bank (Oscillator section). Each of the two extra slots
// can be one of:
//   Wavetable -- not a table of samples but a table of SPECTRA (32 partial amplitudes per
//                frame); the position morphs between frames and the result is rendered by
//                the same rotating-phasor bank as the main oscillator. Alias-free, and every
//                partial keeps its own life (presence, low cut, feedback PM apply the same
//                way). Built-in tables are generated; a user table is analysed from a WAV
//                with 2048-sample frames (the common wavetable layout).
//   FM        -- a two-operator pair (carrier at the slot pitch, modulator at FM Ratio),
//                index limited automatically for high notes.
//   Texture   -- a granular player over a loaded sample (field recording, flute air,
//                metal), grains around Position with a slow wander; Follow = Note pitches
//                the sample to the note (assuming it was recorded at C4).
// Every slot has level, octave, a just ratio to the note and a pan, and goes through the
// voice's filter, envelope and distance like the main bank.
#pragma once
#include "Dsp.h"
#include <cstdint>
#include <vector>

namespace ambient {

constexpr int kSlots         = 2;
constexpr int kTableFrames   = 64;
constexpr int kTablePartials = 32;
constexpr int kSlotGrains    = 8;

enum class SourceType : int { Off = 0, Wavetable, Fm, Texture };

constexpr int kNumSourceTypes = 4;
constexpr int kNumTables      = 6;      // Classic, Organ, Vocal, Glass, Metal, User
constexpr int kNumSlotRatios  = 10;
extern const char* const kSourceTypeNames[kNumSourceTypes];
extern const char* const kTableNames[kNumTables];
extern const char* const kSlotRatioNames[kNumSlotRatios];
extern const double      kSlotRatios[kNumSlotRatios];
extern const char* const kFollowNames[2];

struct Wavetable {
    int   frames = 0;
    float amp[kTableFrames][kTablePartials] = {};
    // Spectrum at position 0..1 (linear between frames), 32 amplitudes.
    void spectrumAt(float pos, float* out) const;
    // Builds a table from raw samples laid out as consecutive single-cycle frames of
    // `frameLen` samples (2048 = Serum/Vital layout). Up to kTableFrames frames are kept
    // (evenly picked when there are more). Returns false if there is not one full frame.
    bool analyse(const float* mono, int n, int frameLen = 2048);
};

const Wavetable& builtinTable(int index);   // 0 .. kNumTables-2 (the last index is the user slot)

// Base pitch from a texture file name: a trailing "_A3" / "-C#4" / " Bb2" note token before
// the extension (as written by Tools/TextureGen) gives the frequency at A4 = 440 Hz; 0 if none.
double baseHzFromName(const char* fileName);

struct Texture {
    std::vector<float> mono;
    double sampleRate = 48000.0;
    double baseHz = 261.6256;   // assumed pitch of the sample for Follow = Note
    bool empty() const { return mono.size() < 64; }
};

struct SlotParams {
    SourceType type = SourceType::Off;
    float level = 0.5f;
    int   octave = 0;            // -2 .. 2
    int   ratio = 0;             // index into kSlotRatios
    float pan = 0.0f;            // -1 .. 1
    int   table = 0;             // index into kTableNames (last = user)
    float position = 0.0f;       // wavetable frame position / texture position
    float positionDrift = 0.3f;  // how far the position wanders on its own
    float fmRatio = 2.0f;        // modulator / carrier
    float fmIndex = 1.0f;        // modulation index (radians / 2pi at low notes)
    float grainMs = 200.0f;
    float density = 12.0f;       // grains per second
    bool  follow = false;        // texture pitched to the note
};

class SourceSlot {
public:
    void prepare(double sampleRate, uint64_t seed);
    void noteOn(bool fresh);
    // Adds n (<= kControlBlock) samples of this slot into outL/outR. Control values are
    // refreshed once per call; level and pan ramp across the block.
    void render(float* outL, float* outR, int n, double noteHz, const SlotParams& p,
                const Wavetable* table, const Texture* texture, float driftRate);

private:
    void renderWavetable(float* out, int n, double hz, const SlotParams& p, const Wavetable* table, float dt);
    void renderFm(float* out, int n, double hz, const SlotParams& p, float dt);
    void renderTexture(float* outL, int n, double hz, double speed, const SlotParams& p, const Texture* tex, float dt);

    // Wavetable: phasor bank like Voice::Strand.
    float  pc_[kTablePartials] = {}, ps_[kTablePartials] = {};
    float  rc_[kTablePartials] = {}, rs_[kTablePartials] = {};
    float  amp_[kTablePartials] = {}, ampStep_[kTablePartials] = {};
    int    active_ = 0;
    // FM
    double phC_ = 0.0, phM_ = 0.0;
    float  hpX_ = 0.0f, hpY_ = 0.0f;   // DC blocker state
    // Texture grains
    struct Grain { double pos = 0.0; double rate = 1.0; int len = 0; int age = 0; float gain = 0.0f; float pan = 0.0f; bool on = false; };
    Grain  grains_[kSlotGrains];
    double spawnIn_ = 0.0;   // seconds until the next grain
    Drifter posDrift_, idxDrift_;
    Rng    rng_;
    double sr_ = 48000.0;
    float  gL_ = 0.0f, gR_ = 0.0f;
    float  scratch_[64] = {};
    SourceType lastType_ = SourceType::Off;
};

} // namespace ambient
