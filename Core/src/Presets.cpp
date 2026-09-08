#include "ambient/Presets.h"
#include <cstring>
#include <cstdlib>

namespace ambient {

namespace {


const Preset kPresets[] = {


    // ---------------------------------------------------------------- 0..9 originals
    { "Init", "" },
    { "Sleep Concert",
      "brain_density=6;brain_rate=35;brain_hold_min=60;brain_hold_max=240;depth=0.85;far_decay=35;"
      "far_highcut=2500;air=0.2;arc=0.5;arc_period=60;brightness=0.6;attack=10;release=25;sub_level=0.35;"
      "sub_binaural=4;bloom=0.5;bloom_time=90;sub_source=Difference;breath=0.3;pad_low_cut=90;"
      "lfo1_rate=0.019456;lfo1_depth=0.59;lfo2_rate=0.012025;lfo2_depth=0.58;lfo3_rate=0.0074316;"
      "lfo3_depth=0.42;lfo4_rate=0.004593;lfo4_depth=0.59;z_mode=Series;z_shape=Glass;z_x=0.40;z_y=0.67;"
      "z_z=0.19;z_res=0.57;z_mix=0.29;z_rate=0.0313;z_depth=0.22;odd_even=-0.19;src2_type=Additive;"
      "src2_level=0.18;src2_ratio=5/3;src2_octave=0;src2_partials=6;src2_bright=0.53;src2_drift=2.7;"
      "far_width=0.472",
      nullptr, nullptr, nullptr,
      "lfo1>spread:0.148;lfo2>pan_drift:0.234;lfo3>air:-0.213;lfo4>cutoff:0.238;beat>brightness:0.102;slide>tilt:0.185:u;wheel>z_y:0.443:u" },
    { "Glass Cathedral",
      "partials=24;tilt=0.8;brightness=0.95;inharmonic=0.15;cutoff=6000;far_size=3;far_decay=45;"
      "far_highcut=7000;far_damp=0.3;ens_mix=0.6;shimmer=0.6;depth=0.6;scale=Harmonic 8-16;root=E;"
      "lfo1_rate=0.02136;lfo1_depth=0.49;lfo2_rate=0.013201;lfo2_depth=0.41;lfo3_rate=0.0081587;"
      "lfo3_depth=0.40;lfo4_rate=0.0050424;lfo4_depth=0.53;odd_even=-0.13;src2_type=Additive;src2_level=0.14;"
      "src2_ratio=2/1;src2_octave=1;src2_partials=6;src2_bright=0.36;src2_drift=5.1;far_width=0.742",
      nullptr, nullptr, nullptr,
      "lfo1>spread:0.179;lfo2>brightness:0.131;lfo3>z_y:-0.146;lfo4>shimmer:0.147;beat>cutoff:0.107;slide>tilt:0.200:u;pressure>far_level:0.211:u" },
    { "Subharmonic Deep",
      "scale=Subharmonic 16-8;brain_low=28;brain_high=60;tilt=1.8;cutoff=900;bass_mono=200;brain_density=4;"
      "partials=10;far_decay=20;far_highcut=1800;air=0.05;strands=4;detune=12;lfo1_rate=0.017683;"
      "lfo1_depth=0.53;lfo2_rate=0.010929;lfo2_depth=0.64;lfo3_rate=0.0067544;lfo3_depth=0.52;"
      "lfo4_rate=0.0041745;lfo4_depth=0.45;odd_even=-0.18;src2_type=Additive;src2_level=0.18;src2_ratio=2/1;"
      "src2_octave=1;src2_partials=9;src2_bright=0.59;src2_drift=2.8;far_width=0.719",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:-0.156;lfo2>shimmer:-0.165;lfo3>cutoff:0.104;lfo4>brightness:-0.135;beat>far_size:0.090;wheel>air:0.354:u" },
    { "Breath of Flutes",
      "air=0.5;air_color=2;air_q=14;partials=6;tilt=1.5;keys_depth=0;depth=0.5;brain_low=55;brain_high=84;"
      "near_mix=0.3;far_level=0.6;dly_mix=0.3;dly_feedback=0.4;attack=3;release=8;master_gain=-10;"
      "lfo1_rate=0.016856;lfo1_depth=0.58;lfo2_rate=0.010418;lfo2_depth=0.61;lfo3_rate=0.0064385;"
      "lfo3_depth=0.52;lfo4_rate=0.0039792;lfo4_depth=0.54;z_mode=Series;z_shape=Glass;z_x=0.32;z_y=0.43;"
      "z_z=0.10;z_res=0.44;z_mix=0.40;z_rate=0.0184;z_depth=0.21;odd_even=-0.19;src2_type=Additive;"
      "src2_level=0.15;src2_ratio=4/3;src2_octave=1;src2_partials=9;src2_bright=0.56;src2_drift=4.8;"
      "far_width=0.477;haas=0.236;haas_time=14.2",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:-0.162;lfo2>arc:0.139;lfo3>brightness:-0.102;lfo4>pan_drift:0.170;beat>sub_level:0.086;wheel>far_level:0.351:u;pressure>z_x:0.291:u" },
    { "Bohlen Night",
      "scale=Bohlen-Pierce (JI);keymap=Consecutive degrees;brain_consonance=0.4;brain_wander=0.5;brain_low=40;"
      "brain_high=80;odd_even=0.6;far_decay=30;depth=0.75;lfo1_rate=0.013895;lfo1_depth=0.61;"
      "lfo2_rate=0.0085879;lfo2_depth=0.43;lfo3_rate=0.0053076;lfo3_depth=0.40;lfo4_rate=0.0032803;"
      "lfo4_depth=0.53;src2_type=Additive;src2_level=0.15;src2_ratio=3/2;src2_octave=1;src2_partials=6;"
      "src2_bright=0.38;src2_drift=5.4",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:0.196;lfo2>cutoff:0.141;lfo3>z_y:0.111;lfo4>brightness:0.223;beat>resonance:0.109;wheel>filter_fold:0.335:u" },
    { "Otonal Shimmer",
      "scale=Otonality 1-11;shimmer=0.9;shimmer_rate=0.4;dly_time_l=0.45;dly_time_r=0.68;dly_cross=0.7;"
      "dly_feedback=0.6;dly_mix=0.35;brightness=0.8;depth=0.6;brain_density=6;lfo1_rate=0.01553;"
      "lfo1_depth=0.59;lfo2_rate=0.0095982;lfo2_depth=0.58;lfo3_rate=0.005932;lfo3_depth=0.64;"
      "lfo4_rate=0.0036662;lfo4_depth=0.52;odd_even=-0.23;src2_type=Additive;src2_level=0.19;src2_ratio=5/3;"
      "src2_octave=1;src2_partials=13;src2_bright=0.53;src2_drift=5.8;far_width=0.798;haas=0.245;haas_time=19.7",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:0.179;lfo2>air:-0.166;lfo3>resonance:0.227;lfo4>ens_depth:-0.239;beat>cosmos_smear:0.136;pressure>brightness:0.168:u;wheel>filter_fold:0.403:u" },
    { "Distant Storm",
      "depth=1;keys_depth=0.8;brain_consonance=0.15;brain_density=8;brain_rate=12;brain_hold_min=20;"
      "brain_hold_max=90;far_decay=60;far_level=1;far_highcut=2000;dly_feedback=0.8;dly_to_far=0.8;"
      "dly_mix=0.15;cutoff=1500;inharmonic=0.3;strands=5;detune=20;master_gain=-9;fb_bus=0.25;fb_drive=0.8;"
      "fb_tone=1200;lfo1_rate=0.016881;lfo1_depth=0.46;lfo2_rate=0.010433;lfo2_depth=0.53;lfo3_rate=0.0064481;"
      "lfo3_depth=0.48;lfo4_rate=0.0039851;lfo4_depth=0.44;odd_even=-0.17;src2_type=Additive;src2_level=0.10;"
      "src2_ratio=5/4;src2_octave=1;src2_partials=8;src2_bright=0.48;src2_drift=5.3;haas=0.150;haas_time=16.5;"
      "filter_fold=0.226",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.155;lfo2>brightness:0.173;lfo3>resonance:0.131;lfo4>shimmer:-0.172;beat>far_size:0.159;slide>odd_even:0.351:u;pressure>cutoff:0.318:u" },
    { "Dry Foreground Keys",
      "brain_on=off;keys_depth=0;near_mix=0.25;near_decay=1.5;far_level=0.3;dly_mix=0.2;ens_mix=0.5;attack=1.5;"
      "release=6;air=0.25;depth=0;presence=3;strands=3;stack=Major;detune=0;drift=1;lfo1_rate=0.014425;"
      "lfo1_depth=0.54;lfo2_rate=0.0089152;lfo2_depth=0.57;lfo3_rate=0.0055099;lfo3_depth=0.64;"
      "lfo4_rate=0.0034053;lfo4_depth=0.49;src2_type=Additive;src2_level=0.16;src2_ratio=5/3;src2_octave=1;"
      "src2_partials=13;src2_bright=0.52;src2_drift=3.9;ens_mode=Microshift;ens_depth=0.724;ens_rate=0.0705",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.141;lfo2>shimmer:-0.216;lfo3>tilt:0.239;lfo4>pan_drift:0.214;beat>brightness:0.166;pressure>far_level:0.115:u;slide>inharmonic:0.241:u" },
    { "Sleep Concert II",
      "scale=JI Minor;root=A;brain_density=5;brain_rate=45;brain_hold_min=90;brain_hold_max=300;depth=0.9;"
      "far_decay=40;far_highcut=2000;arc=0.6;arc_period=90;attack=12;release=30;sub_level=0.3;"
      "sub_source=Difference;breath=0.35;breath_rate=0.02;lfo1_rate=0.020793;lfo1_depth=0.47;"
      "lfo2_rate=0.012851;lfo2_depth=0.57;lfo3_rate=0.0079421;lfo3_depth=0.55;lfo4_rate=0.0049085;"
      "lfo4_depth=0.57;z_mode=Series;z_shape=Choir;z_x=0.43;z_y=0.50;z_z=0.25;z_res=0.42;z_mix=0.33;"
      "z_rate=0.0207;z_depth=0.28;odd_even=-0.14;src2_type=Additive;src2_level=0.10;src2_ratio=5/3;"
      "src2_octave=1;src2_partials=10;src2_bright=0.52;src2_drift=3.2;far_width=0.726",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:0.147;lfo2>ens_depth:0.103;lfo3>z_y:0.231;lfo4>shimmer:0.170;beat>depth:0.148;wheel>filter_fold:0.404:u;pressure>far_level:0.190:u" },



    // ---------------------------------------------------------------- 10..19 sleep / night
    { "Midnight Pentatonic",
      "scale=JI Pentatonic;root=F#;brain_density=4;brain_low=40;brain_high=76;air=0.25;far_decay=30;attack=8;"
      "release=20;lfo1_rate=0.020604;lfo1_depth=0.42;lfo2_rate=0.012734;lfo2_depth=0.40;lfo3_rate=0.00787;"
      "lfo3_depth=0.56;lfo4_rate=0.0048639;lfo4_depth=0.58;odd_even=-0.19;src2_type=Additive;src2_level=0.13;"
      "src2_ratio=3/2;src2_octave=0;src2_partials=11;src2_bright=0.35;src2_drift=4.7",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.112;lfo2>shimmer:-0.127;lfo3>spread:-0.215;lfo4>cutoff:-0.177;beat>depth:0.107;slide>tilt:0.345:u" },
    { "Deep Sleep Sub",
      "root=C;brain_low=24;brain_high=55;tilt=2;cutoff=600;bass_mono=220;brain_density=3;far_decay=50;"
      "far_highcut=1200;master_gain=-4;attack=15;release=40;sub_level=0.6;sub_octave=-2;sub_binaural=2;"
      "sub_glide=15;lfo1_rate=0.014744;lfo1_depth=0.57;lfo2_rate=0.0091121;lfo2_depth=0.48;lfo3_rate=0.0056316;"
      "lfo3_depth=0.60;lfo4_rate=0.0034805;lfo4_depth=0.64;z_mode=Series;z_shape=Soprano;z_x=0.26;z_y=0.70;"
      "z_z=0.27;z_res=0.64;z_mix=0.42;z_rate=0.0263;z_depth=0.34;odd_even=-0.13;src2_type=Additive;"
      "src2_level=0.19;src2_ratio=4/3;src2_octave=0;src2_partials=12;src2_bright=0.43;src2_drift=3.9",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:0.187;lfo2>z_y:0.167;lfo3>pan_drift:0.200;lfo4>detune:0.133;beat>brightness:0.158;slide>odd_even:0.199:u" },
    { "Dawn Drift",
      "brightness=0.85;arc=1;arc_period=120;brain_density=6;scale=JI Major (Ptolemy);root=D;far_decay=20;"
      "air=0.15;lfo1_rate=0.016881;lfo1_depth=0.44;lfo2_rate=0.010433;lfo2_depth=0.44;lfo3_rate=0.0064481;"
      "lfo3_depth=0.40;lfo4_rate=0.0039851;lfo4_depth=0.53;odd_even=-0.20;src2_type=Additive;src2_level=0.13;"
      "src2_ratio=3/2;src2_octave=1;src2_partials=6;src2_bright=0.39;src2_drift=2.8;far_width=0.695",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:-0.144;lfo2>arc:0.120;lfo3>resonance:-0.206;lfo4>ens_depth:0.115;beat>cutoff:0.083;slide>inharmonic:0.311:u" },
    { "Night Rain",
      "air=0.45;air_color=6;air_q=6;partials=8;brain_density=5;dly_mix=0.3;dly_feedback=0.6;dly_damp=0.8;"
      "far_decay=15;master_gain=-8;lfo1_rate=0.029412;lfo1_depth=0.55;lfo2_rate=0.018177;lfo2_depth=0.61;"
      "lfo3_rate=0.011234;lfo3_depth=0.52;lfo4_rate=0.0069432;lfo4_depth=0.57;odd_even=-0.19;"
      "src2_type=Additive;src2_level=0.19;src2_ratio=5/3;src2_octave=1;src2_partials=9;src2_bright=0.56;"
      "src2_drift=5.5;far_width=0.741",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:0.235;lfo2>detune:0.170;lfo3>z_y:0.139;lfo4>air:-0.111;beat>depth:0.091;slide>inharmonic:0.166:u" },
    { "Slow Tide",
      "arc=1;arc_period=30;brain_density=6;brain_rate=20;brain_hold_min=40;brain_hold_max=160;depth=0.8;"
      "far_decay=30;lfo1_rate=0.014517;lfo1_depth=0.50;lfo2_rate=0.0089722;lfo2_depth=0.48;lfo3_rate=0.0055452;"
      "lfo3_depth=0.44;lfo4_rate=0.0034271;lfo4_depth=0.53;z_mode=Series;z_shape=Choir;z_x=0.22;z_y=0.47;"
      "z_z=0.29;z_res=0.42;z_mix=0.43;z_rate=0.0226;z_depth=0.43;odd_even=-0.26;src2_type=Additive;"
      "src2_level=0.16;src2_ratio=3/2;src2_octave=1;src2_partials=7;src2_bright=0.43;src2_drift=3.4",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:0.197;lfo2>arc:-0.150;lfo3>z_y:-0.121;lfo4>pan_drift:-0.163;beat>purity:0.133;wheel>filter_fold:0.528:u;slide>odd_even:0.335:u" },
    { "Hypnos",
      "attack=30;release=60;brain_hold_min=120;brain_hold_max=400;brain_rate=60;brain_density=7;far_decay=60;"
      "far_highcut=1800;depth=0.9;sub_level=0.4;sub_binaural=6;bloom=0.8;bloom_time=180;breath=0.5;"
      "breath_rate=0.015;lfo1_rate=0.020529;lfo1_depth=0.64;lfo2_rate=0.012688;lfo2_depth=0.43;"
      "lfo3_rate=0.0078415;lfo3_depth=0.43;lfo4_rate=0.0048463;lfo4_depth=0.62;z_mode=Series;z_shape=Soprano;"
      "z_x=0.58;z_y=0.58;z_z=0.11;z_res=0.44;z_mix=0.38;z_rate=0.0329;z_depth=0.38;odd_even=-0.17;"
      "src2_type=Additive;src2_level=0.17;src2_ratio=5/3;src2_octave=0;src2_partials=7;src2_bright=0.38;"
      "src2_drift=4.2;far_width=0.582",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:0.186;lfo2>air:-0.228;lfo3>z_y:0.222;lfo4>arc:0.178;beat>purity:0.123;pressure>z_x:0.303:u;wheel>far_level:0.237:u" },
    { "Warm Blanket",
      "tilt=1.6;brightness=0.5;cutoff=1200;air=0.1;scale=JI Major (Ptolemy);root=G;near_mix=0.35;far_level=0.5;"
      "depth=0.4;breath=0.4;lfo1_rate=0.013539;lfo1_depth=0.58;lfo2_rate=0.0083677;lfo2_depth=0.55;"
      "lfo3_rate=0.0051715;lfo3_depth=0.61;lfo4_rate=0.0031962;lfo4_depth=0.61;odd_even=-0.20;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=5/3;src2_octave=0;src2_partials=12;src2_bright=0.50;"
      "src2_drift=5.0",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.185;lfo2>brightness:0.140;lfo3>air:-0.111;lfo4>spread:0.153;beat>purity:0.109;pressure>resonance:0.284:u" },
    { "Somnus Harmonics",
      "scale=Harmonic 8-16;root=C;brain_low=48;brain_high=84;partials=12;shimmer=0.6;depth=0.7;far_decay=30;"
      "strands=4;stack=Harmonics;detune=0;drift=1;brain_density=3;lfo1_rate=0.013678;lfo1_depth=0.64;"
      "lfo2_rate=0.0084534;lfo2_depth=0.52;lfo3_rate=0.0052245;lfo3_depth=0.63;lfo4_rate=0.0032289;"
      "lfo4_depth=0.62;odd_even=-0.14;src2_type=Additive;src2_level=0.13;src2_ratio=4/3;src2_octave=0;"
      "src2_partials=13;src2_bright=0.47;src2_drift=4.4",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:-0.220;lfo2>tilt:0.125;lfo3>resonance:-0.198;lfo4>shimmer:-0.237;beat>air:0.152;slide>inharmonic:0.234:u;wheel>far_level:0.389:u" },
    { "Breathing Dark",
      "scale=JI Minor;root=E;odd_even=0.5;tilt=1.4;shimmer=0.8;shimmer_rate=0.05;brain_density=5;far_decay=35;"
      "far_highcut=2200;z_mode=Series;z_shape=Wood;z_rate=0.015;z_depth=1;z_mix=0.5;lfo1_rate=0.021461;"
      "lfo1_depth=0.60;lfo2_rate=0.013264;lfo2_depth=0.61;lfo3_rate=0.0081973;lfo3_depth=0.49;"
      "lfo4_rate=0.0050662;lfo4_depth=0.44;src2_type=Additive;src2_level=0.17;src2_ratio=4/3;src2_octave=1;"
      "src2_partials=8;src2_bright=0.56;src2_drift=4.1",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.138;lfo2>detune:-0.225;lfo3>spread:0.213;lfo4>air:0.195;beat>depth:0.100;wheel>far_level:0.219:u;pressure>shimmer:0.272:u" },
    { "Velvet Hours",
      "tilt=1.5;brightness=0.55;cutoff=1500;strands=4;detune=6;air=0.12;brain_density=5;brain_rate=40;"
      "brain_hold_min=80;brain_hold_max=260;far_decay=38;far_highcut=2400;depth=0.8;arc=0.4;arc_period=75;"
      "pad_low_cut=100;sub_level=0.25;sub_source=Difference;lfo1_rate=0.0213;lfo1_depth=0.41;"
      "lfo2_rate=0.013164;lfo2_depth=0.62;lfo3_rate=0.0081358;lfo3_depth=0.62;lfo4_rate=0.0050282;"
      "lfo4_depth=0.43;odd_even=-0.24;src2_type=Additive;src2_level=0.16;src2_ratio=5/3;src2_octave=1;"
      "src2_partials=12;src2_bright=0.57;src2_drift=3.6",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.172;lfo2>shimmer:0.148;lfo3>spread:0.147;lfo4>cutoff:0.120;beat>purity:0.110;slide>tilt:0.345:u" },



    // ---------------------------------------------------------------- 20..29 cathedral / glass
    { "Ice Cathedral",
      "partials=32;tilt=0.7;brightness=1;inharmonic=0.25;cutoff=9000;far_size=3;far_decay=60;far_highcut=9000;"
      "far_damp=0.2;brain_density=5;brain_low=55;brain_high=96;lfo1_rate=0.021853;lfo1_depth=0.53;"
      "lfo2_rate=0.013506;lfo2_depth=0.63;lfo3_rate=0.0083472;lfo3_depth=0.49;lfo4_rate=0.0051589;"
      "lfo4_depth=0.60;odd_even=-0.22;src2_type=Additive;src2_level=0.17;src2_ratio=4/3;src2_octave=0;"
      "src2_partials=8;src2_bright=0.58;src2_drift=2.6",
      nullptr, nullptr, nullptr,
      "lfo1>spread:-0.114;lfo2>z_y:0.223;lfo3>cutoff:0.151;lfo4>ens_depth:-0.182;beat>resonance:0.122;slide>odd_even:0.230:u;pressure>far_level:0.216:u" },
    { "Glass Choir",
      "partials=20;air=0.3;air_color=4;air_q=20;ens_mix=0.7;far_decay=30;far_highcut=6000;"
      "scale=JI Major (Ptolemy);root=A;pad_low_cut=120;presence=2;lfo1_rate=0.025341;lfo1_depth=0.44;"
      "lfo2_rate=0.015662;lfo2_depth=0.44;lfo3_rate=0.0096795;lfo3_depth=0.56;lfo4_rate=0.0059822;"
      "lfo4_depth=0.51;z_mode=Series;z_shape=Soprano;z_x=0.48;z_y=0.68;z_z=0.27;z_res=0.52;z_mix=0.44;"
      "z_rate=0.0378;z_depth=0.23;odd_even=-0.21;src2_type=Additive;src2_level=0.12;src2_ratio=5/3;"
      "src2_octave=1;src2_partials=11;src2_bright=0.39;src2_drift=3.6;far_width=0.683",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.108;lfo2>ens_depth:-0.188;lfo3>resonance:0.211;lfo4>arc:-0.194;beat>cosmos_smear:0.094;wheel>air:0.164:u;pressure>brightness:0.288:u" },
    { "Crystal Bells",
      "inharmonic=0.5;partials=24;tilt=0.9;attack=0.5;decay=20;sustain=0.3;release=20;brain_rate=8;"
      "brain_hold_min=5;brain_hold_max=20;brain_density=6;far_decay=25;far_highcut=8000;lfo1_rate=0.023145;"
      "lfo1_depth=0.46;lfo2_rate=0.014305;lfo2_depth=0.50;lfo3_rate=0.0088407;lfo3_depth=0.48;"
      "lfo4_rate=0.0054639;lfo4_depth=0.60;odd_even=-0.22;src2_type=Additive;src2_level=0.16;src2_ratio=4/3;"
      "src2_octave=0;src2_partials=8;src2_bright=0.45;src2_drift=2.1;far_width=0.776",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:-0.184;lfo2>detune:-0.149;lfo3>ens_depth:-0.129;lfo4>spread:-0.137;beat>air:0.155;slide>tilt:0.192:u;wheel>filter_fold:0.330:u" },
    { "Wineglass Rim",
      "partials=3;tilt=0.5;strands=6;detune=3;air=0.35;air_color=1;air_q=30;far_decay=30;brain_low=60;"
      "brain_high=90;far_highcut=8000;master_gain=-9;lfo1_rate=0.015107;lfo1_depth=0.53;lfo2_rate=0.0093364;"
      "lfo2_depth=0.48;lfo3_rate=0.0057702;lfo3_depth=0.53;lfo4_rate=0.0035662;lfo4_depth=0.48;z_mode=Series;"
      "z_shape=Choir;z_x=0.28;z_y=0.59;z_z=0.15;z_res=0.47;z_mix=0.38;z_rate=0.0217;z_depth=0.21;"
      "odd_even=-0.30;src2_type=Additive;src2_level=0.17;src2_ratio=3/2;src2_octave=1;src2_partials=10;"
      "src2_bright=0.43;src2_drift=2.9;far_width=0.541",
      nullptr, nullptr, nullptr,
      "lfo1>spread:-0.176;lfo2>arc:-0.227;lfo3>tilt:-0.239;lfo4>shimmer:0.196;beat>air:0.168;pressure>far_level:0.223:u;wheel>z_y:0.328:u" },
    { "High Organ",
      "odd_even=-0.5;partials=16;brightness=0.9;cutoff=8000;near_mix=0.4;near_decay=3;far_level=0.6;"
      "scale=Pythagorean;root=D;strands=4;stack=Fifths;detune=0;drift=0.5;lfo1_rate=0.02132;lfo1_depth=0.58;"
      "lfo2_rate=0.013176;lfo2_depth=0.49;lfo3_rate=0.0081434;lfo3_depth=0.50;lfo4_rate=0.0050329;"
      "lfo4_depth=0.63;src2_type=Additive;src2_level=0.16;src2_ratio=5/3;src2_octave=0;src2_partials=9;"
      "src2_bright=0.44;src2_drift=5.1;far_width=0.814",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.204;lfo2>arc:0.150;lfo3>ens_depth:-0.182;lfo4>brightness:-0.149;beat>depth:0.111;wheel>filter_fold:0.463:u;pressure>shimmer:0.205:u" },
    { "Stained Light",
      "brightness=0.8;shimmer=1;shimmer_rate=0.6;partials=24;far_decay=40;far_highcut=7000;"
      "scale=Otonality 1-11;root=F;lfo1_rate=0.01552;lfo1_depth=0.47;lfo2_rate=0.0095917;lfo2_depth=0.63;"
      "lfo3_rate=0.005928;lfo3_depth=0.58;lfo4_rate=0.0036637;lfo4_depth=0.58;odd_even=-0.21;"
      "src2_type=Additive;src2_level=0.19;src2_ratio=5/3;src2_octave=0;src2_partials=11;src2_bright=0.58;"
      "src2_drift=2.8;far_width=0.763",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.235;lfo2>spread:0.240;lfo3>ens_depth:0.205;lfo4>shimmer:-0.141;beat>sub_level:0.170;wheel>filter_fold:0.489:u" },
    { "Frozen Chapel",
      "far_decay=90;far_highcut=5000;far_size=3;brain_density=3;attack=20;release=50;partials=20;"
      "brightness=0.85;bloom=1;bloom_time=200;lfo1_rate=0.016172;lfo1_depth=0.51;lfo2_rate=0.0099948;"
      "lfo2_depth=0.54;lfo3_rate=0.0061771;lfo3_depth=0.60;lfo4_rate=0.0038177;lfo4_depth=0.64;odd_even=-0.22;"
      "src2_type=Additive;src2_level=0.19;src2_ratio=3/2;src2_octave=0;src2_partials=12;src2_bright=0.49;"
      "src2_drift=4.3",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.187;lfo2>brightness:0.167;lfo3>resonance:0.174;lfo4>cutoff:0.148;beat>z_y:0.091;pressure>far_level:0.241:u;wheel>filter_fold:0.271:u" },
    { "Silver Threads",
      "strands=6;detune=25;drift=12;partials=12;brightness=0.9;dly_mix=0.3;dly_time_l=1.7;dly_time_r=2.3;"
      "dly_feedback=0.7;far_decay=20;far_highcut=6000;lfo1_rate=0.015435;lfo1_depth=0.40;lfo2_rate=0.0095395;"
      "lfo2_depth=0.56;lfo3_rate=0.0058957;lfo3_depth=0.58;lfo4_rate=0.0036438;lfo4_depth=0.45;odd_even=-0.27;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=5/3;src2_octave=1;src2_partials=11;src2_bright=0.51;"
      "src2_drift=2.3;haas=0.151;haas_time=12.2",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:-0.191;lfo2>pan_drift:0.211;lfo3>brightness:-0.221;lfo4>arc:-0.107;beat>air:0.120;pressure>resonance:0.201:u;slide>tilt:0.203:u" },
    { "Bright Pentatonic",
      "scale=JI Pentatonic;root=E;brightness=0.9;cutoff=7000;brain_density=6;brain_low=52;brain_high=92;"
      "air=0.2;far_highcut=6000;lfo1_rate=0.01752;lfo1_depth=0.56;lfo2_rate=0.010828;lfo2_depth=0.48;"
      "lfo3_rate=0.0066919;lfo3_depth=0.57;lfo4_rate=0.0041358;lfo4_depth=0.48;odd_even=-0.26;"
      "src2_type=Additive;src2_level=0.12;src2_ratio=4/3;src2_octave=1;src2_partials=11;src2_bright=0.43;"
      "src2_drift=2.3;far_width=0.688",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.106;lfo2>resonance:-0.188;lfo3>spread:-0.167;lfo4>cutoff:0.236;beat>far_size:0.162;slide>tilt:0.317:u" },
    { "Prism",
      "inharmonic=0.15;shimmer=0.7;partials=28;tilt=1;brightness=1;far_highcut=8000;ens_mix=0.5;width=1.6;"
      "side_air=4;cutoff=8000;lfo1_rate=0.014416;lfo1_depth=0.63;lfo2_rate=0.0089096;lfo2_depth=0.43;"
      "lfo3_rate=0.0055064;lfo3_depth=0.43;lfo4_rate=0.0034032;lfo4_depth=0.47;odd_even=-0.21;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=3/2;src2_octave=1;src2_partials=7;src2_bright=0.38;"
      "src2_drift=2.9;far_width=0.525;ens_mode=Microshift;ens_depth=0.667;ens_rate=0.0460",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.157;lfo2>resonance:-0.235;lfo3>detune:-0.126;lfo4>air:-0.224;beat>cosmos_smear:0.147;slide>inharmonic:0.240:u" },



    // ---------------------------------------------------------------- 30..39 deep / sub / dark
    { "Abyss",
      "scale=Subharmonic 16-8;root=E;brain_low=24;brain_high=50;tilt=2.2;cutoff=500;partials=8;far_decay=45;"
      "far_highcut=900;bass_mono=250;brain_density=4;lfo1_rate=0.027389;lfo1_depth=0.48;lfo2_rate=0.016928;"
      "lfo2_depth=0.63;lfo3_rate=0.010462;lfo3_depth=0.46;lfo4_rate=0.0064658;lfo4_depth=0.56;odd_even=-0.30;"
      "src2_type=Additive;src2_level=0.16;src2_ratio=2/1;src2_octave=1;src2_partials=7;src2_bright=0.58;"
      "src2_drift=4.5",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.199;lfo2>brightness:-0.150;lfo3>z_y:0.112;lfo4>detune:-0.109;beat>far_size:0.111;slide>odd_even:0.343:u;wheel>air:0.238:u" },
    { "Tectonic",
      "root=C;brain_low=24;brain_high=48;strands=5;detune=15;tilt=1.8;cutoff=700;far_decay=30;dly_mix=0.2;"
      "dly_time_l=2.5;dly_time_r=3.3;dly_feedback=0.6;dly_damp=0.9;far_highcut=1200;lfo1_rate=0.01293;"
      "lfo1_depth=0.42;lfo2_rate=0.0079912;lfo2_depth=0.46;lfo3_rate=0.0049388;lfo3_depth=0.47;"
      "lfo4_rate=0.0030524;lfo4_depth=0.44;odd_even=-0.30;src2_type=Additive;src2_level=0.12;src2_ratio=3/2;"
      "src2_octave=1;src2_partials=8;src2_bright=0.41;src2_drift=4.5;far_width=0.641",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:0.225;lfo2>z_y:-0.187;lfo3>tilt:0.132;lfo4>detune:0.216;beat>cosmos_smear:0.161;slide>inharmonic:0.272:u;wheel>filter_fold:0.327:u" },
    { "Whale Song",
      "brain_low=30;brain_high=62;drift=25;drift_rate=0.05;partials=6;air=0.3;air_color=2;air_q=8;far_decay=40;"
      "scale=JI Minor;root=D;far_highcut=2500;lfo1_rate=0.015137;lfo1_depth=0.57;lfo2_rate=0.0093551;"
      "lfo2_depth=0.58;lfo3_rate=0.0057818;lfo3_depth=0.61;lfo4_rate=0.0035733;lfo4_depth=0.64;odd_even=-0.27;"
      "src2_type=Additive;src2_level=0.14;src2_ratio=4/3;src2_octave=0;src2_partials=12;src2_bright=0.53;"
      "src2_drift=3.9",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:-0.117;lfo2>pan_drift:-0.127;lfo3>shimmer:-0.128;lfo4>spread:-0.224;beat>z_y:0.152;wheel>far_level:0.309:u;slide>inharmonic:0.311:u" },
    { "Cave Water",
      "air=0.5;air_color=8;air_q=4;partials=6;cutoff=900;near_mix=0.5;near_decay=4;far_decay=25;"
      "brain_density=3;brain_low=30;brain_high=60;master_gain=-8;cloud_send=0.4;cloud_pitch=0;cloud_size=400;"
      "cloud_spray=1.5;lfo1_rate=0.013475;lfo1_depth=0.51;lfo2_rate=0.0083279;lfo2_depth=0.44;"
      "lfo3_rate=0.0051469;lfo3_depth=0.62;lfo4_rate=0.003181;lfo4_depth=0.65;z_mode=Series;z_shape=Choir;"
      "z_x=0.47;z_y=0.39;z_z=0.13;z_res=0.44;z_mix=0.25;z_rate=0.0081;z_depth=0.29;odd_even=-0.13;"
      "src2_type=Additive;src2_level=0.12;src2_ratio=3/2;src2_octave=0;src2_partials=13;src2_bright=0.39;"
      "src2_drift=3.5;haas=0.341;haas_time=15.8",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:-0.152;lfo2>cutoff:0.182;lfo3>detune:0.210;lfo4>z_y:0.151;beat>sub_level:0.093;pressure>shimmer:0.398:u;slide>odd_even:0.169:u" },
    { "Undertow",
      "scale=Subharmonic 16-8;root=A;brain_density=5;brain_consonance=0.5;tilt=1.5;cutoff=1000;far_size=3;"
      "far_decay=50;far_highcut=1500;lfo1_rate=0.018905;lfo1_depth=0.62;lfo2_rate=0.011684;lfo2_depth=0.55;"
      "lfo3_rate=0.0072212;lfo3_depth=0.64;lfo4_rate=0.004463;lfo4_depth=0.59;odd_even=-0.14;"
      "src2_type=Additive;src2_level=0.14;src2_ratio=5/4;src2_octave=0;src2_partials=13;src2_bright=0.50;"
      "src2_drift=2.7;far_width=0.739",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.148;lfo2>pan_drift:0.199;lfo3>cutoff:0.229;lfo4>air:0.152;beat>far_size:0.141;slide>inharmonic:0.265:u;wheel>filter_fold:0.281:u" },
    { "Bass Temple",
      "scale=JI Major (Ptolemy);root=C;brain_low=28;brain_high=52;odd_even=-0.6;cutoff=800;near_mix=0.3;"
      "far_level=0.6;bass_mono=200;lfo1_rate=0.018672;lfo1_depth=0.64;lfo2_rate=0.01154;lfo2_depth=0.49;"
      "lfo3_rate=0.0071321;lfo3_depth=0.54;lfo4_rate=0.0044079;lfo4_depth=0.45;src2_type=Additive;"
      "src2_level=0.11;src2_ratio=5/4;src2_octave=1;src2_partials=10;src2_bright=0.44;src2_drift=4.7;"
      "far_width=0.750",
      nullptr, nullptr, nullptr,
      "lfo1>air:0.121;lfo2>spread:-0.180;lfo3>arc:-0.166;lfo4>tilt:0.148;beat>z_y:0.160;slide>inharmonic:0.354:u;wheel>far_level:0.216:u" },
    { "Slow Magma",
      "inharmonic=0.4;partials=10;brain_low=26;brain_high=54;tilt=1.7;cutoff=600;resonance=0.5;"
      "filter_drift=0.8;far_decay=35;far_highcut=1500;lfo1_rate=0.020382;lfo1_depth=0.64;lfo2_rate=0.012597;"
      "lfo2_depth=0.59;lfo3_rate=0.0077852;lfo3_depth=0.52;lfo4_rate=0.0048115;lfo4_depth=0.41",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.154;lfo2>shimmer:-0.165;lfo3>resonance:0.192;lfo4>spread:-0.228;beat>purity:0.135;wheel>air:0.399:u;slide>tilt:0.157:u" },
    { "Deep Space Hum",
      "scale=12-TET;root=C;brain_low=24;brain_high=44;brain_density=3;partials=4;cutoff=400;far_decay=60;"
      "far_highcut=800;air=0.15;air_color=12;air_q=3;lfo1_rate=0.021938;lfo1_depth=0.58;lfo2_rate=0.013558;"
      "lfo2_depth=0.55;lfo3_rate=0.0083795;lfo3_depth=0.48;lfo4_rate=0.0051788;lfo4_depth=0.44;odd_even=-0.20;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=4/3;src2_octave=1;src2_partials=8;src2_bright=0.50;"
      "src2_drift=5.6;far_width=0.720",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.103;lfo2>tilt:0.135;lfo3>brightness:0.163;lfo4>spread:0.218;beat>purity:0.153;wheel>filter_fold:0.516:u" },
    { "Subterranean Choir",
      "scale=Harmonic 8-16;root=C;brain_low=36;brain_high=64;air=0.3;air_color=3;air_q=12;cutoff=1500;"
      "far_decay=30;lfo1_rate=0.013019;lfo1_depth=0.48;lfo2_rate=0.0080462;lfo2_depth=0.53;lfo3_rate=0.0049728;"
      "lfo3_depth=0.48;lfo4_rate=0.0030734;lfo4_depth=0.53;odd_even=-0.25;src2_type=Additive;src2_level=0.16;"
      "src2_ratio=5/4;src2_octave=1;src2_partials=8;src2_bright=0.48;src2_drift=4.0;far_width=0.714",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:0.179;lfo2>shimmer:0.149;lfo3>arc:0.103;lfo4>air:-0.153;beat>cutoff:0.088;slide>inharmonic:0.211:u;pressure>far_level:0.120:u" },
    { "Low Pythagorean",
      "scale=Pythagorean;root=G;brain_low=29;brain_high=60;tilt=1.5;strands=4;detune=10;far_decay=28;"
      "dly_mix=0.25;far_highcut=2000;lfo1_rate=0.01672;lfo1_depth=0.47;lfo2_rate=0.010333;lfo2_depth=0.56;"
      "lfo3_rate=0.0063863;lfo3_depth=0.64;lfo4_rate=0.003947;lfo4_depth=0.52;odd_even=-0.18;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=3/2;src2_octave=1;src2_partials=13;src2_bright=0.51;"
      "src2_drift=2.8",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.196;lfo2>brightness:0.176;lfo3>spread:0.236;lfo4>resonance:0.178;beat>z_y:0.160;slide>inharmonic:0.383:u;wheel>filter_fold:0.214:u" },



    // ---------------------------------------------------------------- 40..49 breath / flute / voice
    { "Shakuhachi Air",
      "air=0.7;air_color=1;air_q=6;partials=5;tilt=1.8;keys_depth=0;brain_low=60;brain_high=88;brain_density=3;"
      "brain_rate=10;brain_hold_min=8;brain_hold_max=30;attack=1.5;release=5;far_decay=12;master_gain=-8;"
      "lfo1_rate=0.014564;lfo1_depth=0.55;lfo2_rate=0.009001;lfo2_depth=0.57;lfo3_rate=0.0055629;"
      "lfo3_depth=0.42;lfo4_rate=0.0034381;lfo4_depth=0.50;odd_even=-0.19;src2_type=Additive;src2_level=0.11;"
      "src2_ratio=3/2;src2_octave=1;src2_partials=6;src2_bright=0.52;src2_drift=4.9;far_width=0.583",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.107;lfo2>arc:-0.109;lfo3>resonance:-0.118;lfo4>pan_drift:-0.110;beat>brightness:0.126;pressure>cutoff:0.424:u" },
    { "Ocarina Clouds",
      "air=0.4;air_color=2;air_q=18;partials=3;brain_low=64;brain_high=92;brain_density=5;far_decay=25;"
      "ens_mix=0.5;master_gain=-8;lfo1_rate=0.013435;lfo1_depth=0.47;lfo2_rate=0.0083033;lfo2_depth=0.63;"
      "lfo3_rate=0.0051317;lfo3_depth=0.43;lfo4_rate=0.0031716;lfo4_depth=0.43;z_mode=Series;z_shape=Glass;"
      "z_x=0.24;z_y=0.36;z_z=0.17;z_res=0.41;z_mix=0.30;z_rate=0.0202;z_depth=0.21;odd_even=-0.14;"
      "src2_type=Additive;src2_level=0.19;src2_ratio=3/2;src2_octave=1;src2_partials=6;src2_bright=0.58;"
      "src2_drift=3.0;far_width=0.614",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.225;lfo2>z_y:-0.169;lfo3>tilt:-0.227;lfo4>cutoff:0.213;beat>resonance:0.136;pressure>brightness:0.172:u;wheel>air:0.161:u" },
    { "Bamboo Wind",
      "air=0.6;air_color=5;air_q=5;partials=6;brain_low=55;brain_high=84;dly_mix=0.35;dly_feedback=0.5;"
      "far_decay=20;master_gain=-8;lfo1_rate=0.013475;lfo1_depth=0.63;lfo2_rate=0.0083279;lfo2_depth=0.49;"
      "lfo3_rate=0.0051469;lfo3_depth=0.63;lfo4_rate=0.003181;lfo4_depth=0.46;odd_even=-0.24;"
      "src2_type=Additive;src2_level=0.14;src2_ratio=2/1;src2_octave=1;src2_partials=13;src2_bright=0.44;"
      "src2_drift=4.0;far_width=0.752;haas=0.324;haas_time=15.4",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.227;lfo2>ens_depth:0.134;lfo3>pan_drift:-0.198;lfo4>spread:0.141;beat>purity:0.153;pressure>cutoff:0.394:u" },
    { "Alto Voices",
      "air=0.3;air_color=3;air_q=14;cosmos_send=0.5;cosmos_vowel=0.8;cosmos_vowel_rate=0.03;cosmos_return=0.8;"
      "cosmos_to_far=0.3;brain_low=50;brain_high=76;partials=14;scale=JI Major (Ptolemy);root=F;"
      "master_gain=-10;lfo1_rate=0.021063;lfo1_depth=0.45;lfo2_rate=0.013017;lfo2_depth=0.59;"
      "lfo3_rate=0.0080452;lfo3_depth=0.52;lfo4_rate=0.0049722;lfo4_depth=0.41;z_mode=Series;"
      "z_shape=Rounded Vowels;z_x=0.39;z_y=0.50;z_z=0.29;z_res=0.52;z_mix=0.26;z_rate=0.0082;z_depth=0.39;"
      "odd_even=-0.22;src2_type=Additive;src2_level=0.11;src2_ratio=4/3;src2_octave=1;src2_partials=9;"
      "src2_bright=0.54;src2_drift=4.2;far_width=0.813",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:-0.136;lfo2>cutoff:0.181;lfo3>arc:-0.105;lfo4>brightness:0.223;beat>z_y:0.093;wheel>far_level:0.333:u" },
    { "Boys Choir",
      "cosmos_send=0.6;cosmos_vowel=0.9;cosmos_vowel_rate=0.08;cosmos_return=0.9;cosmos_to_far=0.5;"
      "brain_low=60;brain_high=88;partials=12;tilt=1.4;scale=JI Major (Ptolemy);root=C;brain_density=6;"
      "lfo1_rate=0.018642;lfo1_depth=0.45;lfo2_rate=0.011521;lfo2_depth=0.44;lfo3_rate=0.0071204;"
      "lfo3_depth=0.53;lfo4_rate=0.0044007;lfo4_depth=0.42;odd_even=-0.27;src2_type=Additive;src2_level=0.19;"
      "src2_ratio=5/3;src2_octave=1;src2_partials=10;src2_bright=0.39;src2_drift=4.2;far_width=0.655",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:-0.189;lfo2>brightness:-0.167;lfo3>arc:-0.174;lfo4>detune:0.210;beat>sub_level:0.091;slide>cosmos_vowel:0.238:u;wheel>filter_fold:0.564:u" },
    { "Throat Drone",
      "odd_even=0.4;brain_low=36;brain_high=55;cosmos_send=0.5;cosmos_vowel=0.7;cosmos_vowel_rate=0.02;"
      "cosmos_return=0.7;brain_density=3;partials=20;air=0.2;air_color=8;air_q=6;lfo1_rate=0.024815;"
      "lfo1_depth=0.56;lfo2_rate=0.015336;lfo2_depth=0.58;lfo3_rate=0.0094785;lfo3_depth=0.45;"
      "lfo4_rate=0.005858;lfo4_depth=0.50;src2_type=Additive;src2_level=0.16;src2_ratio=5/4;src2_octave=1;"
      "src2_partials=7;src2_bright=0.53;src2_drift=3.1",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.125;lfo2>pan_drift:-0.145;lfo3>shimmer:0.155;lfo4>cutoff:0.182;beat>depth:0.132;pressure>far_level:0.195:u;wheel>cosmos_send:0.406:u" },
    { "Whispered",
      "air=0.8;air_color=10;air_q=3;partials=2;master_gain=-10;brain_low=60;brain_high=96;brain_density=4;"
      "far_decay=20;near_mix=0.4;lfo1_rate=0.014658;lfo1_depth=0.54;lfo2_rate=0.0090591;lfo2_depth=0.54;"
      "lfo3_rate=0.0055989;lfo3_depth=0.61;lfo4_rate=0.0034603;lfo4_depth=0.64;odd_even=-0.19;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=5/3;src2_octave=0;src2_partials=12;src2_bright=0.49;"
      "src2_drift=3.4;far_width=0.614;haas=0.272;haas_time=15.5",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.222;lfo2>pan_drift:0.108;lfo3>ens_depth:-0.170;lfo4>arc:-0.209;beat>depth:0.081;wheel>filter_fold:0.464:u;slide>odd_even:0.271:u" },
    { "Reed Organ",
      "odd_even=0.8;partials=24;tilt=1.1;air=0.15;air_color=4;near_mix=0.4;near_decay=2;far_level=0.5;"
      "keys_depth=0.2;scale=JI Major (Ptolemy);root=D;lfo1_rate=0.013629;lfo1_depth=0.50;lfo2_rate=0.0084229;"
      "lfo2_depth=0.44;lfo3_rate=0.0052056;lfo3_depth=0.62;lfo4_rate=0.0032173;lfo4_depth=0.43;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=2/1;src2_octave=1;src2_partials=13;src2_bright=0.39;"
      "src2_drift=2.4;far_width=0.729",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.102;lfo2>spread:0.179;lfo3>cutoff:0.105;lfo4>shimmer:0.197;beat>resonance:0.093;wheel>far_level:0.200:u;pressure>brightness:0.171:u" },
    { "Wooden Flutes",
      "air=0.5;air_color=2;air_q=10;partials=4;tilt=2;ens_mix=0.6;ens_depth=0.6;brain_low=55;brain_high=84;"
      "brain_hold_min=10;brain_hold_max=40;brain_rate=8;far_decay=15;master_gain=-8;lfo1_rate=0.018281;"
      "lfo1_depth=0.55;lfo2_rate=0.011298;lfo2_depth=0.42;lfo3_rate=0.0069828;lfo3_depth=0.50;"
      "lfo4_rate=0.0043156;lfo4_depth=0.41;odd_even=-0.18;src2_type=Additive;src2_level=0.18;src2_ratio=3/2;"
      "src2_octave=1;src2_partials=9;src2_bright=0.37;src2_drift=4.7;far_width=0.750",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.153;lfo2>spread:-0.165;lfo3>air:0.104;lfo4>z_y:0.144;beat>far_size:0.090;slide>tilt:0.317:u;pressure>cutoff:0.398:u" },
    { "Sirens Far Away",
      "air=0.4;air_color=3;air_q=12;drift=20;drift_rate=0.03;depth=1;keys_depth=0.9;far_decay=45;"
      "far_highcut=3000;brain_low=60;brain_high=90;brain_density=4;lfo1_rate=0.020437;lfo1_depth=0.55;"
      "lfo2_rate=0.012631;lfo2_depth=0.54;lfo3_rate=0.0078062;lfo3_depth=0.61;lfo4_rate=0.0048245;"
      "lfo4_depth=0.55;odd_even=-0.23;src2_type=Additive;src2_level=0.11;src2_ratio=2/1;src2_octave=1;"
      "src2_partials=12;src2_bright=0.49;src2_drift=4.7",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.163;lfo2>spread:0.104;lfo3>cutoff:0.197;lfo4>pan_drift:0.115;beat>air:0.148;wheel>filter_fold:0.363:u" },



    // ---------------------------------------------------------------- 50..59 exotic scales
    { "Slendro Dusk",
      "scale=Slendro (JI);root=D;brain_density=5;brain_low=48;brain_high=84;partials=10;inharmonic=0.2;"
      "attack=0.3;decay=10;sustain=0.4;release=15;brain_rate=6;brain_hold_min=5;brain_hold_max=25;far_decay=20;"
      "lfo1_rate=0.013912;lfo1_depth=0.55;lfo2_rate=0.0085984;lfo2_depth=0.61;lfo3_rate=0.0053141;"
      "lfo3_depth=0.46;lfo4_rate=0.0032843;lfo4_depth=0.44;odd_even=-0.25;src2_type=Additive;src2_level=0.13;"
      "src2_ratio=2/1;src2_octave=1;src2_partials=7;src2_bright=0.56;src2_drift=5.4;far_width=0.503",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.173;lfo2>cutoff:-0.122;lfo3>shimmer:-0.180;lfo4>brightness:-0.175;beat>depth:0.106;pressure>resonance:0.198:u;slide>inharmonic:0.355:u" },
    { "Bohlen Cathedral",
      "scale=Bohlen-Pierce (JI);keymap=Consecutive degrees;root=C;brain_density=6;partials=24;brightness=0.9;"
      "far_decay=40;far_highcut=6000;lfo1_rate=0.015007;lfo1_depth=0.48;lfo2_rate=0.0092747;lfo2_depth=0.54;"
      "lfo3_rate=0.0057321;lfo3_depth=0.42;lfo4_rate=0.0035426;lfo4_depth=0.43;odd_even=-0.17;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=4/3;src2_octave=1;src2_partials=6;src2_bright=0.49;"
      "src2_drift=4.9",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.120;lfo2>arc:0.136;lfo3>pan_drift:-0.181;lfo4>shimmer:-0.236;beat>z_y:0.108;pressure>cutoff:0.263:u;wheel>air:0.359:u" },
    { "Pythagorean Drone",
      "scale=Pythagorean;root=G;brain_consonance=0.9;brain_density=4;tilt=1.3;far_decay=30;lfo1_rate=0.015637;"
      "lfo1_depth=0.48;lfo2_rate=0.0096643;lfo2_depth=0.41;lfo3_rate=0.0059729;lfo3_depth=0.50;"
      "lfo4_rate=0.0036914;lfo4_depth=0.57;odd_even=-0.30;src2_type=Additive;src2_level=0.18;src2_ratio=5/4;"
      "src2_octave=1;src2_partials=9;src2_bright=0.36;src2_drift=4.8;far_width=0.822",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.112;lfo2>ens_depth:-0.232;lfo3>tilt:-0.222;lfo4>brightness:0.108;beat>purity:0.124;wheel>filter_fold:0.580:u" },
    { "Otonal Cloud",
      "scale=Otonality 1-11;root=C;brain_density=8;brain_consonance=0.3;brain_hold_min=20;brain_hold_max=80;"
      "brain_rate=8;far_decay=30;depth=0.8;lfo1_rate=0.020736;lfo1_depth=0.49;lfo2_rate=0.012815;"
      "lfo2_depth=0.63;lfo3_rate=0.0079203;lfo3_depth=0.55;lfo4_rate=0.004895;lfo4_depth=0.61;odd_even=-0.28;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=5/3;src2_octave=0;src2_partials=10;src2_bright=0.58;"
      "src2_drift=5.2",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.220;lfo2>cutoff:0.160;lfo3>z_y:-0.183;lfo4>shimmer:0.149;beat>resonance:0.112;wheel>far_level:0.226:u;slide>tilt:0.258:u" },
    { "Subharmonic Bells",
      "scale=Subharmonic 16-8;root=E;inharmonic=0.35;attack=0.2;decay=15;sustain=0.2;release=20;brain_rate=7;"
      "brain_hold_min=5;brain_hold_max=15;brain_density=7;lfo1_rate=0.019473;lfo1_depth=0.56;"
      "lfo2_rate=0.012035;lfo2_depth=0.48;lfo3_rate=0.007438;lfo3_depth=0.44;lfo4_rate=0.0045969;"
      "lfo4_depth=0.50;odd_even=-0.15;src2_type=Additive;src2_level=0.16;src2_ratio=4/3;src2_octave=1;"
      "src2_partials=7;src2_bright=0.43;src2_drift=2.7;far_width=0.693",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:0.124;lfo2>brightness:0.145;lfo3>arc:0.225;lfo4>detune:-0.152;beat>purity:0.132;slide>inharmonic:0.246:u;pressure>far_level:0.177:u" },
    { "Harmonic Ladder",
      "scale=Harmonic 8-16;root=D;brain_low=50;brain_high=98;brain_consonance=1;brain_density=8;brain_rate=12;"
      "brain_wander=0;lfo1_rate=0.016732;lfo1_depth=0.58;lfo2_rate=0.010341;lfo2_depth=0.48;"
      "lfo3_rate=0.0063911;lfo3_depth=0.44;lfo4_rate=0.0039499;lfo4_depth=0.56;odd_even=-0.30;"
      "src2_type=Additive;src2_level=0.10;src2_ratio=3/2;src2_octave=1;src2_partials=7;src2_bright=0.43;"
      "src2_drift=4.8;far_width=0.632",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:-0.111;lfo2>ens_depth:-0.171;lfo3>brightness:0.183;lfo4>spread:-0.210;beat>resonance:0.114;wheel>filter_fold:0.206:u;slide>tilt:0.239:u" },
    { "Seven Limit Garden",
      "scale=JI 7-limit;root=F;brain_density=7;brain_consonance=0.55;brain_wander=0.6;brain_hold_min=30;"
      "brain_hold_max=120;far_decay=25;lfo1_rate=0.01358;lfo1_depth=0.60;lfo2_rate=0.0083927;lfo2_depth=0.46;"
      "lfo3_rate=0.005187;lfo3_depth=0.47;lfo4_rate=0.0032057;lfo4_depth=0.50;odd_even=-0.15;"
      "src2_type=Additive;src2_level=0.13;src2_ratio=4/3;src2_octave=1;src2_partials=8;src2_bright=0.41;"
      "src2_drift=3.4",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.107;lfo2>tilt:0.197;lfo3>arc:0.167;lfo4>spread:-0.227;beat>brightness:0.163;pressure>far_level:0.218:u" },
    { "Minor Third Field",
      "scale=JI Minor;root=B;brain_density=6;brain_consonance=0.6;brain_low=40;brain_high=80;far_decay=35;"
      "dly_mix=0.2;lfo1_rate=0.013786;lfo1_depth=0.40;lfo2_rate=0.0085201;lfo2_depth=0.56;lfo3_rate=0.0052657;"
      "lfo3_depth=0.54;lfo4_rate=0.0032544;lfo4_depth=0.45;odd_even=-0.17;src2_type=Additive;src2_level=0.12;"
      "src2_ratio=3/2;src2_octave=1;src2_partials=10;src2_bright=0.51;src2_drift=2.4;haas=0.163;haas_time=16.2",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.173;lfo2>tilt:0.113;lfo3>z_y:0.127;lfo4>arc:0.119;beat>cosmos_smear:0.150;slide>inharmonic:0.159:u;wheel>dly_mix:0.310:u" },
    { "Twelve Tone Fog",
      "scale=12-TET;root=C;brain_consonance=0.2;brain_density=7;brain_rate=15;brain_hold_min=15;"
      "brain_hold_max=60;far_decay=40;far_highcut=2500;depth=0.9;lfo1_rate=0.014352;lfo1_depth=0.50;"
      "lfo2_rate=0.0088702;lfo2_depth=0.60;lfo3_rate=0.0054821;lfo3_depth=0.46;lfo4_rate=0.0033881;"
      "lfo4_depth=0.59;odd_even=-0.20;src2_type=Additive;src2_level=0.16;src2_ratio=2/1;src2_octave=0;"
      "src2_partials=7;src2_bright=0.55;src2_drift=3.4;far_width=0.621",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:0.149;lfo2>resonance:-0.147;lfo3>spread:-0.182;lfo4>shimmer:-0.184;beat>purity:0.110;slide>odd_even:0.151:u" },
    { "Pentatonic Rain",
      "scale=JI Pentatonic;root=A;attack=0.05;decay=6;sustain=0;release=10;brain_rate=3;brain_hold_min=5;"
      "brain_hold_max=8;brain_density=8;dly_mix=0.4;dly_feedback=0.6;dly_time_l=0.62;dly_time_r=0.93;"
      "far_decay=15;lfo1_rate=0.014444;lfo1_depth=0.63;lfo2_rate=0.0089266;lfo2_depth=0.55;lfo3_rate=0.0055169;"
      "lfo3_depth=0.54;lfo4_rate=0.0034097;lfo4_depth=0.57;odd_even=-0.21;src2_type=Additive;src2_level=0.19;"
      "src2_ratio=2/1;src2_octave=1;src2_partials=10;src2_bright=0.50;src2_drift=3.4;far_width=0.823",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.235;lfo2>ens_depth:-0.170;lfo3>resonance:-0.192;lfo4>tilt:0.167;beat>purity:0.136;slide>odd_even:0.370:u;pressure>cutoff:0.444:u" },



    // ---------------------------------------------------------------- 60..69 shimmer / delay / motion
    { "Shimmer Rise",
      "cosmos_shimmer=0.7;cosmos_shimmer_pitch=+12;far_decay=20;far_level=0.9;depth=0.6;brain_density=4;"
      "lfo1_rate=0.015573;lfo1_depth=0.58;lfo2_rate=0.0096246;lfo2_depth=0.52;lfo3_rate=0.0059483;"
      "lfo3_depth=0.41;lfo4_rate=0.0036763;lfo4_depth=0.62;odd_even=-0.18;src2_type=Additive;src2_level=0.11;"
      "src2_ratio=5/3;src2_octave=0;src2_partials=6;src2_bright=0.47;src2_drift=5.3;far_width=0.526",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.221;lfo2>shimmer:0.107;lfo3>cutoff:-0.118;lfo4>z_y:0.215;beat>brightness:0.126;pressure>far_level:0.197:u;slide>tilt:0.340:u" },
    { "Fifth Shimmer",
      "cosmos_shimmer=0.6;cosmos_shimmer_pitch=+7;scale=JI Major (Ptolemy);root=C;far_decay=25;brain_density=4;"
      "brightness=0.8;lfo1_rate=0.016477;lfo1_depth=0.45;lfo2_rate=0.010183;lfo2_depth=0.59;"
      "lfo3_rate=0.0062937;lfo3_depth=0.58;lfo4_rate=0.0038897;lfo4_depth=0.61;odd_even=-0.12;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=2/1;src2_octave=0;src2_partials=11;src2_bright=0.54;"
      "src2_drift=4.8",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.132;lfo2>cutoff:0.233;lfo3>shimmer:-0.187;lfo4>tilt:-0.210;beat>purity:0.124;slide>odd_even:0.258:u" },
    { "Octave Below",
      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=-12;far_decay=30;far_highcut=2000;brain_low=48;brain_high=84;"
      "brain_density=4;lfo1_rate=0.015321;lfo1_depth=0.61;lfo2_rate=0.0094687;lfo2_depth=0.65;"
      "lfo3_rate=0.005852;lfo3_depth=0.56;lfo4_rate=0.0036167;lfo4_depth=0.61;odd_even=-0.14;"
      "src2_type=Additive;src2_level=0.14;src2_ratio=4/3;src2_octave=0;src2_partials=10;src2_bright=0.60;"
      "src2_drift=5.4;far_width=0.580",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.132;lfo2>spread:-0.198;lfo3>air:-0.211;lfo4>ens_depth:0.124;beat>brightness:0.096;pressure>resonance:0.164:u" },
    { "Echo Canyon",
      "dly_time_l=1.5;dly_time_r=2.25;dly_feedback=0.75;dly_cross=0.5;dly_damp=0.6;dly_mix=0.45;dly_to_far=0.6;"
      "attack=0.5;brain_rate=10;brain_hold_min=8;brain_hold_max=30;brain_density=4;far_decay=12;dly2_mix=0.3;"
      "dly2_feedback=0.5;lfo1_rate=0.019778;lfo1_depth=0.44;lfo2_rate=0.012224;lfo2_depth=0.41;"
      "lfo3_rate=0.0075546;lfo3_depth=0.49;lfo4_rate=0.004669;lfo4_depth=0.57;odd_even=-0.21;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=5/4;src2_octave=1;src2_partials=9;src2_bright=0.36;"
      "src2_drift=3.2;far_width=0.558;haas=0.188;haas_time=19.9",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.147;lfo2>shimmer:0.234;lfo3>ens_depth:0.143;lfo4>arc:-0.164;beat>sub_level:0.102;wheel>air:0.319:u;slide>odd_even:0.274:u" },
    { "Ping Pong Drift",
      "dly_time_l=0.33;dly_time_r=0.5;dly_cross=1;dly_feedback=0.7;dly_mix=0.4;attack=0.3;decay=3;sustain=0.5;"
      "release=6;brain_rate=4;brain_hold_min=3;brain_hold_max=10;brain_density=5;lfo1_rate=0.019523;"
      "lfo1_depth=0.49;lfo2_rate=0.012066;lfo2_depth=0.47;lfo3_rate=0.0074572;lfo3_depth=0.47;"
      "lfo4_rate=0.0046088;lfo4_depth=0.60;odd_even=-0.16;src2_type=Additive;src2_level=0.17;src2_ratio=4/3;"
      "src2_octave=0;src2_partials=8;src2_bright=0.42;src2_drift=5.2;far_width=0.710",
      nullptr, nullptr, nullptr,
      "lfo1>spread:-0.184;lfo2>shimmer:0.228;lfo3>air:-0.117;lfo4>arc:0.206;beat>cutoff:0.123;pressure>brightness:0.186:u" },
    { "Rotating Sky",
      "pan_drift=1;itd=1;drift_rate=0.3;ens_mix=0.6;ens_rate=0.5;far_asym=1;width=1.8;brain_density=5;"
      "lfo1_rate=0.019128;lfo1_depth=0.46;lfo2_rate=0.011822;lfo2_depth=0.60;lfo3_rate=0.0073064;"
      "lfo3_depth=0.49;lfo4_rate=0.0045156;lfo4_depth=0.57;odd_even=-0.25;src2_type=Additive;src2_level=0.18;"
      "src2_ratio=3/2;src2_octave=1;src2_partials=8;src2_bright=0.55;src2_drift=2.2;far_width=0.566",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.147;lfo2>ens_depth:-0.164;lfo3>z_y:0.148;lfo4>arc:-0.120;beat>cutoff:0.113;pressure>resonance:0.223:u" },
    { "Slow Chorus Field",
      "ens_mix=0.8;ens_depth=0.8;ens_rate=0.05;strands=6;detune=12;far_decay=20;brain_density=5;"
      "lfo1_rate=0.025285;lfo1_depth=0.63;lfo2_rate=0.015627;lfo2_depth=0.62;lfo3_rate=0.0096579;"
      "lfo3_depth=0.65;lfo4_rate=0.0059689;lfo4_depth=0.65;odd_even=-0.28;src2_type=Additive;src2_level=0.11;"
      "src2_ratio=5/4;src2_octave=0;src2_partials=13;src2_bright=0.57;src2_drift=3.1;far_width=0.481",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.222;lfo2>shimmer:0.178;lfo3>ens_depth:-0.131;lfo4>arc:0.198;beat>far_size:0.160;pressure>resonance:0.110:u;slide>tilt:0.289:u" },
    { "Tape Echo Drone",
      "dly_time_l=0.42;dly_time_r=0.42;dly_feedback=0.85;dly_damp=0.9;dly_mix=0.35;dly_to_far=0.2;far_decay=8;"
      "cutoff=1800;brain_density=4;lfo1_rate=0.02237;lfo1_depth=0.57;lfo2_rate=0.013825;lfo2_depth=0.45;"
      "lfo3_rate=0.0085446;lfo3_depth=0.53;lfo4_rate=0.0052808;lfo4_depth=0.54;odd_even=-0.20;"
      "src2_type=Additive;src2_level=0.12;src2_ratio=2/1;src2_octave=1;src2_partials=10;src2_bright=0.40;"
      "src2_drift=4.1;far_width=0.748;haas=0.265;haas_time=16.9",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.180;lfo2>air:-0.114;lfo3>brightness:0.171;lfo4>cutoff:0.227;beat>cosmos_smear:0.082;wheel>dly_mix:0.300:u" },
    { "Shimmer Cathedral",
      "cosmos_shimmer=0.8;cosmos_shimmer_pitch=+12;far_size=3;far_decay=45;far_highcut=7000;partials=20;"
      "brightness=0.9;brain_density=5;lfo1_rate=0.021121;lfo1_depth=0.52;lfo2_rate=0.013054;lfo2_depth=0.51;"
      "lfo3_rate=0.0080677;lfo3_depth=0.63;lfo4_rate=0.0049861;lfo4_depth=0.59;odd_even=-0.22;"
      "src2_type=Additive;src2_level=0.17;src2_ratio=5/4;src2_octave=0;src2_partials=13;src2_bright=0.46;"
      "src2_drift=5.2;far_width=0.784",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.113;lfo2>z_y:-0.223;lfo3>detune:0.186;lfo4>ens_depth:-0.219;beat>far_size:0.122;slide>odd_even:0.315:u;pressure>resonance:0.244:u" },
    { "Nineteenth Above",
      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=+19;far_decay=35;far_highcut=5000;scale=JI Major (Ptolemy);"
      "root=A;brain_density=4;lfo1_rate=0.019473;lfo1_depth=0.59;lfo2_rate=0.012035;lfo2_depth=0.61;"
      "lfo3_rate=0.007438;lfo3_depth=0.46;lfo4_rate=0.0045969;lfo4_depth=0.53;odd_even=-0.19;"
      "src2_type=Additive;src2_level=0.10;src2_ratio=5/4;src2_octave=1;src2_partials=7;src2_bright=0.56;"
      "src2_drift=2.2",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:-0.162;lfo2>tilt:-0.104;lfo3>air:0.170;lfo4>z_y:0.174;beat>cutoff:0.081;slide>odd_even:0.340:u;pressure>resonance:0.104:u" },



    // ---------------------------------------------------------------- 70..99 cosmos / science fiction
    { "Nebula Drift",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.8;cosmos_return=0.7;cosmos_to_far=0.5;far_decay=30;"
      "brain_density=5;cloud_send=0.6;cloud_pitch=0.4;lfo1_rate=0.018179;lfo1_depth=0.43;lfo2_rate=0.011235;"
      "lfo2_depth=0.47;lfo3_rate=0.0069437;lfo3_depth=0.53;lfo4_rate=0.0042914;lfo4_depth=0.54;odd_even=-0.14;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=2/1;src2_octave=1;src2_partials=10;src2_bright=0.42;"
      "src2_drift=5.2;far_width=0.485",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:-0.162;lfo2>z_y:0.165;lfo3>spread:0.235;lfo4>brightness:-0.126;beat>far_size:0.158;pressure>shimmer:0.331:u;slide>odd_even:0.365:u" },
    { "Frozen Nebula",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.98;cosmos_return=0.8;cosmos_to_far=0.6;far_decay=20;"
      "brain_density=4;lfo1_rate=0.022459;lfo1_depth=0.50;lfo2_rate=0.01388;lfo2_depth=0.57;"
      "lfo3_rate=0.0085784;lfo3_depth=0.45;lfo4_rate=0.0053017;lfo4_depth=0.63;odd_even=-0.21;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=5/3;src2_octave=0;src2_partials=7;src2_bright=0.52;"
      "src2_drift=2.6;far_width=0.721",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.168;lfo2>z_y:-0.104;lfo3>tilt:-0.170;lfo4>pan_drift:-0.104;beat>depth:0.081;pressure>far_level:0.197:u;wheel>air:0.194:u" },
    { "Alien Choir",
      "cosmos_send=0.8;cosmos_vowel=1;cosmos_vowel_rate=0.02;cosmos_shift=3;cosmos_return=0.8;"
      "cosmos_to_far=0.6;partials=16;brain_density=6;far_decay=25;lfo1_rate=0.01934;lfo1_depth=0.56;"
      "lfo2_rate=0.011953;lfo2_depth=0.51;lfo3_rate=0.0073873;lfo3_depth=0.48;lfo4_rate=0.0045656;"
      "lfo4_depth=0.60;odd_even=-0.21;src2_type=Additive;src2_level=0.11;src2_ratio=2/1;src2_octave=0;"
      "src2_partials=8;src2_bright=0.46;src2_drift=2.2",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:0.236;lfo2>spread:-0.108;lfo3>cutoff:-0.232;lfo4>resonance:-0.134;beat>air:0.149;slide>odd_even:0.349:u;wheel>filter_fold:0.592:u" },
    { "Forbidden Planet",
      "cosmos_send=1;cosmos_shift=120;cosmos_shift_drift=1;cosmos_res=0.6;cosmos_res_pitch=1.5;"
      "cosmos_res_fb=0.9;cosmos_return=0.7;cosmos_to_far=0.5;scale=12-TET;brain_consonance=0.2;brain_density=5;"
      "lfo1_rate=0.014016;lfo1_depth=0.52;lfo2_rate=0.0086621;lfo2_depth=0.45;lfo3_rate=0.0053535;"
      "lfo3_depth=0.59;lfo4_rate=0.0033086;lfo4_depth=0.61;odd_even=-0.30;src2_type=Additive;src2_level=0.15;"
      "src2_ratio=5/4;src2_octave=0;src2_partials=12;src2_bright=0.40;src2_drift=5.4;far_width=0.651",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:-0.185;lfo2>ens_depth:-0.210;lfo3>air:-0.221;lfo4>detune:-0.142;beat>brightness:0.120;pressure>shimmer:0.281:u;slide>odd_even:0.314:u" },
    { "Hull Resonance",
      "cosmos_send=1;cosmos_res=0.9;cosmos_res_pitch=0.5;cosmos_res_fb=0.96;cosmos_return=0.8;brain_low=30;"
      "brain_high=60;partials=6;far_decay=20;far_highcut=1500;lfo1_rate=0.016195;lfo1_depth=0.41;"
      "lfo2_rate=0.010009;lfo2_depth=0.56;lfo3_rate=0.0061859;lfo3_depth=0.51;lfo4_rate=0.0038231;"
      "lfo4_depth=0.54;odd_even=-0.20;src2_type=Additive;src2_level=0.14;src2_ratio=3/2;src2_octave=1;"
      "src2_partials=9;src2_bright=0.51;src2_drift=3.8;far_width=0.768",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:0.232;lfo2>air:-0.205;lfo3>z_y:-0.159;lfo4>brightness:-0.174;beat>far_size:0.142;pressure>far_level:0.205:u;wheel>filter_fold:0.406:u" },
    { "Signal From Deep Space",
      "cosmos_send=0.8;cosmos_shift=220;cosmos_shift_drift=0.6;cosmos_nebula=0.5;cosmos_smear=0.6;"
      "cosmos_return=0.6;cosmos_to_far=0.8;air=0.3;air_color=9;air_q=20;brain_density=3;brain_rate=40;"
      "lfo1_rate=0.02911;lfo1_depth=0.54;lfo2_rate=0.017991;lfo2_depth=0.45;lfo3_rate=0.011119;lfo3_depth=0.53;"
      "lfo4_rate=0.006872;lfo4_depth=0.48;z_mode=Series;z_shape=Rounded Vowels;z_x=0.47;z_y=0.56;z_z=0.18;"
      "z_res=0.45;z_mix=0.33;z_rate=0.0170;z_depth=0.43;odd_even=-0.23;src2_type=Additive;src2_level=0.16;"
      "src2_ratio=3/2;src2_octave=1;src2_partials=10;src2_bright=0.40;src2_drift=4.0;far_width=0.779",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:-0.193;lfo2>tilt:0.150;lfo3>ens_depth:-0.208;lfo4>pan_drift:-0.168;beat>cosmos_smear:0.088;pressure>resonance:0.235:u" },
    { "Event Horizon",
      "cosmos_send=1;cosmos_shift=-40;cosmos_shift_drift=0.5;cosmos_nebula=0.8;cosmos_smear=0.9;"
      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=-12;cosmos_return=0.5;cosmos_to_far=1;far_decay=60;"
      "far_highcut=1500;depth=1;brain_density=4;lfo1_rate=0.026159;lfo1_depth=0.42;lfo2_rate=0.016167;"
      "lfo2_depth=0.50;lfo3_rate=0.0099917;lfo3_depth=0.47;lfo4_rate=0.0061752;lfo4_depth=0.44;odd_even=-0.24;"
      "src2_type=Additive;src2_level=0.14;src2_ratio=4/3;src2_octave=1;src2_partials=8;src2_bright=0.45;"
      "src2_drift=4.6;far_width=0.705",
      nullptr, nullptr, nullptr,
      "lfo1>air:0.138;lfo2>z_y:-0.199;lfo3>arc:-0.202;lfo4>pan_drift:-0.150;beat>brightness:0.163;slide>inharmonic:0.186:u" },
    { "Ion Wind",
      "air=0.6;air_color=14;air_q=3;cosmos_send=1;cosmos_shift=60;cosmos_shift_drift=1;cosmos_return=0.8;"
      "partials=4;far_decay=25;brain_density=4;master_gain=-8;lfo1_rate=0.020604;lfo1_depth=0.43;"
      "lfo2_rate=0.012734;lfo2_depth=0.44;lfo3_rate=0.00787;lfo3_depth=0.53;lfo4_rate=0.0048639;"
      "lfo4_depth=0.57;odd_even=-0.13;src2_type=Additive;src2_level=0.18;src2_ratio=5/3;src2_octave=1;"
      "src2_partials=10;src2_bright=0.39;src2_drift=2.2;far_width=0.595",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.235;lfo2>detune:-0.161;lfo3>cutoff:-0.139;lfo4>brightness:0.111;beat>depth:0.090;wheel>cosmos_send:0.291:u;pressure>shimmer:0.187:u" },
    { "Cryo Chamber",
      "cosmos_send=1;cosmos_nebula=0.9;cosmos_smear=0.95;cosmos_res=0.4;cosmos_res_pitch=4;cosmos_res_fb=0.85;"
      "cosmos_return=0.6;cosmos_to_far=0.6;partials=24;brightness=0.9;inharmonic=0.3;far_decay=40;"
      "far_highcut=6000;brain_density=4;lfo1_rate=0.021686;lfo1_depth=0.62;lfo2_rate=0.013403;lfo2_depth=0.58;"
      "lfo3_rate=0.0082834;lfo3_depth=0.64;lfo4_rate=0.0051194;lfo4_depth=0.56;odd_even=-0.22;"
      "src2_type=Additive;src2_level=0.14;src2_ratio=4/3;src2_octave=1;src2_partials=13;src2_bright=0.53;"
      "src2_drift=2.6;far_width=0.551",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.216;lfo2>arc:-0.204;lfo3>z_y:-0.185;lfo4>tilt:0.237;beat>purity:0.119;slide>inharmonic:0.220:u" },
    { "Pulsar Field",
      "cosmos_send=1;cosmos_shift=300;cosmos_shift_drift=0;cosmos_return=0.5;cosmos_to_far=0.5;attack=0.05;"
      "decay=4;sustain=0.2;release=8;brain_rate=3;brain_hold_min=3;brain_hold_max=8;brain_density=8;"
      "dly_mix=0.4;dly_feedback=0.6;scale=12-TET;brain_consonance=0;lfo1_rate=0.0246;lfo1_depth=0.64;"
      "lfo2_rate=0.015204;lfo2_depth=0.65;lfo3_rate=0.0093963;lfo3_depth=0.49;lfo4_rate=0.0058072;"
      "lfo4_depth=0.51;odd_even=-0.18;src2_type=Additive;src2_level=0.15;src2_ratio=5/3;src2_octave=1;"
      "src2_partials=8;src2_bright=0.60;src2_drift=4.6;far_width=0.737",
      nullptr, nullptr, nullptr,
      "lfo1>spread:0.160;lfo2>pan_drift:0.209;lfo3>cutoff:0.116;lfo4>ens_depth:-0.136;beat>sub_level:0.120;wheel>cosmos_send:0.264:u" },
    { "Ring World",
      "cosmos_send=1;cosmos_shift=55;cosmos_res=0.5;cosmos_res_pitch=3;cosmos_res_fb=0.9;cosmos_return=0.8;"
      "scale=Otonality 1-11;root=C;brain_density=5;lfo1_rate=0.02136;lfo1_depth=0.46;lfo2_rate=0.013201;"
      "lfo2_depth=0.56;lfo3_rate=0.0081587;lfo3_depth=0.42;lfo4_rate=0.0050424;lfo4_depth=0.59;odd_even=-0.17;"
      "src2_type=Additive;src2_level=0.17;src2_ratio=3/2;src2_octave=0;src2_partials=6;src2_bright=0.51;"
      "src2_drift=2.1;far_width=0.751",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.148;lfo2>ens_depth:-0.155;lfo3>air:0.130;lfo4>arc:-0.207;beat>z_y:0.156;pressure>cutoff:0.203:u" },
    { "Solar Wind",
      "cosmos_send=0.7;cosmos_nebula=1;cosmos_smear=0.7;cosmos_shimmer=0.6;cosmos_shimmer_pitch=+12;"
      "cosmos_return=0.5;cosmos_to_far=0.8;air=0.4;air_color=6;air_q=5;far_decay=40;brain_density=5;"
      "lfo1_rate=0.018858;lfo1_depth=0.44;lfo2_rate=0.011655;lfo2_depth=0.59;lfo3_rate=0.0072032;"
      "lfo3_depth=0.55;lfo4_rate=0.0044518;lfo4_depth=0.51;odd_even=-0.26;src2_type=Additive;src2_level=0.14;"
      "src2_ratio=4/3;src2_octave=1;src2_partials=10;src2_bright=0.54;src2_drift=2.7;far_width=0.815",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:-0.230;lfo2>ens_depth:0.134;lfo3>z_y:0.128;lfo4>pan_drift:-0.119;beat>air:0.153;slide>tilt:0.245:u" },
    { "Derelict Ship",
      "cosmos_send=1;cosmos_res=0.8;cosmos_res_pitch=0.75;cosmos_res_fb=0.95;cosmos_shift=-15;"
      "cosmos_shift_drift=1;cosmos_return=0.7;cosmos_to_far=0.6;brain_low=28;brain_high=60;inharmonic=0.5;"
      "partials=10;brain_density=4;far_decay=30;far_highcut=1200;lfo1_rate=0.014307;lfo1_depth=0.53;"
      "lfo2_rate=0.0088422;lfo2_depth=0.51;lfo3_rate=0.0054648;lfo3_depth=0.48;lfo4_rate=0.0033774;"
      "lfo4_depth=0.47;odd_even=-0.16;src2_type=Additive;src2_level=0.11;src2_ratio=5/3;src2_octave=1;"
      "src2_partials=8;src2_bright=0.46;src2_drift=2.9",
      nullptr, nullptr, nullptr,
      "lfo1>spread:-0.105;lfo2>ens_depth:0.179;lfo3>brightness:-0.157;lfo4>resonance:-0.174;beat>cutoff:0.138;slide>odd_even:0.360:u;pressure>far_level:0.164:u" },
    { "Telepathy",
      "cosmos_send=0.9;cosmos_vowel=0.9;cosmos_vowel_rate=0.2;cosmos_shift=8;cosmos_shift_drift=1;"
      "cosmos_nebula=0.4;cosmos_smear=0.5;cosmos_return=0.8;brain_density=5;partials=12;lfo1_rate=0.021043;"
      "lfo1_depth=0.60;lfo2_rate=0.013005;lfo2_depth=0.55;lfo3_rate=0.0080377;lfo3_depth=0.51;"
      "lfo4_rate=0.0049676;lfo4_depth=0.63;odd_even=-0.23;src2_type=Additive;src2_level=0.17;src2_ratio=4/3;"
      "src2_octave=0;src2_partials=9;src2_bright=0.50;src2_drift=4.2;far_width=0.631",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.116;lfo2>shimmer:0.154;lfo3>detune:0.138;lfo4>pan_drift:-0.146;beat>cosmos_smear:0.089;wheel>far_level:0.269:u;slide>odd_even:0.306:u" },
    { "Warp Core",
      "cosmos_send=1;cosmos_shift=-200;cosmos_shift_drift=0.3;cosmos_res=0.7;cosmos_res_pitch=0.25;"
      "cosmos_res_fb=0.96;cosmos_return=0.8;cosmos_to_far=0.4;brain_low=24;brain_high=48;brain_density=3;"
      "bass_mono=300;far_decay=20;lfo1_rate=0.020567;lfo1_depth=0.61;lfo2_rate=0.012711;lfo2_depth=0.64;"
      "lfo3_rate=0.0078557;lfo3_depth=0.62;lfo4_rate=0.0048551;lfo4_depth=0.58;odd_even=-0.15;"
      "src2_type=Additive;src2_level=0.16;src2_ratio=4/3;src2_octave=0;src2_partials=12;src2_bright=0.59;"
      "src2_drift=4.7;far_width=0.686",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.130;lfo2>z_y:-0.216;lfo3>arc:0.239;lfo4>detune:-0.170;beat>far_size:0.166;pressure>shimmer:0.355:u;wheel>filter_fold:0.504:u" },
    { "Comet Tail",
      "cosmos_send=1;cosmos_shimmer=0.9;cosmos_shimmer_pitch=+24;cosmos_nebula=0.6;cosmos_smear=0.8;"
      "cosmos_return=0.4;cosmos_to_far=1;far_decay=50;far_highcut=9000;brightness=0.9;brain_density=4;"
      "lfo1_rate=0.014734;lfo1_depth=0.55;lfo2_rate=0.0091062;lfo2_depth=0.45;lfo3_rate=0.0056279;"
      "lfo3_depth=0.62;lfo4_rate=0.0034782;lfo4_depth=0.62;odd_even=-0.19;src2_type=Additive;src2_level=0.15;"
      "src2_ratio=2/1;src2_octave=0;src2_partials=13;src2_bright=0.40;src2_drift=4.9;far_width=0.740",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.185;lfo2>tilt:-0.140;lfo3>brightness:-0.225;lfo4>spread:0.108;beat>resonance:0.131;slide>odd_even:0.176:u;pressure>shimmer:0.233:u" },
    { "Andromeda",
      "cosmos_send=0.8;cosmos_nebula=1;cosmos_smear=0.85;cosmos_shift=5;cosmos_shift_drift=1;cosmos_return=0.7;"
      "cosmos_to_far=0.7;partials=24;shimmer=0.8;far_decay=45;brain_density=6;arc=0.8;arc_period=20;"
      "lfo1_rate=0.015017;lfo1_depth=0.46;lfo2_rate=0.0092808;lfo2_depth=0.60;lfo3_rate=0.0057359;"
      "lfo3_depth=0.55;lfo4_rate=0.003545;lfo4_depth=0.51;odd_even=-0.28;src2_type=Additive;src2_level=0.18;"
      "src2_ratio=4/3;src2_octave=1;src2_partials=10;src2_bright=0.55;src2_drift=5.9;far_width=0.486",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.125;lfo2>tilt:-0.233;lfo3>arc:0.178;lfo4>ens_depth:0.149;beat>z_y:0.101;wheel>cosmos_send:0.353:u;pressure>shimmer:0.184:u" },
    { "Ganymede Ice",
      "cosmos_send=1;cosmos_res=0.6;cosmos_res_pitch=6;cosmos_res_fb=0.92;cosmos_shift=30;cosmos_return=0.6;"
      "cosmos_to_far=0.5;inharmonic=0.4;partials=24;brightness=1;far_decay=35;far_highcut=8000;brain_density=4;"
      "brain_low=55;brain_high=96;lfo1_rate=0.024441;lfo1_depth=0.47;lfo2_rate=0.015105;lfo2_depth=0.63;"
      "lfo3_rate=0.0093356;lfo3_depth=0.49;lfo4_rate=0.0057697;lfo4_depth=0.44;odd_even=-0.18;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=4/3;src2_octave=1;src2_partials=8;src2_bright=0.58;"
      "src2_drift=2.6;far_width=0.766",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:0.173;lfo2>detune:0.140;lfo3>resonance:0.120;lfo4>shimmer:-0.101;beat>sub_level:0.131;wheel>air:0.290:u" },
    { "Lost Transmission",
      "cosmos_send=1;cosmos_shift=150;cosmos_shift_drift=1;cosmos_vowel=0.5;cosmos_vowel_rate=0.4;"
      "cosmos_nebula=0.3;cosmos_return=0.9;cosmos_to_far=0.2;dly_mix=0.4;dly_feedback=0.7;dly_damp=0.8;air=0.4;"
      "air_color=12;air_q=8;brain_density=3;brain_rate=30;lfo1_rate=0.029222;lfo1_depth=0.64;lfo2_rate=0.01806;"
      "lfo2_depth=0.59;lfo3_rate=0.011162;lfo3_depth=0.45;lfo4_rate=0.0068985;lfo4_depth=0.56;odd_even=-0.17;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=2/1;src2_octave=1;src2_partials=7;src2_bright=0.54;"
      "src2_drift=5.0;far_width=0.670",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.111;lfo2>cutoff:0.162;lfo3>spread:-0.148;lfo4>z_y:-0.103;beat>sub_level:0.112;slide>cosmos_vowel:0.304:u;wheel>cosmos_send:0.224:u" },
    { "Stellar Nursery",
      "cosmos_send=0.6;cosmos_nebula=1;cosmos_smear=0.9;cosmos_shimmer=0.4;cosmos_shimmer_pitch=+7;"
      "cosmos_return=0.6;cosmos_to_far=0.8;scale=JI Major (Ptolemy);root=E;far_decay=45;brain_density=6;"
      "depth=0.9;cloud_send=0.8;cloud_pitch=0.6;cloud_density=8;lfo1_rate=0.0158;lfo1_depth=0.52;"
      "lfo2_rate=0.0097652;lfo2_depth=0.48;lfo3_rate=0.0060352;lfo3_depth=0.53;lfo4_rate=0.00373;"
      "lfo4_depth=0.42;odd_even=-0.17;src2_type=Additive;src2_level=0.15;src2_ratio=5/3;src2_octave=1;"
      "src2_partials=10;src2_bright=0.43;src2_drift=5.3",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.136;lfo2>ens_depth:-0.137;lfo3>detune:0.225;lfo4>resonance:-0.178;beat>far_size:0.131;pressure>brightness:0.253:u;slide>inharmonic:0.237:u" },
    { "Xenomorph Hive",
      "cosmos_send=1;cosmos_vowel=0.8;cosmos_vowel_rate=0.5;cosmos_shift=-80;cosmos_shift_drift=1;"
      "cosmos_res=0.5;cosmos_res_pitch=0.5;cosmos_res_fb=0.9;cosmos_return=0.8;cosmos_to_far=0.5;scale=12-TET;"
      "brain_consonance=0.1;brain_density=7;brain_low=30;brain_high=70;cutoff=1500;lfo1_rate=0.01944;"
      "lfo1_depth=0.48;lfo2_rate=0.012014;lfo2_depth=0.63;lfo3_rate=0.0074253;lfo3_depth=0.55;"
      "lfo4_rate=0.0045891;lfo4_depth=0.51;odd_even=-0.20;src2_type=Additive;src2_level=0.11;src2_ratio=5/4;"
      "src2_octave=1;src2_partials=10;src2_bright=0.58;src2_drift=3.7;far_width=0.607",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.178;lfo2>spread:0.105;lfo3>tilt:0.135;lfo4>cutoff:-0.190;beat>air:0.081;pressure>far_level:0.202:u;slide>cosmos_vowel:0.262:u" },
    { "Zero Gravity",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.75;cosmos_return=1;cosmos_to_far=0.3;near_mix=0;"
      "far_level=0.4;pan_drift=1;itd=1;brain_density=5;lfo1_rate=0.013862;lfo1_depth=0.48;lfo2_rate=0.0085669;"
      "lfo2_depth=0.41;lfo3_rate=0.0052946;lfo3_depth=0.49;lfo4_rate=0.0032723;lfo4_depth=0.47;odd_even=-0.20;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=3/2;src2_octave=1;src2_partials=9;src2_bright=0.36;"
      "src2_drift=3.9;far_width=0.589",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.193;lfo2>z_y:-0.211;lfo3>spread:0.186;lfo4>shimmer:0.123;beat>sub_level:0.120;pressure>far_level:0.234:u" },
    { "Monolith",
      "cosmos_send=1;cosmos_res=1;cosmos_res_pitch=1;cosmos_res_fb=0.97;cosmos_return=0.6;cosmos_to_far=0.6;"
      "brain_low=36;brain_high=60;brain_density=3;partials=32;tilt=0.8;far_decay=60;far_highcut=2500;attack=30;"
      "release=60;lfo1_rate=0.013387;lfo1_depth=0.65;lfo2_rate=0.0082739;lfo2_depth=0.43;lfo3_rate=0.0051136;"
      "lfo3_depth=0.47;lfo4_rate=0.0031604;lfo4_depth=0.60;odd_even=-0.27;src2_type=Additive;src2_level=0.12;"
      "src2_ratio=2/1;src2_octave=0;src2_partials=8;src2_bright=0.38;src2_drift=6.0",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.149;lfo2>air:0.112;lfo3>shimmer:0.118;lfo4>resonance:0.101;beat>cosmos_smear:0.127;wheel>filter_fold:0.490:u;pressure>cutoff:0.440:u" },
    { "Quasar",
      "cosmos_send=1;cosmos_shift=280;cosmos_shift_drift=1;cosmos_shimmer=0.7;cosmos_shimmer_pitch=+19;"
      "cosmos_return=0.5;cosmos_to_far=0.8;brightness=1;partials=32;far_decay=30;far_highcut=10000;"
      "brain_density=5;lfo1_rate=0.012982;lfo1_depth=0.63;lfo2_rate=0.0080232;lfo2_depth=0.49;"
      "lfo3_rate=0.0049586;lfo3_depth=0.47;lfo4_rate=0.0030646;lfo4_depth=0.41;odd_even=-0.26;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=5/4;src2_octave=1;src2_partials=8;src2_bright=0.44;"
      "src2_drift=3.5;far_width=0.661",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:0.136;lfo2>spread:-0.111;lfo3>air:-0.136;lfo4>resonance:-0.111;beat>cosmos_smear:0.082;pressure>cutoff:0.346:u;slide>tilt:0.206:u" },
    { "Dark Matter",
      "cosmos_send=1;cosmos_shift=-300;cosmos_nebula=0.9;cosmos_smear=0.9;cosmos_return=0.7;cosmos_to_far=0.7;"
      "brain_low=24;brain_high=52;cutoff=500;far_decay=60;far_highcut=1000;brain_density=3;lfo1_rate=0.017123;"
      "lfo1_depth=0.64;lfo2_rate=0.010583;lfo2_depth=0.59;lfo3_rate=0.0065405;lfo3_depth=0.45;"
      "lfo4_rate=0.0040422;lfo4_depth=0.59;odd_even=-0.26;src2_type=Additive;src2_level=0.15;src2_ratio=5/3;"
      "src2_octave=0;src2_partials=7;src2_bright=0.54;src2_drift=5.3;far_width=0.793",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:0.166;lfo2>cutoff:0.139;lfo3>arc:-0.137;lfo4>shimmer:0.172;beat>far_size:0.086;slide>odd_even:0.220:u" },
    { "Orbital Decay",
      "cosmos_send=0.8;cosmos_shift=40;cosmos_shift_drift=1;cosmos_res=0.4;cosmos_res_pitch=2;"
      "cosmos_res_fb=0.8;cosmos_return=0.7;dly_time_l=3;dly_time_r=4;dly_feedback=0.8;dly_mix=0.3;"
      "dly_to_far=0.8;brain_density=4;far_decay=30;lfo1_rate=0.025979;lfo1_depth=0.62;lfo2_rate=0.016056;"
      "lfo2_depth=0.52;lfo3_rate=0.0099231;lfo3_depth=0.63;lfo4_rate=0.0061328;lfo4_depth=0.62;odd_even=-0.15;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=5/4;src2_octave=0;src2_partials=13;src2_bright=0.47;"
      "src2_drift=2.5;haas=0.300;haas_time=19.4",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:-0.185;lfo2>arc:-0.140;lfo3>detune:-0.181;lfo4>shimmer:-0.131;beat>brightness:0.109;slide>tilt:0.322:u" },
    { "First Contact",
      "cosmos_send=0.7;cosmos_vowel=0.6;cosmos_vowel_rate=0.01;cosmos_shimmer=0.5;cosmos_shimmer_pitch=+12;"
      "cosmos_return=0.7;cosmos_to_far=0.6;scale=JI Major (Ptolemy);root=C;air=0.2;brain_density=5;"
      "far_decay=35;lfo1_rate=0.014583;lfo1_depth=0.54;lfo2_rate=0.0090126;lfo2_depth=0.42;lfo3_rate=0.0055701;"
      "lfo3_depth=0.59;lfo4_rate=0.0034425;lfo4_depth=0.49;odd_even=-0.27;src2_type=Additive;src2_level=0.14;"
      "src2_ratio=2/1;src2_octave=1;src2_partials=12;src2_bright=0.37;src2_drift=3.4",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:0.194;lfo2>z_y:-0.132;lfo3>detune:0.216;lfo4>spread:0.116;beat>cosmos_smear:0.108;wheel>air:0.373:u;pressure>brightness:0.240:u" },
    { "Plasma Sea",
      "cosmos_send=1;cosmos_nebula=0.7;cosmos_smear=0.6;cosmos_shift=12;cosmos_shift_drift=1;cosmos_res=0.3;"
      "cosmos_res_pitch=5;cosmos_res_fb=0.8;cosmos_return=0.8;cosmos_to_far=0.6;air=0.3;air_color=7;air_q=4;"
      "brain_density=6;lfo1_rate=0.018969;lfo1_depth=0.47;lfo2_rate=0.011723;lfo2_depth=0.41;"
      "lfo3_rate=0.0072454;lfo3_depth=0.49;lfo4_rate=0.0044779;lfo4_depth=0.44;odd_even=-0.23;"
      "src2_type=Additive;src2_level=0.19;src2_ratio=4/3;src2_octave=1;src2_partials=9;src2_bright=0.36;"
      "src2_drift=3.2",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:0.191;lfo2>spread:0.237;lfo3>air:-0.231;lfo4>shimmer:0.240;beat>cutoff:0.147;slide>tilt:0.269:u" },
    { "Void Whisper",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.95;cosmos_return=0.5;cosmos_to_far=0.9;air=0.5;"
      "air_color=10;air_q=6;partials=3;master_gain=-8;far_decay=50;far_highcut=2500;brain_density=3;"
      "lfo1_rate=0.020907;lfo1_depth=0.51;lfo2_rate=0.012922;lfo2_depth=0.57;lfo3_rate=0.0079859;"
      "lfo3_depth=0.45;lfo4_rate=0.0049356;lfo4_depth=0.53;odd_even=-0.16;src2_type=Additive;src2_level=0.18;"
      "src2_ratio=4/3;src2_octave=1;src2_partials=7;src2_bright=0.52;src2_drift=4.2;far_width=0.516",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:-0.162;lfo2>z_y:0.165;lfo3>arc:-0.209;lfo4>spread:-0.221;beat>depth:0.090;slide>tilt:0.223:u;wheel>filter_fold:0.228:u" },
    { "Alien Cathedral",
      "cosmos_send=0.8;cosmos_vowel=0.5;cosmos_vowel_rate=0.03;cosmos_shimmer=0.6;cosmos_shimmer_pitch=+12;"
      "cosmos_nebula=0.4;cosmos_smear=0.7;cosmos_return=0.6;cosmos_to_far=0.8;far_size=3;far_decay=60;"
      "far_highcut=6000;partials=24;brain_density=6;scale=Bohlen-Pierce (JI);keymap=Consecutive degrees;root=C;"
      "lfo1_rate=0.014471;lfo1_depth=0.50;lfo2_rate=0.0089437;lfo2_depth=0.57;lfo3_rate=0.0055275;"
      "lfo3_depth=0.64;lfo4_rate=0.0034162;lfo4_depth=0.49;odd_even=-0.24;src2_type=Additive;src2_level=0.11;"
      "src2_ratio=2/1;src2_octave=1;src2_partials=13;src2_bright=0.52;src2_drift=2.4",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.194;lfo2>tilt:0.106;lfo3>resonance:-0.170;lfo4>cutoff:0.113;beat>air:0.081;slide>inharmonic:0.396:u;wheel>far_level:0.399:u" },



    // ---------------------------------------------------------------- 100..109 playable keys (brain off)
    { "Warm Keys",
      "brain_on=off;keys_depth=0;attack=0.8;decay=4;sustain=0.7;release=5;near_mix=0.3;near_decay=2;"
      "far_level=0.4;ens_mix=0.4;partials=12;tilt=1.4;lfo1_rate=0.020585;lfo1_depth=0.51;lfo2_rate=0.012722;"
      "lfo2_depth=0.51;lfo3_rate=0.0078628;lfo3_depth=0.63;lfo4_rate=0.0048595;lfo4_depth=0.65;"
      "src2_type=Additive;src2_level=0.12;src2_ratio=5/3;src2_octave=0;src2_partials=13;src2_bright=0.46;"
      "src2_drift=4.7;ens_mode=Microshift;ens_depth=0.359;ens_rate=0.0706",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.187;lfo2>arc:-0.184;lfo3>cutoff:0.123;lfo4>spread:-0.189;beat>depth:0.139;pressure>resonance:0.166:u" },
    { "Glass Keys",
      "brain_on=off;attack=0.02;decay=8;sustain=0.2;release=8;inharmonic=0.3;partials=24;brightness=0.9;"
      "far_level=0.6;far_decay=20;keys_depth=0.2;cutoff=6000;lfo1_rate=0.020327;lfo1_depth=0.54;"
      "lfo2_rate=0.012563;lfo2_depth=0.64;lfo3_rate=0.0077643;lfo3_depth=0.55;lfo4_rate=0.0047986;"
      "lfo4_depth=0.64;src2_type=Additive;src2_level=0.13;src2_ratio=2/1;src2_octave=0;src2_partials=10;"
      "src2_bright=0.59;src2_drift=3.7",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.204;lfo2>shimmer:0.194;lfo3>pan_drift:-0.202;lfo4>ens_depth:0.159;beat>brightness:0.163;pressure>far_level:0.162:u;wheel>filter_fold:0.409:u" },
    { "Pad Keys",
      "brain_on=off;attack=3;release=10;strands=6;detune=15;ens_mix=0.6;far_level=0.7;far_decay=25;"
      "keys_depth=0.3;lfo1_rate=0.014957;lfo1_depth=0.63;lfo2_rate=0.0092441;lfo2_depth=0.52;"
      "lfo3_rate=0.0057132;lfo3_depth=0.60;lfo4_rate=0.0035309;lfo4_depth=0.61;src2_type=Additive;"
      "src2_level=0.13;src2_ratio=2/1;src2_octave=0;src2_partials=12;src2_bright=0.47;src2_drift=2.9;"
      "far_width=0.820",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:0.202;lfo2>spread:0.194;lfo3>resonance:0.220;lfo4>arc:0.230;beat>depth:0.118;slide>odd_even:0.336:u;wheel>air:0.249:u" },
    { "Flute Keys",
      "brain_on=off;air=0.6;air_color=2;air_q=12;partials=5;attack=0.3;release=3;near_mix=0.3;far_level=0.5;"
      "master_gain=-8;lfo1_rate=0.013155;lfo1_depth=0.52;lfo2_rate=0.0081301;lfo2_depth=0.63;"
      "lfo3_rate=0.0050247;lfo3_depth=0.49;lfo4_rate=0.0031054;lfo4_depth=0.47;z_mode=Series;z_shape=Glass;"
      "z_x=0.52;z_y=0.34;z_z=0.20;z_res=0.51;z_mix=0.34;z_rate=0.0359;z_depth=0.23;src2_type=Additive;"
      "src2_level=0.17;src2_ratio=5/3;src2_octave=1;src2_partials=8;src2_bright=0.58;src2_drift=5.5;"
      "far_width=0.497",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.193;lfo2>detune:-0.228;lfo3>shimmer:-0.204;lfo4>pan_drift:0.150;beat>resonance:0.168;wheel>air:0.258:u" },
    { "Organ Keys",
      "brain_on=off;odd_even=-0.4;partials=20;attack=0.05;release=0.5;sustain=1;near_mix=0.35;near_decay=3;"
      "far_level=0.5;cutoff=6000;lfo1_rate=0.019032;lfo1_depth=0.44;lfo2_rate=0.011763;lfo2_depth=0.56;"
      "lfo3_rate=0.0072697;lfo3_depth=0.48;lfo4_rate=0.0044929;lfo4_depth=0.57;src2_type=Additive;"
      "src2_level=0.15;src2_ratio=5/3;src2_octave=1;src2_partials=8;src2_bright=0.51;src2_drift=3.2",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:0.112;lfo2>pan_drift:0.206;lfo3>spread:0.220;lfo4>z_y:0.230;beat>air:0.119;slide>odd_even:0.337:u" },
    { "Cosmos Keys",
      "brain_on=off;cosmos_send=1;cosmos_shift=30;cosmos_shift_drift=1;cosmos_nebula=0.5;cosmos_smear=0.7;"
      "cosmos_return=0.7;cosmos_to_far=0.6;attack=1;release=12;far_decay=30;lfo1_rate=0.020058;lfo1_depth=0.60;"
      "lfo2_rate=0.012396;lfo2_depth=0.58;lfo3_rate=0.0076614;lfo3_depth=0.64;lfo4_rate=0.004735;"
      "lfo4_depth=0.46;src2_type=Additive;src2_level=0.14;src2_ratio=5/3;src2_octave=1;src2_partials=13;"
      "src2_bright=0.53;src2_drift=3.7;far_width=0.622",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:-0.157;lfo2>ens_depth:0.200;lfo3>spread:-0.185;lfo4>air:0.105;beat>z_y:0.119;wheel>far_level:0.233:u;pressure>shimmer:0.360:u" },
    { "Shimmer Keys",
      "brain_on=off;cosmos_shimmer=0.7;cosmos_shimmer_pitch=+12;attack=0.5;release=8;far_level=0.9;"
      "far_decay=25;keys_depth=0.3;lfo1_rate=0.015702;lfo1_depth=0.61;lfo2_rate=0.0097044;lfo2_depth=0.43;"
      "lfo3_rate=0.0059977;lfo3_depth=0.50;lfo4_rate=0.0037068;lfo4_depth=0.54;src2_type=Additive;"
      "src2_level=0.18;src2_ratio=4/3;src2_octave=1;src2_partials=9;src2_bright=0.38;src2_drift=5.3",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.232;lfo2>shimmer:0.161;lfo3>cutoff:0.235;lfo4>tilt:-0.170;beat>resonance:0.157;pressure>brightness:0.330:u" },
    { "Sub Keys",
      "brain_on=off;tilt=2;cutoff=600;partials=8;attack=0.3;release=4;bass_mono=250;far_level=0.3;near_mix=0.2;"
      "lfo1_rate=0.02066;lfo1_depth=0.49;lfo2_rate=0.012769;lfo2_depth=0.54;lfo3_rate=0.0078915;"
      "lfo3_depth=0.60;lfo4_rate=0.0048772;lfo4_depth=0.49;src2_type=Additive;src2_level=0.10;src2_ratio=5/4;"
      "src2_octave=1;src2_partials=12;src2_bright=0.49;src2_drift=5.7",
      nullptr, nullptr, nullptr,
      "lfo1>spread:0.159;lfo2>ens_depth:0.174;lfo3>tilt:0.175;lfo4>resonance:-0.201;beat>sub_level:0.092;slide>odd_even:0.308:u;wheel>far_level:0.334:u" },
    { "Vowel Keys",
      "brain_on=off;cosmos_send=1;cosmos_vowel=1;cosmos_vowel_rate=0.1;cosmos_return=1;cosmos_to_far=0.3;"
      "partials=16;attack=0.5;release=5;master_gain=-9;lfo1_rate=0.014686;lfo1_depth=0.60;lfo2_rate=0.0090767;"
      "lfo2_depth=0.46;lfo3_rate=0.0056097;lfo3_depth=0.41;lfo4_rate=0.003467;lfo4_depth=0.62;z_mode=Series;"
      "z_shape=Choir;z_x=0.22;z_y=0.64;z_z=0.12;z_res=0.53;z_mix=0.36;z_rate=0.0139;z_depth=0.39;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=5/4;src2_octave=0;src2_partials=6;src2_bright=0.41;"
      "src2_drift=3.4;far_width=0.681",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.150;lfo2>ens_depth:-0.208;lfo3>resonance:-0.150;lfo4>air:-0.147;beat>brightness:0.120;pressure>far_level:0.193:u" },
    { "Resonant Keys",
      "brain_on=off;cosmos_send=1;cosmos_res=0.7;cosmos_res_pitch=2;cosmos_res_fb=0.93;cosmos_return=0.8;"
      "attack=0.2;release=6;near_mix=0.3;lfo1_rate=0.028101;lfo1_depth=0.62;lfo2_rate=0.017368;lfo2_depth=0.62;"
      "lfo3_rate=0.010734;lfo3_depth=0.52;lfo4_rate=0.0066338;lfo4_depth=0.48;src2_type=Additive;"
      "src2_level=0.16;src2_ratio=4/3;src2_octave=1;src2_partials=9;src2_bright=0.57;src2_drift=2.5;"
      "far_width=0.794;haas=0.225;haas_time=16.9",
      nullptr, nullptr, nullptr,
      "lfo1>air:0.193;lfo2>pan_drift:-0.220;lfo3>z_y:-0.177;lfo4>tilt:-0.175;beat>sub_level:0.099;slide>inharmonic:0.286:u" },



    // ---------------------------------------------------------------- 110..119 long-form night arcs
    { "All Night Arc",
      "arc=1;arc_period=240;brain_density=5;brain_rate=40;brain_hold_min=90;brain_hold_max=400;far_decay=40;"
      "depth=0.85;scale=JI 7-limit;root=D;sub_level=0.3;sub_binaural=3;bloom=0.6;bloom_time=120;"
      "lfo1_rate=0.017188;lfo1_depth=0.58;lfo2_rate=0.010623;lfo2_depth=0.45;lfo3_rate=0.0065652;"
      "lfo3_depth=0.44;lfo4_rate=0.0040575;lfo4_depth=0.62;z_mode=Series;z_shape=Church Bell;z_x=0.37;z_y=0.49;"
      "z_z=0.07;z_res=0.40;z_mix=0.28;z_rate=0.0122;z_depth=0.20;odd_even=-0.23;src2_type=Additive;"
      "src2_level=0.12;src2_ratio=5/4;src2_octave=0;src2_partials=7;src2_bright=0.40;src2_drift=5.3",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:0.238;lfo2>z_y:-0.187;lfo3>brightness:-0.105;lfo4>tilt:-0.170;beat>air:0.094;slide>inharmonic:0.174:u;wheel>far_level:0.318:u" },
    { "Tidal Hours",
      "arc=1;arc_period=120;brain_density=6;brain_hold_min=60;brain_hold_max=240;far_decay=35;"
      "scale=JI Major (Ptolemy);root=G;air=0.2;lfo1_rate=0.015228;lfo1_depth=0.46;lfo2_rate=0.0094115;"
      "lfo2_depth=0.63;lfo3_rate=0.0058167;lfo3_depth=0.43;lfo4_rate=0.0035949;lfo4_depth=0.62;odd_even=-0.28;"
      "src2_type=Additive;src2_level=0.17;src2_ratio=5/3;src2_octave=0;src2_partials=6;src2_bright=0.58;"
      "src2_drift=5.4;far_width=0.788",
      nullptr, nullptr, nullptr,
      "lfo1>spread:-0.168;lfo2>detune:0.157;lfo3>shimmer:0.112;lfo4>resonance:0.214;beat>air:0.112;pressure>brightness:0.251:u;slide>odd_even:0.260:u" },
    { "Slow Sunrise",
      "arc=0.8;arc_period=90;brightness=0.4;brain_density=4;far_decay=30;scale=JI Pentatonic;root=C;bloom=0.8;"
      "bloom_time=150;lfo1_rate=0.017085;lfo1_depth=0.45;lfo2_rate=0.010559;lfo2_depth=0.50;"
      "lfo3_rate=0.0065257;lfo3_depth=0.51;lfo4_rate=0.0040331;lfo4_depth=0.44;odd_even=-0.17;"
      "src2_type=Additive;src2_level=0.12;src2_ratio=4/3;src2_octave=1;src2_partials=9;src2_bright=0.45;"
      "src2_drift=4.8",
      nullptr, nullptr, nullptr,
      "lfo1>z_x:0.208;lfo2>depth:-0.115;lfo3>far_size:-0.223;lfo4>far_predelay:-0.160;beat>brightness:0.127;wheel>far_level:0.379:u;slide>odd_even:0.340:u" },
    { "Ninety Minute Cycle",
      "arc=1;arc_period=90;brain_density=5;brain_rate=30;brain_hold_min=60;brain_hold_max=200;cosmos_send=0.4;"
      "cosmos_nebula=0.6;cosmos_smear=0.8;cosmos_return=0.5;cosmos_to_far=0.5;lfo1_rate=0.017399;"
      "lfo1_depth=0.50;lfo2_rate=0.010753;lfo2_depth=0.54;lfo3_rate=0.0066457;lfo3_depth=0.42;"
      "lfo4_rate=0.0041073;lfo4_depth=0.59;odd_even=-0.16;src2_type=Additive;src2_level=0.18;src2_ratio=3/2;"
      "src2_octave=0;src2_partials=6;src2_bright=0.49;src2_drift=2.3;far_width=0.806",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.113;lfo2>resonance:0.162;lfo3>spread:0.191;lfo4>cutoff:-0.202;beat>air:0.135;slide>odd_even:0.222:u" },
    { "REM Drift",
      "arc=0.7;arc_period=45;brain_density=6;brain_consonance=0.5;brain_wander=0.6;far_decay=35;depth=0.9;"
      "lfo1_rate=0.025398;lfo1_depth=0.55;lfo2_rate=0.015697;lfo2_depth=0.45;lfo3_rate=0.0097011;"
      "lfo3_depth=0.53;lfo4_rate=0.0059956;lfo4_depth=0.60;odd_even=-0.15;src2_type=Additive;src2_level=0.10;"
      "src2_ratio=5/3;src2_octave=0;src2_partials=10;src2_bright=0.40;src2_drift=5.1;far_width=0.670",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.114;lfo2>air:0.171;lfo3>tilt:-0.104;lfo4>z_y:0.179;beat>brightness:0.091;pressure>resonance:0.113:u;wheel>far_level:0.353:u" },
    { "Dream Corridor",
      "arc=0.6;arc_period=60;dly_mix=0.25;dly_time_l=2;dly_time_r=3;dly_feedback=0.7;dly_to_far=0.8;"
      "far_decay=45;brain_density=5;dly2_mix=0.35;dly2_feedback=0.6;dly2_time_l=3.1;dly2_time_r=3.7;"
      "lfo1_rate=0.01367;lfo1_depth=0.54;lfo2_rate=0.0084483;lfo2_depth=0.51;lfo3_rate=0.0052213;"
      "lfo3_depth=0.60;lfo4_rate=0.0032269;lfo4_depth=0.55;odd_even=-0.16;src2_type=Additive;src2_level=0.13;"
      "src2_ratio=3/2;src2_octave=1;src2_partials=12;src2_bright=0.46;src2_drift=3.9;haas=0.218;haas_time=16.4",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.163;lfo2>resonance:0.192;lfo3>spread:-0.185;lfo4>pan_drift:-0.114;beat>purity:0.117;slide>odd_even:0.213:u" },
    { "Deep Night Harmonics",
      "arc=0.9;arc_period=150;scale=Harmonic 8-16;root=C;brain_low=40;brain_high=88;brain_density=6;"
      "far_decay=40;far_highcut=2500;lfo1_rate=0.013255;lfo1_depth=0.56;lfo2_rate=0.0081918;lfo2_depth=0.45;"
      "lfo3_rate=0.0050628;lfo3_depth=0.44;lfo4_rate=0.003129;lfo4_depth=0.62;odd_even=-0.22;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=4/3;src2_octave=0;src2_partials=7;src2_bright=0.40;"
      "src2_drift=6.0;far_width=0.783",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:0.203;lfo2>ens_depth:-0.212;lfo3>pan_drift:-0.212;lfo4>air:-0.124;beat>z_y:0.098;pressure>resonance:0.129:u;wheel>filter_fold:0.209:u" },
    { "Long Otonal Night",
      "arc=0.8;arc_period=180;scale=Otonality 1-11;root=E;brain_density=7;brain_consonance=0.6;"
      "brain_hold_min=90;brain_hold_max=300;far_decay=50;lfo1_rate=0.025006;lfo1_depth=0.59;lfo2_rate=0.015455;"
      "lfo2_depth=0.55;lfo3_rate=0.0095515;lfo3_depth=0.42;lfo4_rate=0.0059032;lfo4_depth=0.43;odd_even=-0.13;"
      "src2_type=Additive;src2_level=0.13;src2_ratio=2/1;src2_octave=1;src2_partials=6;src2_bright=0.50;"
      "src2_drift=2.6;far_width=0.721",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.190;lfo2>resonance:-0.193;lfo3>pan_drift:0.193;lfo4>cutoff:-0.150;beat>sub_level:0.140;wheel>air:0.362:u;slide>inharmonic:0.240:u" },
    { "Sleeping Nebula",
      "arc=0.9;arc_period=120;cosmos_send=0.6;cosmos_nebula=0.8;cosmos_smear=0.9;cosmos_return=0.5;"
      "cosmos_to_far=0.8;far_decay=50;far_highcut=2000;brain_density=4;depth=1;lfo1_rate=0.020255;"
      "lfo1_depth=0.63;lfo2_rate=0.012518;lfo2_depth=0.65;lfo3_rate=0.0077366;lfo3_depth=0.43;"
      "lfo4_rate=0.0047815;lfo4_depth=0.59;odd_even=-0.25;src2_type=Additive;src2_level=0.11;src2_ratio=2/1;"
      "src2_octave=0;src2_partials=6;src2_bright=0.60;src2_drift=3.7",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:0.131;lfo2>shimmer:-0.111;lfo3>detune:-0.153;lfo4>resonance:0.112;beat>purity:0.127;pressure>far_level:0.244:u" },
    { "Morning Fade",
      "arc=1;arc_period=60;brain_density=3;brain_rate=60;brain_hold_min=120;brain_hold_max=400;brightness=0.5;"
      "far_decay=30;attack=30;release=60;lfo1_rate=0.020201;lfo1_depth=0.44;lfo2_rate=0.012485;lfo2_depth=0.44;"
      "lfo3_rate=0.0077159;lfo3_depth=0.56;lfo4_rate=0.0047687;lfo4_depth=0.58;odd_even=-0.26;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=3/2;src2_octave=0;src2_partials=11;src2_bright=0.39;"
      "src2_drift=2.7",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.182;lfo2>resonance:-0.140;lfo3>tilt:-0.225;lfo4>cutoff:0.108;beat>brightness:0.131;pressure>far_level:0.216:u;wheel>filter_fold:0.517:u" },



    // ---------------------------------------------------------------- 120..127 storm / cluster / texture
    { "Cluster Storm",
      "brain_consonance=0;brain_density=10;brain_rate=6;brain_hold_min=10;brain_hold_max=40;scale=12-TET;"
      "far_decay=40;depth=0.9;strands=5;detune=25;fb_bus=0.2;fb_fm=0.2;lfo1_rate=0.019882;lfo1_depth=0.60;"
      "lfo2_rate=0.012288;lfo2_depth=0.64;lfo3_rate=0.0075943;lfo3_depth=0.46;lfo4_rate=0.0046935;"
      "lfo4_depth=0.63;odd_even=-0.30;src2_type=Additive;src2_level=0.16;src2_ratio=5/4;src2_octave=0;"
      "src2_partials=7;src2_bright=0.59;src2_drift=3.7",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:0.186;lfo2>tilt:0.219;lfo3>cutoff:-0.213;lfo4>spread:0.142;beat>resonance:0.099;slide>inharmonic:0.226:u" },
    { "Micro Cluster",
      "brain_consonance=0.05;brain_density=8;brain_low=60;brain_high=72;far_decay=30;partials=8;scale=12-TET;"
      "lfo1_rate=0.019242;lfo1_depth=0.49;lfo2_rate=0.011892;lfo2_depth=0.57;lfo3_rate=0.0073497;"
      "lfo3_depth=0.42;lfo4_rate=0.0045424;lfo4_depth=0.40;odd_even=-0.14;src2_type=Additive;src2_level=0.14;"
      "src2_ratio=5/4;src2_octave=1;src2_partials=6;src2_bright=0.52;src2_drift=5.2",
      nullptr, nullptr, nullptr,
      "lfo1>air:0.170;lfo2>shimmer:0.201;lfo3>cutoff:0.229;lfo4>spread:0.204;beat>resonance:0.141;wheel>far_level:0.186:u" },
    { "Rising Swarm",
      "cosmos_shimmer=0.8;cosmos_shimmer_pitch=+12;brain_consonance=0.2;brain_density=9;brain_rate=5;"
      "brain_hold_min=8;brain_hold_max=30;far_decay=25;cloud_send=0.7;cloud_pitch=0.8;cloud_density=20;"
      "lfo1_rate=0.014928;lfo1_depth=0.62;lfo2_rate=0.0092259;lfo2_depth=0.43;lfo3_rate=0.0057019;"
      "lfo3_depth=0.40;lfo4_rate=0.003524;lfo4_depth=0.53;odd_even=-0.23;src2_type=Additive;src2_level=0.10;"
      "src2_ratio=5/4;src2_octave=1;src2_partials=6;src2_bright=0.38;src2_drift=2.4",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:0.126;lfo2>z_y:0.172;lfo3>spread:0.157;lfo4>arc:0.200;beat>far_size:0.136;wheel>cloud_send:0.395:u;pressure>cutoff:0.339:u" },
    { "Thunder Head",
      "brain_low=24;brain_high=60;brain_density=8;brain_consonance=0.3;tilt=1.8;cutoff=900;resonance=0.4;"
      "far_decay=30;air=0.3;air_color=6;air_q=3;lfo1_rate=0.017479;lfo1_depth=0.62;lfo2_rate=0.010803;"
      "lfo2_depth=0.49;lfo3_rate=0.0066764;lfo3_depth=0.41;lfo4_rate=0.0041263;lfo4_depth=0.40;odd_even=-0.23;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=5/3;src2_octave=1;src2_partials=6;src2_bright=0.44;"
      "src2_drift=2.8;far_width=0.677",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:-0.223;lfo2>pan_drift:0.160;lfo3>detune:0.156;lfo4>z_y:0.104;beat>resonance:0.135;slide>tilt:0.310:u;pressure>brightness:0.268:u" },
    { "Granular Sky",
      "cosmos_send=1;cosmos_nebula=1;cosmos_smear=0.5;cosmos_return=1;cosmos_to_far=0.5;attack=0.1;decay=2;"
      "sustain=0.3;release=4;brain_rate=2;brain_hold_min=2;brain_hold_max=6;brain_density=10;"
      "scale=JI Pentatonic;root=D;cloud_send=1;cloud_density=30;cloud_size=120;cloud_pitch=0.5;"
      "lfo1_rate=0.013612;lfo1_depth=0.43;lfo2_rate=0.0084128;lfo2_depth=0.47;lfo3_rate=0.0051994;"
      "lfo3_depth=0.53;lfo4_rate=0.0032134;lfo4_depth=0.60;odd_even=-0.23;src2_type=Additive;src2_level=0.12;"
      "src2_ratio=5/4;src2_octave=0;src2_partials=10;src2_bright=0.42;src2_drift=6.0;far_width=0.661",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.237;lfo2>spread:-0.117;lfo3>arc:0.180;lfo4>resonance:0.201;beat>sub_level:0.105;wheel>far_level:0.380:u;slide>tilt:0.314:u" },
    { "Inharmonic Field",
      "inharmonic=1;partials=32;tilt=1;brain_density=6;brain_consonance=0.4;far_decay=35;far_highcut=5000;"
      "fb_fm=0.4;fb_tone=800;lfo1_rate=0.013193;lfo1_depth=0.65;lfo2_rate=0.0081537;lfo2_depth=0.62;"
      "lfo3_rate=0.0050393;lfo3_depth=0.55;lfo4_rate=0.0031144;lfo4_depth=0.54;odd_even=-0.15;"
      "src2_type=Additive;src2_level=0.17;src2_ratio=5/4;src2_octave=1;src2_partials=10;src2_bright=0.57;"
      "src2_drift=6.0;far_width=0.727",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.128;lfo2>resonance:0.154;lfo3>shimmer:0.200;lfo4>pan_drift:-0.176;beat>sub_level:0.156;slide>inharmonic:0.199:u" },
    { "Dissonant Cathedral",
      "brain_consonance=0.1;brain_density=7;far_size=3;far_decay=60;far_highcut=4000;partials=20;"
      "brightness=0.8;scale=12-TET;lfo1_rate=0.013012;lfo1_depth=0.42;lfo2_rate=0.0080416;lfo2_depth=0.50;"
      "lfo3_rate=0.00497;lfo3_depth=0.51;lfo4_rate=0.0030716;lfo4_depth=0.51;odd_even=-0.29;src2_type=Additive;"
      "src2_level=0.12;src2_ratio=5/4;src2_octave=1;src2_partials=9;src2_bright=0.45;src2_drift=5.0",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.107;lfo2>spread:0.118;lfo3>ens_depth:0.127;lfo4>resonance:-0.119;beat>sub_level:0.150;pressure>cutoff:0.242:u;slide>inharmonic:0.305:u" },
    { "White Storm",
      "air=1;air_color=8;air_q=2;partials=2;brain_density=8;brain_consonance=0;master_gain=-10;far_decay=30;"
      "dly_mix=0.3;dly_feedback=0.7;scale=12-TET;lfo1_rate=0.015845;lfo1_depth=0.50;lfo2_rate=0.0097925;"
      "lfo2_depth=0.44;lfo3_rate=0.0060521;lfo3_depth=0.56;lfo4_rate=0.0037404;lfo4_depth=0.45;odd_even=-0.15;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=5/3;src2_octave=1;src2_partials=11;src2_bright=0.39;"
      "src2_drift=2.3;far_width=0.793",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.174;lfo2>z_y:0.140;lfo3>ens_depth:-0.234;lfo4>shimmer:0.152;beat>sub_level:0.154;slide>odd_even:0.353:u;wheel>filter_fold:0.528:u" },



    // ---------------------------------------------------------------- 128..135 sources (wavetable, FM, feedback)
    { "Vocal Morph Choir",
      "src2_type=Wavetable;src2_table=Vocal;src2_pos=0.2;src2_pos_drift=1;src2_level=0.6;src2_ratio=1/1;"
      "src3_type=Wavetable;src3_table=Vocal;src3_pos=0.7;src3_pos_drift=1;src3_level=0.5;src3_ratio=3/2;"
      "src3_octave=-1;osc_level=0.5;partials=8;brain_density=4;far_decay=30;far_highcut=4000;"
      "scale=JI Major (Ptolemy);root=A;z_mode=Series;z_shape=Vowel Morph;z_rate=0.03;z_depth=1;z_mix=0.5;"
      "lfo1_rate=0.014024;lfo1_depth=0.58;lfo2_rate=0.0086674;lfo2_depth=0.45;lfo3_rate=0.0053568;"
      "lfo3_depth=0.59;lfo4_rate=0.0033107;lfo4_depth=0.58;odd_even=-0.23;far_width=0.780",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.165;lfo2>air:0.200;lfo3>detune:-0.229;lfo4>z_y:-0.231;beat>sub_level:0.141;slide>odd_even:0.225:u;pressure>cutoff:0.255:u" },
    { "Glass Table Drift",
      "src2_type=Wavetable;src2_table=Glass;src2_pos=0.3;src2_pos_drift=0.8;src2_level=0.7;src2_octave=1;"
      "osc_level=0.4;partials=6;tilt=1.5;far_size=3;far_decay=45;far_highcut=8000;brain_low=48;brain_high=88;"
      "attack=8;release=20;lfo1_rate=0.026189;lfo1_depth=0.62;lfo2_rate=0.016186;lfo2_depth=0.58;"
      "lfo3_rate=0.010003;lfo3_depth=0.64;lfo4_rate=0.0061823;lfo4_depth=0.56;odd_even=-0.28",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.216;lfo2>resonance:-0.116;lfo3>cutoff:-0.110;lfo4>ens_depth:-0.127;beat>sub_level:0.105;pressure>far_level:0.211:u" },
    { "Organ Mixture Cloud",
      "src2_type=Wavetable;src2_table=Organ;src2_pos=0.6;src2_pos_drift=0.4;src2_level=0.6;src3_type=Wavetable;"
      "src3_table=Organ;src3_pos=0.9;src3_level=0.4;src3_octave=-1;src3_ratio=3/2;osc_level=0.3;"
      "scale=Pythagorean;root=D;near_mix=0.4;near_decay=3;far_level=0.5;brain_density=4;cloud_send=0.3;"
      "lfo1_rate=0.016894;lfo1_depth=0.50;lfo2_rate=0.010441;lfo2_depth=0.47;lfo3_rate=0.0064529;"
      "lfo3_depth=0.47;lfo4_rate=0.0039881;lfo4_depth=0.53;odd_even=-0.12;far_width=0.800;haas=0.265;"
      "haas_time=16.7",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.127;lfo2>z_y:0.198;lfo3>arc:-0.150;lfo4>pan_drift:-0.199;beat>brightness:0.118;wheel>cloud_send:0.368:u" },
    { "Metal Field",
      "src2_type=Wavetable;src2_table=Metal;src2_pos=0.5;src2_pos_drift=1;src2_level=0.6;src2_pan=-0.5;"
      "src3_type=Wavetable;src3_table=Metal;src3_pos=0.1;src3_pos_drift=1;src3_level=0.5;src3_pan=0.5;"
      "src3_ratio=7/4;src3_octave=-1;osc_level=0.3;inharmonic=0.4;partials=12;cutoff=1800;far_decay=40;"
      "far_highcut=3000;brain_consonance=0.3;brain_density=5;fb_fm=0.3;z_mode=Series;z_shape=Metal Bars;"
      "z_rate=0.02;z_depth=0.8;z_res=0.7;z_mix=0.4;lfo1_rate=0.014325;lfo1_depth=0.62;lfo2_rate=0.0088534;"
      "lfo2_depth=0.43;lfo3_rate=0.0054717;lfo3_depth=0.47;lfo4_rate=0.0033817;lfo4_depth=0.60;odd_even=-0.13",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.131;lfo2>arc:0.189;lfo3>brightness:0.114;lfo4>detune:-0.223;beat>z_y:0.117;pressure>cutoff:0.432:u" },
    { "FM Bell Drone",
      "src2_type=FM;src2_fm_ratio=3.5;src2_fm_index=2.5;src2_pos_drift=0.8;src2_level=0.5;src2_octave=1;"
      "osc_level=0.5;partials=6;attack=2;decay=20;sustain=0.4;release=25;far_decay=35;far_highcut=6000;"
      "brain_rate=15;brain_hold_min=10;brain_hold_max=60;scale=JI Pentatonic;root=E;lfo1_rate=0.024078;"
      "lfo1_depth=0.47;lfo2_rate=0.014881;lfo2_depth=0.53;lfo3_rate=0.009197;lfo3_depth=0.60;"
      "lfo4_rate=0.0056841;lfo4_depth=0.52;odd_even=-0.17",
      nullptr, nullptr, nullptr,
      "lfo1>spread:0.143;lfo2>shimmer:-0.190;lfo3>ens_depth:-0.106;lfo4>tilt:-0.170;beat>resonance:0.095;pressure>far_level:0.140:u;slide>inharmonic:0.364:u" },
    { "Slow FM Tide",
      "src2_type=FM;src2_fm_ratio=1;src2_fm_index=1.5;src2_pos_drift=1;src2_level=0.6;src2_pan=-0.4;"
      "src3_type=FM;src3_fm_ratio=0.5;src3_fm_index=1;src3_pos_drift=1;src3_level=0.5;src3_pan=0.4;"
      "src3_octave=-1;osc_level=0.4;tilt=1.6;cutoff=1500;arc=0.6;arc_period=30;brain_density=4;far_decay=40;"
      "breath=0.4;lfo1_rate=0.015467;lfo1_depth=0.62;lfo2_rate=0.009559;lfo2_depth=0.55;lfo3_rate=0.0059078;"
      "lfo3_depth=0.64;lfo4_rate=0.0036512;lfo4_depth=0.55;odd_even=-0.18;far_width=0.681",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.233;lfo2>detune:0.196;lfo3>pan_drift:0.229;lfo4>ens_depth:-0.204;beat>far_size:0.140;slide>inharmonic:0.272:u;wheel>air:0.379:u" },
    { "Feedback Hiss",
      "fb_bus=0.6;fb_drive=1;fb_tone=3000;fb_fm=0.3;src2_type=Wavetable;src2_table=Classic;src2_pos=0.55;"
      "src2_level=0.4;osc_level=0.6;partials=10;cutoff=2000;far_decay=30;dly_feedback=0.7;dly_mix=0.3;"
      "brain_density=4;master_gain=-9;lfo1_rate=0.019064;lfo1_depth=0.62;lfo2_rate=0.011782;lfo2_depth=0.65;"
      "lfo3_rate=0.0072819;lfo3_depth=0.46;lfo4_rate=0.0045004;lfo4_depth=0.53;z_mode=Series;"
      "z_shape=Church Bell;z_x=0.33;z_y=0.54;z_z=0.26;z_res=0.45;z_mix=0.38;z_rate=0.0104;z_depth=0.20;"
      "odd_even=-0.30;haas=0.341;haas_time=20.0",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:-0.127;lfo2>air:0.172;lfo3>tilt:-0.113;lfo4>cutoff:0.197;beat>far_size:0.114;pressure>resonance:0.157:u" },
    { "Three Voices, One Key",
      "brain_on=off;keys_depth=0;stack=Major;strands=3;detune=0;drift=1;osc_level=0.7;src2_type=Wavetable;"
      "src2_table=Classic;src2_pos=0.25;src2_level=0.4;src2_octave=-1;src3_type=FM;src3_fm_ratio=2;"
      "src3_fm_index=0.8;src3_level=0.3;src3_octave=1;src3_ratio=3/2;near_mix=0.3;far_level=0.4;attack=2;"
      "release=8;presence=2;lfo1_rate=0.021811;lfo1_depth=0.46;lfo2_rate=0.01348;lfo2_depth=0.53;"
      "lfo3_rate=0.0083312;lfo3_depth=0.54;lfo4_rate=0.0051489;lfo4_depth=0.54;haas=0.268;haas_time=15.0",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.127;lfo2>shimmer:-0.102;lfo3>ens_depth:0.100;lfo4>cutoff:0.126;beat>resonance:0.080;wheel>far_level:0.319:u;slide>inharmonic:0.386:u" },



    // ---------------------------------------------------------------- 136..147 z-plane / morphing filter
    { "Morphing Vowels",
      "z_mode=Replace;z_shape=Vowel Morph;z_x=0.3;z_y=0.4;z_rate=0.02;z_depth=1;z_res=0.5;z_mix=1;"
      "z_keytrack=0.2;partials=24;tilt=0.9;brightness=0.9;brain_density=4;brain_rate=30;far_decay=30;attack=8;"
      "release=20;scale=JI Major (Ptolemy);root=A;lfo1_rate=0.02348;lfo1_depth=0.45;lfo2_rate=0.014512;"
      "lfo2_depth=0.44;lfo3_rate=0.0089687;lfo3_depth=0.47;lfo4_rate=0.005543;lfo4_depth=0.53;odd_even=-0.14;"
      "src2_type=Additive;src2_level=0.19;src2_ratio=5/4;src2_octave=1;src2_partials=8;src2_bright=0.39;"
      "src2_drift=4.6",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.179;lfo2>cutoff:0.166;lfo3>shimmer:0.200;lfo4>spread:-0.176;beat>depth:0.158;pressure>resonance:0.110:u;wheel>z_y:0.483:u" },
    { "Choir Behind Glass",
      "z_mode=Series;z_shape=Choir;z_x=0.5;z_y=0.5;z_rate=0.012;z_depth=1;z_res=0.6;z_mix=0.8;partials=20;"
      "air=0.2;brain_density=5;far_decay=40;far_highcut=4000;depth=0.75;attack=10;release=25;scale=JI Minor;"
      "root=D;lfo1_rate=0.025285;lfo1_depth=0.62;lfo2_rate=0.015627;lfo2_depth=0.43;lfo3_rate=0.0096579;"
      "lfo3_depth=0.53;lfo4_rate=0.0059689;lfo4_depth=0.54;odd_even=-0.26;src2_type=Additive;src2_level=0.15;"
      "src2_ratio=3/2;src2_octave=1;src2_partials=10;src2_bright=0.38;src2_drift=2.6;far_width=0.667",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.145;lfo2>brightness:-0.208;lfo3>arc:0.115;lfo4>air:0.215;beat>purity:0.120;pressure>far_level:0.199:u;slide>odd_even:0.269:u" },
    { "Nasal Drone",
      "z_mode=Replace;z_shape=Nasal;z_x=0.2;z_y=0.7;z_rate=0.008;z_depth=0.8;z_res=0.7;z_mix=1;partials=28;"
      "tilt=0.8;brain_density=3;brain_hold_min=60;brain_hold_max=200;far_decay=35;master_gain=-8;"
      "lfo1_rate=0.012952;lfo1_depth=0.60;lfo2_rate=0.0080049;lfo2_depth=0.64;lfo3_rate=0.0049473;"
      "lfo3_depth=0.52;lfo4_rate=0.0030576;lfo4_depth=0.42;odd_even=-0.16;src2_type=Additive;src2_level=0.15;"
      "src2_ratio=5/3;src2_octave=1;src2_partials=9;src2_bright=0.59;src2_drift=4.0;far_width=0.733",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:-0.189;lfo2>arc:-0.141;lfo3>brightness:0.129;lfo4>spread:-0.111;beat>z_y:0.154;wheel>far_level:0.342:u" },
    { "Filter Tide",
      "z_mode=Replace;z_shape=Low Sweep;z_x=0.3;z_y=0.6;z_rate=0.01;z_depth=1;z_res=0.8;z_mix=1;z_keytrack=0.5;"
      "partials=32;tilt=0.7;brightness=1;brain_density=5;far_decay=40;arc=0.6;arc_period=40;attack=12;"
      "release=30;lfo1_rate=0.026129;lfo1_depth=0.47;lfo2_rate=0.016148;lfo2_depth=0.63;lfo3_rate=0.0099802;"
      "lfo3_depth=0.49;lfo4_rate=0.0061681;lfo4_depth=0.47;odd_even=-0.14;src2_type=Additive;src2_level=0.12;"
      "src2_ratio=5/3;src2_octave=1;src2_partials=8;src2_bright=0.58;src2_drift=2.5",
      nullptr, nullptr, nullptr,
      "lfo1>detune:-0.105;lfo2>shimmer:-0.118;lfo3>brightness:0.206;lfo4>ens_depth:-0.220;beat>sub_level:0.083;wheel>air:0.359:u" },
    { "Phase Field",
      "z_mode=Series;z_shape=Phaser;z_x=0.4;z_y=0.5;z_rate=0.03;z_depth=1;z_res=0.4;z_mix=0.9;partials=24;"
      "shimmer=0.5;brain_density=6;far_decay=35;dly_mix=0.25;dly_feedback=0.6;width=1.5;lfo1_rate=0.018475;"
      "lfo1_depth=0.41;lfo2_rate=0.011418;lfo2_depth=0.49;lfo3_rate=0.0070567;lfo3_depth=0.54;"
      "lfo4_rate=0.0043613;lfo4_depth=0.42;odd_even=-0.12;src2_type=Additive;src2_level=0.14;src2_ratio=5/4;"
      "src2_octave=1;src2_partials=10;src2_bright=0.44;src2_drift=3.2",
      nullptr, nullptr, nullptr,
      "lfo1>pan_drift:-0.171;lfo2>air:0.201;lfo3>tilt:-0.133;lfo4>brightness:0.111;beat>resonance:0.164;wheel>dly_mix:0.234:u;pressure>z_x:0.239:u" },
    { "Comb Cathedral",
      "z_mode=Series;z_shape=Comb;z_x=0.5;z_y=0.4;z_rate=0.006;z_depth=0.7;z_res=0.6;z_mix=0.7;z_keytrack=1;"
      "partials=28;far_size=3;far_decay=50;far_highcut=6000;brain_density=4;attack=10;release=30;"
      "scale=Harmonic 8-16;root=C;lfo1_rate=0.013475;lfo1_depth=0.50;lfo2_rate=0.0083279;lfo2_depth=0.44;"
      "lfo3_rate=0.0051469;lfo3_depth=0.56;lfo4_rate=0.003181;lfo4_depth=0.48;odd_even=-0.28;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=5/4;src2_octave=1;src2_partials=11;src2_bright=0.39;"
      "src2_drift=2.5",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.106;lfo2>z_y:-0.109;lfo3>brightness:-0.197;lfo4>spread:-0.176;beat>far_size:0.149;wheel>air:0.192:u;slide>inharmonic:0.312:u" },
    { "Notch Winds",
      "z_mode=Replace;z_shape=Notch Cluster;z_x=0.5;z_y=0.5;z_rate=0.02;z_depth=1;z_res=0.5;z_mix=1;air=0.6;"
      "air_color=5;air_q=6;partials=10;brain_density=5;far_decay=30;master_gain=-8;lfo1_rate=0.012945;"
      "lfo1_depth=0.58;lfo2_rate=0.0080003;lfo2_depth=0.52;lfo3_rate=0.0049445;lfo3_depth=0.60;"
      "lfo4_rate=0.0030558;lfo4_depth=0.55;odd_even=-0.16;src2_type=Additive;src2_level=0.10;src2_ratio=3/2;"
      "src2_octave=1;src2_partials=12;src2_bright=0.47;src2_drift=5.0",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:0.111;lfo2>arc:0.101;lfo3>air:0.126;lfo4>resonance:-0.215;beat>brightness:0.148;pressure>far_level:0.162:u;wheel>z_y:0.417:u" },
    { "String Body",
      "z_mode=Replace;z_shape=Strings;z_x=0.4;z_y=0.5;z_rate=0.01;z_depth=0.6;z_res=0.7;z_mix=1;z_keytrack=0.8;"
      "partials=20;attack=3;decay=15;sustain=0.5;release=18;brain_rate=15;brain_density=5;far_decay=25;"
      "lfo1_rate=0.023603;lfo1_depth=0.44;lfo2_rate=0.014587;lfo2_depth=0.56;lfo3_rate=0.0090154;"
      "lfo3_depth=0.45;lfo4_rate=0.0055718;lfo4_depth=0.63;odd_even=-0.27;src2_type=Additive;src2_level=0.17;"
      "src2_ratio=2/1;src2_octave=0;src2_partials=7;src2_bright=0.51;src2_drift=3.6;far_width=0.815",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.151;lfo2>shimmer:-0.226;lfo3>spread:0.204;lfo4>cutoff:0.168;beat>brightness:0.168;slide>odd_even:0.209:u" },
    { "Struck Bars",
      "z_mode=Replace;z_shape=Metal Bars;z_x=0.3;z_y=0.4;z_rate=0.015;z_depth=0.8;z_res=0.8;z_mix=1;"
      "partials=16;inharmonic=0.3;attack=1;decay=20;sustain=0.35;release=25;brain_rate=10;brain_hold_min=8;"
      "brain_hold_max=40;far_decay=35;lfo1_rate=0.014389;lfo1_depth=0.55;lfo2_rate=0.0088926;lfo2_depth=0.61;"
      "lfo3_rate=0.005496;lfo3_depth=0.64;lfo4_rate=0.0033967;lfo4_depth=0.43;odd_even=-0.27;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=5/4;src2_octave=1;src2_partials=13;src2_bright=0.56;"
      "src2_drift=4.9;far_width=0.824",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:0.207;lfo2>brightness:-0.177;lfo3>shimmer:0.105;lfo4>tilt:0.144;beat>sub_level:0.092;wheel>z_y:0.369:u;slide>inharmonic:0.357:u" },
    { "Glass Needles",
      "z_mode=Replace;z_shape=Glass;z_x=0.5;z_y=0.6;z_rate=0.02;z_depth=1;z_res=0.9;z_mix=1;partials=24;"
      "brightness=1;brain_low=55;brain_high=92;brain_density=5;far_decay=40;far_highcut=9000;master_gain=-9;"
      "lfo1_rate=0.021441;lfo1_depth=0.44;lfo2_rate=0.013251;lfo2_depth=0.47;lfo3_rate=0.0081896;"
      "lfo3_depth=0.56;lfo4_rate=0.0050614;lfo4_depth=0.51;odd_even=-0.12;src2_type=Additive;src2_level=0.15;"
      "src2_ratio=5/3;src2_octave=1;src2_partials=11;src2_bright=0.42;src2_drift=2.6;far_width=0.633",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:-0.143;lfo2>shimmer:-0.208;lfo3>z_y:0.107;lfo4>air:0.100;beat>resonance:0.097;pressure>z_x:0.275:u;slide>odd_even:0.341:u" },
    { "Harmonic Sieve",
      "z_mode=Replace;z_shape=Peaks;z_x=0.4;z_y=0.5;z_rate=0.008;z_depth=0.6;z_res=0.8;z_mix=1;z_keytrack=1;"
      "air=0.4;air_mode=Ghost;air_q=16;partials=20;brain_density=4;far_decay=45;scale=Otonality 1-11;root=F;"
      "lfo1_rate=0.014564;lfo1_depth=0.63;lfo2_rate=0.009001;lfo2_depth=0.49;lfo3_rate=0.0055629;"
      "lfo3_depth=0.44;lfo4_rate=0.0034381;lfo4_depth=0.56;odd_even=-0.24;src2_type=Additive;src2_level=0.18;"
      "src2_ratio=2/1;src2_octave=1;src2_partials=7;src2_bright=0.44;src2_drift=2.9",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.111;lfo2>tilt:0.162;lfo3>cutoff:0.235;lfo4>detune:-0.222;beat>purity:0.157;slide>z_y:0.154:u;pressure>z_x:0.206:u" },
    { "Endless Resonance",
      "z_mode=Series;z_shape=Infinite;z_x=0.4;z_y=0.3;z_rate=0.004;z_depth=1;z_res=0.6;z_mix=0.6;"
      "z_keytrack=0.5;partials=12;tilt=1.4;brain_density=3;brain_hold_min=90;brain_hold_max=300;far_decay=60;"
      "far_highcut=3000;attack=15;release=40;master_gain=-9;lfo1_rate=0.014545;lfo1_depth=0.46;"
      "lfo2_rate=0.0089895;lfo2_depth=0.41;lfo3_rate=0.0055558;lfo3_depth=0.53;lfo4_rate=0.0034337;"
      "lfo4_depth=0.63;odd_even=-0.16;src2_type=Additive;src2_level=0.19;src2_ratio=3/2;src2_octave=0;"
      "src2_partials=10;src2_bright=0.36;src2_drift=5.9;far_width=0.774",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.239;lfo2>spread:-0.240;lfo3>brightness:-0.152;lfo4>arc:-0.199;beat>depth:0.125;slide>odd_even:0.194:u;wheel>air:0.188:u" },








    // ---------------------------------------------------------------- 148..157 granular
    { "Frozen Grain",
      "src3_type=Texture;src3_level=0.55;src3_grain=600;src3_density=8;src3_grains=12;src3_spread=0.004;"
      "src3_follow=Note;src3_pan=0.2;src3_pos=0.35;src3_pos_drift=0.05;osc_level=0.55;partials=12;"
      "brightness=0.5;attack=12;release=25;far_decay=35;depth=0.8;master_gain=-11;lfo1_rate=0.027456;"
      "lfo1_depth=0.47;lfo2_rate=0.016969;lfo2_depth=0.53;lfo3_rate=0.010487;lfo3_depth=0.64;"
      "lfo4_rate=0.0064814;lfo4_depth=0.59;odd_even=-0.25",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.200;lfo2>cutoff:-0.159;lfo3>shimmer:0.217;lfo4>pan_drift:-0.116;beat>depth:0.112;pressure>far_level:0.191:u;wheel>filter_fold:0.512:u" },
    { "Grain Field",
      "src3_type=Texture;src3_level=0.75;src3_grain=180;src3_density=28;src3_grains=32;src3_spread=0.85;"
      "src3_follow=Free;src3_pan=-0.2;src3_pos=0.5;src3_pos_drift=0.6;osc_level=0.22;partials=10;tilt=1.6;"
      "brightness=0.4;air=0.25;far_decay=25;depth=0.9;master_gain=-10;lfo1_rate=0.019406;lfo1_depth=0.55;"
      "lfo2_rate=0.011994;lfo2_depth=0.57;lfo3_rate=0.0074126;lfo3_depth=0.58;lfo4_rate=0.0045812;"
      "lfo4_depth=0.61;z_mode=Series;z_shape=Cave;z_x=0.56;z_y=0.74;z_z=0.16;z_res=0.51;z_mix=0.26;"
      "z_rate=0.0269;z_depth=0.28;odd_even=-0.29;far_width=0.500",
      nullptr, nullptr, nullptr,
      "lfo1>sub_level:0.237;lfo2>near_decay:0.117;lfo3>far_damp:0.180;lfo4>far_size:-0.122;beat>cutoff:0.105;wheel>far_level:0.230:u;slide>odd_even:0.244:u" },
    { "Grain Swarm",
      "src2_type=Texture;src2_level=0.7;src2_grain=90;src2_density=45;src2_grains=56;src2_spread=0.35;"
      "src2_follow=Free;src2_pan=-0.5;src3_type=Texture;src3_level=0.7;src3_grain=110;src3_density=40;"
      "src3_grains=56;src3_spread=0.45;src3_follow=Free;src3_pan=0.5;osc_level=0.18;partials=8;brain_density=4;"
      "far_decay=30;width=1.5;master_gain=-10;lfo1_rate=0.016323;lfo1_depth=0.46;lfo2_rate=0.010088;"
      "lfo2_depth=0.59;lfo3_rate=0.0062348;lfo3_depth=0.49;lfo4_rate=0.0038533;lfo4_depth=0.60;z_mode=Series;"
      "z_shape=Soprano;z_x=0.42;z_y=0.44;z_z=0.25;z_res=0.42;z_mix=0.30;z_rate=0.0315;z_depth=0.35;"
      "odd_even=-0.22;far_width=0.709",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.114;lfo2>z_y:0.180;lfo3>brightness:0.140;lfo4>resonance:-0.155;beat>cosmos_smear:0.093;pressure>cutoff:0.210:u" },
    { "Slow Scatter",
      "src3_type=Texture;src3_level=0.55;src3_grain=800;src3_density=4;src3_grains=8;src3_spread=0.6;"
      "src3_follow=Free;src3_pos_drift=0.8;osc_level=0.5;partials=14;attack=20;release=40;brain_rate=90;"
      "brain_hold_min=90;far_decay=45;master_gain=-11;lfo1_rate=0.016335;lfo1_depth=0.59;lfo2_rate=0.010095;"
      "lfo2_depth=0.61;lfo3_rate=0.0062393;lfo3_depth=0.52;lfo4_rate=0.0038561;lfo4_depth=0.60;odd_even=-0.15;"
      "far_width=0.547",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.132;lfo2>air:-0.172;lfo3>cutoff:-0.104;lfo4>tilt:0.214;beat>z_y:0.092;pressure>brightness:0.168:u;slide>inharmonic:0.269:u" },
    { "Grain Choir",
      "src3_type=Texture;src3_level=0.55;src3_grain=400;src3_density=14;src3_grains=24;src3_spread=0.02;"
      "src3_follow=Note;src3_ratio=3/2;osc_level=0.5;partials=18;scale=JI 7-limit;root=G;stack=Fifths;"
      "strands=4;ens_mix=0.5;far_decay=40;master_gain=-11;lfo1_rate=0.015822;lfo1_depth=0.51;"
      "lfo2_rate=0.0097788;lfo2_depth=0.51;lfo3_rate=0.0060437;lfo3_depth=0.57;lfo4_rate=0.0037352;"
      "lfo4_depth=0.42;odd_even=-0.21;ens_mode=Microshift;ens_depth=0.819;ens_rate=0.0470",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.224;lfo2>detune:0.178;lfo3>ens_depth:0.140;lfo4>air:0.111;beat>far_size:0.092;slide>odd_even:0.397:u" },
    { "Pulverised",
      "src2_type=Texture;src2_level=0.8;src2_grain=45;src2_density=55;src2_grains=64;src2_spread=1;"
      "src2_follow=Free;osc_level=0.15;partials=8;inharmonic=0.4;cutoff=1400;cloud_send=0.3;cloud_density=25;"
      "cloud_size=120;far_decay=25;master_gain=-10;lfo1_rate=0.024362;lfo1_depth=0.50;lfo2_rate=0.015057;"
      "lfo2_depth=0.44;lfo3_rate=0.0093056;lfo3_depth=0.44;lfo4_rate=0.0057512;lfo4_depth=0.50;z_mode=Series;"
      "z_shape=Church Bell;z_x=0.36;z_y=0.66;z_z=0.19;z_res=0.45;z_mix=0.33;z_rate=0.0207;z_depth=0.34;"
      "odd_even=-0.17",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:-0.124;lfo2>shimmer:-0.145;lfo3>air:0.225;lfo4>cutoff:-0.134;beat>depth:0.132;slide>inharmonic:0.316:u" },
    { "Grain Cavern",
      "src3_type=Texture;src3_level=0.55;src3_grain=500;src3_density=10;src3_grains=16;src3_spread=0.05;"
      "src3_follow=Note;src3_octave=-1;osc_level=0.45;partials=10;tilt=2;brightness=0.25;cutoff=800;"
      "sub_level=0.4;far_size=3;far_decay=60;far_highcut=1600;depth=1;master_gain=-10;lfo1_rate=0.017921;"
      "lfo1_depth=0.56;lfo2_rate=0.011076;lfo2_depth=0.48;lfo3_rate=0.006845;lfo3_depth=0.63;"
      "lfo4_rate=0.0042305;lfo4_depth=0.49;z_mode=Series;z_shape=Metal Bars;z_x=0.33;z_y=0.54;z_z=0.11;"
      "z_res=0.56;z_mix=0.29;z_rate=0.0238;z_depth=0.28;odd_even=-0.29;far_width=0.602",
      nullptr, nullptr, nullptr,
      "lfo1>far_size:0.211;lfo2>far_predelay:-0.124;lfo3>sub_level:-0.128;lfo4>inharmonic:-0.180;beat>cutoff:0.151;pressure>shimmer:0.390:u" },
    { "Grain Shimmer",
      "src3_type=Texture;src3_level=0.5;src3_grain=250;src3_density=20;src3_grains=28;src3_spread=0.12;"
      "src3_follow=Note;osc_level=0.5;partials=22;brightness=0.85;shimmer=0.6;cosmos_send=0.35;"
      "cosmos_shimmer=0.5;cosmos_shimmer_pitch=+12;cosmos_return=0.6;far_decay=35;master_gain=-11;"
      "lfo1_rate=0.015446;lfo1_depth=0.59;lfo2_rate=0.009546;lfo2_depth=0.55;lfo3_rate=0.0058998;"
      "lfo3_depth=0.64;lfo4_rate=0.0036462;lfo4_depth=0.62;odd_even=-0.16;far_width=0.551",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:-0.150;lfo2>spread:-0.164;lfo3>air:-0.104;lfo4>pan_drift:-0.205;beat>purity:0.090;slide>odd_even:0.173:u;pressure>shimmer:0.234:u" },
    { "Grain Tape",
      "src3_type=Texture;src3_level=0.55;src3_grain=320;src3_density=16;src3_grains=22;src3_spread=0.2;"
      "src3_follow=Free;osc_level=0.45;partials=14;fb_bus=0.12;fb_drive=0.7;fb_tape=0.6;fb_tone=2000;"
      "dly_mix=0.3;dly_feedback=0.7;far_decay=28;master_gain=-11;lfo1_rate=0.014191;lfo1_depth=0.62;"
      "lfo2_rate=0.0087704;lfo2_depth=0.49;lfo3_rate=0.0054204;lfo3_depth=0.54;lfo4_rate=0.00335;"
      "lfo4_depth=0.42;z_mode=Series;z_shape=Church Bell;z_x=0.54;z_y=0.69;z_z=0.34;z_res=0.52;z_mix=0.26;"
      "z_rate=0.0120;z_depth=0.30;odd_even=-0.25;far_width=0.606;haas=0.164;haas_time=15.4;filter_fold=0.295",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:-0.101;lfo2>resonance:0.126;lfo3>tilt:-0.224;lfo4>shimmer:0.213;beat>cutoff:0.129;wheel>dly_mix:0.310:u;slide>odd_even:0.307:u" },
    { "Grain Horizon",
      "src2_type=Texture;src2_level=0.45;src2_grain=700;src2_density=6;src2_grains=10;src2_spread=0.008;"
      "src2_follow=Note;src2_pan=-0.6;src3_type=Texture;src3_level=0.45;src3_grain=650;src3_density=6;"
      "src3_grains=10;src3_spread=0.008;src3_follow=Note;src3_pan=0.6;src3_ratio=3/2;osc_level=0.4;partials=16;"
      "attack=18;release=35;arc=0.6;arc_period=90;far_decay=50;depth=0.95;master_gain=-11;lfo1_rate=0.019016;"
      "lfo1_depth=0.62;lfo2_rate=0.011753;lfo2_depth=0.55;lfo3_rate=0.0072636;lfo3_depth=0.58;"
      "lfo4_rate=0.0044891;lfo4_depth=0.52;z_mode=Series;z_shape=Glass;z_x=0.59;z_y=0.47;z_z=0.18;z_res=0.48;"
      "z_mix=0.28;z_rate=0.0161;z_depth=0.24;odd_even=-0.14",
      nullptr, nullptr, nullptr,
      "lfo1>arc:-0.108;lfo2>pan_drift:-0.135;lfo3>detune:0.234;lfo4>air:0.205;beat>purity:0.153;pressure>far_level:0.240:u;slide>z_y:0.258:u" },





    // ---------------------------------------------------------------- 158..167 noise
    { "Pink Bed",
      "src3_type=Noise;src3_noise=Pink;src3_level=0.5;src3_pan=0.2;src3_pos=0.4;src3_pos_drift=0.3;"
      "osc_level=0.6;partials=12;brightness=0.5;attack=14;release=30;far_decay=40;depth=0.85;master_gain=-11;"
      "lfo1_rate=0.013938;lfo1_depth=0.42;lfo2_rate=0.0086142;lfo2_depth=0.62;lfo3_rate=0.0053239;"
      "lfo3_depth=0.62;lfo4_rate=0.0032903;lfo4_depth=0.43;odd_even=-0.20",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.172;lfo2>brightness:-0.218;lfo3>cutoff:0.230;lfo4>resonance:0.152;beat>z_y:0.144;wheel>air:0.387:u;slide>inharmonic:0.276:u" },
    { "Brown Floor",
      "src3_type=Noise;src3_noise=Brown;src3_level=0.6;src3_pan=-0.1;src3_pos=0.15;osc_level=0.5;partials=10;"
      "tilt=2;brightness=0.25;cutoff=700;sub_level=0.4;bass_mono=220;far_decay=50;far_highcut=1600;depth=0.95;"
      "master_gain=-11;lfo1_rate=0.015637;lfo1_depth=0.52;lfo2_rate=0.0096643;lfo2_depth=0.51;"
      "lfo3_rate=0.0059729;lfo3_depth=0.51;lfo4_rate=0.0036914;lfo4_depth=0.60;z_mode=Series;"
      "z_shape=Frame Drum;z_x=0.24;z_y=0.47;z_z=0.10;z_res=0.59;z_mix=0.32;z_rate=0.0318;z_depth=0.26;"
      "odd_even=-0.23;far_width=0.641",
      nullptr, nullptr, nullptr,
      "lfo1>drift:-0.219;lfo2>far_damp:0.239;lfo3>near_decay:0.109;lfo4>inharmonic:0.206;beat>z_y:0.102;wheel>air:0.292:u" },
    { "Wind Over Stone",
      "src2_type=Noise;src2_noise=Wind;src2_level=0.6;src2_noise_q=0.55;src2_pos=0.45;src2_pos_drift=0.9;"
      "src2_pan=-0.5;src3_type=Noise;src3_noise=Wind;src3_level=0.6;src3_noise_q=0.6;src3_pos=0.55;"
      "src3_pos_drift=0.9;src3_pan=0.5;osc_level=0.3;partials=8;far_size=3;far_decay=45;width=1.5;"
      "master_gain=-11;lfo1_rate=0.014744;lfo1_depth=0.56;lfo2_rate=0.0091121;lfo2_depth=0.54;"
      "lfo3_rate=0.0056316;lfo3_depth=0.64;lfo4_rate=0.0034805;lfo4_depth=0.52;odd_even=-0.17;far_width=0.585",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.161;lfo2>cutoff:0.191;lfo3>pan_drift:-0.123;lfo4>air:-0.136;beat>sub_level:0.140;slide>odd_even:0.340:u" },
    { "Vinyl Dust",
      "src3_type=Noise;src3_noise=Crackle;src3_level=0.45;src3_density=6;src3_pos=0.35;osc_level=0.55;"
      "partials=14;brightness=0.45;attack=10;release=25;near_mix=0.3;far_decay=25;master_gain=-11;"
      "lfo1_rate=0.016537;lfo1_depth=0.52;lfo2_rate=0.01022;lfo2_depth=0.48;lfo3_rate=0.0063166;"
      "lfo3_depth=0.50;lfo4_rate=0.0039039;lfo4_depth=0.60;odd_even=-0.24",
      nullptr, nullptr, nullptr,
      "lfo1>inharmonic:0.237;lfo2>pan_drift:-0.231;lfo3>cosmos_smear:0.239;lfo4>near_decay:0.179;beat>sub_level:0.169;wheel>air:0.168:u" },
    { "Formant Noise",
      "src2_type=Noise;src2_noise=Band;src2_level=0.55;src2_noise_q=0.75;src2_pos=0.35;src2_follow=Note;"
      "src2_pos_drift=0.4;osc_level=0.45;partials=16;z_mode=Series;z_shape=Vowel Morph;z_x=0.4;z_y=0.5;"
      "z_rate=0.02;z_depth=0.8;z_mix=0.6;far_decay=35;master_gain=-11;lfo1_rate=0.020641;lfo1_depth=0.51;"
      "lfo2_rate=0.012757;lfo2_depth=0.45;lfo3_rate=0.0078843;lfo3_depth=0.41;lfo4_rate=0.0048728;"
      "lfo4_depth=0.43;odd_even=-0.17;far_width=0.575",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:-0.172;lfo2>pan_drift:-0.175;lfo3>resonance:0.201;lfo4>brightness:0.115;beat>cutoff:0.160;slide>inharmonic:0.370:u;wheel>filter_fold:0.203:u" },
    { "Violet Air",
      "src3_type=Noise;src3_noise=Violet;src3_level=0.35;src3_pos=0.8;osc_level=0.55;partials=20;"
      "brightness=0.85;shimmer=0.5;far_highcut=12000;far_decay=30;side_air=4;width=1.5;master_gain=-12;"
      "lfo1_rate=0.017921;lfo1_depth=0.41;lfo2_rate=0.011076;lfo2_depth=0.53;lfo3_rate=0.006845;"
      "lfo3_depth=0.45;lfo4_rate=0.0042305;lfo4_depth=0.53;odd_even=-0.27;far_width=0.727",
      nullptr, nullptr, nullptr,
      "lfo1>brightness:0.127;lfo2>pan_drift:0.224;lfo3>spread:0.125;lfo4>arc:-0.163;beat>air:0.145;pressure>resonance:0.267:u;slide>tilt:0.161:u" },
    { "Grey Chamber",
      "src3_type=Noise;src3_noise=Grey;src3_level=0.5;src3_pos=0.5;src3_pos_drift=0.2;osc_level=0.5;"
      "partials=12;near_mix=0.35;near_decay=2;far_level=0.5;far_decay=15;far_size=1;depth=0.4;master_gain=-11;"
      "lfo1_rate=0.026159;lfo1_depth=0.60;lfo2_rate=0.016167;lfo2_depth=0.64;lfo3_rate=0.0099917;"
      "lfo3_depth=0.59;lfo4_rate=0.0061752;lfo4_depth=0.52;odd_even=-0.27",
      nullptr, nullptr, nullptr,
      "lfo1>air:0.196;lfo2>resonance:-0.193;lfo3>arc:-0.141;lfo4>tilt:0.173;beat>brightness:0.095;wheel>far_level:0.170:u;slide>odd_even:0.180:u" },
    { "Digital Rain",
      "src2_type=Noise;src2_noise=Digital;src2_level=0.4;src2_pos=0.55;src2_pos_drift=0.5;src2_pan=-0.4;"
      "osc_level=0.4;partials=10;inharmonic=0.3;cutoff=3000;dly_mix=0.3;dly_feedback=0.7;dly_time_l=0.37;"
      "dly_time_r=0.53;far_decay=30;master_gain=-11;lfo1_rate=0.019607;lfo1_depth=0.53;lfo2_rate=0.012118;"
      "lfo2_depth=0.60;lfo3_rate=0.0074894;lfo3_depth=0.64;lfo4_rate=0.0046287;lfo4_depth=0.59;odd_even=-0.24;"
      "haas=0.294;haas_time=17.9",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.166;lfo2>ens_depth:-0.139;lfo3>shimmer:0.137;lfo4>z_y:-0.225;beat>depth:0.086;wheel>dly_mix:0.300:u" },
    { "Blue Sheen",
      "src3_type=Noise;src3_noise=Blue;src3_level=0.4;src3_pos=0.7;src3_pos_drift=0.4;osc_level=0.5;"
      "partials=18;brightness=0.8;cosmos_send=0.3;cosmos_shimmer=0.4;cosmos_return=0.6;far_decay=35;"
      "far_highcut=9000;master_gain=-11;lfo1_rate=0.02348;lfo1_depth=0.44;lfo2_rate=0.014512;lfo2_depth=0.44;"
      "lfo3_rate=0.0089687;lfo3_depth=0.53;lfo4_rate=0.005543;lfo4_depth=0.45;odd_even=-0.18",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.103;lfo2>pan_drift:-0.109;lfo3>detune:0.206;lfo4>air:0.133;beat>cutoff:0.081;wheel>far_level:0.151:u;slide>inharmonic:0.258:u" },
    { "Noise Cathedral",
      "src2_type=Noise;src2_noise=White;src2_level=0.35;src2_pan=-0.6;src3_type=Noise;src3_noise=Pink;"
      "src3_level=0.45;src3_pan=0.6;src3_pos=0.3;osc_level=0.35;partials=8;far_size=3;far_decay=70;"
      "far_highcut=5000;far_level=1;depth=1;room_level=0.3;master_gain=-11;lfo1_rate=0.022109;lfo1_depth=0.65;"
      "lfo2_rate=0.013664;lfo2_depth=0.56;lfo3_rate=0.0084447;lfo3_depth=0.61;lfo4_rate=0.0052191;"
      "lfo4_depth=0.52;odd_even=-0.26",
      nullptr, nullptr, nullptr,
      "lfo1>spread:-0.213;lfo2>arc:-0.221;lfo3>pan_drift:-0.169;lfo4>tilt:0.192;beat>brightness:0.167;wheel>air:0.331:u;slide>inharmonic:0.339:u" },







    // ---------------------------------------------------------------- 168..177 rich studies
    { "Breathing Room",
      "phase_width=0.7;phase_rate=0.02;breath=0.3;breath_rate=0.02;doppler=0.6;depth=0.8;osc_level=0.7;"
      "partials=14;brightness=0.55;shimmer=0.45;attack=12;release=30;far_decay=40;master_gain=-11;"
      "lfo1_rate=0.01293;lfo1_depth=0.47;lfo2_rate=0.0079912;lfo2_depth=0.47;lfo3_rate=0.0049388;"
      "lfo3_depth=0.57;lfo4_rate=0.0030524;lfo4_depth=0.58;odd_even=-0.28;src2_type=Additive;src2_level=0.10;"
      "src2_ratio=3/2;src2_octave=0;src2_partials=11;src2_bright=0.42;src2_drift=3.0",
      nullptr, nullptr, nullptr,
      "lfo1>resonance:0.147;lfo2>detune:0.173;lfo3>z_y:0.104;lfo4>pan_drift:-0.205;beat>far_size:0.092;pressure>cutoff:0.246:u;wheel>far_level:0.281:u" },
    { "Fog Bank",
      "blur_mix=0.65;blur_smear=0.8;osc_level=0.7;partials=18;brightness=0.6;attack=1.5;release=20;"
      "keys_depth=0.2;depth=0.85;far_decay=45;far_highcut=4000;dly_mix=0.2;dly_feedback=0.6;master_gain=-11;"
      "lfo1_rate=0.014705;lfo1_depth=0.52;lfo2_rate=0.0090885;lfo2_depth=0.48;lfo3_rate=0.005617;"
      "lfo3_depth=0.41;lfo4_rate=0.0034715;lfo4_depth=0.62;odd_even=-0.19;src2_type=Additive;src2_level=0.17;"
      "src2_ratio=3/2;src2_octave=0;src2_partials=6;src2_bright=0.43;src2_drift=4.9;far_width=0.480;haas=0.180;"
      "haas_time=17.1",
      nullptr, nullptr, nullptr,
      "lfo1>air:0.185;lfo2>arc:0.228;lfo3>z_y:0.126;lfo4>shimmer:0.163;beat>far_size:0.146;slide>inharmonic:0.182:u;pressure>far_level:0.211:u" },
    { "Throat Singer",
      "filter_model=Formant;cutoff=900;resonance=0.55;filter_drift=0.7;keytrack=0;filter_env=0;osc_level=0.8;"
      "partials=24;tilt=0.9;brightness=0.9;odd_even=0.3;shimmer=0.35;strands=2;detune=3;attack=8;release=25;"
      "air=0.1;far_decay=30;depth=0.6;master_gain=-11;lfo1_rate=0.016195;lfo1_depth=0.63;lfo2_rate=0.010009;"
      "lfo2_depth=0.49;lfo3_rate=0.0061859;lfo3_depth=0.63;lfo4_rate=0.0038231;lfo4_depth=0.65;z_mode=Series;"
      "z_shape=Soprano;z_x=0.32;z_y=0.54;z_z=0.07;z_res=0.43;z_mix=0.25;z_rate=0.0156;z_depth=0.33;"
      "src2_type=Additive;src2_level=0.19;src2_ratio=4/3;src2_octave=0;src2_partials=13;src2_bright=0.44;"
      "src2_drift=3.8;far_width=0.666",
      nullptr, nullptr, nullptr,
      "lfo1>arc:0.187;lfo2>ens_depth:0.237;lfo3>air:-0.205;lfo4>tilt:0.168;beat>brightness:0.170;pressure>z_x:0.163:u" },
    { "Plucked Void",
      "strike_level=0.55;strike_type=String;strike_decay=1.2;strike_damp=0.35;keys_depth=0.1;osc_level=0.45;"
      "partials=12;brightness=0.4;attack=6;release=40;far_size=3;far_decay=60;far_level=1;depth=0.95;"
      "dly_mix=0.25;dly_feedback=0.7;dly_to_far=0.6;master_gain=-11;lfo1_rate=0.015746;lfo1_depth=0.60;"
      "lfo2_rate=0.0097314;lfo2_depth=0.52;lfo3_rate=0.0060143;lfo3_depth=0.51;lfo4_rate=0.003717;"
      "lfo4_depth=0.48;odd_even=-0.18;src2_type=Additive;src2_level=0.19;src2_ratio=3/2;src2_octave=1;"
      "src2_partials=9;src2_bright=0.47;src2_drift=3.3",
      nullptr, nullptr, nullptr,
      "lfo1>cosmos_smear:0.193;lfo2>sub_level:0.167;lfo3>far_predelay:-0.104;lfo4>far_size:-0.223;beat>cutoff:0.091;wheel>filter_fold:0.433:u" },
    { "Wooden Knock",
      "strike_level=0.5;strike_type=Wood;strike_decay=0.25;strike_damp=0.8;strike_who=Keys + Brain;"
      "brain_rate=14;brain_density=4;osc_level=0.5;partials=10;tilt=1.6;brightness=0.35;attack=9;release=28;"
      "far_decay=35;depth=0.9;room_level=0.35;master_gain=-11;lfo1_rate=0.026249;lfo1_depth=0.60;"
      "lfo2_rate=0.016223;lfo2_depth=0.58;lfo3_rate=0.010026;lfo3_depth=0.49;lfo4_rate=0.0061966;"
      "lfo4_depth=0.47;z_mode=Series;z_shape=Metal Bars;z_x=0.48;z_y=0.34;z_z=0.24;z_res=0.48;z_mix=0.28;"
      "z_rate=0.0236;z_depth=0.40;odd_even=-0.15;src2_type=Additive;src2_level=0.12;src2_ratio=2/1;"
      "src2_octave=1;src2_partials=8;src2_bright=0.53;src2_drift=3.0;far_width=0.510",
      nullptr, nullptr, nullptr,
      "lfo1>z_x:0.175;lfo2>inharmonic:0.184;lfo3>far_size:0.236;lfo4>depth:-0.178;beat>z_y:0.161;slide>odd_even:0.337:u;wheel>air:0.280:u" },
    { "Bell Metal",
      "strike_level=0.45;strike_type=Metal;strike_decay=2.5;strike_damp=0.15;keys_depth=0.15;osc_level=0.5;"
      "partials=20;inharmonic=0.35;brightness=0.7;shimmer=0.3;attack=10;release=35;cosmos_send=0.3;"
      "cosmos_shimmer=0.4;cosmos_return=0.5;far_decay=45;far_highcut=7000;master_gain=-12;lfo1_rate=0.015393;"
      "lfo1_depth=0.45;lfo2_rate=0.0095136;lfo2_depth=0.56;lfo3_rate=0.0058798;lfo3_depth=0.42;"
      "lfo4_rate=0.0036339;lfo4_depth=0.53;odd_even=-0.26;src2_type=Additive;src2_level=0.15;src2_ratio=2/1;"
      "src2_octave=1;src2_partials=6;src2_bright=0.51;src2_drift=3.8;far_width=0.541",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.179;lfo2>resonance:0.210;lfo3>brightness:-0.177;lfo4>tilt:-0.184;beat>z_y:0.098;slide>odd_even:0.355:u;pressure>cutoff:0.320:u" },
    { "Three Reeds",
      "src1_ratio=1/1;src1_drift=3;src2_type=Additive;src2_level=0.5;src2_ratio=3/2;src2_drift=5;src2_pan=-0.5;"
      "src2_partials=14;src2_tilt=1.1;src2_bright=0.6;src3_type=Additive;src3_level=0.45;src3_ratio=5/4;"
      "src3_drift=4;src3_pan=0.5;src3_partials=12;src3_tilt=1.3;src3_bright=0.5;osc_level=0.55;partials=16;"
      "strands=1;drift_rate=0.02;attack=10;release=30;far_decay=35;depth=0.7;master_gain=-12;"
      "lfo1_rate=0.020291;lfo1_depth=0.43;lfo2_rate=0.01254;lfo2_depth=0.59;lfo3_rate=0.0077504;"
      "lfo3_depth=0.46;lfo4_rate=0.00479;lfo4_depth=0.41;odd_even=-0.15;far_width=0.717",
      nullptr, nullptr, nullptr,
      "lfo1>spread:-0.170;lfo2>shimmer:-0.113;lfo3>pan_drift:-0.171;lfo4>z_y:0.201;beat>resonance:0.082;wheel>air:0.396:u;slide>tilt:0.189:u" },
    { "Drowned Echo",
      "dly_mix=0.5;dly_feedback=0.9;dly_absorb=0.9;dly_time_l=0.9;dly_time_r=1.3;dly_cross=0.5;dly_to_far=0.7;"
      "dly2_mix=0.3;dly2_feedback=0.8;dly2_absorb=0.7;dly2_time_l=2.1;dly2_time_r=2.9;osc_level=0.6;"
      "partials=14;brightness=0.6;attack=3;release=25;keys_depth=0.15;far_decay=40;master_gain=-11;"
      "lfo1_rate=0.027489;lfo1_depth=0.60;lfo2_rate=0.016989;lfo2_depth=0.58;lfo3_rate=0.0105;lfo3_depth=0.61;"
      "lfo4_rate=0.0064892;lfo4_depth=0.55;z_mode=Series;z_shape=Choir;z_x=0.54;z_y=0.46;z_z=0.06;z_res=0.43;"
      "z_mix=0.25;z_rate=0.0081;z_depth=0.29;odd_even=-0.25;src2_type=Additive;src2_level=0.15;src2_ratio=5/4;"
      "src2_octave=1;src2_partials=12;src2_bright=0.53;src2_drift=3.0;far_width=0.648;haas=0.311;haas_time=19.8",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.111;lfo2>resonance:0.206;lfo3>spread:0.177;lfo4>tilt:0.201;beat>depth:0.097;slide>odd_even:0.198:u;pressure>shimmer:0.284:u" },
    { "Evening Tide",
      "tide=8;tide_period=6;purity_drift=0.3;purity_rate=0.008;osc_level=0.65;partials=16;brightness=0.5;"
      "shimmer=0.5;shimmer_rate=0.05;attack=16;release=40;sub_level=0.3;sub_glide=12;far_decay=50;depth=0.9;"
      "master_gain=-11;lfo1_rate=0.019177;lfo1_depth=0.64;lfo2_rate=0.011852;lfo2_depth=0.62;"
      "lfo3_rate=0.0073249;lfo3_depth=0.43;lfo4_rate=0.004527;lfo4_depth=0.56;z_mode=Series;z_shape=Glass;"
      "z_x=0.34;z_y=0.43;z_z=0.29;z_res=0.49;z_mix=0.38;z_rate=0.0218;z_depth=0.31;odd_even=-0.20;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=5/3;src2_octave=1;src2_partials=6;src2_bright=0.57;"
      "src2_drift=5.2;far_width=0.422",
      nullptr, nullptr, nullptr,
      "lfo1>z_y:0.146;lfo2>resonance:-0.164;lfo3>pan_drift:0.130;lfo4>brightness:-0.216;beat>cutoff:0.158;wheel>air:0.269:u;pressure>z_x:0.301:u" },
    { "Turning Sky",
      "far_rotate=0.8;far_level=1;far_size=3;far_decay=70;far_asym=0.8;phase_width=0.5;phase_rate=0.012;"
      "osc_level=0.5;partials=12;brightness=0.45;attack=14;release=45;depth=1;width=1.4;master_gain=-11;"
      "lfo1_rate=0.015477;lfo1_depth=0.57;lfo2_rate=0.0095655;lfo2_depth=0.45;lfo3_rate=0.0059118;"
      "lfo3_depth=0.56;lfo4_rate=0.0036537;lfo4_depth=0.55;z_mode=Series;z_shape=Metal Bars;z_x=0.22;z_y=0.47;"
      "z_z=0.06;z_res=0.40;z_mix=0.25;z_rate=0.0343;z_depth=0.41;odd_even=-0.19;src2_type=Additive;"
      "src2_level=0.10;src2_ratio=3/2;src2_octave=1;src2_partials=11;src2_bright=0.40;src2_drift=4.3",
      nullptr, nullptr, nullptr,
      "lfo1>far_size:0.163;lfo2>depth:0.174;lfo3>inharmonic:-0.166;lfo4>sub_level:0.157;beat>purity:0.159;slide>z_y:0.259:u;pressure>shimmer:0.229:u" },






    // ---------------------------------------------------------------- 178..186 body and place
    { "Soundboard",
      "body_level=0.8;body_material=Wood;body_pitch=0.5;body_decay=3;body_tone=0.3;body_spread=0.7;"
      "osc_level=0.6;partials=16;tilt=1.4;brightness=0.5;attack=10;release=30;far_decay=35;depth=0.8;"
      "master_gain=-13;lfo1_rate=0.019658;lfo1_depth=0.60;lfo2_rate=0.01215;lfo2_depth=0.52;"
      "lfo3_rate=0.0075088;lfo3_depth=0.48;lfo4_rate=0.0046407;lfo4_depth=0.53;z_mode=Series;z_shape=Soprano;"
      "z_x=0.36;z_y=0.49;z_z=0.14;z_res=0.60;z_mix=0.27;z_rate=0.0084;z_depth=0.23;odd_even=-0.21;"
      "src2_type=Additive;src2_level=0.15;src2_ratio=5/3;src2_octave=1;src2_partials=8;src2_bright=0.47;"
      "src2_drift=3.2;far_width=0.774",
      nullptr, nullptr, nullptr,
      "lfo1>spread:0.144;lfo2>air:-0.138;lfo3>brightness:0.172;lfo4>tilt:0.140;beat>sub_level:0.086;slide>odd_even:0.359:u;pressure>far_level:0.215:u" },
    { "Struck Plate",
      "body_level=0.7;body_material=Plate;body_decay=6;body_tone=0.6;strike_level=0.4;strike_type=Metal;"
      "strike_decay=1.5;strike_damp=0.3;osc_level=0.45;partials=14;brightness=0.55;attack=6;release=25;"
      "keys_depth=0.15;far_decay=40;master_gain=-13;lfo1_rate=0.015007;lfo1_depth=0.54;lfo2_rate=0.0092747;"
      "lfo2_depth=0.54;lfo3_rate=0.0057321;lfo3_depth=0.51;lfo4_rate=0.0035426;lfo4_depth=0.41;odd_even=-0.13;"
      "src2_type=Additive;src2_level=0.17;src2_ratio=2/1;src2_octave=1;src2_partials=9;src2_bright=0.49;"
      "src2_drift=3.9;far_width=0.713",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:0.188;lfo2>shimmer:-0.158;lfo3>air:0.156;lfo4>pan_drift:0.191;beat>sub_level:0.134;wheel>filter_fold:0.362:u;pressure>brightness:0.155:u" },
    { "Cathedral Bell",
      "body_level=0.75;body_material=Bell;body_pitch=1;body_decay=14;body_tone=0.7;body_spread=0.8;"
      "osc_level=0.4;partials=18;inharmonic=0.25;brightness=0.6;attack=12;release=40;far_size=3;far_decay=60;"
      "far_level=1;depth=0.95;master_gain=-14;lfo1_rate=0.012982;lfo1_depth=0.41;lfo2_rate=0.0080232;"
      "lfo2_depth=0.59;lfo3_rate=0.0049586;lfo3_depth=0.42;lfo4_rate=0.0030646;lfo4_depth=0.53;z_mode=Series;"
      "z_shape=Choir;z_x=0.35;z_y=0.55;z_z=0.15;z_res=0.50;z_mix=0.34;z_rate=0.0246;z_depth=0.44;"
      "odd_even=-0.12;src2_type=Additive;src2_level=0.14;src2_ratio=5/3;src2_octave=1;src2_partials=6;"
      "src2_bright=0.54;src2_drift=3.5",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.214;lfo2>air:-0.203;lfo3>brightness:0.177;lfo4>cutoff:-0.236;beat>resonance:0.097;pressure>z_x:0.388:u;wheel>far_level:0.294:u" },
    { "Sympathetic Strings",
      "body_level=0.6;body_material=String;body_decay=5;body_tone=0.5;brain2_on=on;brain2_rate=45;"
      "brain2_density=2;brain2_interval=7;brain2_depth=0.95;osc_level=0.55;partials=20;attack=14;release=35;"
      "far_decay=45;depth=0.85;master_gain=-13;lfo1_rate=0.028312;lfo1_depth=0.59;lfo2_rate=0.017497;"
      "lfo2_depth=0.42;lfo3_rate=0.010814;lfo3_depth=0.53;lfo4_rate=0.0066834;lfo4_depth=0.45;odd_even=-0.23;"
      "src2_type=Additive;src2_level=0.18;src2_ratio=5/4;src2_octave=1;src2_partials=10;src2_bright=0.37;"
      "src2_drift=2.0",
      nullptr, nullptr, nullptr,
      "lfo1>ens_depth:-0.138;lfo2>air:-0.234;lfo3>resonance:-0.135;lfo4>detune:0.102;beat>brightness:0.169;pressure>shimmer:0.235:u;slide>inharmonic:0.363:u" },
    { "Clearing Fog",
      "far_unmask=0.85;far_level=1;far_size=2.6;far_decay=55;osc_level=0.6;partials=16;brightness=0.55;"
      "attack=8;release=28;dly_mix=0.25;dly_to_far=0.7;depth=0.9;master_gain=-11;lfo1_rate=0.021121;"
      "lfo1_depth=0.46;lfo2_rate=0.013054;lfo2_depth=0.59;lfo3_rate=0.0080677;lfo3_depth=0.49;"
      "lfo4_rate=0.0049861;lfo4_depth=0.57;z_mode=Series;z_shape=Glass;z_x=0.44;z_y=0.45;z_z=0.29;z_res=0.52;"
      "z_mix=0.41;z_rate=0.0373;z_depth=0.26;odd_even=-0.25;src2_type=Additive;src2_level=0.13;src2_ratio=3/2;"
      "src2_octave=1;src2_partials=8;src2_bright=0.54;src2_drift=5.7",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.199;lfo2>ens_depth:-0.194;lfo3>air:-0.158;lfo4>cutoff:0.217;beat>sub_level:0.140;wheel>far_level:0.355:u" },
    { "Old Machine",
      "patina=0.85;patina_wow=0.7;patina_hiss=0.5;patina_age=0.6;osc_level=0.6;partials=14;brightness=0.5;"
      "attack=12;release=35;dly_mix=0.3;dly_feedback=0.7;dly_absorb=0.7;far_decay=40;master_gain=-11;"
      "lfo1_rate=0.024388;lfo1_depth=0.60;lfo2_rate=0.015073;lfo2_depth=0.61;lfo3_rate=0.0093156;"
      "lfo3_depth=0.58;lfo4_rate=0.0057573;lfo4_depth=0.61;z_mode=Series;z_shape=Choir;z_x=0.26;z_y=0.36;"
      "z_z=0.32;z_res=0.62;z_mix=0.30;z_rate=0.0126;z_depth=0.39;odd_even=-0.22;src2_type=Additive;"
      "src2_level=0.16;src2_ratio=4/3;src2_octave=0;src2_partials=11;src2_bright=0.56;src2_drift=3.1;"
      "far_width=0.743;filter_fold=0.194",
      nullptr, nullptr, nullptr,
      "lfo1>shimmer:0.132;lfo2>detune:-0.146;lfo3>ens_depth:0.111;lfo4>brightness:-0.118;beat>resonance:0.110;wheel>far_level:0.234:u" },
    { "Around The Head",
      "externalise=0.9;phase_width=0.6;phase_rate=0.02;itd=0.9;pan_drift=0.6;breath=0.35;doppler=0.5;"
      "osc_level=0.55;partials=16;brightness=0.6;attack=10;release=30;far_decay=40;depth=0.75;width=1.3;"
      "master_gain=-12;lfo1_rate=0.027928;lfo1_depth=0.46;lfo2_rate=0.017261;lfo2_depth=0.53;"
      "lfo3_rate=0.010668;lfo3_depth=0.51;lfo4_rate=0.006593;lfo4_depth=0.63;odd_even=-0.18;src2_type=Additive;"
      "src2_level=0.15;src2_ratio=2/1;src2_octave=0;src2_partials=9;src2_bright=0.48;src2_drift=5.5",
      nullptr, nullptr, nullptr,
      "lfo1>detune:0.151;lfo2>pan_drift:-0.208;lfo3>air:-0.115;lfo4>arc:0.197;beat>brightness:0.120;wheel>far_level:0.386:u;pressure>resonance:0.196:u" },
    { "Two Conductors",
      "brain2_on=on;brain2_rate=30;brain2_density=3;brain2_interval=-12;brain2_low=24;brain2_high=48;"
      "brain2_depth=1;brain_rate=35;brain_density=4;brain_low=48;brain_high=79;osc_level=0.5;partials=16;"
      "attack=12;release=35;far_decay=50;depth=0.9;master_gain=-12;lfo1_rate=0.014859;lfo1_depth=0.65;"
      "lfo2_rate=0.0091836;lfo2_depth=0.62;lfo3_rate=0.0056758;lfo3_depth=0.65;lfo4_rate=0.0035078;"
      "lfo4_depth=0.43;z_mode=Series;z_shape=Glass;z_x=0.44;z_y=0.61;z_z=0.08;z_res=0.62;z_mix=0.30;"
      "z_rate=0.0314;z_depth=0.41;odd_even=-0.28;src2_type=Additive;src2_level=0.11;src2_ratio=4/3;"
      "src2_octave=1;src2_partials=13;src2_bright=0.57;src2_drift=5.4",
      nullptr, nullptr, nullptr,
      "lfo1>air:-0.120;lfo2>shimmer:-0.180;lfo3>cutoff:0.166;lfo4>resonance:-0.209;beat>purity:0.160;pressure>brightness:0.156:u" },
    { "Under The Hand",
      "brain_on=off;keys_depth=0.7;press_distance=0.8;press_bright=0.4;press_level=0.5;slide_cutoff=2;"
      "slide_z=0.6;bend_range=12;mpe=on;attack=1.5;release=8;osc_level=0.7;partials=20;brightness=0.4;"
      "z_mode=Series;z_shape=Vowel Morph;z_mix=0.6;far_decay=25;master_gain=-11;lfo1_rate=0.025598;"
      "lfo1_depth=0.50;lfo2_rate=0.015821;lfo2_depth=0.60;lfo3_rate=0.0097777;lfo3_depth=0.55;"
      "lfo4_rate=0.0060429;lfo4_depth=0.45;src2_type=Additive;src2_level=0.16;src2_ratio=5/3;src2_octave=1;"
      "src2_partials=10;src2_bright=0.55;src2_drift=2.6;far_width=0.525",
      nullptr, nullptr, nullptr,
      "lfo1>far_size:0.156;lfo2>breath:-0.217;lfo3>sub_level:0.221;lfo4>far_damp:-0.221;beat>cutoff:0.121;slide>odd_even:0.257:u" },





    // ---------------------------------------------------------------- 187..195 autoplay / progressions
    { "Slow Progression",
      "auto_mode=Chords;auto_rate=60;auto_lead=3;auto_tension=0.15;auto_root_move=0.25;brain_density=5;"
      "brain_low=45;brain_high=76;brain_consonance=0.8;osc_level=0.55;partials=18;tilt=1.3;brightness=0.5;"
      "attack=14;release=40;far_size=2.4;far_decay=45;far_level=0.9;depth=0.85;dly_mix=0.2;master_gain=-12;"
      "lfo1_rate=0.026744;lfo1_depth=0.49;lfo2_rate=0.016529;lfo2_depth=0.44;lfo3_rate=0.010215;"
      "lfo3_depth=0.41;lfo4_rate=0.0063134;lfo4_depth=0.49;odd_even=-0.26;src2_type=Additive;src2_level=0.18;"
      "src2_ratio=2/1;src2_octave=1;src2_partials=6;src2_bright=0.39;src2_drift=5.0;haas=0.338;haas_time=16.5",
      nullptr, nullptr, nullptr,
      "lfo1>tilt:-0.194;lfo2>brightness:-0.229;lfo3>z_y:0.187;lfo4>resonance:0.123;beat>cutoff:0.123;pressure>far_level:0.241:u" },
    { "Turning Harmony",
      "auto_mode=Chords;auto_rate=35;auto_lead=7;auto_tension=0.5;auto_root_move=0.8;brain_density=6;"
      "brain_low=40;brain_high=79;brain_consonance=0.55;osc_level=0.5;partials=20;inharmonic=0.12;"
      "brightness=0.6;attack=10;release=32;shimmer=0.25;far_size=2.8;far_decay=55;far_level=1;depth=0.9;"
      "dly_mix=0.25;dly_to_far=0.6;master_gain=-12;lfo1_rate=0.020093;lfo1_depth=0.60;lfo2_rate=0.012418;"
      "lfo2_depth=0.55;lfo3_rate=0.007675;lfo3_depth=0.45;lfo4_rate=0.0047434;lfo4_depth=0.53;odd_even=-0.12;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=5/4;src2_octave=1;src2_partials=7;src2_bright=0.50;"
      "src2_drift=3.2;haas=0.225;haas_time=13.3",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.232;lfo2>pan_drift:0.178;lfo3>ens_depth:-0.236;lfo4>tilt:0.152;beat>brightness:0.160;pressure>far_level:0.165:u;slide>odd_even:0.380:u" },
    { "Chord Ladder",
      "auto_mode=Chords;auto_sync=4 bars;auto_lead=12;auto_tension=0.7;auto_root_move=0.5;tempo=64;"
      "brain_density=5;brain_low=48;brain_high=84;brain_consonance=0.4;osc_level=0.5;partials=16;"
      "brightness=0.65;attack=6;release=20;dly_mix=0.35;dly_sync_l=1/2;dly_sync_r=3/4;dly_feedback=0.55;"
      "far_decay=35;depth=0.75;master_gain=-12;lfo1_rate=0.0184;lfo1_depth=0.65;lfo2_rate=0.011372;"
      "lfo2_depth=0.56;lfo3_rate=0.0070281;lfo3_depth=0.61;lfo4_rate=0.0043436;lfo4_depth=0.58;odd_even=-0.20;"
      "src2_type=Additive;src2_level=0.14;src2_ratio=3/2;src2_octave=0;src2_partials=12;src2_bright=0.51;"
      "src2_drift=5.7;far_width=0.695;haas=0.151;haas_time=16.8",
      nullptr, nullptr, nullptr,
      "lfo1>cutoff:-0.148;lfo2>z_y:0.129;lfo3>arc:0.154;lfo4>ens_depth:-0.191;beat>resonance:0.130;wheel>dly_mix:0.182:u;slide>odd_even:0.235:u" },
    { "By Hand",
      "auto_mode=Chords;auto_rate=900;auto_lead=5;auto_tension=0.25;auto_root_move=0.35;brain_density=5;"
      "brain_low=43;brain_high=74;brain_consonance=0.75;osc_level=0.6;partials=18;tilt=1.5;brightness=0.45;"
      "attack=18;release=45;body_level=0.35;body_material=Wood;body_decay=4;far_size=2.2;far_decay=40;"
      "depth=0.8;master_gain=-12;lfo1_rate=0.020774;lfo1_depth=0.56;lfo2_rate=0.012839;lfo2_depth=0.54;"
      "lfo3_rate=0.0079348;lfo3_depth=0.51;lfo4_rate=0.004904;lfo4_depth=0.54;odd_even=-0.30;"
      "src2_type=Additive;src2_level=0.11;src2_ratio=5/4;src2_octave=1;src2_partials=9;src2_bright=0.49;"
      "src2_drift=2.2;far_width=0.706",
      nullptr, nullptr, nullptr,
      "lfo1>z_x:0.127;lfo2>near_decay:0.180;lfo3>sub_level:0.114;lfo4>cosmos_smear:-0.136;beat>resonance:0.115;pressure>shimmer:0.341:u;wheel>air:0.297:u" },












    { "Long Arc",
      "brain_density=5;brain_rate=40;brain_hold_min=70;brain_hold_max=260;brain_low=33;brain_high=68;"
      "partials=18;tilt=1.35;brightness=0.5;odd_even=-0.25;attack=14;release=40;cutoff=1400;depth=0.85;"
      "far_size=2.6;far_decay=40;far_highcut=3200;air=0.18;arc=0.35;arc_period=120;sub_level=0.3;near_mix=0.2;"
      "master_gain=-11;env1_mode=One Shot;env1_time=45;env1_depth=1;haas=0.190;haas_time=11.2",
      nullptr, nullptr, nullptr,
      "env1>brightness:0.34;env1>far_size:0.28;env1>cutoff:0.26;env1>air:0.16;wheel>far_level:0.207:u;pressure>shimmer:0.355:u",
      "0:0:0.4/1:0.35:0/2.2:0.15:0.3/3.5:0.62:-0.2/4.6:0.4:0/5.8:0.85:-0.35/7:0.55:0.2/8.2:0.95:-0.4/9.1:0.6:0/10:0.72:0.25/11:0.28:0.4/12:0:0" },

    { "Held Breath",
      "brain_on=off;keys_depth=1;partials=14;tilt=1.2;brightness=0.55;odd_even=-0.18;attack=2.5;decay=6;"
      "sustain=0.85;release=12;cutoff=2200;resonance=0.25;depth=0.7;far_size=2.2;far_decay=26;far_highcut=4200;"
      "air=0.22;near_mix=0.25;master_gain=-10;env1_mode=Sustain Loop;env1_time=6;env1_depth=1",
      nullptr, nullptr, nullptr,
      "env1>brightness:0.30;env1>far_size:0.22;env1>air:0.18;pressure>shimmer:0.392:u",
      "0:0:0.35/0.9:0.95:-0.25/2.2:0.6:0/8:0.45:0.2/11:0:0!s2" },

    { "Six Hands",
      "brain_density=6;brain_rate=45;brain_hold_min=60;brain_hold_max=240;brain_low=31;brain_high=71;"
      "partials=20;tilt=1.5;brightness=0.5;odd_even=-0.28;inharmonic=0.08;attack=16;release=45;cutoff=1800;"
      "depth=0.8;far_size=2.2;far_decay=45;far_highcut=3600;air=0.2;detune=5;strands=3;near_mix=0.22;"
      "pan_drift=0.4;sub_level=0.25;master_gain=-8;env1_mode=Loop;env1_time=9;env1_depth=0.9;env2_mode=Loop;"
      "env2_time=14.6;env2_depth=0.85;env3_mode=Loop;env3_time=23.6;env3_depth=0.8;env4_mode=Loop;"
      "env4_time=38.2;env4_depth=0.75;env5_mode=Loop;env5_time=61.8;env5_depth=0.7;env6_mode=One Shot;"
      "env6_time=100;env6_depth=0.65;far_width=0.644;haas=0.319;haas_time=18.2",
      nullptr, nullptr, nullptr,
      "env1>cutoff:0.24;env2>far_size:0.20;env3>depth:0.16;env4>detune:0.22;env5>near_mix:0.18;env6>brightness:0.26;wheel>filter_fold:0.219:u",
      "0:0:0.3/1:0.8:-0.3/2.4:-0.4:0.2/4:0:0!l0-2~0:0:-0.2/1.4:0.7:0.3/3:-0.55:-0.2/4.5:0:0!l0-2~0:0:0.5/1.1:-0.65:0/2.6:0.75:-0.4/4.2:0:0!l0-2~0:0:0/1.6:0.6:0.4/3.2:-0.35:-0.3/5:0:0!l0-2~0:0:-0.4/2:0.85:0.2/4.4:-0.5:0/6:0:0!l0-2~0:-0.6:0.3/2:0.2:0/4.5:0.8:-0.35/7:0.35:0.2/9:0:0" },

    { "Sixteen Points",
      "brain_density=3;brain_rate=55;brain_hold_min=90;brain_hold_max=300;brain_low=38;brain_high=72;"
      "partials=24;tilt=1;brightness=0.62;odd_even=-0.3;inharmonic=0.05;attack=10;release=30;cutoff=1000;"
      "resonance=0.45;depth=0.7;far_size=1.6;far_decay=26;far_highcut=6000;air=0.05;near_mix=0.18;width=1.1;"
      "master_gain=-8;env1_mode=Loop;env1_time=14;env1_depth=1",
      nullptr, nullptr, nullptr,
      "env1>cutoff:0.50;env1>brightness:0.26;env1>resonance:0.18;slide>inharmonic:0.386:u",
      "0:0:0.2/1:0.6:-0.3/2:-0.35:0.4/3:0.8:0/4:-0.5:-0.2/5:0.9:0.3/6:0.15:0/7:-0.75:-0.4/8:0.55:0.2/9:-0.25:0/10:0.85:-0.3/11:-0.45:0.4/12:0.65:0/13:-0.6:-0.2/14:0.4:0.3/15:0:0!l0-14" },

    { "Dwelling Curve",
      "brain_density=4;brain_rate=35;brain_hold_min=60;brain_hold_max=200;partials=16;tilt=1.4;brightness=0.45;"
      "odd_even=-0.22;attack=12;release=35;cutoff=1200;depth=0.8;far_size=2.5;far_decay=38;far_highcut=3000;"
      "air=0.16;sub_level=0.28;near_mix=0.2;master_gain=-11;env1_mode=Loop;env1_time=26;env1_depth=0.9;"
      "env2_mode=Loop;env2_time=26;env2_depth=0.9;far_width=0.487",
      nullptr, nullptr, nullptr,
      "env1>cutoff:0.30;env2>far_size:0.26;wheel>filter_fold:0.233:u;pressure>brightness:0.259:u",
      "0:0:0.9/1:1:0.9/3:-0.7:0.9/5:0:0!l0-3~0:0:-0.9/1:1:-0.9/3:-0.7:-0.9/5:0:0!l0-3" },
};
}

namespace {
#include "CosmosPresets.inc"
#include "ZPlanePresets.inc"
#include "StrikePresets.inc"
}



int numCosmosPresets() { return static_cast<int>(sizeof(kCosmosPresets) / sizeof(kCosmosPresets[0])); }
int cosmosPresetCategory(int i) { return (i >= 0 && i < numCosmosPresets()) ? kCosmosPresetCategory[i] : 255; }
int numCosmosPresetFamilies() { return static_cast<int>(sizeof(kCosmosPresetsFamilyNames) / sizeof(kCosmosPresetsFamilyNames[0])); }
const char* cosmosPresetFamily(int f) { return (f >= 0 && f < numCosmosPresetFamilies()) ? kCosmosPresetsFamilyNames[f] : ""; }

int numZPresets() { return static_cast<int>(sizeof(kZPresets) / sizeof(kZPresets[0])); }
const Preset& zPreset(int index)
{
    const int n = numZPresets();
    if (index < 0 || index >= n) index = 0;
    return kZPresets[index];
}
int zPresetCategory(int i) { return (i >= 0 && i < numZPresets()) ? kZPresetCategory[i] : 255; }

int numStrikePresets() { return static_cast<int>(sizeof(kStrikePresets) / sizeof(kStrikePresets[0])); }
const Preset& strikePreset(int index)
{
    const int n = numStrikePresets();
    if (index < 0 || index >= n) index = 0;
    return kStrikePresets[index];
}
int strikePresetCategory(int i) { return (i >= 0 && i < numStrikePresets()) ? kStrikePresetCategory[i] : 255; }
int numStrikePresetFamilies() { return static_cast<int>(sizeof(kStrikePresetsFamilyNames) / sizeof(kStrikePresetsFamilyNames[0])); }
const char* strikePresetFamily(int f) { return (f >= 0 && f < numStrikePresetFamilies()) ? kStrikePresetsFamilyNames[f] : ""; }
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
