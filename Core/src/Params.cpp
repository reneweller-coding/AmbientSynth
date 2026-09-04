#include "ambient/Params.h"
#include "ambient/Sources.h"   // choice names of the source slots
#include "ambient/ZPlane.h"    // choice names of the z-plane filter
#include <cstring>

namespace ambient {

const char* const kScaleNames[kNumScaleChoices] = {
    "12-TET",
    "JI Major (Ptolemy)",
    "JI Minor",
    "JI 7-limit",
    "Pythagorean",
    "JI Pentatonic",
    "Harmonic 8-16",
    "Subharmonic 16-8",
    "Slendro (JI)",
    "Bohlen-Pierce (JI)",
    "Otonality 1-11",
    "User (Scala)",
};

const char* const kRootNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* const kKeyMapNames[2] = { "Snap to 12 keys", "Consecutive degrees" };
const char* const kShimmerPitchNames[kNumShimmerPitches] = { "+12", "+7", "+5", "+19", "-12", "+24" };
const char* const kSubOctaveNames[2] = { "-1", "-2" };
const char* const kSubSourceNames[2] = { "Root", "Difference" };
const char* const kRoomSourceNames[2] = { "Far", "Near" };
const char* const kStackNames[kNumStacks] = { "Detune", "Octaves", "Fifths", "Major", "Minor", "Seventh", "Harmonics", "Subharmonics" };
// Strand ratios, ordered so that fewer strands still make sense (2 = root + fifth, 3 = a triad...).
const double kStackRatios[kNumStacks][6] = {
    { 1.0, 1.0,       1.0,       1.0,       1.0,       1.0       },   // Detune (classic unison)
    { 1.0, 2.0,       0.5,       4.0,       0.25,      1.0       },   // Octaves
    { 1.0, 1.5,       2.0,       3.0,       0.5,       2.25      },   // Fifths: 1 3/2 2 3 1/2 9/4
    { 1.0, 1.5,       1.25,      2.0,       2.5,       0.5       },   // Major: 1 3/2 5/4 2 5/2 1/2
    { 1.0, 1.5,       1.2,       2.0,       2.4,       0.5       },   // Minor: 1 3/2 6/5 2 12/5 1/2
    { 1.0, 1.5,       1.25,      1.75,      2.0,       0.5       },   // Seventh: 1 3/2 5/4 7/4 2 1/2
    { 1.0, 2.0,       3.0,       4.0,       5.0,       6.0       },   // Harmonics
    { 1.0, 0.5,       1.0 / 3.0, 0.25,      0.2,       1.0 / 6.0 },   // Subharmonics
};
const float kShimmerPitchSemitones[kNumShimmerPitches] = { 12.0f, 7.0f, 5.0f, 19.0f, -12.0f, 24.0f };

namespace {
using K = ParamKind;
constexpr ParamDesc F(ParamId id, const char* key, const char* name, const char* sec,
                      float mn, float mx, float def, float skew, const char* unit)
{ return { id, key, name, sec, K::Float, mn, mx, def, skew, unit, nullptr, 0 }; }
constexpr ParamDesc I(ParamId id, const char* key, const char* name, const char* sec,
                      float mn, float mx, float def, const char* unit = "")
{ return { id, key, name, sec, K::Int, mn, mx, def, 1.0f, unit, nullptr, 0 }; }
constexpr ParamDesc B(ParamId id, const char* key, const char* name, const char* sec, bool def)
{ return { id, key, name, sec, K::Bool, 0.0f, 1.0f, def ? 1.0f : 0.0f, 1.0f, "", nullptr, 0 }; }
constexpr ParamDesc C(ParamId id, const char* key, const char* name, const char* sec,
                      const char* const* choices, int n, int def)
{ return { id, key, name, sec, K::Choice, 0.0f, static_cast<float>(n - 1), static_cast<float>(def), 1.0f, "", choices, n }; }

const std::array<ParamDesc, kNumParams> kTable = {{
    F(ParamId::MasterGain,  "master_gain",  "Master",        "Master",     -40.f, 12.f,   -6.f,  1.f,  "dB"),

    F(ParamId::OscLevel,    "osc_level",    "Level",         "Oscillator", 0.f,   1.f,    1.f,   1.f,  ""),
    I(ParamId::Partials,    "partials",     "Partials",      "Oscillator", 1.f,   32.f,   16.f),
    F(ParamId::Tilt,        "tilt",         "Spectral Tilt", "Oscillator", 0.3f,  3.f,    1.2f,  1.f,  ""),
    F(ParamId::Brightness,  "brightness",   "Brightness",    "Oscillator", 0.f,   1.f,    0.7f,  1.f,  ""),
    F(ParamId::OddEven,     "odd_even",     "Odd / Even",    "Oscillator", -1.f,  1.f,    0.f,   1.f,  ""),
    F(ParamId::Inharmonic,  "inharmonic",   "Inharmonic",    "Oscillator", 0.f,   1.f,    0.f,   1.f,  ""),
    F(ParamId::Shimmer,     "shimmer",      "Shimmer",       "Oscillator", 0.f,   1.f,    0.4f,  1.f,  ""),
    F(ParamId::ShimmerRate, "shimmer_rate", "Shimmer Rate",  "Oscillator", 0.01f, 2.f,    0.15f, 0.4f, "Hz"),
    I(ParamId::Unison,      "strands",      "Strands",       "Oscillator", 1.f,   6.f,    3.f),
    F(ParamId::Detune,      "detune",       "Detune",        "Oscillator", 0.f,   50.f,   8.f,   0.6f, "ct"),
    F(ParamId::Drift,       "drift",        "Pitch Drift",   "Oscillator", 0.f,   30.f,   4.f,   0.6f, "ct"),
    F(ParamId::DriftRate,   "drift_rate",   "Drift Rate",    "Oscillator", 0.01f, 1.f,    0.08f, 0.4f, "Hz"),
    F(ParamId::Spread,      "spread",       "Stereo Spread", "Oscillator", 0.f,   1.f,    0.7f,  1.f,  ""),
    F(ParamId::Bloom,       "bloom",        "Bloom",         "Oscillator", 0.f,   1.f,    0.f,   1.f,  ""),
    F(ParamId::BloomTime,   "bloom_time",   "Bloom Time",    "Oscillator", 5.f,   300.f,  60.f,  0.4f, "s"),
    C(ParamId::Stack,       "stack",        "Stack",         "Oscillator", kStackNames, kNumStacks, 0),
    F(ParamId::RateWander,  "rate_wander",  "Rate Wander",   "Oscillator", 0.f,   1.f,    0.3f,  1.f,  ""),

    C(ParamId::Src2Type,     "src2_type",     "Type",      "Source 2", kSourceTypeNames, kNumSourceTypes, 0),
    F(ParamId::Src2Level,    "src2_level",    "Level",     "Source 2", 0.f,   1.f,    0.5f,   1.f,  ""),
    I(ParamId::Src2Octave,   "src2_octave",   "Octave",    "Source 2", -2.f,  2.f,    0.f),
    C(ParamId::Src2Ratio,    "src2_ratio",    "Ratio",     "Source 2", kSlotRatioNames, kNumSlotRatios, 0),
    F(ParamId::Src2Pan,      "src2_pan",      "Pan",       "Source 2", -1.f,  1.f,    -0.3f,  1.f,  ""),
    C(ParamId::Src2Table,    "src2_table",    "Table",     "Source 2", kTableNames, kNumTables, 0),
    F(ParamId::Src2Position, "src2_pos",      "Position",  "Source 2", 0.f,   1.f,    0.3f,   1.f,  ""),
    F(ParamId::Src2PosDrift, "src2_pos_drift","Pos Drift", "Source 2", 0.f,   1.f,    0.3f,   1.f,  ""),
    F(ParamId::Src2FmRatio,  "src2_fm_ratio", "FM Ratio",  "Source 2", 0.25f, 8.f,    2.f,    0.5f, "x"),
    F(ParamId::Src2FmIndex,  "src2_fm_index", "FM Index",  "Source 2", 0.f,   8.f,    1.f,    0.6f, ""),
    F(ParamId::Src2Grain,    "src2_grain",    "Grain",     "Source 2", 30.f,  1000.f, 200.f,  0.5f, "ms"),
    F(ParamId::Src2Density,  "src2_density",  "Density",   "Source 2", 1.f,   60.f,   12.f,   0.5f, "/s"),
    C(ParamId::Src2Follow,   "src2_follow",   "Pitch",     "Source 2", kFollowNames, 2, 0),

    C(ParamId::Src3Type,     "src3_type",     "Type",      "Source 3", kSourceTypeNames, kNumSourceTypes, 0),
    F(ParamId::Src3Level,    "src3_level",    "Level",     "Source 3", 0.f,   1.f,    0.5f,   1.f,  ""),
    I(ParamId::Src3Octave,   "src3_octave",   "Octave",    "Source 3", -2.f,  2.f,    0.f),
    C(ParamId::Src3Ratio,    "src3_ratio",    "Ratio",     "Source 3", kSlotRatioNames, kNumSlotRatios, 0),
    F(ParamId::Src3Pan,      "src3_pan",      "Pan",       "Source 3", -1.f,  1.f,    0.3f,   1.f,  ""),
    C(ParamId::Src3Table,    "src3_table",    "Table",     "Source 3", kTableNames, kNumTables, 0),
    F(ParamId::Src3Position, "src3_pos",      "Position",  "Source 3", 0.f,   1.f,    0.3f,   1.f,  ""),
    F(ParamId::Src3PosDrift, "src3_pos_drift","Pos Drift", "Source 3", 0.f,   1.f,    0.3f,   1.f,  ""),
    F(ParamId::Src3FmRatio,  "src3_fm_ratio", "FM Ratio",  "Source 3", 0.25f, 8.f,    2.f,    0.5f, "x"),
    F(ParamId::Src3FmIndex,  "src3_fm_index", "FM Index",  "Source 3", 0.f,   8.f,    1.f,    0.6f, ""),
    F(ParamId::Src3Grain,    "src3_grain",    "Grain",     "Source 3", 30.f,  1000.f, 200.f,  0.5f, "ms"),
    F(ParamId::Src3Density,  "src3_density",  "Density",   "Source 3", 1.f,   60.f,   12.f,   0.5f, "/s"),
    C(ParamId::Src3Follow,   "src3_follow",   "Pitch",     "Source 3", kFollowNames, 2, 0),

    F(ParamId::SubLevel,    "sub_level",    "Level",         "Foundation", 0.f,   1.f,    0.f,   1.f,  ""),
    C(ParamId::SubOctave,   "sub_octave",   "Octave",        "Foundation", kSubOctaveNames, 2, 0),
    F(ParamId::SubGlide,    "sub_glide",    "Glide",         "Foundation", 0.1f,  30.f,   8.f,   0.4f, "s"),
    F(ParamId::SubBinaural, "sub_binaural", "Binaural",      "Foundation", 0.f,   12.f,   0.f,   0.6f, "Hz"),
    F(ParamId::SubTone,     "sub_tone",     "Tone",          "Foundation", 0.f,   1.f,    0.2f,  1.f,  ""),
    C(ParamId::SubSource,   "sub_source",   "Source",        "Foundation", kSubSourceNames, 2, 0),
    F(ParamId::PadLowCut,   "pad_low_cut",  "Pad Low Cut",   "Foundation", 0.f,   300.f,  0.f,   0.6f, "Hz"),

    F(ParamId::Air,         "air",          "Air",           "Air",        0.f,   1.f,    0.15f, 1.f,  ""),
    F(ParamId::AirColor,    "air_color",    "Color",         "Air",        1.f,   16.f,   3.f,   0.5f, "x f0"),
    F(ParamId::AirQ,        "air_q",        "Q",             "Air",        1.f,   40.f,   10.f,  0.5f, ""),

    F(ParamId::Attack,      "attack",       "Attack",        "Envelope",   0.01f, 60.f,   6.f,   0.3f, "s"),
    F(ParamId::Decay,       "decay",        "Decay",         "Envelope",   0.01f, 60.f,   4.f,   0.3f, "s"),
    F(ParamId::Sustain,     "sustain",      "Sustain",       "Envelope",   0.f,   1.f,    0.8f,  1.f,  ""),
    F(ParamId::Release,     "release",      "Release",       "Envelope",   0.05f, 120.f,  12.f,  0.3f, "s"),

    F(ParamId::Cutoff,      "cutoff",       "Cutoff",        "Filter",     40.f,  18000.f, 2500.f, 0.25f, "Hz"),
    F(ParamId::Resonance,   "resonance",    "Resonance",     "Filter",     0.f,   1.f,    0.15f, 1.f,  ""),
    F(ParamId::FilterEnv,   "filter_env",   "Env Amount",    "Filter",     -1.f,  1.f,    0.3f,  1.f,  ""),
    F(ParamId::FilterDrift, "filter_drift", "Drift",         "Filter",     0.f,   1.f,    0.3f,  1.f,  ""),
    F(ParamId::KeyTrack,    "keytrack",     "Key Track",     "Filter",     0.f,   1.f,    0.5f,  1.f,  ""),

    C(ParamId::ZMode,       "z_mode",       "Mode",          "Z-Plane",    kZModeNames, 3, 0),
    C(ParamId::ZShape,      "z_shape",      "Shape",         "Z-Plane",    kZShapeNames, kZShapes, 0),
    F(ParamId::ZX,          "z_x",          "X",             "Z-Plane",    0.f,   1.f,    0.5f,  1.f,  ""),
    F(ParamId::ZY,          "z_y",          "Y",             "Z-Plane",    0.f,   1.f,    0.5f,  1.f,  ""),
    F(ParamId::ZRate,       "z_rate",       "Rate",          "Z-Plane",    0.005f, 1.f,   0.05f, 0.4f, "Hz"),
    F(ParamId::ZDepth,      "z_depth",      "Depth",         "Z-Plane",    0.f,   1.f,    0.5f,  1.f,  ""),
    F(ParamId::ZResonance,  "z_res",        "Resonance",     "Z-Plane",    0.f,   1.f,    0.5f,  1.f,  ""),
    F(ParamId::ZKeyTrack,   "z_keytrack",   "Key Track",     "Z-Plane",    0.f,   1.f,    0.f,   1.f,  ""),
    F(ParamId::ZMix,        "z_mix",        "Mix",           "Z-Plane",    0.f,   1.f,    0.7f,  1.f,  ""),

    F(ParamId::Depth,       "depth",        "Depth",         "Space",      0.f,   1.f,    0.7f,  1.f,  ""),
    F(ParamId::KeysDepth,   "keys_depth",   "Keys Depth",    "Space",      0.f,   1.f,    0.f,   1.f,  ""),
    F(ParamId::PanDrift,    "pan_drift",    "Pan Drift",     "Space",      0.f,   1.f,    0.4f,  1.f,  ""),
    F(ParamId::Itd,         "itd",          "Time Width",    "Space",      0.f,   1.f,    0.6f,  1.f,  ""),
    F(ParamId::ArcAmount,   "arc",          "Arc",           "Space",      0.f,   1.f,    0.3f,  1.f,  ""),
    F(ParamId::ArcPeriod,   "arc_period",   "Arc Period",    "Space",      2.f,   240.f,  40.f,  0.4f, "min"),
    F(ParamId::Presence,    "presence",     "Presence",      "Space",      0.f,   6.f,    0.f,   1.f,  "dB"),
    F(ParamId::Breath,      "breath",       "Breath",        "Space",      0.f,   1.f,    0.f,   1.f,  ""),
    F(ParamId::BreathRate,  "breath_rate",  "Breath Rate",   "Space",      0.005f, 0.2f,  0.03f, 0.5f, "Hz"),

    F(ParamId::EnsembleMix,   "ens_mix",   "Mix",   "Ensemble", 0.f,   1.f, 0.4f, 1.f,  ""),
    F(ParamId::EnsembleDepth, "ens_depth", "Depth", "Ensemble", 0.f,   1.f, 0.4f, 1.f,  ""),
    F(ParamId::EnsembleRate,  "ens_rate",  "Rate",  "Ensemble", 0.02f, 2.f, 0.2f, 0.4f, "Hz"),

    F(ParamId::DelayTimeL,    "dly_time_l",   "Time L",    "Delay", 0.02f, 4.f,   0.75f, 0.4f, "s"),
    F(ParamId::DelayTimeR,    "dly_time_r",   "Time R",    "Delay", 0.02f, 4.f,   1.1f,  0.4f, "s"),
    F(ParamId::DelayFeedback, "dly_feedback", "Feedback",  "Delay", 0.f,   0.95f, 0.5f,  1.f,  ""),
    F(ParamId::DelayCross,    "dly_cross",    "Cross",     "Delay", 0.f,   1.f,   0.3f,  1.f,  ""),
    F(ParamId::DelayDamp,     "dly_damp",     "Damping",   "Delay", 0.f,   1.f,   0.5f,  1.f,  ""),
    F(ParamId::DelayMix,      "dly_mix",      "Mix",       "Delay", 0.f,   1.f,   0.25f, 1.f,  ""),
    F(ParamId::DelayToFar,    "dly_to_far",   "To Far",    "Delay", 0.f,   1.f,   0.4f,  1.f,  ""),

    F(ParamId::Delay2TimeL,    "dly2_time_l",   "Time L",    "Delay 2", 0.02f, 4.f,   1.5f,  0.4f, "s"),
    F(ParamId::Delay2TimeR,    "dly2_time_r",   "Time R",    "Delay 2", 0.02f, 4.f,   2.2f,  0.4f, "s"),
    F(ParamId::Delay2Feedback, "dly2_feedback", "Feedback",  "Delay 2", 0.f,   0.95f, 0.4f,  1.f,  ""),
    F(ParamId::Delay2Cross,    "dly2_cross",    "Cross",     "Delay 2", 0.f,   1.f,   0.5f,  1.f,  ""),
    F(ParamId::Delay2Damp,     "dly2_damp",     "Damping",   "Delay 2", 0.f,   1.f,   0.7f,  1.f,  ""),
    F(ParamId::Delay2Mix,      "dly2_mix",      "Mix",       "Delay 2", 0.f,   1.f,   0.f,   1.f,  ""),
    F(ParamId::Delay2ToFar,    "dly2_to_far",   "To Far",    "Delay 2", 0.f,   1.f,   0.5f,  1.f,  ""),

    F(ParamId::NearMix,   "near_mix",   "Mix",     "Near Reverb", 0.f,  1.f, 0.2f, 1.f,  ""),
    F(ParamId::NearDecay, "near_decay", "Decay",   "Near Reverb", 0.2f, 6.f, 1.2f, 0.5f, "s"),
    F(ParamId::NearDamp,  "near_damp",  "Damping", "Near Reverb", 0.f,  1.f, 0.3f, 1.f,  ""),

    F(ParamId::FarLevel,    "far_level",    "Level",     "Far Reverb", 0.f,   1.f,     0.8f,   1.f,  ""),
    F(ParamId::FarSize,     "far_size",     "Size",      "Far Reverb", 0.5f,  3.f,     2.f,    1.f,  ""),
    F(ParamId::FarDecay,    "far_decay",    "Decay",     "Far Reverb", 1.f,   90.f,    25.f,   0.3f, "s"),
    F(ParamId::FarDamp,     "far_damp",     "Damping",   "Far Reverb", 0.f,   1.f,     0.5f,   1.f,  ""),
    F(ParamId::FarPreDelay, "far_predelay", "Pre-Delay", "Far Reverb", 0.f,   500.f,   60.f,   0.5f, "ms"),
    F(ParamId::FarAsym,     "far_asym",     "Asymmetry", "Far Reverb", 0.f,   1.f,     0.5f,   1.f,  ""),
    F(ParamId::FarHighcut,  "far_highcut",  "Tail Cut",  "Far Reverb", 500.f, 16000.f, 3500.f, 0.3f, "Hz"),
    B(ParamId::FarFreeze,   "far_freeze",   "Freeze",    "Far Reverb", false),

    F(ParamId::FeedbackBus,   "fb_bus",   "To Bus",   "Feedback", 0.f,   1.f,     0.f,    1.f,  ""),
    F(ParamId::FeedbackFm,    "fb_fm",    "To Pitch", "Feedback", 0.f,   1.f,     0.f,    1.f,  ""),
    F(ParamId::FeedbackTone,  "fb_tone",  "Tone",     "Feedback", 200.f, 8000.f,  1500.f, 0.4f, "Hz"),
    F(ParamId::FeedbackDrive, "fb_drive", "Drive",    "Feedback", 0.f,   1.f,     0.5f,   1.f,  ""),

    F(ParamId::RoomLevel,    "room_level",    "Level",     "Room", 0.f,   1.f,     0.f,    1.f,  ""),
    C(ParamId::RoomSource,   "room_source",   "Source",    "Room", kRoomSourceNames, 2, 0),
    F(ParamId::RoomPreDelay, "room_predelay", "Pre-Delay", "Room", 0.f,   300.f,   20.f,   0.5f, "ms"),
    F(ParamId::RoomHighcut,  "room_highcut",  "Tail Cut",  "Room", 500.f, 16000.f, 5000.f, 0.3f, "Hz"),

    F(ParamId::CosmosSend,        "cosmos_send",        "Send",        "Cosmos", 0.f,    1.f,    0.f,   1.f,  ""),
    F(ParamId::CosmosShift,       "cosmos_shift",       "Shift",       "Cosmos", -300.f, 300.f,  0.f,   1.f,  "Hz"),
    F(ParamId::CosmosShiftDrift,  "cosmos_shift_drift", "Shift Drift", "Cosmos", 0.f,    1.f,    0.3f,  1.f,  ""),
    F(ParamId::CosmosRes,         "cosmos_res",         "Resonator",   "Cosmos", 0.f,    1.f,    0.f,   1.f,  ""),
    F(ParamId::CosmosResPitch,    "cosmos_res_pitch",   "Res Pitch",   "Cosmos", 0.25f,  8.f,    2.f,   0.5f, "x root"),
    F(ParamId::CosmosResFeedback, "cosmos_res_fb",      "Res Feedback","Cosmos", 0.f,    0.97f,  0.8f,  1.f,  ""),
    F(ParamId::CosmosVowel,       "cosmos_vowel",       "Vowel",       "Cosmos", 0.f,    1.f,    0.f,   1.f,  ""),
    F(ParamId::CosmosVowelRate,   "cosmos_vowel_rate",  "Vowel Rate",  "Cosmos", 0.005f, 0.5f,   0.05f, 0.4f, "Hz"),
    F(ParamId::CosmosNebula,      "cosmos_nebula",      "Nebula",      "Cosmos", 0.f,    1.f,    0.f,   1.f,  ""),
    F(ParamId::CosmosSmear,       "cosmos_smear",       "Smear",       "Cosmos", 0.f,    1.f,    0.7f,  1.f,  ""),
    F(ParamId::CosmosShimmer,     "cosmos_shimmer",     "Shimmer",     "Cosmos", 0.f,    1.f,    0.f,   1.f,  ""),
    C(ParamId::CosmosShimmerPitch,"cosmos_shimmer_pitch","Shimmer Pitch","Cosmos", kShimmerPitchNames, kNumShimmerPitches, 0),
    F(ParamId::CosmosReturn,      "cosmos_return",      "Return",      "Cosmos", 0.f,    1.f,    0.5f,  1.f,  ""),
    F(ParamId::CosmosToFar,       "cosmos_to_far",      "To Far",      "Cosmos", 0.f,    1.f,    0.5f,  1.f,  ""),

    F(ParamId::CloudSend,    "cloud_send",    "Send",    "Cloud", 0.f,   1.f,    0.f,   1.f,  ""),
    F(ParamId::CloudDensity, "cloud_density", "Density", "Cloud", 1.f,   60.f,   12.f,  0.5f, "/s"),
    F(ParamId::CloudSize,    "cloud_size",    "Grain",   "Cloud", 30.f,  800.f,  250.f, 0.5f, "ms"),
    F(ParamId::CloudPitch,   "cloud_pitch",   "Pitch",   "Cloud", 0.f,   1.f,    0.3f,  1.f,  ""),
    F(ParamId::CloudSpray,   "cloud_spray",   "Spray",   "Cloud", 0.05f, 2.f,    0.8f,  0.5f, "s"),
    F(ParamId::CloudLevel,   "cloud_level",   "Level",   "Cloud", 0.f,   1.f,    0.7f,  1.f,  ""),

    F(ParamId::BassMono, "bass_mono", "Bass Mono", "Master", 40.f, 300.f, 150.f, 0.5f, "Hz"),
    F(ParamId::SideAir,  "side_air",  "Side Air",  "Master", 0.f,  6.f,   2.f,   1.f,  "dB"),
    F(ParamId::Width,    "width",     "Width",     "Master", 0.f,  2.f,   1.2f,  1.f,  ""),

    B(ParamId::BrainOn,         "brain_on",         "Active",     "Cluster Brain", true),
    I(ParamId::BrainDensity,    "brain_density",    "Density",    "Cluster Brain", 1.f,  10.f,  5.f),
    F(ParamId::BrainRate,       "brain_rate",       "Event Rate", "Cluster Brain", 2.f,  300.f, 25.f, 0.4f, "s"),
    F(ParamId::BrainHoldMin,    "brain_hold_min",   "Hold Min",   "Cluster Brain", 5.f,  600.f, 30.f, 0.4f, "s"),
    F(ParamId::BrainHoldMax,    "brain_hold_max",   "Hold Max",   "Cluster Brain", 5.f,  600.f, 120.f, 0.4f, "s"),
    I(ParamId::BrainLow,        "brain_low",        "Lowest",     "Cluster Brain", 24.f, 96.f,  36.f),
    I(ParamId::BrainHigh,       "brain_high",       "Highest",    "Cluster Brain", 24.f, 108.f, 79.f),
    F(ParamId::BrainConsonance, "brain_consonance", "Consonance", "Cluster Brain", 0.f,  1.f,   0.7f, 1.f, ""),
    F(ParamId::BrainWander,     "brain_wander",     "Wander",     "Cluster Brain", 0.f,  1.f,   0.3f, 1.f, ""),

    C(ParamId::Scale,     "scale",     "Scale",     "Tuning", kScaleNames, kNumScaleChoices, 3),
    C(ParamId::KeyMap,    "keymap",    "Keys",      "Tuning", kKeyMapNames, 2, 0),
    C(ParamId::RootNote,  "root",      "Root",      "Tuning", kRootNames, 12, 2),
    F(ParamId::RefPitch,  "ref_pitch", "A4",        "Tuning", 415.f, 466.f, 440.f, 1.f, "Hz"),
    I(ParamId::Seed,      "seed",      "Seed",      "Tuning", 0.f, 9999.f, 1.f),
    B(ParamId::Hold,      "hold",      "Hold",      "Tuning", false),
    F(ParamId::TunePurity,    "purity",       "Purity",       "Tuning",     0.f,    1.f,   1.f,   1.f,  ""),
    F(ParamId::TuneDrift,     "purity_drift", "Purity Drift", "Tuning",     0.f,    1.f,   0.f,   1.f,  ""),
    F(ParamId::TuneDriftRate, "purity_rate",  "Drift Rate",   "Tuning",     0.002f, 0.1f,  0.01f, 0.5f, "Hz"),
    B(ParamId::Freeze,        "freeze",       "Freeze",       "Oscillator", false),

    B(ParamId::MorphActive, "morph_active", "Active",   "Morph", false),
    F(ParamId::MorphPos,    "morph",        "Position", "Morph", 0.f, 1.f,   0.f,  1.f,  ""),
    F(ParamId::MorphGlide,  "morph_glide",  "Glide",    "Morph", 0.f, 900.f, 30.f, 0.4f, "s"),

    F(ParamId::MacroA, "macro_a", "Space",  "Macros", 0.f, 1.f, 0.f, 1.f, ""),
    F(ParamId::MacroB, "macro_b", "Alien",  "Macros", 0.f, 1.f, 0.f, 1.f, ""),
    F(ParamId::MacroC, "macro_c", "Motion", "Macros", 0.f, 1.f, 0.f, 1.f, ""),
    F(ParamId::MacroD, "macro_d", "Bloom",  "Macros", 0.f, 1.f, 0.f, 1.f, ""),
    F(ParamId::MacroE, "macro_e", "Density",   "Macros", 0.f, 1.f, 0.f, 1.f, ""),
    F(ParamId::MacroF, "macro_f", "Distance",  "Macros", 0.f, 1.f, 0.f, 1.f, ""),
    F(ParamId::MacroG, "macro_g", "Evolution", "Macros", 0.f, 1.f, 0.f, 1.f, ""),
    F(ParamId::MacroH, "macro_h", "Air",       "Macros", 0.f, 1.f, 0.f, 1.f, ""),

    B(ParamId::MapActive, "map_active", "Active", "Map", false),
    F(ParamId::MapX,      "map_x",      "X",      "Map", 0.f,   1.f,  0.5f,  1.f, ""),
    F(ParamId::MapY,      "map_y",      "Y",      "Map", 0.f,   1.f,  0.5f,  1.f, ""),
    F(ParamId::MapRadius, "map_radius", "Radius", "Map", 0.02f, 0.4f, 0.08f, 1.f, ""),

    B(ParamId::RouteActive, "route_active", "Play",  "Route", false),
    F(ParamId::RouteSpeed,  "route_speed",  "Speed", "Route", 0.25f, 4.f, 1.f, 0.5f, "x"),
    B(ParamId::RouteLoop,   "route_loop",   "Loop",  "Route", true),
}};
} // namespace

const std::array<ParamDesc, kNumParams>& paramTable() { return kTable; }

const ParamDesc* findParam(const char* key)
{
    if (key == nullptr) return nullptr;
    for (const auto& d : kTable) if (std::strcmp(key, d.key) == 0) return &d;
    return nullptr;
}

} // namespace ambient
