#include "ambient/Help.h"
#include <cstring>
#include <string>

namespace ambient {

namespace {

// One entry per parameter key. The slot, LFO, envelope and delay families share their texts: a
// key like "src2_pos" is looked up as "srcN_pos", "lfo3_rate" as "lfoN_rate", "dly2_mix" as
// "dly_mix" -- so the table stays readable and the three slots can never drift apart.
struct HelpEntry { const char* key; const char* text; };

const HelpEntry kHelp[] = {
    { "master_gain", "Output level after the mid/side stage and before the soft clipper. There is no compressor anywhere in this instrument: what you hear is the dynamics of the drone." },

    // ---- sources (shared by the three slots)
    { "srcN_type", "What this slot is. Additive: a bank of partials shaped by tilt, brightness, odd/even and shimmer (in Source 1 the strand bank with unison, detune and stacks). Wavetable: a table of spectra, morphed by Position. FM: a two-operator pair. Texture: a granular player over a loaded clip. Noise: ten colours. Off: silent." },
    { "osc_level", "Level of Source 1 (the strand bank when Additive, otherwise the slot). Levels of the three sources mix before the filter." },
    { "srcN_level", "Level of this source. All three sources are normalised so the same Level means about the same loudness, whatever the type." },
    { "partials", "How many harmonics the bank generates, 1 to 32. Partials above Nyquist are simply not made, so nothing aliases at any pitch." },
    { "tilt", "Spectral tilt: partial h has amplitude h to the power of minus Tilt. 1 is a saw-like slope, 2 a triangle-like one, 0.3 nearly flat." },
    { "brightness", "A window that fades the upper partials out: at 1 all 32 sound, at 0 only the fundamental. This is what Bloom opens over time." },
    { "odd_even", "Weight between odd and even harmonics. Negative thins the odd ones, positive the even ones; +1 is a hollow, square-like spectrum." },
    { "inharmonic", "Stretches the partials away from whole-number ratios like a stiff string or a bell; 0 is exactly harmonic." },
    { "shimmer", "Each partial has its own slow random drift of amplitude. Shimmer is how deep; this is the breathing of a Rich drone." },
    { "shimmer_rate", "How fast the partials' amplitudes drift. Slow means a spectrum that changes over a minute, fast a flickering one." },
    { "srcN_partials", "How many harmonics this slot's additive bank generates, 1 to 32." },
    { "srcN_tilt", "Spectral tilt of this slot's additive bank: partial h has amplitude h to the power of minus Tilt." },
    { "srcN_bright", "Brightness window of this slot's additive bank: fades the upper partials out." },
    { "srcN_odd_even", "Odd/even weight of this slot's additive bank; +1 hollow, -1 without the odd harmonics above the fundamental." },
    { "srcN_inharmonic", "Inharmonic stretch of this slot's additive bank, 0 = harmonic." },
    { "srcN_shimmer", "Depth of the per-partial amplitude drift in this slot's additive bank." },
    { "srcN_shimmer_rate", "Rate of the per-partial amplitude drift in this slot's additive bank." },
    { "srcN_octave", "Transposes the source by octaves. In Source 1 it moves the whole strand bank too." },
    { "srcN_ratio", "A just ratio to the note (1/1 .. 2/1): 3/2 puts the source a pure fifth up, 7/4 a harmonic seventh. Stays in tune with the scale because it is a ratio, not semitones." },
    { "srcN_pan", "Where the source sits, left to right. For Source 1 in Additive it shifts the strand bank's wandering centre." },
    { "srcN_table", "Which spectrum table the Wavetable type reads: five built in, or User for a table loaded with the button (Serum/Vital layout, 2048-sample frames)." },
    { "srcN_pos", "Wavetable: the frame position, morphing between frames. Texture: where in the clip the grains start. Noise: the band centre or the colour's character." },
    { "srcN_pos_drift", "How far Position wanders on its own slow random curve. For FM it wanders the index instead." },
    { "srcN_fm_ratio", "FM: the modulator's frequency as a multiple of the carrier. Whole numbers are harmonic, fractions clangorous." },
    { "srcN_fm_index", "FM: how deep the modulator bends the carrier. Automatically reduced on high notes so nothing aliases." },
    { "srcN_grain", "Texture: the length of one grain in milliseconds. Short grains smear the clip into a texture, long ones keep its identity." },
    { "srcN_density", "Texture: how many grains start per second. Noise Crackle: how many crackles." },
    { "srcN_density_sync", "Ties the grain (or crackle) rate to the tempo: one per chosen note value instead of Density per second." },
    { "srcN_follow", "Texture: Note pitches the clip to the played note (the clip is assumed recorded at its named pitch); Free plays it at its own speed. Noise Band/Wind: the band follows the note." },
    { "srcN_grains", "Texture: how many grains may sound at once, up to 64. More grains, denser and smoother; the level is normalised for the overlap." },
    { "srcN_spread", "Texture: scatters each grain's start point around Position, as a fraction of the clip. At 1 a grain may come from anywhere." },
    { "srcN_noise", "The noise colour: White, Pink (-3 dB/oct), Brown (-6), Blue (+3), Violet (+6), Grey (flat to the ear), Band (a resonant band at Position), Wind (a wandering band), Crackle (sparse impulses), Digital (sample-and-hold)." },
    { "srcN_noise_q", "Width of the Band and Wind colours: 0 wide open, 1 a whistle." },
    { "srcN_drift", "A slow, independent pitch drift of this source in cents. Three sources on just ratios each drifting on their own curve beat like real instruments in a changing room, never symmetrically." },

    // ---- strands (Source 1 additive only)
    { "strands", "How many detuned or stacked copies of the bank a voice plays, 1 to 6. Each has its own pitch drift and pan." },
    { "detune", "Spread of the strands in cents around the note. 0 with Stack Detune is a single beat-free bank." },
    { "drift", "Depth of each strand's slow random pitch drift in cents -- the tape-like wobble of an old analogue pad, but never a step." },
    { "drift_rate", "How fast the pitch drift moves. Very slow is a drone that leans; fast is vibrato-like." },
    { "spread", "Stereo spread of the strands around the voice's wandering centre." },
    { "bloom", "How much of the brightness is held back at the start of a note and opened over Bloom Time -- a spectrum that blossoms." },
    { "bloom_time", "Seconds the bloom takes to open fully, with a gentle start." },
    { "stack", "Places the strands on pure ratios instead of detuning them: octaves, fifths, a just major or minor chord, seventh, harmonics or subharmonics -- one key becomes a just chord." },
    { "rate_wander", "Every voice's drift, shimmer and breath rates themselves wander by up to an octave on a slow curve, so five minutes never look like the five before." },
    { "freeze", "Holds the spectrum and pitch still: shimmer, pitch drift, breath and bloom stop moving. The envelopes and effects keep their own time." },

    // ---- foundation
    { "sub_level", "Level of the sub voice, a sine (with Tone, a little more) one or two octaves under the brain's root, mono, injected after the mid/side stage so Bass Mono cannot thin it." },
    { "sub_octave", "How far under the root the sub sits." },
    { "sub_glide", "Seconds the sub takes to slide to a new root, in the log domain." },
    { "sub_binaural", "Offsets the sub's left and right frequencies by this many hertz -- a binaural beat inside the bass." },
    { "sub_tone", "Adds a little second and third harmonic to the sub so it is audible on small speakers." },
    { "sub_source", "Root follows the brain's root. Difference follows the combination tone of the two lowest sounding voices (their frequency difference), folded into the sub's octave -- the ghost bass of a just chord." },
    { "pad_low_cut", "Partials below this frequency fall away at 12 dB per octave, leaving the bottom to the sub and keeping wide pads out of the sub's register." },

    // ---- air
    { "air", "Level of the breath layer: filtered noise inside every voice, following its pitch and its distance." },
    { "air_color", "Centre of the air band as a multiple of the note's fundamental." },
    { "air_q", "Narrowness of the air band. High Q is a whistling resonance, low a broad hiss." },
    { "air_mode", "Band: one wandering band pass. Ghost: the noise through six sharp resonators on the note's harmonics 1 2 3 5 7 9, so the harmony is filtered out of the chaos." },

    // ---- envelope
    { "attack", "Seconds the voice takes to swell in. Minutes are allowed." },
    { "decay", "Seconds from the peak down to the sustain level." },
    { "sustain", "Level held while the note is down (the brain holds its notes for Hold Min .. Hold Max)." },
    { "release", "Seconds the voice takes to fade after the note ends -- up to two minutes." },

    // ---- filter
    { "filter_on", "Switches the voice filter in or out. Off leaves the sources to the z-plane alone (or dry, if that is off too)." },
    { "filter_model", "The filter's character behind the same knobs: LP 6/12/24 low passes, HP 12, BP 12, Notch, Peak (a bell), Ladder (four-pole with saturating feedback), Comb (tuned to Cutoff, a resonating body)." },
    { "cutoff", "The filter's frequency. Key Track, Env Amount, Drift and the voice's distance move it from here." },
    { "resonance", "Emphasis at the cutoff. On the Ladder it self-oscillates near the top; on the Comb it deepens the dips between the peaks." },
    { "filter_env", "How far the amplitude envelope opens the filter, in octaves times four. Negative closes it as the note swells." },
    { "filter_drift", "Depth of the cutoff's slow random wander, in octaves times two." },
    { "keytrack", "How much the cutoff follows the note: 1 keeps the same partials in the passband on every key." },
    { "filter_drive", "Soft saturation ahead of the filter, level-compensated: adds harmonics, not loudness." },

    // ---- z-plane
    { "z_mode", "Off, or the z-plane filter in Series (it hears the voice filter, Mix is its dry/wet) or Replace (it is the only filter)." },
    { "z_route", "With both filters on: Series puts the z-plane after the voice filter; Parallel feeds both the dry sum and Mix balances them." },
    { "z_shape", "One of sixteen frame sets, four frames on the corners of a square: vowel morphs, bell clusters, resonator banks, the sweeps." },
    { "z_x", "The point's horizontal position in the frame square; the filter interpolates the four corners' poles and zeros." },
    { "z_y", "The point's vertical position in the frame square." },
    { "z_rate", "How fast the point wanders around (X, Y) on two slow random curves." },
    { "z_depth", "How far the point wanders." },
    { "z_res", "Narrows every section's bandwidth: 1 is a quarter of the frame's, 0 double." },
    { "z_keytrack", "Moves the whole frame with the note's pitch (1 = fully)." },
    { "z_mix", "Dry/wet of the z-plane stage -- or, in Parallel, the balance between the voice filter and the z-plane." },

    // ---- space
    { "depth", "How deep the brain places its notes: 40 % land close, 60 % deep into the background, scaled by this. Per unit distance a voice loses 2.5 octaves of cutoff and 6 dB, and is heard only through the far reverb." },
    { "keys_depth", "The plane MIDI keys are played on (0 = at the ear, 1 = infinite background). The brain has Depth." },
    { "pan_drift", "How far each voice's centre wanders left and right on its own slow curve." },
    { "itd", "Time Width: the interaural time difference, up to 0.65 ms, applied to the far ear from the voice's pan -- width from time, not from level." },
    { "arc", "The hour-scale arc: one very slow drift that leans on density, brightness and depth, so the whole night has a shape." },
    { "arc_period", "Minutes the arc takes for one swing." },
    { "arc_sync", "Ties the arc's period to the tempo, in bars, instead of minutes." },
    { "presence", "A broad bell at 2-5 kHz on the near plane only (up to 6 dB), gone on the far plane -- foreground articulation the way Rich carves it." },
    { "breath", "Every voice's distance itself wanders by up to this much of the plane (0.35 at 1): the room breathes." },
    { "breath_rate", "How fast the distances breathe. 0.03 Hz is half a minute per swing." },
    { "phase_width", "Two all-pass stages per ear whose corners drift in opposite directions: the phase between left and right changes slowly and the room seems to change size rather than the sound to move. Off at 0." },
    { "phase_rate", "How fast the phase field drifts. Keep it slow: the effect is space, not tremolo." },
    { "doppler", "As a voice breathes closer or further away its pitch bends a little, the way a moving source does. A few cents at most; the ear reads approach and retreat from it." },

    // ---- ensemble
    { "ens_mix", "Amount of the ensemble (a slow stereo chorus) on the near bus." },
    { "ens_depth", "Modulation depth of the ensemble's delay lines." },
    { "ens_rate", "Speed of the ensemble's modulation." },
    { "ensemble_sync", "Ties the ensemble's rate to the tempo." },

    // ---- delays
    { "dly_time_l", "Left delay time in seconds. The two sides are independent: asymmetry is what makes the space wide." },
    { "dly_time_r", "Right delay time in seconds." },
    { "dly_sync_l", "Ties the left delay time to a note value at the current tempo; the knob is then ignored." },
    { "dly_sync_r", "Ties the right delay time to a note value at the current tempo." },
    { "dly_feedback", "How much of the delay returns into itself. Near 1 the echoes last for minutes." },
    { "dly_cross", "How much the left echo feeds the right and vice versa -- ping-pong at 1." },
    { "dly_damp", "Low-pass in the feedback path: each repeat darker than the last." },
    { "dly_absorb", "Absorption: with Absorb up, the loop also loses its low end and its high cut moves down as Feedback rises, so long echoes drown into a warm fog instead of merely getting quieter." },
    { "dly_mix", "Level of the echoes on the near (dry) bus." },
    { "dly_to_far", "Level of the echoes sent into the far reverb instead: echoes that recede into the background." },

    // ---- reverbs
    { "near_mix", "Amount of the small foreground room around the dry voices." },
    { "near_decay", "Decay of the near room in seconds." },
    { "near_damp", "High-frequency damping of the near room." },
    { "far_level", "Level of the far reverb, the infinite background that every distant voice is heard through." },
    { "far_size", "Size of the far reverb's space (its delay lines)." },
    { "far_decay", "Decay time of the far reverb in seconds -- tens of seconds are the point." },
    { "far_damp", "High-frequency damping inside the far reverb: darker with every reflection." },
    { "far_predelay", "Milliseconds before the far reverb starts, separating it from the near plane." },
    { "far_asym", "Stretches the right half of the reverb's lines and delays its output slightly, so the two ears hear different reflections." },
    { "far_highcut", "Low-pass on the far reverb's tail." },
    { "far_freeze", "Holds the far reverb's tail forever: an instant infinite pad of whatever was in it." },
    { "far_unmask", "The background steps aside for the foreground, band by band: while a voice sounds, the far reverb loses that part of the spectrum and lets it back in over a second when the voice goes. A dense pad keeps its own notes audible instead of swallowing them." },
    { "body_level", "Level of the resonating body: twelve modes tuned to the root, fed from the finished mix and returned to it. Not a reverb -- a reverb is a statistical tail, this is a handful of pitched resonances, which is the difference the ear hears between a room and an instrument." },
    { "body_material", "Which set of mode ratios: Wood (a soundboard's irregular low modes), Plate (the stretched series of flat metal), Bell (hum, prime, tierce, quint, nominal), String (harmonic with a little stiffness)." },
    { "body_pitch", "What the body is tuned to, as a multiple of the brain's root. 1 is the root itself; 0.5 puts the body an octave below the music, which is what a large soundboard does." },
    { "body_decay", "How long the lowest mode rings. The higher modes die away faster, at a rate that belongs to the material." },
    { "body_tone", "Tilts the modes: at 0 only the low ones speak (dark and wooden), at 1 the high ones are as loud (bright and metallic)." },
    { "body_spread", "How far the modes are scattered across the stereo field. A body is not a point source; this is what makes it read as width rather than as movement." },
    { "patina", "The master's age: tape wow, the highs a worn machine no longer carries, a noise floor under the music, gentle saturation. Off at 0, and then not computed at all. Most of what separates a recording from a render." },
    { "patina_wow", "Depth of the wow and flutter: a wavering pitch, slow and irregular with a little 6 Hz on top." },
    { "patina_hiss", "The noise floor, a touch louder when the tape is carrying more (that is modulation noise, and it is what makes a floor sound like tape rather than like dither)." },
    { "patina_age", "How much top end the machine has lost: from untouched down to about 4 kHz." },
    { "far_rotate", "The whole background slowly turns: the far field's left and right rotate into each other on a minute-scale curve. Depth of the turn." },
    { "blur_mix", "A spectral smear on the near bus itself, ahead of the effects: every attack is wiped into texture, notes flow into each other. Mix of the blurred signal (latency 43 ms on the blurred part)." },
    { "blur_smear", "How much the blur smears: 0 follows the input closely, 1 is a spectral freeze that only lets new energy in slowly." },

    // ---- feedback
    { "fb_bus", "The mixed output returns, low-passed and saturated, into the near bus before the filters and effects -- throttled by the output level so it hisses and holds instead of running away." },
    { "fb_fm", "The returned output phase-modulates every partial of every voice (To Pitch): the sound bends itself." },
    { "fb_tone", "Low-pass on the feedback path." },
    { "fb_drive", "Saturation in the feedback path." },
    { "fb_tape", "Tape in the loop: asymmetric saturation, wow and flutter, a level-dependent noise floor." },

    // ---- room
    { "room_level", "Level of the convolution room, an extra reverb from a loaded impulse response (or the built-in dark hall), in parallel on the far plane." },
    { "room_source", "What the room reverberates: the far sends (before the far reverb) or the finished near bus." },
    { "room_predelay", "Milliseconds before the room's response starts." },
    { "room_highcut", "Low-pass on the room's tail." },

    // ---- cosmos
    { "cosmos_send", "How much of the near bus goes into the Cosmos path (frequency shifter, resonator, vowel, nebula). The dry signal is untouched; Cosmos is additive." },
    { "cosmos_shift", "Frequency shift in hertz: every partial moves by the same amount, so the harmonic series becomes inharmonic. The right channel shifts 3 % less." },
    { "cosmos_shift_drift", "Lets the shift wander slowly around its value." },
    { "cosmos_res", "Level of the comb resonator tuned to the brain's root." },
    { "cosmos_res_pitch", "The resonator's pitch as a multiple of the root." },
    { "cosmos_res_fb", "Resonator feedback: how long it rings." },
    { "cosmos_vowel", "The vowel filter's position, a-e-i-o-u." },
    { "cosmos_vowel_rate", "How fast the vowel wanders." },
    { "cosmos_nebula", "Mix of the Nebula: a spectral smear that scatters the phases of the spectrum, a frozen cloud at full Smear." },
    { "cosmos_smear", "How much the Nebula smears; 1 is a spectral freeze." },
    { "cosmos_shimmer", "Feeds the far reverb's pitch-shifted previous block back into its input: the rising cloud. Regulated by the reverb level so it cannot run into the clipper." },
    { "cosmos_shimmer_pitch", "The shimmer's transposition: an octave up, a fifth, a fourth, an octave and a fifth, an octave down, two up." },
    { "cosmos_return", "How much of the Cosmos path returns to the near plane." },
    { "cosmos_to_far", "How much of the Cosmos path goes into the far reverb." },

    // ---- cloud
    { "cloud_send", "How much of the recent foreground the granular cloud takes." },
    { "cloud_density", "Grains per second in the cloud." },
    { "cloud_sync", "Ties the cloud's grain rate to the tempo." },
    { "cloud_size", "Length of the cloud's grains in milliseconds." },
    { "cloud_pitch", "Transposition of the cloud's grains: octaves and fifths, the cloud a register above the voices." },
    { "cloud_spray", "How far back in time (seconds) the grains are taken from." },
    { "cloud_level", "Level of the cloud, dropped into the far reverb." },

    // ---- master
    { "bass_mono", "Below this frequency the side channel is removed: a mono low end, the foundation of a wide picture." },
    { "side_air", "A broad bell at 3 kHz on the side channel, up to +6 dB: air in the width." },
    { "width", "Stereo width: 1 as recorded, above widens, 0 mono." },

    // ---- cluster brain
    { "brain_on", "The conductor: chooses notes from the scale, places them on the planes, holds them for minutes and lets them go. Off, only your keys play." },
    { "brain_density", "How many notes the brain keeps sounding at once." },
    { "brain_rate", "Seconds between the brain's decisions (a new note or a release), on average." },
    { "brain_sync", "Ties the brain's decision rate to the tempo -- a decision every so many bars." },
    { "brain_hold_min", "Shortest time the brain holds a note, in seconds." },
    { "brain_hold_max", "Longest time the brain holds a note, in seconds." },
    { "brain_low", "Lowest MIDI note the brain may choose." },
    { "brain_high", "Highest MIDI note the brain may choose." },
    { "brain_consonance", "How strongly the brain prefers consonant intervals to the notes already sounding. 1 is pure, 0 anything goes." },
    { "brain_wander", "How readily the brain's root moves to a new centre over time." },

    // ---- tuning
    { "scale", "The tuning: just scales, 12-TET, Bohlen-Pierce, or User for a loaded Scala file. Every note the brain or the keys play comes from here." },
    { "keymap", "Snap: the 12 keys of an octave snap to the nearest scale degree. Consecutive: each key is the next degree, whatever the scale's step count." },
    { "root", "The key's root note. The brain's root lives an octave below it." },
    { "ref_pitch", "Reference pitch of A4 in hertz." },
    { "seed", "Seed of the brain's randomness: the same seed replays the same decisions." },
    { "hold", "Keys latch: a played key stays until Hold is switched off." },
    { "purity", "Blends every note between 12-TET (0) and the chosen scale (1) in the log domain -- the beating locks in as you turn it up." },
    { "purity_drift", "Lets the purity wander, so the tuning locks in and loosens over minutes." },
    { "purity_rate", "How fast the purity wanders." },
    { "tide", "The whole instrument's pitch leans by up to this many cents on a very slow curve, like a tape machine over an evening. Sub and voices move together, so the harmony stays." },
    { "tide_period", "Minutes for one swing of the tide." },
    { "strike_level", "A short plucked or struck impulse at note-on on the near plane, whatever the voice's distance: the intimate contrast that makes the background vast. Level; 0 is off." },
    { "strike_type", "String: a plucked string at the note (Karplus-Strong). Wood: a short, dull knock two octaves up. Metal: the string with an all-pass in its loop, stretched and clangorous." },
    { "strike_decay", "Seconds the strike rings." },
    { "strike_damp", "Brightness loss per round of the string: 0 bright and long, 1 dull and short." },
    { "strike_who", "Keys: only your notes strike. Keys + Brain: the conductor's notes too." },
    { "portamento", "Seconds a new key glides from the last one." },
    { "porta_gravity", "Slows the glide near consonant ratios to the root, so a slide clicks into the harmonic nodes on the way." },

    // ---- coherence
    { "coherence", "Coupling of four slow Kuramoto oscillators: at 0 they run free, near 1 they fall into step. Their sines are the KURA modulation sources." },
    { "coherence_depth", "How much the ring moves brightness, depth, pan and the z-plane point on its own." },
    { "coherence_rate", "Base speed of the ring." },

    // ---- LFOs (shared)
    { "lfoN_shape", "The waveform: Sine, Triangle, Ramp Up, Ramp Down, a soft Square, Random (smooth), Steps (held random), or Table -- a frame of the user wavetable as a shape, which makes any drawn curve an LFO." },
    { "lfoN_rate", "Cycles per second, from one in twenty minutes to 20 Hz. Ignored while Sync is set." },
    { "lfoN_phase", "Where in the cycle the shape starts (0..1)." },
    { "lfoN_depth", "Scales the LFO's output; every matrix route scales it again." },
    { "lfoN_mode", "Global: one phase for the whole instrument, every voice breathes together. Voice: each voice runs its own copy. Retrigger: each voice restarts from Phase." },
    { "lfoN_table", "Which frame of the user wavetable the Table shape reads." },
    { "lfoN_sync", "One cycle per note value at the current tempo; the phase follows the beat position, so it stays on the grid wherever the transport jumps." },

    // ---- envelopes (shared)
    { "envN_mode", "One Shot plays the shape once per note. Loop repeats it between its loop points. Sustain Loop loops while the note is held, then finishes." },
    { "envN_time", "Stretches the whole shape: 0.05 is twenty times faster, 20 twenty times slower. Ignored while Sync is set." },
    { "envN_depth", "Scales the envelope's output before the matrix." },
    { "envN_sync", "The whole shape spans one note value at the current tempo." },

    // ---- morph, macros, map, route
    { "morph_active", "Switches the morph on: the whole instrument is the blend of snapshots A and B at Position." },
    { "morph", "Where between A (0) and B (1) the instrument is. Continuous parameters interpolate in their own curve, choices flip halfway." },
    { "morph_glide", "Seconds the instrument takes to follow a new position -- up to fifteen minutes, so one gesture can carry a piece." },
    { "macro_a", "Macro A -- Space: one knob, several parameters, mapped in the gesture table (Gestures...)." },
    { "macro_b", "Macro B -- Alien." },
    { "macro_c", "Macro C -- Motion." },
    { "macro_d", "Macro D -- Bloom." },
    { "macro_e", "Macro E -- Density." },
    { "macro_f", "Macro F -- Distance." },
    { "macro_g", "Macro G -- Evolution." },
    { "macro_h", "Macro H -- Air." },
    { "inertia", "Every knob glides to its value with this time constant, the analogue slew: even a knob torn open arrives slowly. Modulation is not slewed." },
    { "map_active", "Plays the blend of the presets around the map cursor (Browse > Map) instead of the live parameters." },
    { "map_x", "The map cursor's horizontal position." },
    { "map_y", "The map cursor's vertical position." },
    { "map_radius", "How far around the cursor presets contribute to the blend." },
    { "route_active", "Walks the route of waypoints over the map (Browse > Map)." },
    { "route_speed", "Speed of the route, 1 = as written." },
    { "route_loop", "Starts the route again when it ends." },

    // ---- clock
    { "clock_source", "Where the tempo comes from: Internal (Tempo and Run here), Host (the DAW's play head, if there is one), or MIDI clock at the input. Host and MIDI fall back to Internal when nothing arrives." },
    { "tempo", "The internal clock's tempo in beats per minute. Every Sync choice in the instrument reads it (or the host's / MIDI's tempo instead)." },
    { "clock_run", "Runs the internal clock. Off, synced LFOs hold their phase." },
};

// Normalises a key to its family template: src2_pos -> srcN_pos, lfo3_rate -> lfoN_rate,
// env5_depth -> envN_depth, dly2_mix -> dly_mix. Returns whether anything changed.
std::string familyKey(const char* key)
{
    std::string k(key);
    auto digitAt = [&](size_t i) { return i < k.size() && k[i] >= '1' && k[i] <= '9'; };
    if (k.rfind("src", 0) == 0 && digitAt(3) && k[4] == '_') { k[3] = 'N'; return k; }
    if (k.rfind("lfo", 0) == 0 && digitAt(3) && k[4] == '_') { k[3] = 'N'; return k; }
    if (k.rfind("env", 0) == 0 && digitAt(3) && k[4] == '_') { k[3] = 'N'; return k; }
    if (k.rfind("dly2_", 0) == 0) return "dly" + k.substr(4);
    return k;
}

struct HelpCache {
    const char* text[kNumParams];
    HelpCache()
    {
        for (const ParamDesc& d : paramTable()) {
            const char* found = "";
            const std::string fam = familyKey(d.key);
            for (const HelpEntry& e : kHelp)
                if (std::strcmp(e.key, d.key) == 0 || fam == e.key) { found = e.text; break; }
            text[static_cast<int>(d.id)] = found;
        }
    }
};

const HelpCache& cache() { static const HelpCache c; return c; }

// ---------------------------------------------------------------- the manual

struct Topic { const char* title; const char* text; };

const Topic kTopics[] = {
    { "Overview and signal flow",
R"(AmbientSynth is a drone instrument for slowly breathing clusters: just intonation, additive banks whose partials live their own lives, envelopes measured in minutes, a conductor (the Cluster Brain) that can play a whole night by itself, and a spatial model that treats depth as a landscape rather than an effect.

SIGNAL FLOW

  Cluster Brain / MIDI keys
        each note gets a DISTANCE: 0 at the ear, 1 the infinite background
  Voice (x16)
        Source 1 + Source 2 + Source 3   (additive bank, wavetable, FM, texture grains, noise)
        + Air (filtered noise on the note)
        -> Filter (nine models) and/or Z-plane filter, in series or parallel
        -> Envelope, x (1 - distance/2)
        -> interaural time difference from the pan (the far ear hears later)
        -> NEAR bus by cos(distance), FAR bus by sin(distance)
  NEAR:  Ensemble -> Delay -> Delay 2 -> (+ Near reverb)   the dry, bright foreground
         "to far" from both delays and the Cloud send go into the background
         Cosmos (send / return): shifter, resonator, vowel, nebula -- added, never replacing
  FAR:   Far reverb (dark, wide, asymmetric, minutes long) + Room (convolution) + Shimmer loop
  Mid/side (bass mono, side air, width) -> Master -> soft clip.   No compressor anywhere.
  Feedback: the finished mix can return into the near bus and/or bend every partial's phase.

A voice's plane decides everything at once: how bright it is (2.5 octaves of cutoff per unit of distance), how loud (-6 dB), how dry (the far plane is heard only through the reverb), and how present (the presence bell lives on the near plane). The brain places 40 % of its notes close and 60 % deep; your keys sit at Keys Depth.

THE PAGE

Everything is on one page and nothing scrolls; drag the window corner to zoom. Rows whose sections are of a kind page through tabs: SOURCE 1 / STRANDS / SOURCE 2 / SOURCE 3, FILTER / Z-PLANE / AMP ENV, the effect pairs, BRAIN / TUNING / COHERENCE / CLOCK, MORPH / MACROS. The room a row's knobs leave is a live display drawn from the engine's own numbers. The strip along the bottom holds the modulators. Point at any control and this header line tells you what it does.)" },

    { "Sources",
R"(Every voice has three equal source slots; their levels mix before the filter. Each slot has a Type:

ADDITIVE  A bank of up to 32 partials. Partial h has amplitude h^-Tilt, the Brightness window fades the upper ones out, Odd/Even weights the two families, Inharmonic stretches the series like a stiff string, and Shimmer lets every partial drift in level on its own slow curve -- the breathing. Partials above Nyquist are not generated, so nothing aliases. In Source 1, Additive is the STRAND BANK: up to six copies of the bank, detuned (Detune, Drift) or placed on pure ratios (Stack: octaves, fifths, a just major or minor, seventh, harmonics, subharmonics -- one key becomes a just chord), fanned out in stereo (Spread), with Bloom opening the brightness over Bloom Time and Rate Wander slowly varying every movement rate. In Source 2 and 3, Additive is a single bank with its own Partials, Tilt, Bright, Odd/Even, Inharmonic and Shimmer.

WAVETABLE  Not a table of samples but a table of SPECTRA: 32 partial amplitudes per frame, up to 64 frames; Position morphs between frames and Pos Drift wanders it. Five built-in tables (Classic: sine to pulse; Organ; Vocal a-e-i-o-u; Glass; Metal) and User, loaded from a WAV in the Serum/Vital layout (2048-sample frames) with the button. Alias-free like the bank, and the same partials-based tricks (presence, low cut, feedback FM) apply.

FM  A two-operator pair: carrier at the slot pitch, modulator at FM Ratio, FM Index up to 8, reduced automatically on high notes. Pos Drift wanders the index.

TEXTURE  A granular player over a loaded clip (Texture... button, or the preset's own sample): up to 64 grains (Grains), Grain length, Density per second (or per note value with Sync), starting around Position with Spread, pitched to the note (Pitch = Note; the clip's pitch comes from its file name, e.g. "_A3") or played free. The display shows the grains reading the clip.

NOISE  Ten colours: White, Pink, Brown, Blue, Violet, Grey, Band (a resonant band at Position, Q from Noise Q, tracking the note with Pitch = Note), Wind (a wandering band), Crackle (sparse impulses at Density), Digital (sample-and-hold at a rate from Position). Levels are matched so a colour change does not change the loudness.

Every slot also has Octave, a just Ratio to the note (3/2 a fifth up, 7/4 a harmonic seventh -- ratios, so the scale stays pure) and Pan. In Source 1 these move the strand bank as well.)" },

    { "Filters and Z-plane",
R"(Two filters, each with its own switch, in series or in parallel.

THE VOICE FILTER (Filter section) has nine models behind the same knobs:
  LP 6      one pole, warm and gentle
  LP 12     the state-variable low pass -- the default, the one the instrument always had
  LP 24     two stages, steep
  HP 12     the opposite slope
  BP 12     a band, unity at the cutoff
  Notch     a hole swept through the harmonics
  Peak      a bell of up to +14 dB, narrower with Resonance
  Ladder    four one-poles with saturating feedback, self-oscillating near full Resonance
  Comb      a feedback comb tuned to Cutoff; Resonance deepens the dips -- on a cluster, a second resonating body
Cutoff is moved by Key Track (1 keeps the same partials in the passband on every key), Env Amount (the amplitude envelope, negative closes), Drift (a slow wander) and by the voice's distance (2.5 octaves darker on the far plane). Drive saturates ahead of the filter. On switches it out.

THE Z-PLANE FILTER (after the E-mu Morpheus idea): four filter frames sit on the corners of a square and a point (X, Y) inside it is a filter interpolated from all four -- on the pole and zero parameters, so every point is stable. Sixteen Shapes in six families (vowel morphs, bell clusters, resonator banks, sweeps); the point wanders at Rate by Depth; Resonance narrows every section; Key Track moves the frame with the note. Mode: Off, Series or Replace (the z-plane alone). Route: with both filters on, Series puts the z-plane after the voice filter (Mix is its dry/wet), Parallel feeds both the dry sum and Mix balances them.

The FILTER RESPONSE display draws the voice filter (in the voice colour), the z-plane (in the accent) and what a note actually meets after both, from the same maths the audio path uses.)" },

    { "Space, Air, Foundation",
R"(SPACE is the spatial model. Depth scales how deep the brain places its notes (40 % close, 60 % deep); Keys Depth is the plane your keys play on. Pan Drift wanders each voice's centre; Time Width is the interaural time difference (up to 0.65 ms, the far ear hears later) -- width from time, not level. Presence is a bell at 2-5 kHz on the near plane only, gone on the far plane. Breath lets every voice's distance itself wander (the room breathes) at Breath Rate. Arc is one very slow drift over Arc Period minutes that leans on density, brightness and depth, so a whole night has a shape; Arc Sync ties it to bars.

The STAGE display shows every sounding voice as a dot: left-right by pan, near-far by plane, size by envelope, brain notes in the voice colour, your keys in orange.

AIR is filtered noise inside each voice, at a multiple (Color) of the note's fundamental, with Q; in Ghost mode the noise runs through six sharp resonators on the harmonics 1 2 3 5 7 9, so the harmony is filtered out of the chaos.

FOUNDATION is a mono sub voice one or two octaves under the brain's root (or, with Source = Difference, on the combination tone of the two lowest voices -- the ghost bass of a just chord), gliding in the log domain over Glide seconds, with Binaural offsetting left and right by a few hertz and Tone adding a little harmonic content. It is injected after the mid/side stage so Bass Mono cannot thin it. Pad Low Cut takes the pads out of its register (12 dB/oct below the cut).)" },

    { "Effects: foreground and background",
R"(The near bus (the dry plane) runs through ENSEMBLE (a slow stereo chorus: Mix, Depth, Rate or Sync), DELAY and DELAY 2 in series (independent left and right times or note values, Feedback, Cross for ping-pong, Damping in the loop, Mix onto the near bus and To Far into the background: echoes that recede), and the NEAR REVERB (a small room: Mix, Decay, Damping).

The far bus is the background: FAR REVERB is an eight-line feedback network, 100 % wet, dark and wide -- Size, Decay (tens of seconds), Damping, Pre-Delay, Asymmetry (the right half stretched and delayed so the two ears hear different reflections), Tail Cut, and Freeze for an instant infinite pad. CLOUD takes grains of the recent foreground (Send, Density or Sync, Size, Spray back in time), transposes them (Pitch) and drops them into the far reverb. ROOM is a convolution reverb from a loaded impulse response (Impulse... button, or the preset's own; 200 generated responses ship with the library) or the built-in dark hall, hearing the far sends or the near bus, with Pre-Delay and Tail Cut.

FEEDBACK returns the finished mix: To Bus into the near bus before the filters and effects (throttled by the output level so it hisses and holds instead of running away), To Pitch as phase modulation of every partial (the sound bends itself), through Tone and Drive; Tape adds asymmetric saturation, wow and flutter and a level-dependent noise floor.

MASTER: Bass Mono removes the side channel below a frequency (a mono low end under a wide picture), Side Air lifts the side at 3 kHz, Width scales the stereo image; then the master gain and a soft clipper. There is no compressor.)" },

    { "Cosmos",
R"(Cosmos is a parallel path off the near bus -- Send in, Return to the near plane and To Far into the background -- that adds to the sound and never replaces it. In order:

FREQUENCY SHIFTER  Every partial moves by the same number of hertz (Shift), so a harmonic series becomes inharmonic; the right channel shifts 3 % less, which spreads the picture. Shift Drift wanders it.
RESONATOR  A comb tuned to the brain's root (Res Pitch as a multiple of it), ringing with Res Feedback, at level Resonator.
VOWEL  An a-e-i-o-u formant filter at Vowel, wandering at Vowel Rate.
NEBULA  A spectral smear: the spectrum's phases are scattered by Smear (1 is a spectral freeze); Nebula is its mix.

SHIMMER sits around the far reverb rather than in the Cosmos path: the reverb's previous block, pitch-shifted (Shimmer Pitch: an octave, a fifth, a fourth, an octave and a fifth, an octave down, two up), is fed back into its input -- the rising cloud. It is regulated by the reverb's level so it cannot run into the clipper.

The COSMOS RETURN display shows the spectrum of what the path hands back (nothing while Send is 0).)" },

    { "Conductor: brain, tuning, coherence, clock",
R"(CLUSTER BRAIN is the conductor. While Active it chooses notes from the scale between Lowest and Highest, prefers consonant intervals to what is sounding (Consonance), places each on a plane, keeps Density notes sounding, decides every Event Rate seconds (or every so many bars with Sync), holds each note between Hold Min and Hold Max, and lets its root Wander over time. Seed (in Tuning) makes a night reproducible. The lowest held MIDI key becomes the brain's root. The NOTES display is a roll of what it has played.

TUNING: Scale (just scales, 12-TET, Bohlen-Pierce, or a loaded Scala file), Key Map (snap the twelve keys to the nearest degrees, or walk the degrees consecutively), Root, A4 reference, Hold (keys latch). Purity blends every note between 12-TET and the scale in the log domain, and Purity Drift lets that blend wander at Purity Rate, so the beating locks in and loosens over minutes. Freeze holds every voice's spectrum and pitch still. Portamento glides a new key from the last one, and Gravity slows the glide near consonant ratios so it clicks into the harmonic nodes on the way.

COHERENCE: four slow Kuramoto oscillators coupled by Coherence (free at 0, in step near 1), moving brightness, depth, pan and the z-plane point by Depth at Rate. Their sines are also the KURA 1-4 modulation sources.

CLOCK: where the tempo comes from -- Internal (Tempo, Run), Host (the DAW's play head) or MIDI (MIDI clock at the input). See the topic "Clock and sync".)" },

    { "Modulation: LFOs, envelopes, matrix",
R"(The strip along the bottom holds every modulation source as a card: LFO 1-8, ENV 1-6, MACRO A-H, KURA 1-4, AMP (the voice's own envelope), NOTE, VELO, DIST (the voice's plane) and RAND (a random value per note). Its tabs edit the sources:

LFO  Eight free LFOs with Shape (Sine, Triangle, Ramp Up/Down, soft Square, Random, Steps, or Table -- a frame of the user wavetable as a shape, so any drawn curve is an LFO), Rate from one cycle in twenty minutes to 20 Hz or a note value (Sync), Phase, Depth, and Mode: Global (one phase for the instrument, every voice breathes together), Voice (each voice its own copy), Retrigger (each voice restarts from Phase). The editors show the shape with a running dot.

ENVELOPES  Six multi-segment envelopes: up to sixteen breakpoints with a curve per segment, an optional sustain point, an optional loop, edited as text "t:v:c/t:v:c/... !s2 !l1-3" and drawn. Mode One Shot / Loop / Sustain Loop, Time stretches the shape or Sync spans it over one note value, Depth scales it.

MATRIX  Up to 32 routes "source > target : depth [: via] [: u]". Depth is a fraction of the target's range (-1..1); via scales the depth by a second source (a macro, typically); u treats a bipolar source as 0..1. One source may drive as many targets as it likes.

ROUTING WITHOUT TYPING  Drag a card from the strip onto any knob: a route at a quarter of the range is added. Right-click a card to see and remove its routes; right-click a knob to see what drives it. A modulated knob wears a thin ring in its source's colour (LFOs turquoise, envelopes green, macros pink, coherence blue) and a second arc from its value to where the modulation is pushing it right now. Performance state (morph, macros, map, route, clock) can never be a target.)" },

    { "Morph, macros, perform, gestures",
R"(MORPH holds two full snapshots, A and B: pick a preset for each or capture the current state with A <- now / B <- now. Switch Active on and Position blends the whole instrument between the two worlds; Glide sets how long it takes to follow a new position -- up to fifteen minutes, so one gesture can carry a piece across a quarter of an hour. Continuous parameters interpolate in their own perceptual curve, integers round, choices flip halfway. Morph settings are never part of a preset.

MACROS A-H (Space, Alien, Motion, Bloom, Density, Distance, Evolution, Air) are one knob for several parameters each; the mapping table (Gestures...) says which, with range and smoothing. They are also modulation sources. Inertia is the analogue slew: every knob glides to its value with this time constant.

PERFORM (header button) shows only the eight macros and the morph, large, for playing a set; Record set / Play set log and replay everything you do, with time.

GESTURES: the same layer that will drive the Quest version listens to OSC (/ambient/hand/L|R, /head, /param, /gesture, /note, /preset, /morph, port 9000): hand height, distance, pinch. Calibrate learns your range (hands together and apart, low and high, near and far, for six seconds). The right pinch is the clutch: mappings act only while it is engaged.)" },

    { "Presets, packs, browser, map, routes, sets",
R"(Presets come in two independent layers: the Sound box (voices, space, effects, brain, tuning -- 168 built in) and the Cosmos box (32 presets for the Cosmos section only). Loading one never touches the other; in a DAW the full presets are the programs. Save... / Load... store the whole state as an .ambientsynth file.

PACKS: plain text files (*.ambientpack, one preset per line) that may name a sample, a wavetable, an impulse response, a modulation matrix and envelope shapes of their own. Put them in Documents/AmbientSynth/Packs or point AMBIENT_PACKS at a folder; they appear everywhere the built-in presets do, each pack as a family. The library that ships alongside has 5000 presets in 25 packs, 1200 samples, 608 wavetables and 200 impulse responses.

BROWSE (header button): Columns narrows the list by Family, Character (dark, bright, tonal, noisy, wide, bass), Motion (calm, moving, dense, sparse) and Features, with search, sort and favourites -- every preset was measured by rendering it, not tagged by hand. Map shows all presets as points clustered by what they sound like; click one to load it, or switch on Map blend and drag the cursor: the synth glides to the blend of the presets around it, so the space between two presets is playable. A ROUTE is a list of waypoints (presets or map positions with travel and hold times) the synth walks by itself: twelve route presets of 20-40 minutes, or your own from the cursor; Speed and Loop as you like.

SETS: Record set on the Perform page logs every knob, macro, route step and note with its time into an .ambientset file; Play set replays it.)" },

    { "Clock and sync",
R"(Nothing in this instrument needs a clock to make sound, but the moment it plays with other machines every rate wants to sit on the grid. Every rate that can has a SYNC choice next to its free knob: the eight LFOs (one cycle per note value, the phase following the beat position so it stays on the grid wherever the transport jumps), the six envelopes (the whole shape spans one value), both delays' left and right times, the ensemble rate, the cloud's grain rate, the brain's decision rate, the arc period, and each source's grain density. Free means the knob rules. The values run from 64 bars to 1/32, with dotted (D) and triplet (T) values.

WHERE THE TEMPO COMES FROM (Conductor > CLOCK > Source):
  Internal  The Tempo knob, counting beats while Run is on. This is the standalone's own clock.
  Host      The DAW's play head: its tempo, position and transport. In the standalone there is none, and the engine falls back to Internal.
  MIDI      MIDI clock at the MIDI input (24 ticks a quarter, Start / Continue / Stop). The tempo settles over one beat's worth of ticks; two seconds without a tick and the engine falls back.
The header shows the tempo and the bar the engine is following. The clock's settings are performance state like the morph: no preset changes your tempo.)" },

    { "MIDI, OSC, files",
R"(MIDI: notes play voices on the Keys Depth plane; the lowest held key becomes the brain's root. Right-click any control for MIDI Learn, then move a controller; right-click again to clear. The mapping is saved with the state. MIDI clock and Start/Stop/Continue drive the clock when its Source is MIDI. In the standalone, pick the MIDI input in Options > Audio/MIDI settings.

OSC on UDP port 9000: /ambient/param/<key> <value>, /ambient/paramn/<key> <0..1>, /ambient/note <n> <vel>, /ambient/preset <index>, /ambient/sound and /ambient/cosmos <index>, /ambient/morph <0..1>, /ambient/hand/L and /R <x y z pinch>, /ambient/head, /ambient/gesture, /ambient/calibrate. A second instance simply reports the port as taken.

FILES: .ambientsynth (whole state), .ambientpack (preset packs), .ambientset (recorded sets), Scala .scl (Tuning > Load Scala...), WAV wavetables (2048-sample frames), WAV textures (a trailing note name such as "_A3" gives the clip's pitch), WAV impulse responses (mono or stereo). Rec in the header records the output to a 32-bit WAV.

MEASURING: ambient_render renders any preset offline, deterministically, and prints level, spectral centroid, flatness, width, clicks; Tools/preset_check.py runs the sound test over a library. The same descriptors place the presets on the map.)" },

    { "Shortcuts and tips",
R"(F1 or Help          this manual; Escape closes it
Right-click a knob  MIDI Learn / clear, and the modulation routes that drive it
Right-click a card  the routes this source drives, each removable
Drag a card         onto a knob: a new route at a quarter of the target's range
Window corner       zooms the whole page; the arrangement never reflows
Options             (standalone) audio device, sample rate, MIDI input
Perform / Browse    the two other pages; the header stays

TIPS
- Turn Depth up and let the brain run for ten minutes before judging a patch: the arc, the breath and the bloom need time.
- A drone that clicks is a bug, not a feature: every movement in here is continuous by design. If you hear a step, it is worth reporting.
- Source 1 on Texture with a long clip, Source 2 Additive an octave down, Source 3 Noise Wind at low level: three sources, one instrument.
- The Comb filter on a stacked just chord, Resonance 0.7, Key Track 1: the filter becomes a second body that rings in tune.
- For a DAW session set Clock Source to Host and put LFO 1 on 4 bars: the slow breathing lands on the downbeats.)" },
};

} // namespace

const char* paramHelp(ParamId id)
{
    const int i = static_cast<int>(id);
    return (i >= 0 && i < kNumParams) ? cache().text[i] : "";
}

int numHelpTopics() { return static_cast<int>(sizeof(kTopics) / sizeof(kTopics[0])); }
const char* helpTopicTitle(int index) { return (index >= 0 && index < numHelpTopics()) ? kTopics[index].title : ""; }
const char* helpTopicText(int index)  { return (index >= 0 && index < numHelpTopics()) ? kTopics[index].text : ""; }

} // namespace ambient
