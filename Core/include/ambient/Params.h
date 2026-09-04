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
    Unison, Detune, Drift, DriftRate, Spread,
    // Air (filtered-noise breath layer per voice)
    Air, AirColor, AirQ,
    // Amplitude envelope
    Attack, Decay, Sustain, Release,
    // Filter
    Cutoff, Resonance, FilterEnv, FilterDrift, KeyTrack,
    // Space: front-to-back planes, per-voice interaural time difference, hour-scale arc
    Depth, KeysDepth, PanDrift, Itd, ArcAmount, ArcPeriod,
    // Ensemble
    EnsembleMix, EnsembleDepth, EnsembleRate,
    // Stereo delay (asymmetric L/R)
    DelayTimeL, DelayTimeR, DelayFeedback, DelayCross, DelayDamp, DelayMix, DelayToFar,
    // Near reverb (foreground room)
    NearMix, NearDecay, NearDamp,
    // Far reverb (the infinite background)
    FarLevel, FarSize, FarDecay, FarDamp, FarPreDelay, FarAsym, FarHighcut, FarFreeze,
    // Mid/side master stage
    BassMono, SideAir, Width,
    // Cluster brain (generative sleep-concert mode)
    BrainOn, BrainDensity, BrainRate, BrainHoldMin, BrainHoldMax,
    BrainLow, BrainHigh, BrainConsonance, BrainWander,
    // Tuning
    Scale, KeyMap, RootNote, RefPitch, Seed,
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

} // namespace ambient
