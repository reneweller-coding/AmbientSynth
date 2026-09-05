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
    // Source 1: Type chooses the additive strand bank (the classic Oscillator, default) or any of
    // the slot types below; then the bank's own spectrum parameters
    Src1Type,
    OscLevel, Partials, Tilt, Brightness, OddEven, Inharmonic, Shimmer, ShimmerRate,
    Unison, Detune, Drift, DriftRate, Spread, Bloom, BloomTime,
    // Stack: strands at pure ratios (one key = one just chord) instead of detuned copies;
    // Rate Wander: every voice's drift and shimmer rates themselves wander (nested LFO)
    Stack, RateWander,
    // Source 1's slot fields, for the non-additive types (level is OscLevel, spectrum the eight above)
    Src1Octave, Src1Ratio, Src1Pan, Src1Table, Src1Position, Src1PosDrift,
    Src1FmRatio, Src1FmIndex, Src1Grain, Src1Density, Src1Follow, Src1Grains, Src1Spread, Src1Noise, Src1NoiseQ,
    // Source 2 / Source 3: the same slot, laid out identically (the engine reads them by offset):
    // 17 slot fields, then the seven of the slot's own additive bank
    Src2Type, Src2Level, Src2Octave, Src2Ratio, Src2Pan, Src2Table, Src2Position, Src2PosDrift,
    Src2FmRatio, Src2FmIndex, Src2Grain, Src2Density, Src2Follow, Src2Grains, Src2Spread, Src2Noise, Src2NoiseQ,
    Src2Partials, Src2Tilt, Src2Bright, Src2OddEven, Src2Inharm, Src2Shimmer, Src2ShimmerRate,
    Src3Type, Src3Level, Src3Octave, Src3Ratio, Src3Pan, Src3Table, Src3Position, Src3PosDrift,
    Src3FmRatio, Src3FmIndex, Src3Grain, Src3Density, Src3Follow, Src3Grains, Src3Spread, Src3Noise, Src3NoiseQ,
    Src3Partials, Src3Tilt, Src3Bright, Src3OddEven, Src3Inharm, Src3Shimmer, Src3ShimmerRate,
    // Foundation: a sub voice that follows the brain's root or the ghost tone (difference
    // tone of the two lowest sounding voices); Pad Low Cut keeps the pads out of its register
    SubLevel, SubOctave, SubGlide, SubBinaural, SubTone, SubSource, PadLowCut,
    // Air (filtered-noise breath layer per voice)
    Air, AirColor, AirQ,
    // Amplitude envelope
    Attack, Decay, Sustain, Release,
    // Filter: one of nine models (Filter.h) behind the same knobs; Drive saturates ahead of it
    FilterModel, Cutoff, Resonance, FilterEnv, FilterDrift, KeyTrack, FilterDrive,
    // Z-plane filter: four frames on a square, the point (X, Y) interpolates their poles and wanders
    ZMode, ZShape, ZX, ZY, ZRate, ZDepth, ZResonance, ZKeyTrack, ZMix,
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
    // Purity blends every note between 12-TET (0) and the chosen scale (1) in the log domain; Drift
    // lets that blend wander so the beating locks in and loosens over minutes; Freeze holds every
    // voice's spectrum and pitch still (drifts, shimmer, bloom stop moving)
    TunePurity, TuneDrift, TuneDriftRate, Freeze,
    // Ghost: the Air noise through a bank of sharp resonators on the note's just harmonics
    AirMode,
    // Portamento for keys: a new key glides from the last one; Gravity slows the glide near
    // consonant ratios to the root, so the slide "clicks into" harmonic nodes on the way
    Portamento, PortaGravity,
    // Tape in the feedback loop: asymmetric saturation, wow and flutter, level-dependent noise floor
    FeedbackTape,
    // Coherence: four slow Kuramoto oscillators, coupled by Coherence, modulating brightness,
    // depth, pan drift and the z-plane point by Depth
    Coherence, CoherenceDepth, CoherenceRate,
    // Modulation: eight free LFOs and six multi-segment envelopes. Their shapes and the matrix
    // rows are data, not parameters (see Modulation.h); what sits here is what a host automates.
    Lfo1Shape, Lfo1Rate, Lfo1Phase, Lfo1Depth, Lfo1Mode, Lfo1Table,
    Lfo2Shape, Lfo2Rate, Lfo2Phase, Lfo2Depth, Lfo2Mode, Lfo2Table,
    Lfo3Shape, Lfo3Rate, Lfo3Phase, Lfo3Depth, Lfo3Mode, Lfo3Table,
    Lfo4Shape, Lfo4Rate, Lfo4Phase, Lfo4Depth, Lfo4Mode, Lfo4Table,
    Lfo5Shape, Lfo5Rate, Lfo5Phase, Lfo5Depth, Lfo5Mode, Lfo5Table,
    Lfo6Shape, Lfo6Rate, Lfo6Phase, Lfo6Depth, Lfo6Mode, Lfo6Table,
    Lfo7Shape, Lfo7Rate, Lfo7Phase, Lfo7Depth, Lfo7Mode, Lfo7Table,
    Lfo8Shape, Lfo8Rate, Lfo8Phase, Lfo8Depth, Lfo8Mode, Lfo8Table,
    Env1Mode, Env1Time, Env1Depth,
    Env2Mode, Env2Time, Env2Depth,
    Env3Mode, Env3Time, Env3Depth,
    Env4Mode, Env4Time, Env4Depth,
    Env5Mode, Env5Time, Env5Depth,
    Env6Mode, Env6Time, Env6Depth,
    // Morph between two stored full presets (A/B); never part of a preset itself
    MorphActive, MorphPos, MorphGlide,
    // Macros: eight performance controls routed through the gesture layer (Custom0..7);
    // not part of presets. Named in Rich's vocabulary, not the engine's.
    MacroA, MacroB, MacroC, MacroD, MacroE, MacroF, MacroG, MacroH,
    // Inertia: every float parameter glides to its value with this time constant (the analogue
    // slew), so even a knob torn open arrives slowly; performance state, not in presets
    Inertia,
    // Preset map: a cursor in the plane of all presets blends its neighbours (PresetMap.h);
    // performance state like the morph, never part of a preset
    MapActive, MapX, MapY, MapRadius,
    // Route: the engine walks a route of waypoints over the map (Route.h); performance state
    RouteActive, RouteSpeed, RouteLoop,
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
extern const char* const kAirModeNames[2];     // "Band" (one band-pass) or "Ghost" (resonators on the just harmonics)
constexpr int kNumStacks = 8;
extern const char* const kStackNames[kNumStacks];   // Detune, Octaves, Fifths, Major, Minor, Seventh, Harmonics, Subharmonics
extern const double kStackRatios[kNumStacks][6];    // ratio of strand 0..5 to the note (Detune = all 1)
constexpr int kNumShimmerPitches = 6;
extern const char* const kShimmerPitchNames[kNumShimmerPitches];
extern const float kShimmerPitchSemitones[kNumShimmerPitches];

} // namespace ambient
