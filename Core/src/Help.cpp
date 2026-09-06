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

    // ---- sources (shared by the four slots)
    { "srcN_type", "What this slot is. Additive: a bank of partials shaped by tilt, brightness, odd/even and shimmer (in Source 1 the strand bank with unison, detune and stacks). Wavetable: a table of spectra, morphed by Position. FM: a two-operator pair. Texture: a granular player over a loaded clip. Stretch: the same clip as a continuum, spectrally stretched up to a thousand times. Noise: ten colours. Off: silent." },
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
    { "srcN_grain", "Texture: the length of one grain in milliseconds. Short grains smear the clip into a texture, long ones keep its identity. Stretch: the spectral window -- short is grainy and quick to follow the clip, long (the top third of the range) is the smooth, frozen continuum of a Paulstretch." },
    { "srcN_stretch", "Stretch: how many times slower than life the clip is read. 1 is the recording as it is; 40 turns twenty seconds into a quarter of an hour; 1000 turns them into a night. Pitch is unaffected -- it is set by Follow and the octave and ratio, before the stretch." },
    { "srcN_xfade", "Stretch: the crossfade at the loop's seam, as a fraction of the clip, so the end runs into the start without a bump. Ignored for a clip whose file name carries _loop: that one is seamless already and wraps straight round." },
    { "srcN_density", "Texture: how many grains start per second. Noise Crackle: how many crackles." },
    { "srcN_density_sync", "Ties the grain (or crackle) rate to the tempo: one per chosen note value instead of Density per second." },
    { "srcN_follow", "Texture and Stretch: Note pitches the clip to the played note (the clip is assumed recorded at its named pitch, the _A3 in its name); Free plays it at its own speed, with octave and ratio as a multiplier. For Stretch the pitch is applied before the stretch, so a chromatic sample plays across the keyboard without the high notes getting shorter. Noise Band/Wind: the band follows the note." },
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
    { "filter_fold", "A wavefolder after both filters. Drive flattens what will not fit; a folder turns it back on itself, and a wave mirrored at the fold grows a family of high partials that no saturation makes -- the metallic edge of an industrial record. The positive half folds a third sooner than the negative one, which is where the even harmonics and the body come from. Off at 0." },

    // ---- z-plane
    { "z_mode", "Off, or the z-plane filter in Series (it hears the voice filter, Mix is its dry/wet) or Replace (it is the only filter)." },
    { "z_route", "With both filters on: Series puts the z-plane after the voice filter; Parallel feeds both the dry sum and Mix balances them." },
    { "z_shape", "One of sixteen frame sets, four frames on the corners of a square: vowel morphs, bell clusters, resonator banks, the sweeps." },
    { "z_z", "The third axis of the cube. X and Y move the point around a square of four filters; Transform lifts it out of that square towards a fourth of its own -- usually the same shape far more resonant, sometimes its peaks turned into notches, sometimes an octave up. What that is depends on the shape, and it is written down for each of them in Tools/make_zplane_bank.py. At 0 the filter is exactly the square it always was, which is why every preset made before this knob existed still sounds the way it did." },
    { "z_decay", "Modal only: how long the lowest mode rings, from a tap to forty seconds. This is a T60 -- the time the mode takes to fall by 60 dB -- so 8 s means a struck bell that is still audibly there after eight. Every resonator is normalised to unity gain at its own frequency, so a long decay makes the instrument ring, not clip." },
    { "z_damp", "Modal only: how much shorter the higher modes ring than the lowest. At 0 every mode holds for the same time, which no real object does and which is exactly why it sounds unreal in a useful way. At 1 the decay time is inversely proportional to frequency, which is roughly what wood, metal and skin do. Between them is where most objects live." },
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
    { "haas", "The Haas trick, done to one band only. Delaying a whole channel by ten to thirty milliseconds widens it and destroys it in mono. Between about 1.2 and 4 kHz, where the ear takes its direction from level rather than from time, each side is given the other side's delayed band six decibels down: the edges open and the bass and the top stay exactly where they were. Off at 0." },
    { "haas_time", "How far that band is delayed. Twelve to eighteen milliseconds is the studio figure: long enough to be a separate arrival, short enough that the ear fuses it with the original instead of hearing an echo." },
    { "externalise", "The two cues a headphone image needs to sit outside the head: the notch the pinna cuts into what arrives from the side, and the reflection off the shoulder a quarter of a millisecond later. Both follow each voice's own position. On speakers leave it off." },
    { "doppler", "As a voice breathes closer or further away its pitch bends a little, the way a moving source does. A few cents at most; the ear reads approach and retreat from it." },

    // ---- ensemble
    { "ens_mix", "Amount of the ensemble (a slow stereo chorus) on the near bus." },
    { "ens_depth", "Modulation depth of the ensemble's delay lines." },
    { "ens_rate", "Speed of the ensemble's modulation." },
    { "ens_mode", "Chorus is the three modulated taps. Microshift is the studio's other way of widening: the two channels detuned a few cents in opposite directions and delayed by different amounts, with nothing moving. It survives a mono sum, which a deep chorus at 13 to 22 ms does not -- that is a comb filter waiting to be summed. In this mode Depth is the detune (up to 12 cents) and Rate a very slow wander of it, so the two sides never settle into a fixed phase." },
    { "ensemble_sync", "Ties the ensemble's rate to the tempo." },

    // ---- delays
    { "dly_time_l", "Left delay time in seconds. The two sides are independent: asymmetry is what makes the space wide." },
    { "dly_time_r", "Right delay time in seconds." },
    { "dly_sync_l", "Ties the left delay time to a note value at the current tempo; the knob is then ignored." },
    { "dly_sync_r", "Ties the right delay time to a note value at the current tempo." },
    { "dly_feedback", "How much of the delay returns into itself. Near 1 the echoes last for minutes." },
    { "dly_cross", "How much the left echo feeds the right and vice versa -- ping-pong at 1." },
    { "dly_damp", "Low-pass in the feedback path: each repeat darker than the last." },
    { "dly_duck", "The echoes make room. While the input is loud the high cut inside the feedback loop drops, so a fresh attack does not have to fight the brightness of the last one's tail; as the note settles the loop opens again over about a second. It is the same idea as Unmask in the far reverb -- get out of the way of what is being played -- applied to the delay. At 0 the loop behaves exactly as it always did." },
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
    { "far_lowcut", "High-pass on the far reverb's tail, 12 dB/oct, off at 20 Hz. The other end of the funnel a mixing engineer puts on a reverb return: dense tails and synthetic textures pile up between 200 and 450 Hz, and that is exactly where a background stops being behind the music and starts covering it. Somewhere between 300 and 500 Hz the tail loses its weight and lays itself behind the notes instead of over them." },
    { "near_lowcut", "The same for the near room: the small reverb's own low end, taken out so the foreground keeps its body." },
    { "room_lowcut", "The same for the convolution room. Real impulse responses of large spaces carry a lot of low-mid energy, which is what makes them sound real and what makes them muddy in a mix." },
    { "subsonic", "A steep high-pass (24 dB/oct) on the finished output, off at 0. Below about 20 Hz there is nothing to hear, but there is plenty to move: it takes headroom, it drives amplifiers and speaker cones for nothing, and it makes mastering processors distort early. Off by default and adjustable rather than fixed, because the Foundation two octaves under a low root reaches about 16 Hz -- a mastering engineer's 20 Hz cut would take this instrument's deepest tone with it. Set it under the lowest note you actually want." },
    { "vec_amount", "How much of the Vector: at 0 each source slot plays at the level it is set to and nothing here does anything. Turned up, the levels are taken over by a point in a square (X, Y) whose corners are the four source slots -- the Prophet VS and Wavestation idea, where the timbre is a place rather than a setting." },
    { "vec_x", "Left to right in the square: Source 1 at the left edge, Source 2 at the right." },
    { "vec_y", "Bottom to top: Source 3 at the top left, Source 4 at the top right." },
    { "vec_wander", "The point drifts on its own by this much, on two slow curves that share no ratio, so it never traces the same path twice." },
    { "vec_rate", "How fast it drifts. Drone rates: a whole cycle takes minutes at the low end." },
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
    { "far_diffuse", "Modulated all-passes in front of the far reverb: the tail arrives instead of starting. At zero the reverb answers immediately, as it always has; turned up, the first reflections smear into a slow swell that takes a second to become a room." },
    { "far_rotate", "The whole background slowly turns: the far field's left and right rotate into each other on a minute-scale curve. Depth of the turn." },
    { "far_width", "The width of the background alone, before it is added to the foreground. A mix in which everything is spread as far as it will go has no depth left -- it is a flat wall. Pulling the far plane in towards the centre while the foreground stays wide is the funnel that reads as distance: the ear is drawn into the middle of the horizon. 1 is the reverb as it made itself, 0 a mono background, and above 1 wider." },
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
    { "room_morph", "Crossfades between the two loaded impulses, A and B: one room becomes another over as long as you like. Both convolutions run only while the morph is between them, so at 0 or 1 it costs what one room costs." },

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
    { "master_tilt", "One broad see-saw around the pivot: turn it down and the whole instrument leans dark, up and it leans open. A single tilt does more for an ambient mix than any equaliser with more knobs, because it never carves a hole." },
    { "tilt_pivot", "The frequency the tilt turns around: everything below moves one way, everything above the other." },
    { "bass_mono", "Below this frequency the side channel is removed: a mono low end, the foundation of a wide picture." },
    { "side_air", "A broad bell at 3 kHz on the side channel, up to +6 dB: air in the width." },
    { "mono_guard", "A safety net for mono. Everything in this instrument is built to widen -- all-pass phase width, asymmetric delays, a reverb whose two sides are deliberately different -- and a drone that sounds gigantic in stereo can lose most of itself when a phone, a club system or a radio sums it to mono. With this on, the side channel is measured against the mid over about a second and a half, and if the side really is the louder of the two the width is eased back, by at most a quarter and at about two per cent a second. It never touches the middle of the mix, only how far the sides may go, and on anything that is already mono-safe it does nothing at all. Off if you would rather have the width and check the mono sum yourself." },
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
    { "press_distance", "How far a key's pressure pulls its voice towards the listener. Press harder and the note steps out of the background into the foreground -- brighter, louder, drier, all at once, because the plane decides all of that." },
    { "press_bright", "How much pressure opens (or, negative, closes) the voice's brightness on top of the plane change." },
    { "press_level", "How much pressure raises the voice's level." },
    { "slide_cutoff", "How far a sideways slide (CC 74 on an MPE controller) moves that voice's filter, in octaves." },
    { "slide_z", "How far the slide moves that voice's point in the z-plane filter." },
    { "bend_range", "Pitch bend range in semitones. With MPE every finger bends on its own channel; without it the wheel bends everything." },
    { "mpe", "MPE: channels 2 to 16 each carry one note with its own bend, pressure and slide, the way a Seaboard or Linnstrument plays. Off, pressure and the wheel apply to every sounding voice." },
    { "brain_quantize", "Holds the conductor's decisions until the next note value of the clock: the notes land on the grid instead of wherever the dice fell. Free is how it has always worked." },
    { "auto_mode", "Free is the conductor as it has always been: notes start and stop on their own timers, so the cluster breathes but never really moves. Chords keeps it full and exchanges one voice at a time -- the chord travels instead of churning. Everything else in this section only matters in Chords." },
    { "auto_rate", "Seconds between exchanges. Long is the point: at a minute apart a listener hears a harmony that is going somewhere without ever catching it move." },
    { "auto_sync", "Puts the exchanges on the clock instead of the seconds knob: one every so many bars." },
    { "auto_lead", "How far the exchanged voice may travel, in semitones. Small is voice leading -- the note that leaves is replaced by one near it, and the ear hears the chord shift rather than one note being cut and another started. Large lets the harmony jump." },
    { "auto_tension", "How strictly the arriving note has to fit the ones that stay. At 0 only notes that sit well against the whole chord are considered; turned up, the progression is allowed to lean." },
    { "auto_root_move", "How often an exchange also moves the root. Without it the harmony circles one centre for ever; with it the piece travels." },
    { "auto_step", "Exchange a voice now, whatever the timer says. It is a trigger, not a setting: switch it on and it fires and switches itself back off, so a controller, a macro or the button on the panel can drive the progression by hand." },
    { "brain2_on", "A second conductor, normally for the background: its own register, pace, density and plane, on the first one's root plus an interval. Two of them make a slow counterpoint that neither would play alone." },
    { "brain2_density", "How many notes the second conductor keeps sounding." },
    { "brain2_rate", "Seconds between the second conductor's decisions." },
    { "brain2_hold_min", "Shortest time the second conductor holds a note." },
    { "brain2_hold_max", "Longest time the second conductor holds a note." },
    { "brain2_low", "Lowest note the second conductor may choose." },
    { "brain2_high", "Highest note the second conductor may choose." },
    { "brain2_depth", "The plane the second conductor plays on: 1 puts it deep in the background behind the first." },
    { "brain2_interval", "Semitones between the first conductor's root and the second's. 7 makes it answer a fifth up, -12 an octave down." },
    { "brain2_consonance", "How strongly the second conductor prefers consonant intervals to its own root." },
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
    { "sympathy", "The voices hear each other: the previous block's foreground is fed back into every voice at low level, through that voice's own filter. Strings on one soundboard do this, and with the Comb or Formant model it is unmistakable -- each voice rings at what it is tuned to when another plays. Kept small on purpose; it is a loop." },
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
    { "envN_shape", "The curve itself is edited on it: drag a breakpoint to move it in time and level, double-click the line to add one or a point to remove it, right-click for the sustain point, the loop, the curvature of a segment, and ten shapes to start from -- ADSR among them. Up to sixteen points, each with its own curve, which is a good deal more than an ADSR when you want it and exactly an ADSR when you do not. The first point stays at the start; use the matrix or a delay if you want it to begin late." },
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
        Source 1 + Source 2 + Source 3 + Source 4   (four equal slots: additive bank,
              wavetable, FM, texture grains, spectral stretch, noise)
        the Vector reads the four as the corners of one square; Strike adds a struck body
        + Air (filtered noise on the note)
        -> Filter (ten models, wavefolder) and/or Z-plane filter, in series or parallel
        -> Envelope, x (1 - distance/2)
        -> interaural time difference from the pan (the far ear hears later)
        -> NEAR bus by cos(distance), FAR bus by sin(distance)
  NEAR:  Ensemble (chorus or microshift) -> Delay -> Delay 2 -> (+ Near reverb + Haas band)
         "to far" from both delays and the Cloud send go into the background
         Cosmos (send / return): shifter, resonator, vowel, nebula -- added, never replacing
  FAR:   Far reverb (dark, wide, asymmetric, minutes long, with its own width)
         + Room (convolution) + Shimmer loop, unmasked band by band under the foreground
  Body (twelve tuned modes) -> mid/side (bass mono, side air, width) -> + Foundation sub
         -> Patina -> subsonic -> Master -> soft clip.   No compressor anywhere.
  Feedback: the finished mix can return into the near bus and/or bend every partial's phase.

