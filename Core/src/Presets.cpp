#include "ambient/Presets.h"
#include <cstring>
#include <cstdlib>

namespace ambient {

namespace {
// 148 presets in twelve families. Unspecified parameters keep their defaults
// (see Params.cpp); the Cosmos send is off unless a preset turns it on.
const Preset kPresets[] = {
    // ---------------------------------------------------------------- 0..9 originals
    { "Init", "" },
    { "Sleep Concert",
      "brain_density=6;brain_rate=35;brain_hold_min=60;brain_hold_max=240;depth=0.85;far_decay=35;far_highcut=2500;"
      "air=0.2;arc=0.5;arc_period=60;brightness=0.6;attack=10;release=25;sub_level=0.35;sub_binaural=4;bloom=0.5;bloom_time=90;"
      "sub_source=Difference;breath=0.3;pad_low_cut=90" },
    { "Glass Cathedral",
      "partials=24;tilt=0.8;brightness=0.95;inharmonic=0.15;cutoff=6000;far_size=3;far_decay=45;far_highcut=7000;"
      "far_damp=0.3;ens_mix=0.6;shimmer=0.6;depth=0.6;scale=Harmonic 8-16;root=E" },
    { "Subharmonic Deep",
      "scale=Subharmonic 16-8;brain_low=28;brain_high=60;tilt=1.8;cutoff=900;bass_mono=200;brain_density=4;partials=10;"
      "far_decay=20;far_highcut=1800;air=0.05;strands=4;detune=12" },
    { "Breath of Flutes",
      "air=0.5;air_color=2;air_q=14;partials=6;tilt=1.5;keys_depth=0;depth=0.5;brain_low=55;brain_high=84;near_mix=0.3;"
      "far_level=0.6;dly_mix=0.3;dly_feedback=0.4;attack=3;release=8;master_gain=-10" },
    { "Bohlen Night",
      "scale=Bohlen-Pierce (JI);keymap=Consecutive degrees;brain_consonance=0.4;brain_wander=0.5;brain_low=40;brain_high=80;"
      "odd_even=0.6;far_decay=30;depth=0.75" },
    { "Otonal Shimmer",
      "scale=Otonality 1-11;shimmer=0.9;shimmer_rate=0.4;dly_time_l=0.45;dly_time_r=0.68;dly_cross=0.7;dly_feedback=0.6;"
      "dly_mix=0.35;brightness=0.8;depth=0.6;brain_density=6" },
    { "Distant Storm",
      "depth=1;keys_depth=0.8;brain_consonance=0.15;brain_density=8;brain_rate=12;brain_hold_min=20;brain_hold_max=90;"
      "far_decay=60;far_level=1;far_highcut=2000;dly_feedback=0.8;dly_to_far=0.8;dly_mix=0.15;cutoff=1500;inharmonic=0.3;"
      "strands=5;detune=20;master_gain=-9;fb_bus=0.25;fb_drive=0.8;fb_tone=1200" },
    { "Dry Foreground Keys",
      "brain_on=off;keys_depth=0;near_mix=0.25;near_decay=1.5;far_level=0.3;dly_mix=0.2;ens_mix=0.5;attack=1.5;release=6;"
      "air=0.25;depth=0;presence=3;strands=3;stack=Major;detune=0;drift=1" },
    { "Sleep Concert II",
      "scale=JI Minor;root=A;brain_density=5;brain_rate=45;brain_hold_min=90;brain_hold_max=300;depth=0.9;far_decay=40;"
      "far_highcut=2000;arc=0.6;arc_period=90;attack=12;release=30;sub_level=0.3;sub_source=Difference;breath=0.35;breath_rate=0.02" },

    // ---------------------------------------------------------------- 10..19 sleep / night
    { "Midnight Pentatonic",
      "scale=JI Pentatonic;root=F#;brain_density=4;brain_low=40;brain_high=76;air=0.25;far_decay=30;attack=8;release=20" },
    { "Deep Sleep Sub",
      "root=C;brain_low=24;brain_high=55;tilt=2;cutoff=600;bass_mono=220;brain_density=3;far_decay=50;far_highcut=1200;"
      "master_gain=-4;attack=15;release=40;sub_level=0.6;sub_octave=-2;sub_binaural=2;sub_glide=15" },
    { "Dawn Drift",
      "brightness=0.85;arc=1;arc_period=120;brain_density=6;scale=JI Major (Ptolemy);root=D;far_decay=20;air=0.15" },
    { "Night Rain",
      "air=0.45;air_color=6;air_q=6;partials=8;brain_density=5;dly_mix=0.3;dly_feedback=0.6;dly_damp=0.8;far_decay=15;"
      "master_gain=-8" },
    { "Slow Tide",
      "arc=1;arc_period=30;brain_density=6;brain_rate=20;brain_hold_min=40;brain_hold_max=160;depth=0.8;far_decay=30" },
    { "Hypnos",
      "attack=30;release=60;brain_hold_min=120;brain_hold_max=400;brain_rate=60;brain_density=7;far_decay=60;far_highcut=1800;"
      "depth=0.9;sub_level=0.4;sub_binaural=6;bloom=0.8;bloom_time=180;breath=0.5;breath_rate=0.015" },
    { "Warm Blanket",
      "tilt=1.6;brightness=0.5;cutoff=1200;air=0.1;scale=JI Major (Ptolemy);root=G;near_mix=0.35;far_level=0.5;depth=0.4;breath=0.4" },
    { "Somnus Harmonics",
      "scale=Harmonic 8-16;root=C;brain_low=48;brain_high=84;partials=12;shimmer=0.6;depth=0.7;far_decay=30;"
      "strands=4;stack=Harmonics;detune=0;drift=1;brain_density=3" },
    { "Breathing Dark",
      "scale=JI Minor;root=E;odd_even=0.5;tilt=1.4;shimmer=0.8;shimmer_rate=0.05;brain_density=5;far_decay=35;far_highcut=2200;"
      "z_mode=Series;z_shape=Wood;z_rate=0.015;z_depth=1;z_mix=0.5" },
    { "Velvet Hours",
      "tilt=1.5;brightness=0.55;cutoff=1500;strands=4;detune=6;air=0.12;brain_density=5;brain_rate=40;brain_hold_min=80;"
      "brain_hold_max=260;far_decay=38;far_highcut=2400;depth=0.8;arc=0.4;arc_period=75;pad_low_cut=100;sub_level=0.25;sub_source=Difference" },

    // ---------------------------------------------------------------- 20..29 cathedral / glass
    { "Ice Cathedral",
      "partials=32;tilt=0.7;brightness=1;inharmonic=0.25;cutoff=9000;far_size=3;far_decay=60;far_highcut=9000;far_damp=0.2;"
      "brain_density=5;brain_low=55;brain_high=96" },
    { "Glass Choir",
      "partials=20;air=0.3;air_color=4;air_q=20;ens_mix=0.7;far_decay=30;far_highcut=6000;scale=JI Major (Ptolemy);root=A;"
      "pad_low_cut=120;presence=2" },
    { "Crystal Bells",
      "inharmonic=0.5;partials=24;tilt=0.9;attack=0.5;decay=20;sustain=0.3;release=20;brain_rate=8;brain_hold_min=5;"
      "brain_hold_max=20;brain_density=6;far_decay=25;far_highcut=8000" },
    { "Wineglass Rim",
      "partials=3;tilt=0.5;strands=6;detune=3;air=0.35;air_color=1;air_q=30;far_decay=30;brain_low=60;brain_high=90;"
      "far_highcut=8000;master_gain=-9" },
    { "High Organ",
      "odd_even=-0.5;partials=16;brightness=0.9;cutoff=8000;near_mix=0.4;near_decay=3;far_level=0.6;scale=Pythagorean;root=D;"
      "strands=4;stack=Fifths;detune=0;drift=0.5" },
    { "Stained Light",
      "brightness=0.8;shimmer=1;shimmer_rate=0.6;partials=24;far_decay=40;far_highcut=7000;scale=Otonality 1-11;root=F" },
    { "Frozen Chapel",
      "far_decay=90;far_highcut=5000;far_size=3;brain_density=3;attack=20;release=50;partials=20;brightness=0.85;bloom=1;bloom_time=200" },
    { "Silver Threads",
      "strands=6;detune=25;drift=12;partials=12;brightness=0.9;dly_mix=0.3;dly_time_l=1.7;dly_time_r=2.3;dly_feedback=0.7;"
      "far_decay=20;far_highcut=6000" },
    { "Bright Pentatonic",
      "scale=JI Pentatonic;root=E;brightness=0.9;cutoff=7000;brain_density=6;brain_low=52;brain_high=92;air=0.2;far_highcut=6000" },
    { "Prism",
      "inharmonic=0.15;shimmer=0.7;partials=28;tilt=1;brightness=1;far_highcut=8000;ens_mix=0.5;width=1.6;side_air=4;cutoff=8000" },

    // ---------------------------------------------------------------- 30..39 deep / sub / dark
    { "Abyss",
      "scale=Subharmonic 16-8;root=E;brain_low=24;brain_high=50;tilt=2.2;cutoff=500;partials=8;far_decay=45;far_highcut=900;"
      "bass_mono=250;brain_density=4" },
    { "Tectonic",
      "root=C;brain_low=24;brain_high=48;strands=5;detune=15;tilt=1.8;cutoff=700;far_decay=30;dly_mix=0.2;dly_time_l=2.5;"
      "dly_time_r=3.3;dly_feedback=0.6;dly_damp=0.9;far_highcut=1200" },
    { "Whale Song",
      "brain_low=30;brain_high=62;drift=25;drift_rate=0.05;partials=6;air=0.3;air_color=2;air_q=8;far_decay=40;scale=JI Minor;"
      "root=D;far_highcut=2500" },
    { "Cave Water",
      "air=0.5;air_color=8;air_q=4;partials=6;cutoff=900;near_mix=0.5;near_decay=4;far_decay=25;brain_density=3;brain_low=30;"
      "brain_high=60;master_gain=-8;cloud_send=0.4;cloud_pitch=0;cloud_size=400;cloud_spray=1.5" },
    { "Undertow",
      "scale=Subharmonic 16-8;root=A;brain_density=5;brain_consonance=0.5;tilt=1.5;cutoff=1000;far_size=3;far_decay=50;"
      "far_highcut=1500" },
    { "Bass Temple",
      "scale=JI Major (Ptolemy);root=C;brain_low=28;brain_high=52;odd_even=-0.6;cutoff=800;near_mix=0.3;far_level=0.6;bass_mono=200" },
    { "Slow Magma",
      "inharmonic=0.4;partials=10;brain_low=26;brain_high=54;tilt=1.7;cutoff=600;resonance=0.5;filter_drift=0.8;far_decay=35;"
      "far_highcut=1500" },
    { "Deep Space Hum",
      "scale=12-TET;root=C;brain_low=24;brain_high=44;brain_density=3;partials=4;cutoff=400;far_decay=60;far_highcut=800;"
      "air=0.15;air_color=12;air_q=3" },
    { "Subterranean Choir",
      "scale=Harmonic 8-16;root=C;brain_low=36;brain_high=64;air=0.3;air_color=3;air_q=12;cutoff=1500;far_decay=30" },
    { "Low Pythagorean",
      "scale=Pythagorean;root=G;brain_low=29;brain_high=60;tilt=1.5;strands=4;detune=10;far_decay=28;dly_mix=0.25;far_highcut=2000" },

    // ---------------------------------------------------------------- 40..49 breath / flute / voice
    { "Shakuhachi Air",
      "air=0.7;air_color=1;air_q=6;partials=5;tilt=1.8;keys_depth=0;brain_low=60;brain_high=88;brain_density=3;brain_rate=10;"
      "brain_hold_min=8;brain_hold_max=30;attack=1.5;release=5;far_decay=12;master_gain=-8" },
    { "Ocarina Clouds",
      "air=0.4;air_color=2;air_q=18;partials=3;brain_low=64;brain_high=92;brain_density=5;far_decay=25;ens_mix=0.5;master_gain=-8" },
    { "Bamboo Wind",
      "air=0.6;air_color=5;air_q=5;partials=6;brain_low=55;brain_high=84;dly_mix=0.35;dly_feedback=0.5;far_decay=20;master_gain=-8" },
    { "Alto Voices",
      "air=0.3;air_color=3;air_q=14;cosmos_send=0.5;cosmos_vowel=0.8;cosmos_vowel_rate=0.03;cosmos_return=0.8;cosmos_to_far=0.3;"
      "brain_low=50;brain_high=76;partials=14;scale=JI Major (Ptolemy);root=F;master_gain=-10" },
    { "Boys Choir",
      "cosmos_send=0.6;cosmos_vowel=0.9;cosmos_vowel_rate=0.08;cosmos_return=0.9;cosmos_to_far=0.5;brain_low=60;brain_high=88;"
      "partials=12;tilt=1.4;scale=JI Major (Ptolemy);root=C;brain_density=6" },
    { "Throat Drone",
      "odd_even=0.4;brain_low=36;brain_high=55;cosmos_send=0.5;cosmos_vowel=0.7;cosmos_vowel_rate=0.02;cosmos_return=0.7;"
      "brain_density=3;partials=20;air=0.2;air_color=8;air_q=6" },
    { "Whispered",
      "air=0.8;air_color=10;air_q=3;partials=2;master_gain=-10;brain_low=60;brain_high=96;brain_density=4;far_decay=20;near_mix=0.4" },
    { "Reed Organ",
      "odd_even=0.8;partials=24;tilt=1.1;air=0.15;air_color=4;near_mix=0.4;near_decay=2;far_level=0.5;keys_depth=0.2;"
      "scale=JI Major (Ptolemy);root=D" },
    { "Wooden Flutes",
      "air=0.5;air_color=2;air_q=10;partials=4;tilt=2;ens_mix=0.6;ens_depth=0.6;brain_low=55;brain_high=84;brain_hold_min=10;"
      "brain_hold_max=40;brain_rate=8;far_decay=15;master_gain=-8" },
    { "Sirens Far Away",
      "air=0.4;air_color=3;air_q=12;drift=20;drift_rate=0.03;depth=1;keys_depth=0.9;far_decay=45;far_highcut=3000;brain_low=60;"
      "brain_high=90;brain_density=4" },

    // ---------------------------------------------------------------- 50..59 exotic scales
    { "Slendro Dusk",
      "scale=Slendro (JI);root=D;brain_density=5;brain_low=48;brain_high=84;partials=10;inharmonic=0.2;attack=0.3;decay=10;"
      "sustain=0.4;release=15;brain_rate=6;brain_hold_min=5;brain_hold_max=25;far_decay=20" },
    { "Bohlen Cathedral",
      "scale=Bohlen-Pierce (JI);keymap=Consecutive degrees;root=C;brain_density=6;partials=24;brightness=0.9;far_decay=40;"
      "far_highcut=6000" },
    { "Pythagorean Drone",
      "scale=Pythagorean;root=G;brain_consonance=0.9;brain_density=4;tilt=1.3;far_decay=30" },
    { "Otonal Cloud",
      "scale=Otonality 1-11;root=C;brain_density=8;brain_consonance=0.3;brain_hold_min=20;brain_hold_max=80;brain_rate=8;"
      "far_decay=30;depth=0.8" },
    { "Subharmonic Bells",
      "scale=Subharmonic 16-8;root=E;inharmonic=0.35;attack=0.2;decay=15;sustain=0.2;release=20;brain_rate=7;brain_hold_min=5;"
      "brain_hold_max=15;brain_density=7" },
    { "Harmonic Ladder",
      "scale=Harmonic 8-16;root=D;brain_low=50;brain_high=98;brain_consonance=1;brain_density=8;brain_rate=12;brain_wander=0" },
    { "Seven Limit Garden",
      "scale=JI 7-limit;root=F;brain_density=7;brain_consonance=0.55;brain_wander=0.6;brain_hold_min=30;brain_hold_max=120;"
      "far_decay=25" },
    { "Minor Third Field",
      "scale=JI Minor;root=B;brain_density=6;brain_consonance=0.6;brain_low=40;brain_high=80;far_decay=35;dly_mix=0.2" },
    { "Twelve Tone Fog",
      "scale=12-TET;root=C;brain_consonance=0.2;brain_density=7;brain_rate=15;brain_hold_min=15;brain_hold_max=60;far_decay=40;"
      "far_highcut=2500;depth=0.9" },
    { "Pentatonic Rain",
      "scale=JI Pentatonic;root=A;attack=0.05;decay=6;sustain=0;release=10;brain_rate=3;brain_hold_min=5;brain_hold_max=8;"
      "brain_density=8;dly_mix=0.4;dly_feedback=0.6;dly_time_l=0.62;dly_time_r=0.93;far_decay=15" },

    // ---------------------------------------------------------------- 60..69 shimmer / delay / motion
    { "Shimmer Rise",
      "cosmos_shimmer=0.7;cosmos_shimmer_pitch=+12;far_decay=20;far_level=0.9;depth=0.6;brain_density=4" },
    { "Fifth Shimmer",
      "cosmos_shimmer=0.6;cosmos_shimmer_pitch=+7;scale=JI Major (Ptolemy);root=C;far_decay=25;brain_density=4;brightness=0.8" },
    { "Octave Below",
      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=-12;far_decay=30;far_highcut=2000;brain_low=48;brain_high=84;brain_density=4" },
    { "Echo Canyon",
      "dly_time_l=1.5;dly_time_r=2.25;dly_feedback=0.75;dly_cross=0.5;dly_damp=0.6;dly_mix=0.45;dly_to_far=0.6;attack=0.5;"
      "brain_rate=10;brain_hold_min=8;brain_hold_max=30;brain_density=4;far_decay=12;dly2_mix=0.3;dly2_feedback=0.5" },
    { "Ping Pong Drift",
      "dly_time_l=0.33;dly_time_r=0.5;dly_cross=1;dly_feedback=0.7;dly_mix=0.4;attack=0.3;decay=3;sustain=0.5;release=6;"
      "brain_rate=4;brain_hold_min=3;brain_hold_max=10;brain_density=5" },
    { "Rotating Sky",
      "pan_drift=1;itd=1;drift_rate=0.3;ens_mix=0.6;ens_rate=0.5;far_asym=1;width=1.8;brain_density=5" },
    { "Slow Chorus Field",
      "ens_mix=0.8;ens_depth=0.8;ens_rate=0.05;strands=6;detune=12;far_decay=20;brain_density=5" },
    { "Tape Echo Drone",
      "dly_time_l=0.42;dly_time_r=0.42;dly_feedback=0.85;dly_damp=0.9;dly_mix=0.35;dly_to_far=0.2;far_decay=8;cutoff=1800;"
      "brain_density=4" },
    { "Shimmer Cathedral",
      "cosmos_shimmer=0.8;cosmos_shimmer_pitch=+12;far_size=3;far_decay=45;far_highcut=7000;partials=20;brightness=0.9;"
      "brain_density=5" },
    { "Nineteenth Above",
      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=+19;far_decay=35;far_highcut=5000;scale=JI Major (Ptolemy);root=A;brain_density=4" },

    // ---------------------------------------------------------------- 70..99 cosmos / science fiction
    { "Nebula Drift",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.8;cosmos_return=0.7;cosmos_to_far=0.5;far_decay=30;brain_density=5;"
      "cloud_send=0.6;cloud_pitch=0.4" },
    { "Frozen Nebula",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.98;cosmos_return=0.8;cosmos_to_far=0.6;far_decay=20;brain_density=4" },
    { "Alien Choir",
      "cosmos_send=0.8;cosmos_vowel=1;cosmos_vowel_rate=0.02;cosmos_shift=3;cosmos_return=0.8;cosmos_to_far=0.6;partials=16;"
      "brain_density=6;far_decay=25" },
    { "Forbidden Planet",
      "cosmos_send=1;cosmos_shift=120;cosmos_shift_drift=1;cosmos_res=0.6;cosmos_res_pitch=1.5;cosmos_res_fb=0.9;cosmos_return=0.7;"
      "cosmos_to_far=0.5;scale=12-TET;brain_consonance=0.2;brain_density=5" },
    { "Hull Resonance",
      "cosmos_send=1;cosmos_res=0.9;cosmos_res_pitch=0.5;cosmos_res_fb=0.96;cosmos_return=0.8;brain_low=30;brain_high=60;partials=6;"
      "far_decay=20;far_highcut=1500" },
    { "Signal From Deep Space",
      "cosmos_send=0.8;cosmos_shift=220;cosmos_shift_drift=0.6;cosmos_nebula=0.5;cosmos_smear=0.6;cosmos_return=0.6;cosmos_to_far=0.8;"
      "air=0.3;air_color=9;air_q=20;brain_density=3;brain_rate=40" },
    { "Event Horizon",
      "cosmos_send=1;cosmos_shift=-40;cosmos_shift_drift=0.5;cosmos_nebula=0.8;cosmos_smear=0.9;cosmos_shimmer=0.5;"
      "cosmos_shimmer_pitch=-12;cosmos_return=0.5;cosmos_to_far=1;far_decay=60;far_highcut=1500;depth=1;brain_density=4" },
    { "Ion Wind",
      "air=0.6;air_color=14;air_q=3;cosmos_send=1;cosmos_shift=60;cosmos_shift_drift=1;cosmos_return=0.8;partials=4;far_decay=25;"
      "brain_density=4;master_gain=-8" },
    { "Cryo Chamber",
      "cosmos_send=1;cosmos_nebula=0.9;cosmos_smear=0.95;cosmos_res=0.4;cosmos_res_pitch=4;cosmos_res_fb=0.85;cosmos_return=0.6;"
      "cosmos_to_far=0.6;partials=24;brightness=0.9;inharmonic=0.3;far_decay=40;far_highcut=6000;brain_density=4" },
    { "Pulsar Field",
      "cosmos_send=1;cosmos_shift=300;cosmos_shift_drift=0;cosmos_return=0.5;cosmos_to_far=0.5;attack=0.05;decay=4;sustain=0.2;"
      "release=8;brain_rate=3;brain_hold_min=3;brain_hold_max=8;brain_density=8;dly_mix=0.4;dly_feedback=0.6;scale=12-TET;"
      "brain_consonance=0" },
    { "Ring World",
      "cosmos_send=1;cosmos_shift=55;cosmos_res=0.5;cosmos_res_pitch=3;cosmos_res_fb=0.9;cosmos_return=0.8;scale=Otonality 1-11;"
      "root=C;brain_density=5" },
    { "Solar Wind",
      "cosmos_send=0.7;cosmos_nebula=1;cosmos_smear=0.7;cosmos_shimmer=0.6;cosmos_shimmer_pitch=+12;cosmos_return=0.5;"
      "cosmos_to_far=0.8;air=0.4;air_color=6;air_q=5;far_decay=40;brain_density=5" },
    { "Derelict Ship",
      "cosmos_send=1;cosmos_res=0.8;cosmos_res_pitch=0.75;cosmos_res_fb=0.95;cosmos_shift=-15;cosmos_shift_drift=1;cosmos_return=0.7;"
      "cosmos_to_far=0.6;brain_low=28;brain_high=60;inharmonic=0.5;partials=10;brain_density=4;far_decay=30;far_highcut=1200" },
    { "Telepathy",
      "cosmos_send=0.9;cosmos_vowel=0.9;cosmos_vowel_rate=0.2;cosmos_shift=8;cosmos_shift_drift=1;cosmos_nebula=0.4;cosmos_smear=0.5;"
      "cosmos_return=0.8;brain_density=5;partials=12" },
    { "Warp Core",
      "cosmos_send=1;cosmos_shift=-200;cosmos_shift_drift=0.3;cosmos_res=0.7;cosmos_res_pitch=0.25;cosmos_res_fb=0.96;cosmos_return=0.8;"
      "cosmos_to_far=0.4;brain_low=24;brain_high=48;brain_density=3;bass_mono=300;far_decay=20" },
    { "Comet Tail",
      "cosmos_send=1;cosmos_shimmer=0.9;cosmos_shimmer_pitch=+24;cosmos_nebula=0.6;cosmos_smear=0.8;cosmos_return=0.4;cosmos_to_far=1;"
      "far_decay=50;far_highcut=9000;brightness=0.9;brain_density=4" },
    { "Andromeda",
      "cosmos_send=0.8;cosmos_nebula=1;cosmos_smear=0.85;cosmos_shift=5;cosmos_shift_drift=1;cosmos_return=0.7;cosmos_to_far=0.7;"
      "partials=24;shimmer=0.8;far_decay=45;brain_density=6;arc=0.8;arc_period=20" },
    { "Ganymede Ice",
      "cosmos_send=1;cosmos_res=0.6;cosmos_res_pitch=6;cosmos_res_fb=0.92;cosmos_shift=30;cosmos_return=0.6;cosmos_to_far=0.5;"
      "inharmonic=0.4;partials=24;brightness=1;far_decay=35;far_highcut=8000;brain_density=4;brain_low=55;brain_high=96" },
    { "Lost Transmission",
      "cosmos_send=1;cosmos_shift=150;cosmos_shift_drift=1;cosmos_vowel=0.5;cosmos_vowel_rate=0.4;cosmos_nebula=0.3;cosmos_return=0.9;"
      "cosmos_to_far=0.2;dly_mix=0.4;dly_feedback=0.7;dly_damp=0.8;air=0.4;air_color=12;air_q=8;brain_density=3;brain_rate=30" },
    { "Stellar Nursery",
      "cosmos_send=0.6;cosmos_nebula=1;cosmos_smear=0.9;cosmos_shimmer=0.4;cosmos_shimmer_pitch=+7;cosmos_return=0.6;cosmos_to_far=0.8;"
      "scale=JI Major (Ptolemy);root=E;far_decay=45;brain_density=6;depth=0.9;cloud_send=0.8;cloud_pitch=0.6;cloud_density=8" },
    { "Xenomorph Hive",
      "cosmos_send=1;cosmos_vowel=0.8;cosmos_vowel_rate=0.5;cosmos_shift=-80;cosmos_shift_drift=1;cosmos_res=0.5;cosmos_res_pitch=0.5;"
      "cosmos_res_fb=0.9;cosmos_return=0.8;cosmos_to_far=0.5;scale=12-TET;brain_consonance=0.1;brain_density=7;brain_low=30;"
      "brain_high=70;cutoff=1500" },
    { "Zero Gravity",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.75;cosmos_return=1;cosmos_to_far=0.3;near_mix=0;far_level=0.4;pan_drift=1;itd=1;"
      "brain_density=5" },
    { "Monolith",
      "cosmos_send=1;cosmos_res=1;cosmos_res_pitch=1;cosmos_res_fb=0.97;cosmos_return=0.6;cosmos_to_far=0.6;brain_low=36;brain_high=60;"
      "brain_density=3;partials=32;tilt=0.8;far_decay=60;far_highcut=2500;attack=30;release=60" },
    { "Quasar",
      "cosmos_send=1;cosmos_shift=280;cosmos_shift_drift=1;cosmos_shimmer=0.7;cosmos_shimmer_pitch=+19;cosmos_return=0.5;"
      "cosmos_to_far=0.8;brightness=1;partials=32;far_decay=30;far_highcut=10000;brain_density=5" },
    { "Dark Matter",
      "cosmos_send=1;cosmos_shift=-300;cosmos_nebula=0.9;cosmos_smear=0.9;cosmos_return=0.7;cosmos_to_far=0.7;brain_low=24;brain_high=52;"
      "cutoff=500;far_decay=60;far_highcut=1000;brain_density=3" },
    { "Orbital Decay",
      "cosmos_send=0.8;cosmos_shift=40;cosmos_shift_drift=1;cosmos_res=0.4;cosmos_res_pitch=2;cosmos_res_fb=0.8;cosmos_return=0.7;"
      "dly_time_l=3;dly_time_r=4;dly_feedback=0.8;dly_mix=0.3;dly_to_far=0.8;brain_density=4;far_decay=30" },
    { "First Contact",
      "cosmos_send=0.7;cosmos_vowel=0.6;cosmos_vowel_rate=0.01;cosmos_shimmer=0.5;cosmos_shimmer_pitch=+12;cosmos_return=0.7;"
      "cosmos_to_far=0.6;scale=JI Major (Ptolemy);root=C;air=0.2;brain_density=5;far_decay=35" },
    { "Plasma Sea",
      "cosmos_send=1;cosmos_nebula=0.7;cosmos_smear=0.6;cosmos_shift=12;cosmos_shift_drift=1;cosmos_res=0.3;cosmos_res_pitch=5;"
      "cosmos_res_fb=0.8;cosmos_return=0.8;cosmos_to_far=0.6;air=0.3;air_color=7;air_q=4;brain_density=6" },
    { "Void Whisper",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.95;cosmos_return=0.5;cosmos_to_far=0.9;air=0.5;air_color=10;air_q=6;partials=3;"
      "master_gain=-8;far_decay=50;far_highcut=2500;brain_density=3" },
    { "Alien Cathedral",
      "cosmos_send=0.8;cosmos_vowel=0.5;cosmos_vowel_rate=0.03;cosmos_shimmer=0.6;cosmos_shimmer_pitch=+12;cosmos_nebula=0.4;"
      "cosmos_smear=0.7;cosmos_return=0.6;cosmos_to_far=0.8;far_size=3;far_decay=60;far_highcut=6000;partials=24;brain_density=6;"
      "scale=Bohlen-Pierce (JI);keymap=Consecutive degrees;root=C" },

    // ---------------------------------------------------------------- 100..109 playable keys (brain off)
    { "Warm Keys",
      "brain_on=off;keys_depth=0;attack=0.8;decay=4;sustain=0.7;release=5;near_mix=0.3;near_decay=2;far_level=0.4;ens_mix=0.4;"
      "partials=12;tilt=1.4" },
    { "Glass Keys",
      "brain_on=off;attack=0.02;decay=8;sustain=0.2;release=8;inharmonic=0.3;partials=24;brightness=0.9;far_level=0.6;far_decay=20;"
      "keys_depth=0.2;cutoff=6000" },
    { "Pad Keys",
      "brain_on=off;attack=3;release=10;strands=6;detune=15;ens_mix=0.6;far_level=0.7;far_decay=25;keys_depth=0.3" },
    { "Flute Keys",
      "brain_on=off;air=0.6;air_color=2;air_q=12;partials=5;attack=0.3;release=3;near_mix=0.3;far_level=0.5;master_gain=-8" },
    { "Organ Keys",
      "brain_on=off;odd_even=-0.4;partials=20;attack=0.05;release=0.5;sustain=1;near_mix=0.35;near_decay=3;far_level=0.5;cutoff=6000" },
    { "Cosmos Keys",
      "brain_on=off;cosmos_send=1;cosmos_shift=30;cosmos_shift_drift=1;cosmos_nebula=0.5;cosmos_smear=0.7;cosmos_return=0.7;"
      "cosmos_to_far=0.6;attack=1;release=12;far_decay=30" },
    { "Shimmer Keys",
      "brain_on=off;cosmos_shimmer=0.7;cosmos_shimmer_pitch=+12;attack=0.5;release=8;far_level=0.9;far_decay=25;keys_depth=0.3" },
    { "Sub Keys",
      "brain_on=off;tilt=2;cutoff=600;partials=8;attack=0.3;release=4;bass_mono=250;far_level=0.3;near_mix=0.2" },
    { "Vowel Keys",
      "brain_on=off;cosmos_send=1;cosmos_vowel=1;cosmos_vowel_rate=0.1;cosmos_return=1;cosmos_to_far=0.3;partials=16;attack=0.5;release=5;"
      "master_gain=-9" },
    { "Resonant Keys",
      "brain_on=off;cosmos_send=1;cosmos_res=0.7;cosmos_res_pitch=2;cosmos_res_fb=0.93;cosmos_return=0.8;attack=0.2;release=6;near_mix=0.3" },

    // ---------------------------------------------------------------- 110..119 long-form night arcs
    { "All Night Arc",
      "arc=1;arc_period=240;brain_density=5;brain_rate=40;brain_hold_min=90;brain_hold_max=400;far_decay=40;depth=0.85;"
      "scale=JI 7-limit;root=D;sub_level=0.3;sub_binaural=3;bloom=0.6;bloom_time=120" },
    { "Tidal Hours",
      "arc=1;arc_period=120;brain_density=6;brain_hold_min=60;brain_hold_max=240;far_decay=35;scale=JI Major (Ptolemy);root=G;air=0.2" },
    { "Slow Sunrise",
      "arc=0.8;arc_period=90;brightness=0.4;brain_density=4;far_decay=30;scale=JI Pentatonic;root=C;bloom=0.8;bloom_time=150" },
    { "Ninety Minute Cycle",
      "arc=1;arc_period=90;brain_density=5;brain_rate=30;brain_hold_min=60;brain_hold_max=200;cosmos_send=0.4;cosmos_nebula=0.6;"
      "cosmos_smear=0.8;cosmos_return=0.5;cosmos_to_far=0.5" },
    { "REM Drift",
      "arc=0.7;arc_period=45;brain_density=6;brain_consonance=0.5;brain_wander=0.6;far_decay=35;depth=0.9" },
    { "Dream Corridor",
      "arc=0.6;arc_period=60;dly_mix=0.25;dly_time_l=2;dly_time_r=3;dly_feedback=0.7;dly_to_far=0.8;far_decay=45;brain_density=5;"
      "dly2_mix=0.35;dly2_feedback=0.6;dly2_time_l=3.1;dly2_time_r=3.7" },
    { "Deep Night Harmonics",
      "arc=0.9;arc_period=150;scale=Harmonic 8-16;root=C;brain_low=40;brain_high=88;brain_density=6;far_decay=40;far_highcut=2500" },
    { "Long Otonal Night",
      "arc=0.8;arc_period=180;scale=Otonality 1-11;root=E;brain_density=7;brain_consonance=0.6;brain_hold_min=90;brain_hold_max=300;"
      "far_decay=50" },
    { "Sleeping Nebula",
      "arc=0.9;arc_period=120;cosmos_send=0.6;cosmos_nebula=0.8;cosmos_smear=0.9;cosmos_return=0.5;cosmos_to_far=0.8;far_decay=50;"
      "far_highcut=2000;brain_density=4;depth=1" },
    { "Morning Fade",
      "arc=1;arc_period=60;brain_density=3;brain_rate=60;brain_hold_min=120;brain_hold_max=400;brightness=0.5;far_decay=30;attack=30;"
      "release=60" },

    // ---------------------------------------------------------------- 120..127 storm / cluster / texture
    { "Cluster Storm",
      "brain_consonance=0;brain_density=10;brain_rate=6;brain_hold_min=10;brain_hold_max=40;scale=12-TET;far_decay=40;depth=0.9;"
      "strands=5;detune=25;fb_bus=0.2;fb_fm=0.2" },
    { "Micro Cluster",
      "brain_consonance=0.05;brain_density=8;brain_low=60;brain_high=72;far_decay=30;partials=8;scale=12-TET" },
    { "Rising Swarm",
      "cosmos_shimmer=0.8;cosmos_shimmer_pitch=+12;brain_consonance=0.2;brain_density=9;brain_rate=5;brain_hold_min=8;"
      "brain_hold_max=30;far_decay=25;cloud_send=0.7;cloud_pitch=0.8;cloud_density=20" },
    { "Thunder Head",
      "brain_low=24;brain_high=60;brain_density=8;brain_consonance=0.3;tilt=1.8;cutoff=900;resonance=0.4;far_decay=30;air=0.3;"
      "air_color=6;air_q=3" },
    { "Granular Sky",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.5;cosmos_return=1;cosmos_to_far=0.5;attack=0.1;decay=2;sustain=0.3;release=4;"
      "brain_rate=2;brain_hold_min=2;brain_hold_max=6;brain_density=10;scale=JI Pentatonic;root=D;cloud_send=1;cloud_density=30;"
      "cloud_size=120;cloud_pitch=0.5" },
    { "Inharmonic Field",
      "inharmonic=1;partials=32;tilt=1;brain_density=6;brain_consonance=0.4;far_decay=35;far_highcut=5000;fb_fm=0.4;fb_tone=800" },
    { "Dissonant Cathedral",
      "brain_consonance=0.1;brain_density=7;far_size=3;far_decay=60;far_highcut=4000;partials=20;brightness=0.8;scale=12-TET" },
    { "White Storm",
      "air=1;air_color=8;air_q=2;partials=2;brain_density=8;brain_consonance=0;master_gain=-10;far_decay=30;dly_mix=0.3;"
      "dly_feedback=0.7;scale=12-TET" },

    // ---------------------------------------------------------------- 128..135 sources (wavetable, FM, feedback)
    { "Vocal Morph Choir",
      "src2_type=Wavetable;src2_table=Vocal;src2_pos=0.2;src2_pos_drift=1;src2_level=0.6;src2_ratio=1/1;"
      "src3_type=Wavetable;src3_table=Vocal;src3_pos=0.7;src3_pos_drift=1;src3_level=0.5;src3_ratio=3/2;src3_octave=-1;"
      "osc_level=0.5;partials=8;brain_density=4;far_decay=30;far_highcut=4000;scale=JI Major (Ptolemy);root=A;"
      "z_mode=Series;z_shape=Vowel Morph;z_rate=0.03;z_depth=1;z_mix=0.5" },
    { "Glass Table Drift",
      "src2_type=Wavetable;src2_table=Glass;src2_pos=0.3;src2_pos_drift=0.8;src2_level=0.7;src2_octave=1;"
      "osc_level=0.4;partials=6;tilt=1.5;far_size=3;far_decay=45;far_highcut=8000;brain_low=48;brain_high=88;attack=8;release=20" },
    { "Organ Mixture Cloud",
      "src2_type=Wavetable;src2_table=Organ;src2_pos=0.6;src2_pos_drift=0.4;src2_level=0.6;"
      "src3_type=Wavetable;src3_table=Organ;src3_pos=0.9;src3_level=0.4;src3_octave=-1;src3_ratio=3/2;"
      "osc_level=0.3;scale=Pythagorean;root=D;near_mix=0.4;near_decay=3;far_level=0.5;brain_density=4;cloud_send=0.3" },
    { "Metal Field",
      "src2_type=Wavetable;src2_table=Metal;src2_pos=0.5;src2_pos_drift=1;src2_level=0.6;src2_pan=-0.5;"
      "src3_type=Wavetable;src3_table=Metal;src3_pos=0.1;src3_pos_drift=1;src3_level=0.5;src3_pan=0.5;src3_ratio=7/4;src3_octave=-1;"
      "osc_level=0.3;inharmonic=0.4;partials=12;cutoff=1800;far_decay=40;far_highcut=3000;brain_consonance=0.3;brain_density=5;fb_fm=0.3;"
      "z_mode=Series;z_shape=Metal Bars;z_rate=0.02;z_depth=0.8;z_res=0.7;z_mix=0.4" },
    { "FM Bell Drone",
      "src2_type=FM;src2_fm_ratio=3.5;src2_fm_index=2.5;src2_pos_drift=0.8;src2_level=0.5;src2_octave=1;"
      "osc_level=0.5;partials=6;attack=2;decay=20;sustain=0.4;release=25;far_decay=35;far_highcut=6000;brain_rate=15;brain_hold_min=10;brain_hold_max=60;"
      "scale=JI Pentatonic;root=E" },
    { "Slow FM Tide",
      "src2_type=FM;src2_fm_ratio=1;src2_fm_index=1.5;src2_pos_drift=1;src2_level=0.6;src2_pan=-0.4;"
      "src3_type=FM;src3_fm_ratio=0.5;src3_fm_index=1;src3_pos_drift=1;src3_level=0.5;src3_pan=0.4;src3_octave=-1;"
      "osc_level=0.4;tilt=1.6;cutoff=1500;arc=0.6;arc_period=30;brain_density=4;far_decay=40;breath=0.4" },
    { "Feedback Hiss",
      "fb_bus=0.6;fb_drive=1;fb_tone=3000;fb_fm=0.3;src2_type=Wavetable;src2_table=Classic;src2_pos=0.55;src2_level=0.4;"
      "osc_level=0.6;partials=10;cutoff=2000;far_decay=30;dly_feedback=0.7;dly_mix=0.3;brain_density=4;master_gain=-9" },
    { "Three Voices, One Key",
      "brain_on=off;keys_depth=0;stack=Major;strands=3;detune=0;drift=1;osc_level=0.7;"
      "src2_type=Wavetable;src2_table=Classic;src2_pos=0.25;src2_level=0.4;src2_octave=-1;"
      "src3_type=FM;src3_fm_ratio=2;src3_fm_index=0.8;src3_level=0.3;src3_octave=1;src3_ratio=3/2;"
      "near_mix=0.3;far_level=0.4;attack=2;release=8;presence=2" },

    // ---------------------------------------------------------------- 136..147 z-plane / morphing filter
    { "Morphing Vowels",
      "z_mode=Replace;z_shape=Vowel Morph;z_x=0.3;z_y=0.4;z_rate=0.02;z_depth=1;z_res=0.5;z_mix=1;z_keytrack=0.2;"
      "partials=24;tilt=0.9;brightness=0.9;brain_density=4;brain_rate=30;far_decay=30;attack=8;release=20;scale=JI Major (Ptolemy);root=A" },
    { "Choir Behind Glass",
      "z_mode=Series;z_shape=Choir;z_x=0.5;z_y=0.5;z_rate=0.012;z_depth=1;z_res=0.6;z_mix=0.8;"
      "partials=20;air=0.2;brain_density=5;far_decay=40;far_highcut=4000;depth=0.75;attack=10;release=25;scale=JI Minor;root=D" },
    { "Nasal Drone",
      "z_mode=Replace;z_shape=Nasal;z_x=0.2;z_y=0.7;z_rate=0.008;z_depth=0.8;z_res=0.7;z_mix=1;"
      "partials=28;tilt=0.8;brain_density=3;brain_hold_min=60;brain_hold_max=200;far_decay=35;master_gain=-8" },
    { "Filter Tide",
      "z_mode=Replace;z_shape=Low Sweep;z_x=0.3;z_y=0.6;z_rate=0.01;z_depth=1;z_res=0.8;z_mix=1;z_keytrack=0.5;"
      "partials=32;tilt=0.7;brightness=1;brain_density=5;far_decay=40;arc=0.6;arc_period=40;attack=12;release=30" },
    { "Phase Field",
      "z_mode=Series;z_shape=Phaser;z_x=0.4;z_y=0.5;z_rate=0.03;z_depth=1;z_res=0.4;z_mix=0.9;"
      "partials=24;shimmer=0.5;brain_density=6;far_decay=35;dly_mix=0.25;dly_feedback=0.6;width=1.5" },
    { "Comb Cathedral",
      "z_mode=Series;z_shape=Comb;z_x=0.5;z_y=0.4;z_rate=0.006;z_depth=0.7;z_res=0.6;z_mix=0.7;z_keytrack=1;"
      "partials=28;far_size=3;far_decay=50;far_highcut=6000;brain_density=4;attack=10;release=30;scale=Harmonic 8-16;root=C" },
    { "Notch Winds",
      "z_mode=Replace;z_shape=Notch Cluster;z_x=0.5;z_y=0.5;z_rate=0.02;z_depth=1;z_res=0.5;z_mix=1;"
      "air=0.6;air_color=5;air_q=6;partials=10;brain_density=5;far_decay=30;master_gain=-8" },
    { "String Body",
      "z_mode=Replace;z_shape=Strings;z_x=0.4;z_y=0.5;z_rate=0.01;z_depth=0.6;z_res=0.7;z_mix=1;z_keytrack=0.8;"
      "partials=20;attack=3;decay=15;sustain=0.5;release=18;brain_rate=15;brain_density=5;far_decay=25" },
    { "Struck Bars",
      "z_mode=Replace;z_shape=Metal Bars;z_x=0.3;z_y=0.4;z_rate=0.015;z_depth=0.8;z_res=0.8;z_mix=1;"
      "partials=16;inharmonic=0.3;attack=1;decay=20;sustain=0.35;release=25;brain_rate=10;brain_hold_min=8;brain_hold_max=40;far_decay=35" },
    { "Glass Needles",
      "z_mode=Replace;z_shape=Glass;z_x=0.5;z_y=0.6;z_rate=0.02;z_depth=1;z_res=0.9;z_mix=1;"
      "partials=24;brightness=1;brain_low=55;brain_high=92;brain_density=5;far_decay=40;far_highcut=9000;master_gain=-9" },
    { "Harmonic Sieve",
      "z_mode=Replace;z_shape=Peaks;z_x=0.4;z_y=0.5;z_rate=0.008;z_depth=0.6;z_res=0.8;z_mix=1;z_keytrack=1;"
      "air=0.4;air_mode=Ghost;air_q=16;partials=20;brain_density=4;far_decay=45;scale=Otonality 1-11;root=F" },
    { "Endless Resonance",
      "z_mode=Series;z_shape=Infinite;z_x=0.4;z_y=0.3;z_rate=0.004;z_depth=1;z_res=0.6;z_mix=0.6;z_keytrack=0.5;"
      "partials=12;tilt=1.4;brain_density=3;brain_hold_min=90;brain_hold_max=300;far_decay=60;far_highcut=3000;attack=15;release=40;master_gain=-9" },
};
}

namespace {
// Cosmos-only bank: loadable on top of any sound preset.
const Preset kCosmosPresets[] = {
    { "Cosmos Off", "" },
    { "Gentle Shift",     "cosmos_send=0.6;cosmos_shift=6;cosmos_shift_drift=1;cosmos_return=0.6;cosmos_to_far=0.4" },
    { "Slow Beating",     "cosmos_send=0.8;cosmos_shift=40;cosmos_shift_drift=0.5;cosmos_return=0.7;cosmos_to_far=0.5" },
    { "Metallic Shift",   "cosmos_send=1;cosmos_shift=150;cosmos_shift_drift=0.3;cosmos_return=0.6;cosmos_to_far=0.6" },
    { "Downward Shift",   "cosmos_send=1;cosmos_shift=-120;cosmos_shift_drift=0.6;cosmos_return=0.7;cosmos_to_far=0.5" },
    { "Root Resonator",   "cosmos_send=1;cosmos_res=0.7;cosmos_res_pitch=1;cosmos_res_fb=0.93;cosmos_return=0.7" },
    { "Fifth Resonator",  "cosmos_send=1;cosmos_res=0.6;cosmos_res_pitch=1.5;cosmos_res_fb=0.92;cosmos_return=0.7" },
    { "High Resonator",   "cosmos_send=1;cosmos_res=0.5;cosmos_res_pitch=6;cosmos_res_fb=0.9;cosmos_return=0.6;cosmos_to_far=0.6" },
    { "Hull",             "cosmos_send=1;cosmos_res=0.9;cosmos_res_pitch=0.5;cosmos_res_fb=0.96;cosmos_return=0.8" },
    { "Alien Choir",      "cosmos_send=0.8;cosmos_vowel=1;cosmos_vowel_rate=0.02;cosmos_shift=3;cosmos_return=0.8;cosmos_to_far=0.6" },
    { "Fast Vowels",      "cosmos_send=0.9;cosmos_vowel=0.9;cosmos_vowel_rate=0.3;cosmos_return=0.9" },
    { "Deep Throat",      "cosmos_send=0.7;cosmos_vowel=0.7;cosmos_vowel_rate=0.01;cosmos_shift=-10;cosmos_return=0.7" },
    { "Nebula Drift",     "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.8;cosmos_return=0.7;cosmos_to_far=0.5" },
    { "Frozen Nebula",    "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.98;cosmos_return=0.8;cosmos_to_far=0.6" },
    { "Soft Smear",       "cosmos_send=0.7;cosmos_nebula=0.6;cosmos_smear=0.6;cosmos_return=0.6;cosmos_to_far=0.5" },
    { "Shimmer +12",      "cosmos_shimmer=0.7;cosmos_shimmer_pitch=+12" },
    { "Shimmer +7",       "cosmos_shimmer=0.6;cosmos_shimmer_pitch=+7" },
    { "Shimmer +19",      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=+19" },
    { "Shimmer -12",      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=-12" },
    { "Shimmer +24",      "cosmos_shimmer=0.6;cosmos_shimmer_pitch=+24" },
    { "Forbidden Planet", "cosmos_send=1;cosmos_shift=120;cosmos_shift_drift=1;cosmos_res=0.6;cosmos_res_pitch=1.5;cosmos_res_fb=0.9;"
                          "cosmos_return=0.7;cosmos_to_far=0.5" },
    { "Event Horizon",    "cosmos_send=1;cosmos_shift=-40;cosmos_shift_drift=0.5;cosmos_nebula=0.8;cosmos_smear=0.9;cosmos_shimmer=0.5;"
                          "cosmos_shimmer_pitch=-12;cosmos_return=0.5;cosmos_to_far=1" },
    { "Telepathy",        "cosmos_send=0.9;cosmos_vowel=0.9;cosmos_vowel_rate=0.2;cosmos_shift=8;cosmos_shift_drift=1;cosmos_nebula=0.4;"
                          "cosmos_smear=0.5;cosmos_return=0.8" },
    { "Warp Core",        "cosmos_send=1;cosmos_shift=-200;cosmos_shift_drift=0.3;cosmos_res=0.7;cosmos_res_pitch=0.25;cosmos_res_fb=0.96;"
                          "cosmos_return=0.8;cosmos_to_far=0.4" },
    { "Comet Tail",       "cosmos_send=1;cosmos_shimmer=0.9;cosmos_shimmer_pitch=+24;cosmos_nebula=0.6;cosmos_smear=0.8;cosmos_return=0.4;"
                          "cosmos_to_far=1" },
    { "Xenomorph Hive",   "cosmos_send=1;cosmos_vowel=0.8;cosmos_vowel_rate=0.5;cosmos_shift=-80;cosmos_shift_drift=1;cosmos_res=0.5;"
                          "cosmos_res_pitch=0.5;cosmos_res_fb=0.9;cosmos_return=0.8;cosmos_to_far=0.5" },
    { "Quasar",           "cosmos_send=1;cosmos_shift=280;cosmos_shift_drift=1;cosmos_shimmer=0.7;cosmos_shimmer_pitch=+19;cosmos_return=0.5;"
                          "cosmos_to_far=0.8" },
    { "Dark Matter",      "cosmos_send=1;cosmos_shift=-300;cosmos_nebula=0.9;cosmos_smear=0.9;cosmos_return=0.7;cosmos_to_far=0.7" },
    { "Plasma Sea",       "cosmos_send=1;cosmos_nebula=0.7;cosmos_smear=0.6;cosmos_shift=12;cosmos_shift_drift=1;cosmos_res=0.3;"
                          "cosmos_res_pitch=5;cosmos_res_fb=0.8;cosmos_return=0.8;cosmos_to_far=0.6" },
    { "Void Whisper",     "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.95;cosmos_return=0.5;cosmos_to_far=0.9" },
    { "Lost Transmission","cosmos_send=1;cosmos_shift=150;cosmos_shift_drift=1;cosmos_vowel=0.5;cosmos_vowel_rate=0.4;cosmos_nebula=0.3;"
                          "cosmos_return=0.9;cosmos_to_far=0.2" },
    { "Full Cosmos",      "cosmos_send=1;cosmos_shift=25;cosmos_shift_drift=1;cosmos_res=0.4;cosmos_res_pitch=2;cosmos_res_fb=0.9;"
                          "cosmos_vowel=0.4;cosmos_vowel_rate=0.05;cosmos_nebula=0.5;cosmos_smear=0.7;cosmos_shimmer=0.5;"
                          "cosmos_shimmer_pitch=+12;cosmos_return=0.7;cosmos_to_far=0.7" },
};
}

int numCosmosPresets() { return static_cast<int>(sizeof(kCosmosPresets) / sizeof(kCosmosPresets[0])); }
const Preset& cosmosPreset(int index)
{
    const int n = numCosmosPresets();
    if (index < 0 || index >= n) index = 0;
    return kCosmosPresets[index];
}

int builtinPresetCount() { return static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0])); }
const Preset& builtinPreset(int index)
{
    const int n = builtinPresetCount();
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
