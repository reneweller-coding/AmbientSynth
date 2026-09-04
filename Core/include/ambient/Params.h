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
    // Oscillator (additive partial bank per strand)
    Partials, Tilt, Brightness, OddEven, Inharmonic, Shimmer, ShimmerRate,
    Unison, Detune, Drift, DriftRate, Spread, Bloom, BloomTime,
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
    // Macros: four performance controls routed through the gesture layer; not part of presets
    MacroA, MacroB, MacroC, MacroD,
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
constexpr int kNumShimmerPitches = 6;
extern const char* const kShimmerPitchNames[kNumShimmerPitches];
extern const float kShimmerPitchSemitones[kNumShimmerPitches];

} // namespace ambient
