#include "ambient/Params.h"
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

    F(ParamId::Depth,       "depth",        "Depth",         "Space",      0.f,   1.f,    0.7f,  1.f,  ""),
    F(ParamId::KeysDepth,   "keys_depth",   "Keys Depth",    "Space",      0.f,   1.f,    0.f,   1.f,  ""),
    F(ParamId::PanDrift,    "pan_drift",    "Pan Drift",     "Space",      0.f,   1.f,    0.4f,  1.f,  ""),
    F(ParamId::Itd,         "itd",          "Time Width",    "Space",      0.f,   1.f,    0.6f,  1.f,  ""),
    F(ParamId::ArcAmount,   "arc",          "Arc",           "Space",      0.f,   1.f,    0.3f,  1.f,  ""),
    F(ParamId::ArcPeriod,   "arc_period",   "Arc Period",    "Space",      2.f,   240.f,  40.f,  0.4f, "min"),

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
