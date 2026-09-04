#include "ambient/Presets.h"
#include <cstring>
#include <cstdlib>

namespace ambient {

namespace {
const Preset kPresets[] = {
    { "Init", "" },

    { "Sleep Concert",
      "brain_density=6;brain_rate=35;brain_hold_min=60;brain_hold_max=240;depth=0.85;"
      "far_decay=35;far_highcut=2500;air=0.2;arc=0.5;arc_period=60;brightness=0.6;"
      "attack=10;release=25" },

    { "Glass Cathedral",
      "partials=24;tilt=0.8;brightness=0.95;inharmonic=0.15;cutoff=6000;far_size=3;"
      "far_decay=45;far_highcut=7000;far_damp=0.3;ens_mix=0.6;shimmer=0.6;depth=0.6;"
      "scale=Harmonic 8-16;root=E" },

    { "Subharmonic Deep",
      "scale=Subharmonic 16-8;brain_low=28;brain_high=60;tilt=1.8;cutoff=900;bass_mono=200;"
      "brain_density=4;partials=10;far_decay=20;far_highcut=1800;air=0.05;strands=4;detune=12" },

    { "Breath of Flutes",
      "air=0.5;air_color=2;air_q=14;partials=6;tilt=1.5;keys_depth=0;depth=0.5;brain_low=55;"
      "brain_high=84;near_mix=0.3;far_level=0.6;dly_mix=0.3;dly_feedback=0.4;attack=3;release=8;"
      "master_gain=-10" },

    { "Bohlen Night",
      "scale=Bohlen-Pierce (JI);keymap=Consecutive degrees;brain_consonance=0.4;brain_wander=0.5;"
      "brain_low=40;brain_high=80;odd_even=0.6;far_decay=30;depth=0.75" },

    { "Otonal Shimmer",
      "scale=Otonality 1-11;shimmer=0.9;shimmer_rate=0.4;dly_time_l=0.45;dly_time_r=0.68;"
      "dly_cross=0.7;dly_feedback=0.6;dly_mix=0.35;brightness=0.8;depth=0.6;brain_density=6" },

    { "Distant Storm",
      "depth=1;keys_depth=0.8;brain_consonance=0.15;brain_density=8;brain_rate=12;brain_hold_min=20;"
      "brain_hold_max=90;far_decay=60;far_level=1;far_highcut=2000;dly_feedback=0.8;dly_to_far=0.8;"
      "dly_mix=0.15;cutoff=1500;inharmonic=0.3;strands=5;detune=20" },

    { "Dry Foreground Keys",
      "brain_on=off;keys_depth=0;near_mix=0.25;near_decay=1.5;far_level=0.3;dly_mix=0.2;"
      "ens_mix=0.5;attack=1.5;release=6;air=0.25;depth=0" },
};
}

int numPresets() { return static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0])); }
const Preset& preset(int index)
{
    const int n = numPresets();
    if (index < 0 || index >= n) index = 0;
    return kPresets[index];
}

float paramValueFromText(const ParamDesc& d, const char* text)
{
    if (text == nullptr) return d.def;
    if (d.kind == ParamKind::Choice)
        for (int i = 0; i < d.numChoices; ++i) if (std::strcmp(text, d.choices[i]) == 0) return static_cast<float>(i);
    if (d.kind == ParamKind::Bool) {
        if (!std::strcmp(text, "on") || !std::strcmp(text, "true") || !std::strcmp(text, "yes")) return 1.0f;
        if (!std::strcmp(text, "off") || !std::strcmp(text, "false") || !std::strcmp(text, "no")) return 0.0f;
    }
    const float v = static_cast<float>(std::atof(text));
    return v < d.min ? d.min : (v > d.max ? d.max : v);
}

} // namespace ambient
