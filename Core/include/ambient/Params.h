// AmbientSynth -- parameter table (single source of truth).
// The core engine, the plugin host layer, the GUI and the render tool all read this table.
// No framework dependencies here: this header must compile on Quest/Android.
#pragma once
#include <array>
#include <cstddef>

namespace ambient {

enum class ParamId : int {
    // Master
    MasterGain,
    // Oscillator (additive partial bank per strand = Source 1)
    OscLevel, Partials, Tilt, Brightness, OddEven, Inharmonic, Shimmer, ShimmerRate,
    Unison, Detune, Drift, DriftRate, Spread, Bloom, BloomTime,
    // Stack: strands at pure ratios (one key = one just chord) instead of detuned copies;
    // Rate Wander: every voice's drift and shimmer rates themselves wander (nested LFO)
    Stack, RateWander,
    // Source 2 / Source 3: extra sources per voice (wavetable of spectra, FM pair, texture)
    Src2Type, Src2Level, Src2Octave, Src2Ratio, Src2Pan, Src2Table, Src2Position, Src2PosDrift,
    Src2FmRatio, Src2FmIndex, Src2Grain, Src2Density, Src2Follow,
    Src3Type, Src3Level, Src3Octave, Src3Ratio, Src3Pan, Src3Table, Src3Position, Src3PosDrift,
    Src3FmRatio, Src3FmIndex, Src3Grain, Src3Density, Src3Follow,
    // Foundation: a sub voice that follows the brain's root or the ghost tone (difference
    // tone of the two lowest sounding voices); Pad Low Cut keeps the pads out of its register
    SubLevel, SubOctave, SubGlide, SubBinaural, SubTone, SubSource, PadLowCut,
    // Air (filtered-noise breath layer per voice)
    Air, AirColor, AirQ,
    // Amplitude envelope
    Attack, Decay, Sustain, Release,
    // Filter
    Cutoff, Resonance, FilterEnv, FilterDrift, KeyTrack,
    // Space: front-to-back planes, per-voice interaural time difference, hour-scale arc,
    // presence bell for the near plane, slow breathing of every voice's distance
    Depth, KeysDepth, PanDrift, Itd, ArcAmount, ArcPeriod, Presence, Breath, BreathRate,
    // Ensemble
    EnsembleMix, EnsembleDepth, EnsembleRate,
    // Stereo delay (asymmetric L/R)
    DelayTimeL, DelayTimeR, DelayFeedback, DelayCross, DelayDamp, DelayMix, DelayToFar,
    // Second stereo delay, in series after the first
    Delay2TimeL, Delay2TimeR, Delay2Feedback, Delay2Cross, Delay2Damp, Delay2Mix, Delay2ToFar,
    // Near reverb (foreground room)
    NearMix, NearDecay, NearDamp,
    // Far reverb (the infinite background)
    FarLevel, FarSize, FarDecay, FarDamp, FarPreDelay, FarAsym, FarHighcut, FarFreeze,
    // Feedback: the mixed output (before the master) returns, low-passed and saturated,
    // into the near bus before the filters and effects, and/or as phase modulation of every
    // partial. Throttled by the output level so it hisses and holds instead of running away.
    FeedbackBus, FeedbackFm, FeedbackTone, FeedbackDrive,
    // Room: convolution reverb with a loaded (or generated) impulse, in parallel on the far plane
    RoomLevel, RoomSource, RoomPreDelay, RoomHighcut,
    // Cosmos: science-fiction / deep-space path (send from the near bus)
    CosmosSend, CosmosShift, CosmosShiftDrift, CosmosRes, CosmosResPitch, CosmosResFeedback,
    CosmosVowel, CosmosVowelRate, CosmosNebula, CosmosSmear, CosmosShimmer, CosmosShimmerPitch,
    CosmosReturn, CosmosToFar,
    // Granular cloud on the far plane
    CloudSend, CloudDensity, CloudSize, CloudPitch, CloudSpray, CloudLevel,
    // Mid/side master stage
    BassMono, SideAir, Width,
    // Cluster brain (generative sleep-concert mode)
    BrainOn, BrainDensity, BrainRate, BrainHoldMin, BrainHoldMax,
    BrainLow, BrainHigh, BrainConsonance, BrainWander,
    // Tuning
    Scale, KeyMap, RootNote, RefPitch, Seed, Hold,
    // Morph between two stored full presets (A/B); never part of a preset itself
    MorphActive, MorphPos, MorphGlide,
    // Macros: eight performance controls routed through the gesture layer (Custom0..7);
    // not part of presets. Named in Rich's vocabulary, not the engine's.
    MacroA, MacroB, MacroC, MacroD, MacroE, MacroF, MacroG, MacroH,
    // Preset map: a cursor in the plane of all presets blends its neighbours (PresetMap.h);
    // performance state like the morph, never part of a preset
    MapActive, MapX, MapY, MapRadius,
    Count
};

constexpr int kNumParams = static_cast<int>(ParamId::Count);

enum class ParamKind { Float, Int, Bool, Choice };

struct ParamDesc {
    ParamId     id;
    const char* key;      // stable identifier (automation / presets)
    const char* name;     // display name
    const char* section;  // GUI grouping
    ParamKind   kind;
    float       min;
    float       max;
    float       def;
    float       skew;     // 1 = linear, <1 = more resolution at the low end
    const char* unit;
    const char* const* choices; // ParamKind::Choice only
    int         numChoices;
};

// Ordered by ParamId. Verified by the self test.
const std::array<ParamDesc, kNumParams>& paramTable();
inline const ParamDesc& paramDesc(ParamId id) { return paramTable()[static_cast<size_t>(id)]; }
const ParamDesc* findParam(const char* key);   // nullptr if unknown

// Names used by the Scale choice parameter; index == built-in scale index,
// the last entry is the user slot filled by a loaded Scala file.
constexpr int kNumScaleChoices = 12;
extern const char* const kScaleNames[kNumScaleChoices];
extern const char* const kRootNames[12];
extern const char* const kKeyMapNames[2];   // 0 = snap 12 keys/octave to nearest degree, 1 = consecutive degrees
extern const char* const kSubOctaveNames[2];   // "-1", "-2"
extern const char* const kSubSourceNames[2];   // "Root", "Difference" (ghost tone of the two lowest voices)
extern const char* const kRoomSourceNames[2];  // "Far", "Near": what the convolution room reverberates
constexpr int kNumStacks = 8;
extern const char* const kStackNames[kNumStacks];   // Detune, Octaves, Fifths, Major, Minor, Seventh, Harmonics, Subharmonics
extern const double kStackRatios[kNumStacks][6];    // ratio of strand 0..5 to the note (Detune = all 1)
constexpr int kNumShimmerPitches = 6;
extern const char* const kShimmerPitchNames[kNumShimmerPitches];
extern const float kShimmerPitchSemitones[kNumShimmerPitches];

} // namespace ambient
