#include "ambient/Params.h"

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

    F(ParamId::Attack,      "attack",       "Attack",        "Envelope",   0.01f, 60.f,   6.f,   0.3f, "s"),
    F(ParamId::Decay,       "decay",        "Decay",         "Envelope",   0.01f, 60.f,   4.f,   0.3f, "s"),
    F(ParamId::Sustain,     "sustain",      "Sustain",       "Envelope",   0.f,   1.f,    0.8f,  1.f,  ""),
    F(ParamId::Release,     "release",      "Release",       "Envelope",   0.05f, 120.f,  12.f,  0.3f, "s"),

    F(ParamId::Cutoff,      "cutoff",       "Cutoff",        "Filter",     40.f,  18000.f, 2500.f, 0.25f, "Hz"),
    F(ParamId::Resonance,   "resonance",    "Resonance",     "Filter",     0.f,   1.f,    0.15f, 1.f,  ""),
    F(ParamId::FilterEnv,   "filter_env",   "Env Amount",    "Filter",     -1.f,  1.f,    0.3f,  1.f,  ""),
    F(ParamId::FilterDrift, "filter_drift", "Drift",         "Filter",     0.f,   1.f,    0.3f,  1.f,  ""),
    F(ParamId::KeyTrack,    "keytrack",     "Key Track",     "Filter",     0.f,   1.f,    0.5f,  1.f,  ""),

    F(ParamId::EnsembleMix,   "ens_mix",   "Mix",   "Ensemble", 0.f,   1.f, 0.4f, 1.f,  ""),
    F(ParamId::EnsembleDepth, "ens_depth", "Depth", "Ensemble", 0.f,   1.f, 0.4f, 1.f,  ""),
    F(ParamId::EnsembleRate,  "ens_rate",  "Rate",  "Ensemble", 0.02f, 2.f, 0.2f, 0.4f, "Hz"),

    F(ParamId::ReverbMix,      "rev_mix",      "Mix",       "Reverb", 0.f,  1.f,   0.45f, 1.f,  ""),
    F(ParamId::ReverbSize,     "rev_size",     "Size",      "Reverb", 0.5f, 3.f,   1.6f,  1.f,  ""),
    F(ParamId::ReverbDecay,    "rev_decay",    "Decay",     "Reverb", 0.5f, 60.f,  12.f,  0.3f, "s"),
    F(ParamId::ReverbDamp,     "rev_damp",     "Damping",   "Reverb", 0.f,  1.f,   0.4f,  1.f,  ""),
    F(ParamId::ReverbPreDelay, "rev_predelay", "Pre-Delay", "Reverb", 0.f,  500.f, 40.f,  0.5f, "ms"),
    B(ParamId::ReverbFreeze,   "rev_freeze",   "Freeze",    "Reverb", false),
    F(ParamId::Width,          "width",        "Width",     "Reverb", 0.f,  2.f,   1.2f,  1.f,  ""),

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

} // namespace ambient