A voice's plane decides everything at once: how bright it is (2.5 octaves of cutoff per unit of distance), how loud (-6 dB), how dry (the far plane is heard only through the reverb), and how present (the presence bell lives on the near plane). The brain places 40 % of its notes close and 60 % deep; your keys sit at Keys Depth.

THE PAGE

Everything is on one page and nothing scrolls; drag the window corner to zoom. Rows whose sections are of a kind page through tabs: SOURCE 1 / SOURCE 2 / SOURCE 3 / SOURCE 4 / VECTOR, FILTER / Z-PLANE / AMP ENV / EXPRESSION, the effect pairs, COSMOS / STRIKE, BRAIN / AUTOPLAY / BRAIN 2 / TUNING / COHERENCE / CLOCK, MORPH / MACROS. The strand bank has no tab of its own: it belongs to Source 1's additive type alone and sits under that page's display. The header carries the pages -- Main, Perform, Browse, VR (calibration and gestures) and Help at the end. The room a row's knobs leave is a live display drawn from the engine's own numbers. The strip along the bottom holds the modulators. Point at any control and this header line tells you what it does.)" },

    { "Sources",
R"(Every voice has four equal source slots; their levels mix before the filter. Each slot has its own clip for the Texture type, so four slots can play four different recordings. Each slot has a Type:

ADDITIVE  A bank of up to 32 partials. Partial h has amplitude h^-Tilt, the Brightness window fades the upper ones out, Odd/Even weights the two families, Inharmonic stretches the series like a stiff string, and Shimmer lets every partial drift in level on its own slow curve -- the breathing. Partials above Nyquist are not generated, so nothing aliases. In Source 1, Additive is the STRAND BANK: up to six copies of the bank, detuned (Detune, Drift) or placed on pure ratios (Stack: octaves, fifths, a just major or minor, seventh, harmonics, subharmonics -- one key becomes a just chord), fanned out in stereo (Spread), with Bloom opening the brightness over Bloom Time and Rate Wander slowly varying every movement rate. In Source 2 and 3, Additive is a single bank with its own Partials, Tilt, Bright, Odd/Even, Inharmonic and Shimmer.

WAVETABLE  Not a table of samples but a table of SPECTRA: 32 partial amplitudes per frame, up to 64 frames; Position morphs between frames and Pos Drift wanders it. Five built-in tables (Classic: sine to pulse; Organ; Vocal a-e-i-o-u; Glass; Metal) and User, loaded from a WAV in the Serum/Vital layout (2048-sample frames) with the button. Alias-free like the bank, and the same partials-based tricks (presence, low cut, feedback FM) apply.

FM  A two-operator pair: carrier at the slot pitch, modulator at FM Ratio, FM Index up to 8, reduced automatically on high notes. Pos Drift wanders the index.

TEXTURE  A granular player over a loaded clip (Texture... button, or the preset's own sample): up to 64 grains (Grains), Grain length, Density per second (or per note value with Sync), starting around Position with Spread, pitched to the note (Pitch = Note; the clip's pitch comes from its file name, e.g. "_A3") or played free. The display shows the grains reading the clip.

STRETCH  The same clip read as a continuum instead of as grains: a spectral time stretch, after Paulstretch -- a window (Grain) of the clip is transformed, its magnitudes kept, its phases drawn afresh and the result overlap-added, while the read position crawls through the recording at one Stretch-th of its speed. No grain rhythm, no transient left standing: a field recording becomes weather. Position is where it reads (Pos Drift wanders it), Pitch = Note or Free is applied by resampling BEFORE the stretch so a note played higher does not get shorter, and the loop's seam is crossfaded by Loop Fade unless the clip's name carries _loop, which marks it seamless.

NOISE  Ten colours: White, Pink, Brown, Blue, Violet, Grey, Band (a resonant band at Position, Q from Noise Q, tracking the note with Pitch = Note), Wind (a wandering band), Crackle (sparse impulses at Density), Digital (sample-and-hold at a rate from Position). Levels are matched so a colour change does not change the loudness.

Every slot also has Octave, a just Ratio to the note (3/2 a fifth up, 7/4 a harmonic seventh -- ratios, so the scale stays pure) and Pan. In Source 1 these move the strand bank as well.

THE VECTOR is the four slots read as a place rather than as four levels, after the Prophet VS and the Korg Wavestation: a point in a square whose corners are the four sources. Amount is how much of it there is, and at 0 nothing here does anything -- each slot plays at the level it is set to. Drag the point, or turn X and Y. The centre of the square is neutral by construction: at (0.5, 0.5) every factor is exactly 1, so turning Amount up on a patch you like changes nothing until you move. Wander lets the point drift on its own, on two curves whose rates share no simple ratio, so it never traces the same path twice; Rate is how fast. The bars beside the square say what the point is doing to each slot.)" },

    { "Filters and Z-plane",
R"(Two filters, each with its own switch, in series or in parallel.

THE VOICE FILTER (Filter section) has ten models behind the same knobs:
  LP 6      one pole, warm and gentle
  LP 12     the state-variable low pass -- the default, the one the instrument always had
  LP 24     two stages, steep
  HP 12     the opposite slope
  BP 12     a band, unity at the cutoff
  Notch     a hole swept through the harmonics
  Peak      a bell of up to +14 dB, narrower with Resonance
  Ladder    four one-poles with saturating feedback, self-oscillating near full Resonance
  Comb      a feedback comb tuned to Cutoff; Resonance deepens the dips -- on a cluster, a second resonating body
  Formant  three tracked bands: the filter sings a vowel
FOLD is a wavefolder after both filters. Drive flattens what will not fit through the filter; a folder turns it back on itself instead, and a wave mirrored at the fold grows a family of high partials that no saturation makes -- the metallic edge of an industrial record. Its positive half folds a third sooner than the negative one, so the even harmonics are there too. Off at 0.

Cutoff is moved by Key Track (1 keeps the same partials in the passband on every key), Env Amount (the amplitude envelope, negative closes), Drift (a slow wander) and by the voice's distance (2.5 octaves darker on the far plane). Drive saturates ahead of the filter. On switches it out.

THE Z-PLANE FILTER (after the E-mu Morpheus idea): four filter frames sit on the corners of a square and a point (X, Y) inside it is a filter interpolated from all four -- on the pole and zero parameters, so every point is stable. 155 Shapes in twelve families (vowel morphs, bell clusters, resonator banks, sweeps, and the acoustic ratio families generated for the bank); the point wanders at Rate by Depth; the third axis Z is the cube's own depth, so a shape is a volume rather than a square; Resonance narrows every section; Key Track moves the frame with the note. Mode: Off, Series or Replace (the z-plane alone). Route: with both filters on, Series puts the z-plane after the voice filter (Mix is its dry/wet), Parallel feeds both the dry sum and Mix balances them.

The FILTER RESPONSE display draws the voice filter (in the voice colour), the z-plane (in the accent) and what a note actually meets after both, from the same maths the audio path uses.

The three reverb returns each have a LOW CUT beside their high cut now. Together they are the filter funnel a mixing engineer puts on a return: dense tails pile up between 200 and 450 Hz, which is exactly where a background stops sitting behind the music and starts covering it, and taking that out is what lets a 40-second tail be enormous and transparent at the same time.

The OUTPUT SPECTRUM along the bottom of the left column is the other half of that picture: what is actually coming out, with the same filter curve laid over it on the same decibel scale. Its window is 16384 samples -- 2.9 Hz at 48 kHz -- which is long enough to show the partials of a low drone as separate lines rather than one hump, so a fifth sitting exactly on the third partial is something you can see, and see come apart as Purity Drift loosens it. The bars are the moment; the faint line above them is the loudest each band has been in the last few seconds; the ticks along the bottom edge are the fundamentals of the notes sounding now. Point at it to read a frequency, its nearest note and that band's level.)" },

    { "Space, Air, Foundation",
R"(SPACE is the spatial model. Depth scales how deep the brain places its notes (40 % close, 60 % deep); Keys Depth is the plane your keys play on. Pan Drift wanders each voice's centre; Time Width is the interaural time difference (up to 0.65 ms, the far ear hears later) -- width from time, not level. Presence is a bell at 2-5 kHz on the near plane only, gone on the far plane. Breath lets every voice's distance itself wander (the room breathes) at Breath Rate. Arc is one very slow drift over Arc Period minutes that leans on density, brightness and depth, so a whole night has a shape; Arc Sync ties it to bars.

The STAGE display shows every sounding voice as a dot: left-right by pan, near-far by plane, size by envelope, brain notes in the voice colour, your keys in orange.

AIR is filtered noise inside each voice, at a multiple (Color) of the note's fundamental, with Q; in Ghost mode the noise runs through six sharp resonators on the harmonics 1 2 3 5 7 9, so the harmony is filtered out of the chaos.

FOUNDATION is a mono sub voice one or two octaves under the brain's root (or, with Source = Difference, on the combination tone of the two lowest voices -- the ghost bass of a just chord), gliding in the log domain over Glide seconds, with Binaural offsetting left and right by a few hertz and Tone adding a little harmonic content. It is injected after the mid/side stage so Bass Mono cannot thin it. Pad Low Cut takes the pads out of its register (12 dB/oct below the cut).)" },

    { "Effects: foreground and background",
R"(The near bus (the dry plane) runs through ENSEMBLE (Mix, Depth, Rate or Sync, and a Mode: Chorus is three modulated taps, Microshift detunes the two channels a few cents in opposite directions with nothing moving -- the studio's way of widening a drone that survives a mono sum, where a deep chorus at 13 to 22 ms is a comb filter waiting to be summed), DELAY and DELAY 2 in series (independent left and right times or note values, Feedback, Cross for ping-pong, Damping in the loop, Mix onto the near bus and To Far into the background: echoes that recede), and the NEAR REVERB (a small room: Mix, Decay, Damping, Low Cut).

The HAAS band (in Space) widens the foreground where the ear takes its direction from level rather than from time. Delaying a whole channel by ten to thirty milliseconds widens it and destroys it in mono; done to the band between about 1.2 and 4 kHz, and put into the side channel so that what is added on one side comes off the other, the edges open, the bass and the top stay where they were, and a mono sum is exactly the picture it was. Haas Time is how far that band is delayed. Off at 0.

The far bus is the background: FAR REVERB is an eight-line feedback network, 100 % wet, dark and wide -- Size, Decay (tens of seconds), Damping, Pre-Delay, Asymmetry (the right half stretched and delayed so the two ears hear different reflections), Tail Cut, Low Cut, Freeze for an instant infinite pad, and WIDTH, which is the background's own stereo width before it is added to the foreground. That last one is the funnel: a mix in which everything is spread as far as it will go is a flat wall, and pulling the far plane in towards the centre while the foreground stays wide is what the ear reads as distance. 1 is the reverb as it made itself. CLOUD takes grains of the recent foreground (Send, Density or Sync, Size, Spray back in time), transposes them (Pitch) and drops them into the far reverb. ROOM is a convolution reverb from a loaded impulse response (Impulse... button, or the preset's own; 200 generated responses ship with the library) or the built-in dark hall, hearing the far sends or the near bus, with Pre-Delay and Tail Cut.

FEEDBACK returns the finished mix: To Bus into the near bus before the filters and effects (throttled by the output level so it hisses and holds instead of running away), To Pitch as phase modulation of every partial (the sound bends itself), through Tone and Drive; Tape adds asymmetric saturation, wow and flutter and a level-dependent noise floor.

MASTER: Bass Mono removes the side channel below a frequency (a mono low end under a wide picture), Side Air lifts the side at 3 kHz, Width scales the stereo image, Subsonic is a steep high-pass on the finished output; then the master gain and a soft clipper. There is no compressor.

The LOUDNESS METER under the master reads the finished output to BS.1770: I is the gated integrated value, S the short term, LRA the range, TP the true peak between samples, and crest the peak-to-RMS distance. The band on the bar is -24 to -16 LUFS, where a dark ambient master is asked to land, and the line at -14 is where the streaming services normalise: a master louder than that is turned down again and arrives flat rather than loud. Click the meter to start it again.)" },

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
R"(THE SOURCES a route can be driven by: the eight LFOs, the six envelopes, the voice's own amplitude, the eight macros, the four Kuramoto oscillators of the Coherence ring, the note, its velocity and its distance, one random number per note, the Beat (the instrument listening to how far out of tune it currently is), and the hands: PRESSURE (channel or polyphonic aftertouch), WHEEL (CC 1) and SLIDE (CC 74). Those last three rest at zero, so give them the 0..1 flag and a patch at rest sounds exactly as it did until you move them. Aftertouch on the filter's resonance, the wavetable position and the reverb at once is one gesture with three routes.

The strip along the bottom holds every modulation source as a card: LFO 1-8, ENV 1-6, MACRO A-H, KURA 1-4, AMP (the voice's own envelope), NOTE, VELO, DIST (the voice's plane), RAND (a random value per note) and BEAT. Its tabs edit the sources:

LFO  Eight free LFOs with Shape (Sine, Triangle, Ramp Up/Down, soft Square, Random, Steps, or Table -- a frame of the user wavetable as a shape, so any drawn curve is an LFO), Rate from one cycle in twenty minutes to 20 Hz or a note value (Sync), Phase, Depth, and Mode: Global (one phase for the instrument, every voice breathes together), Voice (each voice its own copy), Retrigger (each voice restarts from Phase). The editors show the shape with a running dot.

ENVELOPES  Six multi-segment envelopes: up to sixteen breakpoints, a curve on every segment, an optional sustain point and an optional loop. Edit the curve on the curve: drag a breakpoint to move it in time and level, double-click the line to add a point or a point to remove it, right-click for the sustain point, the loop, the curvature of a segment, and ten shapes to start from -- ADSR, AD, AR, ramps, a pulse, a slow swell, two peaks, stepped, bipolar. The sustain point wears a ring, the loop points a vertical line. Mode One Shot / Loop / Sustain Loop, Time stretches the shape or Sync spans it over one note value, Depth scales it. (The text form "t:v:c/t:v:c/...!s2!l1-3" is still what a preset stores.)

The voice's own AMP ENVELOPE is a plain ADSR -- Attack, Decay, Sustain, Release -- on the AMP ENV tab, with times in seconds up to a minute for the attack and two for the release. These six are for everything else.

MATRIX  Up to 32 routes "source > target : depth [: via] [: u]". Depth is a fraction of the target's range (-1..1); via scales the depth by a second source (a macro, typically); u treats a bipolar source as 0..1. One source may drive as many targets as it likes.

BEAT  The instrument listening to its own tuning. It takes the two lowest sounding voices, finds the simplest just ratio near the interval they make, and turns at the beat between the harmonics that would coincide if that interval were exact -- which is zero when the chord is in tune and quicker the further it has drifted. An exact octave leaves it standing still; a fifth in equal temperament, two cents narrow, turns it about half a hertz. Route it at a filter, at the Nebula's smear, at anything, and the sound breathes in time with its own harmonic friction rather than at a rate somebody typed into an LFO. Purity Drift is what sets it moving.

ROUTING WITHOUT TYPING  Drag a card from the strip onto any knob: a route at a quarter of the range is added. Right-click a card to see and remove its routes; right-click a knob to see what drives it. A modulated knob wears a thin ring in its source's colour (LFOs turquoise, envelopes green, macros pink, coherence blue) and a second arc from its value to where the modulation is pushing it right now. Performance state (morph, macros, map, route, clock) can never be a target.)" },

    { "Morph, macros, perform, gestures",
R"(MORPH holds two full snapshots, A and B: pick a preset for each or capture the current state with A <- now / B <- now. Switch Active on and Position blends the whole instrument between the two worlds; Glide sets how long it takes to follow a new position -- up to fifteen minutes, so one gesture can carry a piece across a quarter of an hour. Continuous parameters interpolate in their own perceptual curve, integers round, choices flip halfway. Morph settings are never part of a preset.

MACROS A-H (Space, Alien, Motion, Bloom, Density, Distance, Evolution, Air) are one knob for several parameters each; the mapping table (Gestures...) says which, with range and smoothing. They are also modulation sources. Inertia is the analogue slew: every knob glides to its value with this time constant.

PERFORM (header button) shows only the eight macros and the morph, large, for playing a set; Record set / Play set log and replay everything you do, with time.

GESTURES: the same layer that will drive the Quest version listens to OSC (/ambient/hand/L|R, /head, /param, /gesture, /note, /preset, /morph, port 9000): hand height, distance, pinch. Calibrate learns your range (hands together and apart, low and high, near and far, for six seconds). The right pinch is the clutch: mappings act only while it is engaged.)" },

    { "Presets, packs, browser, map, routes, sets",
R"(Presets come in two independent layers: the Sound box (voices, space, effects, brain, tuning -- 191 built in) and the Cosmos box (32 presets for the Cosmos section only), with the z-plane and the Strike layers beside them. Loading one never touches the other; in a DAW the full presets are the programs. Save... / Load... store the whole state as an .ambientsynth file.

THE BUILT-IN PRESETS

The 191 compiled-in presets are the instrument's own repertoire, written by hand over the rounds in which it grew, and every one of them was later given the parts of the instrument it predates -- a matrix, a z-plane, a third source -- and measured afterwards to be sure it still sounded like itself. They are grouped into families by what they are for:

Originals: the first patches, one idea each, kept as they were. Sleep / night: long holds, just intonation, the sub and the binaural beat, made for the hours nobody is listening closely. Cathedral / glass: bright, inharmonic, minutes of reverb. Deep / sub / dark: the low register, almost no treble, the far plane. Breath / flute / voice: the Air section as an instrument, noise shaped into wind and vowels. Exotic scales: Slendro, Bohlen-Pierce, the otonality, the harmonic and subharmonic series. Shimmer / delay / motion: the effects as the subject, patches that never sit still. Cosmos / science fiction: the frequency shifter and the nebula, cold and wide. Playable keys: the brain off, made to be played from a keyboard. Long-form night arcs: patches built around the hour-scale Arc and the tide. Storm / cluster / texture: dense, noisy, granular. Sources: what the wavetable, the FM pair and the feedback do that a bank cannot.

PACKS

Plain text files (*.ambientpack, one preset per line) that may name a sample, a wavetable, an impulse response, a modulation matrix and envelope shapes of their own. Put them in Documents/AmbientSynth/Packs or point AMBIENT_PACKS at a folder (the installer's own folders are read too, and a pack found in two of them loads once); they appear everywhere the built-in presets do, each pack as a family. The library that ships alongside has 6400 presets in 32 packs, 1700 samples, 608 wavetables and 240 impulse responses.

Every pack is one corner of the drone repertoire, written in the spirit of an artist who works there -- nothing is sampled from or affiliated with any of them; the packs are ranges over this synth's own parameters, chosen by ear, then rendered, measured and gain-matched. Two hundred presets each, half of them still, half astir. A pack is a family in the browser, and its name is the first thing to search for.

SLEEP CONCERT (in the spirit of Robert Rich). The all-night concert: just intonation, a binaural sub a few hertz apart between the ears, holds measured in minutes, the brain placing most notes deep. Attacks of ten to thirty seconds, the Bloom opening the spectrum over a minute or two, the Arc leaning on the whole night. For a room, not for headphones' impatience.

DEEP EARTH (Lustmord). Subterranean: the low register, almost no treble, the far reverb long and dark, the sub carrying most of the weight. Brightness under a third, tilt steep, the z-plane in its darker sweeps. Presets that are felt in the floor before they are heard.

PERMAFROST (Thomas Koener). Filtered noise fields, nearly motionless: the noise colours through slow band-passes, brightness low, motion minimal, the far plane wide and cold. What changes, changes over minutes.

FIELD ABSENCE (Francisco Lopez). Granular field recordings, quiet, atonal: the Texture type over clips of rooms and weather, small grains scattered wide, hardly any pitch, levels low enough that the room is the instrument.

ALDEBARAN (Inade). Ritual metal: the Cosmos heavy in every preset -- the resonator on the root, the shifter drifting, the nebula smearing -- with the modal z-plane as struck metal underneath. Ceremonial, slow, with a pulse from the delays.

RITUAL MACHINE (Deutsch Nepal). Saturated feedback loops: the feedback bus and its tape, the drive, the patina, long delays with high feedback that absorb into fog. Grime as a material.

PLANETARY (Michael Stearns). The harmonic series as the subject: stacks on harmonics and subharmonics, wide spreads, shimmer, the far reverb enormous and bright. The widest presets in the library.

TEMPLE OF AIR (Ooephoi). Pure and extremely slow: sine-like banks with few partials, minute-long attacks and releases, a high consonance, the Air section for breath. Nothing here happens quickly, and nothing has an edge.

VAST CHORD (Mathias Grassow). Dense just-intoned chord walls: six-strand stacks on pure ratios, high brain density, purity high, the sub on the difference tone. A single key is a chord; the brain adds four more.

DESERT EMBER (Steve Roach). Warm and organic: the ladder filter and its drive, slow pulses from delays on note values, the wavetable's organ and vocal tables, the Ensemble. The analogue end of the library.

MODULAR NOCTURNE (Ian Boddy). Resonant filter movement and echoing sequences: the filter drift and envelope high, autoplay stepping in Chords, delays with cross-feed, the Cloud. Presets that move like a patch on a modular.

MILLSTONE (Jonathan Coleclough). Acoustic and mechanical: the Strike on wood and metal, granular textures of machinery, the Body's modes, the Room on small designed spaces. The sound of things turning.

SLOW CAROUSEL (Mimir). Warped loops under tape hiss: the Patina high, the feedback's tape, wow on everything, the Texture type reading clips slowly, the delays long. Old and slightly wrong on purpose.

GLASS VITRINE (Mirror). Ghostly harmonium: the Glass and Organ tables, inharmonicity, spectral z-plane shapes, the nebula's smear. Thin, high and see-through.

CHAMBER GREY (In Camera). Small dim rooms: the near reverb doing most of the work, the far plane quiet, short delays, close and dark. The intimate end of the spatial model.

PAINTED FIELD (Andrew Chalk). Blurred warm washes: the Blur high, spectra frozen and let go, the Ensemble wide, brightness middling. Presets like a colour rather than a note.

LOOP STUDIO (Colin Potter). Long tape delays and processed loops: two delays in series at seconds with feedback near the top, absorption, the tape in the loop. Every note keeps arriving for minutes.

GHOST SIGNAL (Bass Communion). Granular, wide, processed strings: the Strings table and bowed additive banks through the Cloud and the shimmer, spread as far as the mono guard allows.

SUSTAIN (Paul Bradley). One long tone, minimal change: one voice, one key, purity near one, drift near zero, the far reverb enormous. The stillest presets in the library, and a test of every reverb.

HULL RUMBLE (SleepResearch_Facility). Machine hum, static and depth: brown and grey noise, the sub, the comb filter on the hull's pitch, the far plane deep. The engine room of a ship at night.

CORRIDOR (Kammarheit). Dark reverberant rooms, sparse: few notes, long holds, the Room convolution on bunkers and caverns, the near plane almost empty. Space with very little in it.

NORTHERN DARK (Gustaf Hildebrand). Cinematic: sub-bass, wide stereo, the far reverb with rotation, slow z-plane sweeps, a presence lift on what is close. Presets for a film that has not been made.

VOID STATION (Tholen). Cold science fiction: the Cosmos shifter, the frequency-shifted feedback, slow z-plane sweeps through the phaser and comb families, digital noise. Nothing organic in it.

STRINGS AT REST (Stars of the Lid). Consonant bowed swells: additive banks with a bowed spectrum, long attacks, a just major, the Ensemble as a string section, the near reverb as a hall's front rows.

TAPE SATURATION (Tim Hecker). Bright, distorted, damaged: the feedback bus driven, the Patina's age high, the fold where there is one, the wavetable's Metal table. The loudest and most broken presets in the library.

STRUCK BODIES (Bernhard Guenter). The z-plane read as a resonator bank, struck and left to ring: the Modal mode, the Strike as the exciter, decays measured in seconds, very quiet. Small sounds with all the space around them.

THREE ALIKE (Eliane Radigue). Three source slots of the same kind beating against each other: identical types on ratios a few cents apart, the purity drift, the Beat source driving the filter. The slowest possible change.

TURNING HARMONY (Pauline Oliveros). Autoplay in Chords: the brain exchanging one voice at a time from the scale, a tension that decides how far, holds long enough to hear each new chord settle. Deep listening as a mechanism.

FILTER CUBES (Alva Noto). The filter cube's third axis: the z-plane's Z moved by the matrix, cold precise shapes, digital noise, short decays. The most exact presets in the library.

OWN TUNING (Catherine Christer Hennix). Purity drift, difference tones and the Beat as a modulator: the sub on the difference of the two lowest voices, the tuning breathing in and out of just, the instrument listening to its own roughness.

FIELD RECORDINGS (Chris Watson). Places, not instruments: five hundred seamless recordings -- rain on twelve kinds of roof, caves, harbours in fog, power stations through a wall -- read as a continuum by the Stretch type, up to four of them on the Vector's four corners, a quiet additive centre underneath.

CRYO CHAMBER (Atrium Carceri). Written for the mixing desk the instrument grew last: the background narrowed as it goes back, the foreground opened by the Haas band, the Ensemble as a microshift, the wavefolder where a saturation used to be, and the convolution room loaded with a struck object rather than a hall -- concrete, chain, iron -- so the pad is played on a piece of the world.

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
    { "Design I: what the instrument is for, and the rules that follow",
R"(AmbientSynth was built for one kind of music: the slowly breathing clusters Robert Rich played at his sleep concerts -- dense chords in just intonation, no rhythm, changes that take minutes, a piece that can run all night without repeating and without anyone touching it. Almost every design decision in the instrument follows from taking that brief literally, and this chapter and the five after it say which decision follows from what. Where a number is given, it was measured on the instrument; the measurements are in the self test and in the project's concept notes, and they are quoted here rather than repeated as claims.

THE TIME SCALE

A drone is heard over minutes, not over the half second a note of a piano occupies. The ear adapts: a steady spectrum stops being noticed after tens of seconds, a steady level after a few, and a change that would be a gesture in a song is the whole event in a drone. So the instrument's rates are drone rates. Attacks reach a minute and releases two; the LFOs run from twenty hertz down to one cycle in twenty minutes; the Arc leans on the whole night on a period measured in minutes; a wandering pitch drifts over a hundred seconds. None of this is a stylistic flourish -- it is the range in which the ear, having adapted to what is there, can still hear that something is moving.

The same time scale is why the instrument has a conductor rather than a sequencer. A sequencer repeats, and repetition is exactly what adaptation punishes: the third pass of a four-bar loop is furniture. The Cluster Brain does not repeat. It draws hold times from a range, intervals from an exponential distribution, notes from a weighted choice, and it forgets nothing because it has nothing to remember; two hours of it never recur.

EVERY MOVEMENT IS CONTINUOUS

A click is the loudest thing a drone can do. In a mix with a drum kit a stepped parameter hides behind a hundred transients; in a bed of held tones a single discontinuity is the only transient there is, and the ear -- which localises and attends by onsets -- turns to it at once. So the standing rule of the whole instrument is that nothing steps. Random values do not jump: the Drifter that stands in for a random LFO produces smoothstep-interpolated curves with zero slope at its knots, so even the direction of change never reverses abruptly. Envelopes are exponential. Partial levels ramp across a control block. Filter cutoffs, delay lengths and reverb line lengths glide. Where a switch cannot be avoided -- a source type, a filter model -- the gains cross-fade over a control block and the state starts from rest. A controller sending seven-bit steps is smoothed inside the voice before it reaches anything. The self test measures the largest sample-to-sample step of every render it makes and the standing figure is 0.06 on the default patch; the rule is written down because it is broken in the world's DSP more often than any other.

MEASURE, DO NOT ONLY LISTEN

The instrument renders deterministically offline (the render tool), prints its own descriptors -- level, spectral centroid and flatness, flux, width, bass, voice count, the largest sample step, and a hash of the samples themselves -- and the self test checks the tuning maths to a billionth and the effects by impulse and sine measurements. That is not caution for its own sake. Ambient sound design is full of confident wrong answers: a widening that widens nothing, a ducking that measures brighter because it measured a ratio, a resonator normalised at its peak that passes almost nothing of a broadband signal, a preset whose envelope does nothing because the noise band on top of it pins the spectrum. Every one of those happened while building this instrument, every one sounded plausible for an afternoon, and every one was caught by a number. The manual says "measured" a great deal for that reason.

ONE TABLE

Every parameter -- name, key, range, default, skew, unit, section -- lives in one table, and the engine, the plugin's automation list, the panel, the presets, the packs, the render tool and this manual all read that table. That is why a parameter cannot exist in the panel and not in the presets, or in the presets and not in the help. It is also why new parameters are appended and never inserted: a host stores automation by slot, a preset stores a choice by its index, and six thousand finished presets must go on meaning what they meant.

WHY ADDITIVE

The voice is a bank of up to thirty-two partials, each a rotating phasor with its own phase and its own slow life, rather than a stored waveform read through a filter. Three consequences of that choice run through the whole instrument. Partials above Nyquist are simply not generated, so there is no aliasing at any pitch, and a drone with a bright, inharmonic spectrum an octave above middle C stays clean. Every partial can move independently -- its level drifting on its own slow curve (Shimmer), its frequency stretched (Inharmonic), its weight set against its neighbours (Odd/Even) -- which is exactly the internal life a Rich drone has and a sample of one does not. And brightness, tilt and inharmonicity become continuous knobs rather than a choice between waveforms, which matters in an instrument where everything is meant to glide.

The additive domain is also where the spatial model lives for free. Presence, the pad's low cut, the distance-dependent darkening: all of them multiply into the partial targets at control rate and cost nothing per sample. The bank runs as independent rotating phasors renormalised once per control block -- no table lookup, no phase accumulator in the inner loop -- which is what let the whole instrument, effects and conductor included, render at about forty times real time on one core.

SPACE IS A LANDSCAPE, NOT AN EFFECT

The last of the guiding decisions is the one the next chapters spend most of their time on. In most synthesisers space is a reverb on the end of the chain. Here every note has a distance from the ear, and that single number decides its brightness, its level, how dry it is and how present -- before any effect. Depth then comes from the contrast between a foreground plane and a background plane, width from time differences rather than from level, and focus from a mono low end. Nothing is compressed anywhere, because the distance between the quietest texture and the loudest swell is what the ear reads as the size of the room.)" },
    { "Design II: the voice and its sources",
R"(This chapter is about what a voice is made of and why: the additive bank and its strands, the four source slots and their types, the two filters and the fold, and the struck layer on top.

STRANDS, DETUNE AND BEATING

A single additive bank is a clean tone. A drone wants a chorus of them, and the strand bank is that: up to six copies of the bank, each with its own pitch offset, its own slow pitch drift and its own place in the stereo field. The psychoacoustics is the psychoacoustics of beating. Two tones a few cents apart produce a slow amplitude beat at their frequency difference -- eight cents at 220 Hz is about one beat a second -- and the ear hears that beat as movement and warmth rather than as two pitches, up to about fifteen hertz, where it turns into roughness. Detune sets the spread; the per-strand Drift, on its own curve per strand, keeps the beats from ever settling into a fixed pattern; Spread fans the strands out so the beats happen between the ears as well as in time.

Stack replaces the detuned copies with pure ratios -- octaves, fifths, a just major or minor, sevenths, harmonics, subharmonics -- ordered so that two strands already make root and fifth and three a triad. With Detune at zero the chord is beat-free: the fifth's second partial is the root's third partial, exactly, and the partials of the strands lock into each other. One key becomes a just chord. Detune and Drift still apply on top, so the same chord can be set slowly beating, which is where much of the instrument's character comes from: harmony that breathes at the rate of its own mistuning.

Bloom holds back the brightness at the start of a note and opens it over a chosen time, because a drone's arrival is heard as a spectrum opening rather than as a level rising; Rate Wander lets every slow rate in the voice vary by up to an octave on a hundred-second curve, so five minutes never resemble the five before -- the nested-LFO idea, the movement itself speeding up and slowing down.

FOUR EQUAL SLOTS

Every voice has four source slots, and they are equal on purpose. A slot may be the strand bank (Additive, in Source 1), a single additive bank, a wavetable, an FM pair, a granular texture, a spectral stretch or a noise. Three slots of the same kind a fifth and an octave apart are a chord out of one key; three of different kinds are an instrument; and the fourth exists so that the Vector -- the four slots as the corners of a square -- has four sources at its corners and not three plus their sum. Each slot goes through the voice's filter, envelope, distance and interaural delay like the bank, so a granular texture is placed in the room the same way a partial bank is.

The Vector is the Prophet VS and Wavestation idea: the timbre as a place rather than as four levels. Its centre is neutral by construction -- at (0.5, 0.5) every factor is exactly one -- so turning Amount up on a patch you like changes nothing until the point moves. That is a rule the whole instrument follows for every feature added after the first presets existed: neutral at the default, so nothing that was finished sounds different.

WAVETABLE AS SPECTRA

The wavetable type is not a table of samples but a table of spectra: thirty-two partial amplitudes per frame, up to sixty-four frames, rendered by the same rotating-phasor bank as the main oscillator. Interpolating between frames is then interpolating between spectra, which is what a wavetable morph sounds like it is doing and, in a sample-based table, is not -- there the morph crossfades waveforms whose partials may be at opposite phases and cancel on the way. Here nothing cancels and nothing aliases, and every trick that works on partials -- the presence bell, the low cut, the feedback's phase modulation -- works on the wavetable as it does on the bank. A user table is analysed from a WAV in the Serum and Vital layout, frame by frame, and becomes spectra too.

FM, TEXTURE, STRETCH, NOISE

The FM pair keeps its index in check by hand: above three kilohertz the index shrinks, so a bell an octave above the staff does not turn to aliased noise. The granular Texture type spawns grains at exponentially distributed intervals -- the same distribution the conductor uses for its events, because it is the one with no rhythm in it -- each with its own start point, window and pan, overlapping freely. Its overlap normalisation was measured rather than assumed (a Hann-windowed stream at overlap N has an RMS of sqrt(N) times 0.612 of the source), because the first version was twenty-two decibels too quiet next to a wavetable slot at the same Level, and the manual's promise that all types land at about the same loudness at the same Level rests on that number.

Stretch reads the same clip as a continuum: Paulstretch inside a voice. A window of the recording is transformed, its magnitudes kept, its phases thrown away and drawn afresh, and the frame overlap-added while the read position crawls through the clip at up to a thousandth of its speed. Every frame is a plausible slice of the clip's spectrum with no memory of where its transients were, which is why a field recording stretched forty times has no grain rhythm and no attack left standing -- it becomes weather. Pitch is applied when the window is read, as a resampling step, and the stretch is applied to how far the read moves between frames; the two do not know about each other, which is what lets a chromatic sample play across the keyboard without a high note ending sooner than a low one.

The noise type's ten colours are level-matched to a wavetable slot at the same Level, and the pink filter is the full seven-term form rather than the common three-pole short form, which is 1.7 dB an octave too steep. Grey is white noise with the ear's most sensitive region taken out, so it sounds flat rather than measuring flat -- the one colour defined by psychoacoustics rather than by physics.

AIR

Air is filtered noise on the note: a band around a harmonic of the fundamental, or -- in Ghost mode -- six sharp resonators on the harmonics 1 2 3 5 7 9, the harmony filtered out of chaos. It exists because a real sustained tone is never only its partials: a bowed string, a blown pipe, a voice all carry broadband noise shaped by the same resonances as the tone, and the ear uses that noise as the cue that the source is physical. Air is also, in this instrument, the cause of more measurement traps than any other section: a broadband band on top of the spectrum pins the centroid, so a filter sweep or an envelope measured with Air on appears to do almost nothing. Anyone measuring the instrument learns to switch it off first.

TWO FILTERS

The voice filter has ten models behind the same knobs. Nine of them are built on Zavalishin's topology-preserving transform -- the trapezoidal integrators with the zero-delay feedback solved algebraically -- which is what lets a filter be swept without detuning or clicking, and a swept filter is the normal state of a filter in this instrument. The one exception is deliberate: the four-pole Ladder keeps a sample of delay in its feedback path, because that delay is part of what the model sounds like and every preset that uses it was voiced with it; a zero-delay ladder would be a different filter and belongs beside it, not in its place. The Comb is tuned to Cutoff with its output scaled so the peaks stay at unity and Resonance deepens the dips rather than raising the level -- on a sustained cluster it is less a filter than a second resonating body. Cutoff follows the key, the envelope, a slow drift and the voice's distance: two and a half octaves darker per unit of depth, which is the air absorption of the spatial model, applied in the voice.)"
R"(

The z-plane filter is the E-mu Morpheus idea: filter frames on the corners of a cube, and a point inside it is a filter whose poles and zeros are interpolated between them. The interpolation is on the pole and zero parameters -- log centre frequency, log bandwidth -- never on coefficients, so every point inside the cube is a stable filter by construction; interpolated coefficients are not, and a filter that is stable at its corners can be unstable halfway between them. The 155 shapes are generated from published acoustics rather than typed: the mode series of bars, bells, membranes and pipes from Fletcher and Rossing, the formant tables of sung vowels, the modes of a room from its dimensions. The bank is normalised twice per control block from a probe grid that includes every pole and zero angle -- a fixed grid alone walks straight past a needle-sharp resonance -- so a shape can be extremely resonant without being loud.

Modal mode reads the same data as what it is. Six biquads in series take away what is not wanted and stop when the input stops; six two-pole resonators in parallel, each with its own decay, keep ringing after the input has gone, which is what a bar, a bell, a membrane or a room does. That is modal synthesis in the sense of Smith and Bilbao, and it cost almost nothing to add because the mode series were already in the building, being used as filter frequencies. Damping decides how much faster the high modes die than the low, from none at all -- which no real object does -- to inversely proportional to frequency, which is roughly what wood, metal and skin do.

THE FOLD

The wavefolder after both filters is there because saturation cannot make a certain sound. A clipper flattens what will not fit, and a flattened wave gains shoulders: odd harmonics, falling off quickly, the sound of warmth or of overdrive. A folder reflects what will not fit back into the range, and a wave mirrored at the fold gains a whole family of high partials whose strength grows with the drive -- the metallic, industrial edge the dark-ambient literature reaches for. The curve here is a sine divided by its own gain, so it is smooth everywhere (nothing to alias off) and passes small signals unchanged; the positive half is driven a third harder than the negative, because a symmetric folder makes odd harmonics only and stays hollow, and the even harmonics are where the body is. The makeup gain was measured: a 0.3-amplitude sine holds its level to within 1.3 dB across the knob, and a loud input loses about nine decibels at the top, which is not a fault -- past the first fold the fundamental itself is being folded away.

THE STRIKE

Strike is a Karplus-Strong loop -- a string, or a wooden body two octaves up with heavy damping, or a metal one with an all-pass in the loop -- excited by a noise burst at note-on and placed on the near plane whatever the voice's distance. It is the attack the instrument otherwise never has. The ear takes proximity from onsets: a sharp attack close and dry in the centre reads as near, and a foreground that has one makes the background behind it read as vast. At a low level it is what makes a pad sound touched.)" },
    { "Design III: space, depth and width",
R"(This is the chapter the instrument is built around. The spatial model comes from Robert Rich's practice as much as from the literature, and the literature it draws on is the psychoacoustics of localisation and distance: how the ear decides where a sound is, how far away, and how wide.

ONE NUMBER PER NOTE

Every note has a distance between 0, at the ear, and 1, the infinite background. The brain places its notes bimodally -- forty per cent close, sixty per cent deep -- and the keys sit at Keys Depth. From that one number the voice derives four things at once, and they are the four cues the ear uses to judge distance.

Brightness: per unit of distance the filter closes by two and a half octaves. Air absorbs high frequencies far more than low ones -- the molecular relaxation of oxygen and nitrogen -- so a distant sound is dark, and the ear reads darkness as distance even when the level says otherwise. Level: minus six decibels per unit, the inverse-distance law, though far less than the darkening because level alone is a weak cue -- a quiet close sound and a loud distant one are told apart by their spectra, not their level. Dryness: the voice is split between the near bus and the far bus by cosine and sine of its distance, so a close voice is heard dry and a far one only through the far reverb, the way a distant source arrives with its direct sound buried in the room's. And presence: a broad bell around three kilohertz, up to six decibels, scaled by one minus the distance, so a note at the ear gets the full lift and a note on the far plane none. That is the proximity effect a close microphone has, and the reason a foreground voice sounds articulate rather than merely loud.

Because the four cues move together from one number, a voice that moves in depth moves believably; a voice whose brightness and reverb were set by separate knobs would have to be adjusted in four places to move at all. Breath lets every voice's distance wander by up to a third of the plane on its own slow curve, and everything hanging on the distance moves with it: the room breathes. Doppler adds the last cue -- a voice coming closer rises a few cents and one receding falls, one plane unit taken as about twenty metres -- which the ear reads as approach and retreat.

WIDTH FROM TIME, NOT FROM LEVEL

Stereo width in this instrument is built on the duplex theory of localisation. Below roughly 1.5 kHz the ear takes direction from the interaural time difference -- a sound arriving at one ear a fraction of a millisecond before the other -- and above it from the interaural level difference, because the head shadows short wavelengths and not long ones. A pan pot only makes level differences, which is the wrong cue for the part of the spectrum a drone lives in. So every voice is rendered with a true interaural time difference: up to 0.65 ms of fractional delay on the far ear's channel, from the voice's pan, gliding rather than jumping. Width from time survives a mono sum in a way width from level does not, and the default patch's stereo correlation fell from 0.36 to 0.11 when the model went in.

Phase Width goes further in the same direction: two all-pass stages per ear whose corners drift apart and back, so the phase between the ears changes slowly while the level does not, and the ear reads a room changing size rather than a sound moving. Externalise adds the two cues a headphone image needs to leave the head -- the notch the pinna cuts into what arrives from the side, whose frequency moves with the angle, and the reflection off the shoulder a quarter of a millisecond later -- the parts of Brown and Duda's structural model that need no measured head. On speakers the room supplies those cues and the switch is left off.

The Haas effect -- one channel delayed by ten to thirty milliseconds -- is the classic widener and the classic mistake: summed to mono the two channels comb, and the first notch lands in the bass. Here it is done to one band only, between about 1.2 and 4 kHz where the ear takes its direction from level and is least troubled by a delayed copy, and the delayed band is put into the side channel -- added on the left, taken off on the right -- so a mono sum is exactly the picture it was, to the sample. The edges open; the bass and the top stay where they were.

The Ensemble's Microshift is the same argument applied to detuning. A chorus at thirteen to twenty-two milliseconds with its taps modulating is a comb filter waiting to be summed; two channels detuned a few cents against each other and delayed by different amounts are never at a fixed phase difference, so there is no comb to cancel into. Its implementation was measured, because the textbook construction -- two taps half a cycle apart under a Hann window -- has both taps audible all the time at a fixed delay difference and on a sustained tone lost a fifth of the signal; the version that ships uses a long ramp and a short hand-over, so the two taps overlap for a thousandth of the cycle.

THREE TIERS, AND THE FUNNEL

There are three reverbs because a room has three kinds of reflection. The near reverb is the small room around the dry voices: short, bright, what makes a foreground sound placed rather than pasted. The far reverb is the infinite background, an eight-line feedback network with a decay in tens of seconds, dark, wide, and heard by the far plane alone. The convolution room is a measured or designed space -- or, with the struck impulses, an object -- in parallel on the far plane. Pre-delay on each is the distance between the direct sound and the first reflection, which the ear reads not as the room's size but as where the source stands in it: a long pre-delay puts the source close and the wall far, a short one merges it with the room.

The far reverb's Asymmetry stretches the right-hand lines and delays the right output slightly, so the two ears hear different reflections -- decorrelation, which is what the ear needs from a reverb to hear it as space rather than as a wash. Its Width is the funnel: the background's own stereo width, pulled in towards the centre while the foreground stays wide. A mix in which everything is spread as far as it will go is a flat wall, because width is a relative cue and a wall of equal width has no depth in it; a narrower background behind a wider foreground reads as distance. Every reverb has a low cut beside its high cut, the filter funnel a mixing engineer puts on a return: dense tails pile up between 200 and 450 Hz, and that is where a background stops sitting behind the music and starts covering it. Unmask lets the background step aside for the foreground band by band -- three bands, fast to duck and slow to return -- which is the frequency masking of the ear turned into a control: a pad that does not duck its own reverb swallows its own notes.

THE MASTER, AND WHY THERE IS NO COMPRESSOR

Bass Mono high-passes the side channel below about 150 Hz. Below roughly 120 Hz the ear carries no directional information worth having, and a low end that differs between the channels only cancels on a mono system and overloads a vinyl cutter; a mono low end under a wide picture is also what makes the picture feel anchored. Side Air lifts the side channel around three kilohertz, where the ear's directional hearing is sharpest, so the width is heard rather than merely present. The mono guard measures the side against the mid over a second and a half and eases the width back only if the side is half again the mid's power -- a threshold that had to be measured, because at "side louder than mid" the guard engaged on thirty of thirty-seven reference presets, minutely, and a safety net that is always slightly on is a change to the sound.)"
R"(

There is no compressor anywhere, and that is a decision, not an omission. The impression of an enormous room comes from the distance between the quietest texture and the loudest swell, and a limiter takes the finest amplitude movement out of a reverb tail and leaves it flat. Instead the instrument measures itself to ITU-R BS.1770: integrated loudness, short term, range, true peak and crest factor, with the window a dark-ambient master is asked to land in marked on the meter and the streaming services' normalisation line drawn across it, so that the person mixing can see what a compressor would have hidden.)" },
    { "Design IV: tuning, harmony and the conductor",
R"(Just intonation is not a period flavour in this instrument; it is the reason the voices can be as many and as slow as they are. This chapter is about why, and about the conductor that plays them.

WHY JUST INTONATION

Two tones in a simple ratio -- 3:2, 5:4 -- share partials: the fifth's second harmonic is the root's third, the major third's fourth is the root's fifth. In equal temperament those partials are a few cents apart and beat, and a chord of six voices with thirty-two partials each is a field of slow beats that never resolves; the ear hears it as busy, and after a while as tired. In just intonation the shared partials coincide exactly, the beating vanishes, and the chord locks into a single complex tone the ear can rest inside for an hour. That is the roughness theory of consonance -- Helmholtz's, refined by Plomp and Levelt -- and it is the physical reason a sleep concert is tuned the way it is. The instrument's built-in scales are the just ones (Ptolemy's major, a just minor, a seven-limit scale, the Pythagorean, a just pentatonic, the harmonic and subharmonic series, slendro, Bohlen-Pierce, an otonality), 12-TET for comparison, and any Scala file.

Purity is the blend between the two worlds, per note, in the log domain: at one the partials lock, at zero they beat like a piano, and in between the beating slows as the intervals close in on their ratios. Purity Drift lets that blend wander over minutes, so the lock-in comes and goes -- harmony that breathes in and out of tune. Tide leans the whole instrument's pitch by up to thirty cents on a minute-scale curve, and the sub follows, so the harmony stays while the pitch centre drifts the way an organ's does with the temperature of the room.

THE INSTRUMENT LISTENING TO ITSELF

Two mechanisms use the physics of intervals as a source of control. The Foundation's Difference mode plays the sub on the combination tone of the two lowest voices: in just intonation two tones produce a tone at their frequency difference in the ear itself -- a fifth gives half the root, a fourth a third of it, a major third a quarter -- and Rich reports these tones carrying so much energy in his concerts that he tames them in mastering. Here the instrument plays the one the ear would make, folded into the sub's register, so the bass is the harmony's own.

BEAT is a modulation source whose rate is the interval's mistuning. Two voices a fifth apart beat at the difference between the harmonics that would coincide if the fifth were just -- twice the upper against three times the lower -- which is silent when the interval is in tune and quicker the further it has drifted. BEAT finds the simplest just ratio near the interval the two lowest voices make and runs at exactly that rate; below a fiftieth of a hertz it holds still, because a chord in tune should leave whatever it drives exactly where it is. Routed at a filter or at the Nebula's smear, the sound breathes at the rate of its own mistuning, and Purity Drift is what sets it moving. The test that verifies it compares an octave (2:1 in every temperament there is: 0.000 Hz) with a tempered fifth (0.469 Hz, two cents narrow at that pitch), because the first version compared two tunings and measured the tuning system instead of the source.

THE CONDUCTOR

The Cluster Brain chooses notes from the scale, places them on the planes, holds them for minutes and lets them go. Its choices are weighted, not random: every note in range not already sounding gets a weight from the consonance of its ratio to the root -- the simplest fraction within ten cents, scored by one over one plus the log of numerator times denominator, so unison is 1, the fifth 0.28, the major third 0.19, a semitone 0.11 -- raised to a power set by Consonance, times a bell over the register. Octave doublings of sounding notes are discouraged, the root's pitch class is favoured when nothing sounds it, and exact duplicate pitches are excluded. At Consonance 1 the brain plays only fifths and octaves; at 0 it plays clusters. Its events come at exponentially distributed intervals, the distribution with no rhythm in it, and its root wanders to the note whose ratio to the old root is closest to a fifth, a fourth, a third or a sixth -- a modulation by consonant step, the way a slow improviser moves.

Autoplay is the conductor in another mode: it keeps the cluster full and exchanges one voice at a time, in free steps or in chords drawn from the scale. One voice at a time is the point. The ear follows a chord as a stream, and a stream survives one of its members changing while the others hold; change them all at once and the stream breaks and a new one begins, which is a cut rather than a movement. Brain 2 is a second conductor for the background alone, with its own register, pace and density, on the first one's root plus an interval and with its own dice, so the two planes stop moving in step and the picture gains a second layer of time -- a slow counterpoint neither would play alone.

Portamento with Gravity is the microtonal glide of a lap steel: a new key slides in from the last over a chosen time, and the slide slows near the consonant ratios to the root and hurries across the dissonant stretches. The first version braked in proportion to the consonance of whatever ratio the slide was passing through -- a quantity that rises and falls smoothly across the whole glide, so the pull was everywhere and nowhere. It is now the distance in cents to the nearest just ratio, half strength at thirty cents and nothing between the nodes: a magnet has almost no reach and then all of it.

COHERENCE

Four slow oscillators coupled after the Kuramoto model -- the mathematics of fireflies falling into step, of pacemaker cells, of any population of rhythms that pull on each other -- move brightness, depth, pan and the z-plane point. At Coherence 0 they run free on their own periods (23, 31, 41 and 53 seconds over Rate: primes, so their cycles share no common period); turned up they lock and the instrument pulses as one. The coupling is deliberately asymmetric: with every pair pulling on the other equally, a high Coherence settled into exact synchrony and stayed there, four oscillators behaving as one, which is the opposite of what the section is for. Each is now pulled a little harder by the oscillator behind it in the ring than by the one in front, and an antisymmetric coupling has no synchronous fixed point, so the bank locks in frequency and keeps a slowly turning spread of phase -- which is what a ring of coupled biological oscillators does, and why they never look identical. Sympathy is coherence of another kind: a little of the whole foreground fed back into every voice through its own filter, so the voices hear each other.)" },
    { "Design V: movement, modulation and the loop",
R"(A drone that does not move is a test tone. This chapter is about how the instrument keeps moving without ever repeating, and about the paths by which the sound feeds itself.

RATES THAT NEVER LINE UP

Two modulations whose periods share a simple ratio repeat their combination quickly: two LFOs at ten and twenty seconds recur every twenty. Two whose periods are incommensurable never do. So the eight LFOs default to rates on a golden-ratio ladder -- 0.03 Hz times the golden ratio to the n-th power -- and the coherence ring's periods are primes, and the library generator draws its LFO rates on a golden ladder anchored on each preset's own base period, so no two presets share the same set of rates either. The rates themselves are drone rates: log-uniform between one cycle in eight seconds and one in forty minutes in the library, so the median route takes a bit over two minutes to come round. That is deliberate, and it means a short render shows little: the instrument is built for half-hour pieces.

The Drifter is the instrument's random LFO, and it is the reason "random" never means "stepped" here. It moves between random targets on smoothstep curves with zero slope at the knots, so the value, its rate of change and the direction of that change are all continuous. Rate Wander applies a Drifter to the rates themselves, the nested modulation that makes five minutes never resemble the previous five; Breath applies one to every voice's distance; Purity Drift to the tuning; Pos Drift to a wavetable's frame or a clip's read position.

ENVELOPES IN MINUTES

The voice's amplitude envelope is a plain ADSR with an attack of up to a minute and a release of up to two. The six modulation envelopes are drawn by hand, up to sixteen breakpoints with a curve on every segment, an optional sustain point and an optional loop. Their clock is a phrase clock: it restarts when a note arrives into silence, not on every note of a cluster, because a shape spanning a minute retriggered by every one of the brain's notes would never get anywhere. Each envelope has its own clock, and a Sustain Loop envelope's clock is placed exactly on the sustain point when the last voice lets go, so the tail plays from the value the hold ended on -- the first version snapped there, by half the shape, which is the one thing this instrument is not allowed to do.

THE MATRIX

Every modulator drives any knob through one matrix, and a depth is a fraction of the target's own range, so the same number means the same thing on a cutoff in hertz and on a mix in nought to one. The routes are data, not parameters: they travel in the preset as text, the way a Scala scale does, because thirty-two rows as four parameters each would put a hundred and twenty entries into a host's automation list for very little gain. Modulation is added after the Inertia glide -- a modulator moves at its own rate and is not slewed by the setting that exists to slow the performer's hand -- and a modulator's own rate or depth can itself be a target, read from the previous control block: one block of delay, under a millisecond and a half, which is the only way an LFO can move another LFO's rate without either needing the other's answer first. That nesting is the figure the ambient literature keeps coming back to, a thirteen-second sweep whose rate is itself moved by a nineteen-second one so the pair only repeats after 247 seconds.

The hands are sources too. Pressure and slide are read from the loudest voice, the wheel from the instrument, smoothed over thirty milliseconds so a seven-bit controller never steps a cutoff. All three rest at zero, and through the matrix's bipolar mapping zero is minus one, so a route wanting "nothing until I move it" carries the 0..1 flag -- which is what made it safe to put such routes into every preset in the library. The four Kuramoto oscillators, the note, its velocity and its distance, a random number drawn once per note, and BEAT are the rest of the list.

THE CIRCLE

Rich's drones are not a chain but a circle: what comes out of the reverb goes back in front of the filter or into the oscillators. The instrument keeps the previous block's output and reads it back two ways. To Bus adds it to the near bus ahead of every effect, throttled by the mix's own mean level so the loop hisses and holds instead of running away -- a loop near unity gain circulates through the delay's cross-feed until both ears carry the same thing, which is how the first threshold collapsed a preset's stereo correlation to 0.6 before it was lowered. To Pitch phase-modulates every partial of every voice by the fed-back signal, and this path is not throttled, because phase modulation moves energy between partials without adding any: a loud drone keeps its modulation. Tape in the loop adds an asymmetric saturation, wow and flutter as a moving read point, and a noise floor that rises with the loop's level, so every generation through the loop goes a little softer and less stable, like a forty-year-old tape.

The Cosmos is a second circle, in parallel, added and never replacing. Its frequency shifter is a single-sideband shifter built from a ninety-degree all-pass pair, and its right channel is shifted three per cent less than the left, so a 200 Hz shift beats at six hertz between the ears -- alien, and wide. Its resonators follow the brain's root; its vowel filter morphs between sung formants on a slow curve; its Nebula is a spectral smear with random phases that at full Smear freezes what is in the buffer; and its Shimmer pitch-shifts the far reverb's output back into the far reverb's input, throttled by the reverb's own level so it blooms to a ceiling and holds there instead of running into the clipper. Blur is the Nebula pointed at the foreground: every attack wiped into texture, notes flowing into one another.

THE ROOM AS A THING

Four stages are about the room the sound is in rather than the sound. The Body is a soundboard: twelve modes tuned to the brain's root, in wood, plate, bell or string ratios, fed from the whole mix and returned into it. Two things had to be measured. A resonator normalised to unity at its peak passes almost nothing of a broadband signal -- the Body knob moved the mix by two hundredths of a decibel -- so the modes are normalised on their expected power, the way the Air band is; and modes scattered at random around the centre are all driven by the same signal and so correlate the two channels (a width of 0.69 collapsed to 0.11), so neighbouring modes sit on opposite sides. The Patina is the master's age -- wow and flutter, lost highs, a noise floor that rises a little with the signal, which is modulation noise and what makes a floor sound like tape rather than like dither, and a gentle saturation -- because every one of those is a defect, and together they are most of what separates a recording from a render. The DC blocker at four hertz exists because several paths can leave an offset behind -- FM at an integer ratio, the tape stage's asymmetric term, a granular window over a clip that carries one -- and an offset costs headroom in the soft clipper without ever being heard.)" },
    { "Design VI: the library, and how it was measured",
R"(Six thousand four hundred presets is not a number anyone can audition, and the design of the library follows from that: nothing in it is described, everything is measured.

STYLES AS RANGES

Each pack is a style: a set of ranges over the instrument's own parameters, written in the spirit of an artist who works in one corner of the drone repertoire -- nothing sampled from or affiliated with any of them -- plus a list of which of the instrument's modules the style reaches for and how often. A preset is drawn from those ranges by a generator that also chooses its sources, its clip, its impulse, its matrix and its envelope shapes, half of each pack still and half astir. The generator is deterministic on its seed, so the same library can always be made again with the same names, which is what lets a pack name a sample that has not been rendered yet.

MEASURED, NOT DESCRIBED

Every preset, built in or generated, is rendered offline -- twelve seconds, a held chord and a busy brain -- and its descriptors are taken from the render: spectral centroid and flatness, flux, stereo width, energy below 150 Hz, the mean voice count. Those become the browser's brightness, motion, width, noisiness, bass and density, the tag bits the columns filter on, and the position on the preset map, which is the first two principal components of the standardised descriptors with the module flags weighted in so the Cosmos, the feedback and the source presets form clusters of their own, followed by a short repulsion pass so that no two points overlap. A point on the map is where a preset sounds, not where somebody put it. The same pass corrects every preset's master gain to the loudness it actually came out at, which is what keeps a library of thousands from having a few dozen presets that jump out, and the browser's level matching trims the gain towards a common target when a preset is loaded so that auditioning a hundred of them is not a ride on the volume knob.

The map's empty space is playable because the blend between presets is defined in the parameter domain: the six nearest points with Gaussian weights of the distance, floats mixed in the skew domain, integers rounded, choices taken from the strongest neighbour, and every parameter gliding towards the blend at the morph's rate. On a point the blend is that preset; between points it is a sound nobody saved.

NEUTRAL AT THE DEFAULT

A rule that shaped the last several rounds of the instrument's growth: every feature added after the first presets existed is neutral at its default, so nothing that was finished sounds different. The Vector's centre is exactly one; the far reverb's width is one; the fold, the Haas band, the unmask, the body, the patina, the tide and the rotation are zero; a new source type's index is appended so the indices stored by thousands of presets do not move; a new random source seeds from a side stream so switching it on never moves the brain's dice or a preset's random phases. A sound oracle -- forty presets rendered and hashed before and after every change to the core -- enforces it, and has caught the cases where it was violated by accident: a fourth random fork in the voice that advanced every stream by one draw, a sampling of the packs that shifted when a pack was added. Where the library was then changed on purpose -- the retrofit that gave every preset the mixing desk's features that suited it -- it was done from each preset's own settings, rendered before and after, and re-measured afterwards.

THE SOURCE MATERIAL

The clips, wavetables and impulses the library plays are generated, and generated with the same discipline. The textures and the five hundred field recordings come from text-to-audio models pointed at instruments and at places; every field recording is made seamless by construction -- the last second and a half faded into the first, the overlap trimmed, the seam measured -- and marked in its file name, which is what the Stretch type reads to skip its own crossfade. The wavetables are spectra generated from recipes or analysed from sound. The impulses are not only rooms: tuned partial banks that ring in key, inharmonic modal metal, reversed swells, combs and pipes, spectral bands with different decay times -- because a convolution reverb fed a drone is a resonator you can shape -- and forty struck objects cut from the field recordings, the sharpest event in a recording of a foundry or a cistern shaped into an impulse of a tenth to half a second. Convolution transfers the resonance of that object onto the synthetic wave, poles, zeros and all, which no filter can fake, and measured on the room stem a pad through a struck object is five to fourteen decibels different from the same pad through a hall, band by band.

WHAT THE LITERATURE SAID, AND WHAT WAS DONE

Two production papers on ambient and dark-ambient sound design were checked against the instrument late in its development, and most of what they ask for was already present: bass mono below the localisation limit, a side lift in the region of sharpest directional hearing, three reverb tiers with pre-delay and low cuts, air absorption with distance, free-running modulators on incommensurable rates, comb filters modulated by slow random curves, Paulstretch, a loudness meter and no limiter. Five things were not, and became parameters that are neutral at their default: the hands as modulation sources, the background's own width, the microshift, the band-limited Haas effect and the wavefolder. Two of the five were wrong in their first version and were caught by measurement rather than by ear -- the textbook microshift that was a comb filter on a sustained tone, and a Haas cross-feed that, fed a mono signal, produced no width at all. Both are described honestly in the chapters above, because the instrument's manual is also its record.

WHAT IS NOT CLAIMED

Some things this instrument does not do, and does not pretend to. It has no compressor, no limiter and no dithering, and does not intend to; mastering is a separate craft with its own tools. The four-pole ladder filter is not zero-delay and is not going to become so in place. The Quest application builds against the same core and has not yet been run on a headset. The manual's pictures are of the running instrument and its numbers are measured, but the psychoacoustics quoted here is the standard account -- the duplex theory, roughness as the basis of consonance, the structural model of the head -- rather than the frontier of the field, and it is quoted because it is what the design used, not as a claim to have tested it.)" },
};

// ---------------------------------------------------------------- the blocks, one by one
//
// What each tab of the panel IS, in a paragraph: the manual prints it under the tab's picture,
// over the list of that tab's parameters. The parameter texts say what a knob does; these say
// what the thing the knobs belong to is for, which is the question a reader has first.
struct TabHelp { const char* name; const char* text; };
const TabHelp kTabHelp[] = {
    // ---- the source row
    { "SOURCE 1", "The first of four equal source slots, and the one with a history: set to Additive it is the strand bank -- up to six copies of a partial bank, detuned or placed on pure ratios, fanned across the stereo field -- and the Strands section under its display belongs to it alone. Set to any other type it renders exactly like the other three. Every slot has a Type, a Level, an Octave, a just Ratio to the note and a Pan; the rest of its knobs light up according to the type." },
    { "SOURCE 2", "The second slot. Where Source 1 carries the melody of a patch, the second is most often its body or its shadow: an octave down at a fraction of the level, a wavetable with a slow position drift under an additive bank, a noise floor. Its own Partials, Tilt, Brightness, Odd/Even, Inharmonic and Shimmer apply when it is Additive; Table and Position when it is a Wavetable; FM Ratio and Index for FM; Grain, Density, Pitch and Grains for the Texture and Stretch types; Noise colour and Q for the noise." },
    { "SOURCE 3", "The third slot, with the same controls as the second. Three sources of the same kind a fifth and an octave apart are a chord out of one key; three of different kinds are an instrument. The Texture... button loads a clip into this slot alone, so it can play a recording the other slots do not." },
    { "SOURCE 4", "The fourth slot, added with the Vector so the four corners of its square are four sources. Off by default -- a preset that did not know about it sounds as it did -- and otherwise identical to slots 2 and 3, with its own clip." },
    { "VECTOR", "The four slots read as a place rather than as four levels, after the Prophet VS and the Korg Wavestation: a point in a square whose corners are the four sources. Amount is how much of the picture the point paints -- at 0 every slot plays at its own Level and nothing here does anything; the centre of the square is neutral by construction, so turning Amount up changes nothing until the point moves. X and Y place it, Wander lets it drift on two curves whose rates share no simple ratio, Rate is how fast. Route the point from an LFO, a macro or the wheel and one gesture moves through four landscapes." },
    // ---- the voice's second row
    { "FILTER", "The voice filter, one per voice, ten models behind one set of knobs: one- to four-pole low passes, a high pass, a band pass, a notch, a peak, the saturating ladder, a tuned comb and a formant. Cutoff follows the key (Key Track), the amplitude envelope (Env Amount), a slow wander (Drift) and the voice's distance -- a far voice is two and a half octaves darker per unit of depth. Drive saturates ahead of the filter; Fold is the wavefolder after it. The Air section beside it is the noise on the note: a band around a harmonic, or six resonators on the just harmonics." },
    { "Z-PLANE", "The second filter, after the E-mu Morpheus: filter frames sit on the corners of a cube, and a point inside it is a filter interpolated from all of them, poles and zeros alike, so every point is stable. 155 shapes in twelve families. The point wanders (Rate, Depth) around X and Y; Z is the cube's third axis; Resonance narrows every section; Key Track moves the frame with the note. Mode puts it after the voice filter, in its place, or -- Modal -- turns the frame into a bank of ringing resonators struck by the voice. Route decides whether the two filters run in series or side by side." },
    { "AMP ENV", "The amplitude envelope of every voice, in the time scale of this instrument: an attack of up to a minute, a release of two. There is no click anywhere in it by design -- a step in level is a bug here, not an effect. The six modulation envelopes on the strip along the bottom are separate and shaped by hand; this one is the four numbers everybody looks for first." },
    { "EXPRESSION", "What a hand on the keyboard can do beyond playing the note: pressure (channel or polyphonic aftertouch) pulls the voice out of the background towards the ear and lifts its level; slide (CC 74) moves the z-plane point; the bend range is here too, and the MPE switch that gives every finger its own channel. All of it is smoothed inside the voice, so a controller sending steps never steps the sound. The same three -- pressure, wheel, slide -- are also sources in the modulation matrix, for anything these fixed routes do not cover." },
    // ---- morph
    { "MORPH", "Two complete snapshots of every parameter, A and B, and a position between them. While Morph is active the instrument plays the interpolation, gliding to the position at the Glide rate -- one continuous gesture, made for a hand in VR, that moves the whole instrument from one world to another without a jump anywhere." },
    { "MACROS", "Eight knobs that mean nothing by themselves and anything through the matrix: route a macro at three targets and one hand turns three knobs at once, in the proportions you chose. They are what the OSC hands, the gestures and a controller's faders land on. Inertia is the slew every parameter passes through -- the analogue slowness that keeps even a torn-open knob from clicking." },
    // ---- foreground
    { "ENSEMBLE + DELAY", "The first two stations of the foreground bus. The Ensemble widens: as a Chorus, three modulated taps; as a Microshift, the two channels detuned a few cents against each other with nothing moving -- the version that survives a mono sum. The Delay is a stereo delay with independent left and right times (or note values, with Sync), feedback, cross-feed for ping-pong, damping and absorption in the loop, and two outputs: Mix onto the foreground, To Far into the background, so echoes recede. Duck pulls the loop's brightness down while the input is loud, so a fresh attack does not fight its own last echo." },
    { "DELAY 2 + NEAR REVERB + BLUR", "The rest of the foreground. Delay 2 is a second stereo delay in series after the first, so echoes of echoes form chains that never fall on a grid. The Near Reverb is the small room around the dry voices -- Mix, Decay, Damping and a Low Cut -- what makes a foreground sound placed rather than pasted. Blur is a spectral smear on the near bus ahead of all of it: every attack is wiped into texture, notes flow into one another, and at full Smear the spectrum freezes and only lets new energy in slowly." },
    // ---- background
    { "CLOUD + FAR REVERB", "The background. The Cloud takes grains of the recent foreground -- Send how much, Density how many a second (or a note value), Size how long, Spray how far back in time it reaches -- transposes them by octaves and fifths and drops them into the far reverb, so the past of the music keeps arriving from behind. The Far Reverb is the infinite background itself: an eight-line feedback network, dark and wide, with a decay measured in tens of seconds, Pre-Delay, Asymmetry so the two ears hear different reflections, a high cut and a low cut on the tail, Freeze, Rotate (the whole field slowly turning), Unmask (it steps aside for the foreground band by band), Diffuse (the tail arrives instead of starting) and its own Width, the funnel that reads as distance." },
    { "FEEDBACK + ROOM", "Two ways of making the instrument hear itself. Feedback returns the finished mix: To Bus into the near bus ahead of the filters and effects, throttled by the output level so it hisses and holds instead of running away; To Pitch as phase modulation of every partial, so the sound bends itself; through Tone, Drive and Tape, which adds the asymmetry, the wow and the noise floor of a machine. The Room is the convolution reverb, on the far plane in parallel: an impulse response loaded with the Impulse... button or named by the preset -- a hall, a plate, a tuned chord, a struck object -- with Pre-Delay, a high cut, a low cut, and Morph between two impulses." },
    { "BODY + PATINA", "The last two stages before the master. The Body is not a reverb but an instrument: twelve tuned modes -- wood, plate, bell or string -- fed from the whole mix and returned into it, tuned to the brain's root at a chosen multiple, ringing for as long as Decay says. The Patina is the master's age: tape wow, the highs a worn machine has lost, a noise floor that lives under the music, a gentle saturation. Every one of them is a defect, and together they are most of what separates a recording from a render." },
    // ---- the conductor
    { "BRAIN", "The conductor: chooses notes from the scale, places them on the planes between the ear and the background, holds them for minutes and lets them go, and can play a whole night by itself. Density is how many it keeps sounding, Rate how often it changes its mind, the Hold range how long a note lives, Register where it plays, Consonance how simple the ratios to the root have to be (1 is only fifths and octaves, 0 is clusters), Wander how far the root drifts. Off, only your keys play. Its display is the stage: every sounding voice as a dot at its distance." },
    { "AUTOPLAY", "The brain's other mode: instead of holding a cluster it exchanges one voice at a time, in Free steps or in Chords drawn from the scale, at a Rate or on the clock, with a Tension that decides how far each step may go and a Step button to make it move now. The way a patient improviser plays a chord instrument: nothing ever changes all at once." },
    { "BRAIN 2", "A second conductor for the background alone. With it on, the far plane gets its own slow life -- its own hold range, its own Depth -- independent of the foreground's, so the two planes stop moving in step and the picture gains a second layer of time." },
    { "TUNING", "What a note means. Scale chooses the tuning -- twelve just and historical scales and a Scala file of your own -- Root its centre, Ref Pitch its A. Purity is how close the instrument sits to the pure ratios, Purity Drift how far it lets them slip and at what rate, so a chord breathes in and out of tune; Tide leans the whole pitch over minutes; Portamento glides between notes, slowing near consonant ratios by Gravity. Hold latches the keys." },
    { "COHERENCE", "Four slow oscillators coupled after the Kuramoto model of fireflies falling into step. At Coherence 0 they run free on their own periods; turned up they lock into one pulse and move brightness, depth, pan and the z-plane point together. The four are also sources in the matrix, so anything can be pulled into that shared breath. Sympathy is a different kind of coherence: the voices hear each other, a little of the whole foreground fed back into every voice through its own filter." },
    { "CLOCK", "Where the tempo comes from -- the internal Tempo and Run, the host, or MIDI clock -- and the beat every Sync choice in the instrument is measured against. Nothing here has to be used: the instrument's own rates are in seconds and minutes, and a synced LFO is a choice, not the default." },
    // ---- cosmos and strike
    { "COSMOS", "A parallel path, send and return, added and never replacing: a frequency shifter (Shift, with a Drift so the shift never sits still), tuned comb resonators that follow the brain's root (Res, Res Pitch), a vowel filter morphing through a-e-i-o-u (Vowel, Vowel Rate), a Nebula that smears the spectrum with random phases until at full Smear it freezes, and a Shimmer loop around the far reverb, self-regulating so it blooms and holds. Return puts the result into the foreground, To Far into the background. Thirty-two presets of its own live in the header." },
    { "STRIKE", "A struck layer on top of the voice: a Karplus-Strong string, a wooden or a metal body, excited at note-on and left to ring. Level, Type, Decay and Damp; Who decides whether only the keys strike or the brain's notes as well. It is the attack this instrument otherwise never has, and at a low level it is what makes a pad sound touched." },
    // ---- sections without a tab of their own
    { "STRANDS", "The strand bank of Source 1: up to six copies of the partial bank, detuned against each other (Detune) or placed on pure ratios (Stack: octaves, fifths, a just major or minor, sevenths, harmonics, subharmonics -- one key becomes a just chord), each drifting in pitch on its own curve (Drift, Drift Rate), fanned across the stereo field (Spread). Bloom opens the brightness over Bloom Time from a duller start; Rate Wander lets every slow rate in the voice vary by up to an octave on a hundred-second curve, so nothing repeats; Freeze holds the spectrum still." },
    { "SPACE", "The spatial model, after Robert Rich: every note has a distance between the ear and the infinite background, and that one number decides its brightness, its level, how dry it is and how present. Depth is how deep the brain places its notes, Keys Depth the plane of the keys, Pan Drift the wandering of each voice's centre, Time Width the interaural time difference the far ear hears later. Arc is the hour-scale drift of the whole night. Presence is the 2-5 kHz lift on the near plane only; Breath lets every distance wander; Phase Width and its rate drift the phase between the ears so the room seems to change size; Doppler bends the pitch of a voice as it breathes closer; Externalise adds the pinna notch and the shoulder reflection headphones need to put the image outside the head. Haas and Haas Time are the band-limited widening of the foreground." },
    { "FOUNDATION", "The sub: one dry sine or triangle on the brain's root, one or two octaves down, gliding between roots, mono, added after the mid/side stage so Bass Mono leaves it alone. Binaural runs the two ears a few hertz apart. Source can be the root itself or the Difference tone of the two lowest sounding voices -- the tone the ear makes by itself in just intonation. Pad Low Cut keeps the voices out of the sub's register." },
    { "MASTER", "The end of the chain, in the header: Tilt, a see-saw of the whole spectrum around Pivot; Bass Mono, the side channel high-passed so the low end stays centred; Side Air, a lift on the sides at 3 kHz; Width; Subsonic, a steep high pass on the finished output; then the master gain and a soft clipper. No compressor anywhere. The loudness meter beside it reads the output to BS.1770." },
    // ---- the strip
    { "LFO", "Eight low-frequency oscillators, each with a shape (sine, triangle, ramps, square, a smoothed random, stepped random, or a wavetable), a rate in hertz or a note value, a phase, a depth and a mode: global, one per voice, or retriggered by each note. Their cards are dragged onto knobs; right-click a knob to see what drives it. The rates worth using here are drone rates -- one cycle in ten seconds to one in forty minutes -- and two rates that share no simple ratio never repeat their combination." },
    { "ENV", "Six modulation envelopes, drawn by hand as points on a curve: any number of segments, a sustain point, and three modes -- one shot, loop, or a loop that holds at the sustain point until the key is released. Time scales the whole shape (or a note value spans it); Depth is how much. A shape that rises over four minutes and falls over eight is an envelope in this instrument's sense of the word." },
    { "MATRIX", "Every route, one row each: a source, a target, a depth as a fraction of the target's own range, an optional second source that scales it (Via), and whether the source is read as 0..1 or -1..1. Sources are the LFOs, the envelopes, the voice's own amplitude, the macros, the Kuramoto ring, the note, its velocity and its distance, a random number per note, the Beat, and the hands -- aftertouch, wheel and slide. A modulator's own rate or depth can be a target as well: an LFO whose rate another LFO moves." },
    // ---- the pages
    { "PERFORM", "The page for playing rather than patching: the macros large, the morph, the map cursor, the note roll and the stage, and the set recorder -- Record set logs every knob, macro, route step and note with its time into a file, Play set replays it." },
    { "BROWSE", "Every preset the instrument knows, built in and from the packs, in one list: narrowed by Family, Character, Motion and Features, searched, sorted, starred. Every descriptor was measured by rendering the preset, not tagged by hand. The Map shows the same presets as points clustered by what they sound like; click one, or switch on Map blend and drag the cursor to play the blend of the presets around it. A Route walks the map by itself." },
    // ---- the source types, for the gallery
    { "TYPE Additive", "A bank of up to 32 partials with lives of their own. Partial h has amplitude h to the power of minus Tilt; Brightness fades the upper ones out; Odd/Even weights the two families; Inharmonic stretches the series like a stiff string; Shimmer lets every partial drift in level on its own slow curve. Partials above Nyquist are never generated, so nothing aliases. In Source 1 this is the strand bank." },
    { "TYPE Wavetable", "Not a table of samples but a table of spectra: 32 partial amplitudes per frame, up to 64 frames, and Position morphs between them while Pos Drift wanders it. Five tables are built in and User loads a WAV in the Serum/Vital layout. Alias-free like the bank, and every trick that works on partials -- presence, low cut, the feedback's phase modulation -- works here." },
    { "TYPE FM", "A two-operator pair: the carrier at the slot's pitch, the modulator at FM Ratio, the index up to 8 and reduced automatically on high notes so the top of the keyboard does not turn to noise. Pos Drift wanders the index. Integer ratios are bells and electric pianos; a ratio a little off an integer is a bell that beats." },
    { "TYPE Texture", "A granular player over a loaded clip: up to 64 grains (Grains) of Grain length, Density a second or per note value, starting around Position with Spread, pitched to the note (Pitch = Note; the clip's own pitch comes from its file name) or played as it is. The display shows the grains reading the clip. The Texture... button loads a clip into this slot; a pack preset names its own." },
    { "TYPE Stretch", "The same clip read as a continuum instead of as grains -- a spectral time stretch after Paulstretch. A window (Grain) of the clip is transformed, its magnitudes kept, its phases drawn afresh and the result overlap-added, while the read position crawls through the recording at one Stretch-th of its speed. No grain rhythm, no transient left standing: a field recording becomes weather. Pitch is applied before the stretch, so a note played higher does not get shorter, and Loop Fade crossfades the loop's seam unless the clip's name says _loop." },
    { "TYPE Noise", "Ten colours: White, Pink, Brown, Blue, Violet, Grey, a resonant Band at Position with Q from Noise Q that tracks the note, Wind (a wandering band), Crackle (sparse impulses at Density) and Digital (sample-and-hold at a rate from Position). The levels are matched, so changing the colour does not change the loudness." },
};


} // namespace

const char* tabHelp(const char* name)
{
    if (name == nullptr) return "";
    for (const TabHelp& t : kTabHelp) if (std::strcmp(t.name, name) == 0) return t.text;
    return "";
}

const char* paramHelp(ParamId id)
{
    const int i = static_cast<int>(id);
    return (i >= 0 && i < kNumParams) ? cache().text[i] : "";
}

int numHelpTopics() { return static_cast<int>(sizeof(kTopics) / sizeof(kTopics[0])); }
const char* helpTopicTitle(int index) { return (index >= 0 && index < numHelpTopics()) ? kTopics[index].title : ""; }
const char* helpTopicText(int index)  { return (index >= 0 && index < numHelpTopics()) ? kTopics[index].text : ""; }

} // namespace ambient
