# AmbientSynth — concept and architecture

## Goal

A synthesizer for the kind of music Robert Rich played at his sleep concerts:
dense, slowly breathing clusters in just intonation, no rhythm, changes that
take minutes, a piece that can run all night without repeating and without
anyone touching it. It must exist as a VST3 for the studio, as a standalone
program for a night's run, and later as a native Quest app with hand-driven
control and a visual world around the sound.

## Guiding decisions

1. **The synthesizer is a library, not a plugin.** Everything that makes sound
   lives in `Core/` and depends on nothing but the C++20 standard library.
   `process(L, R, n)` never allocates; parameters are atomics written from any
   thread. JUCE is only a shell. On the Quest the same library will sit behind
   Oboe (audio) and OpenXR (input, rendering) instead.
2. **Every movement is continuous.** Random values never step; the `Drifter`
   produces smoothstep-interpolated random curves with zero slope at the
   knots. Envelopes are exponential, partial levels ramp across a control
   block, filter cutoff, delay and reverb lengths glide. A drone must never click.
3. **Measure, don't only listen.** `ambient_render` renders deterministically
   and prints per-second RMS, peak, voice count, root and arc; `analyze.py`
   reports clicks, stereo correlation, spectral centroid and the strongest
   peaks. The self test checks the tuning maths to 1e-9 and the effects by
   impulse and sine measurements.
4. **One parameter table.** `Core/include/ambient/Params.h` lists every
   parameter with range, default, skew, unit and section. The engine, the
   JUCE parameter layout, the GUI, the presets and the command-line renderer
   all iterate that table. Adding a parameter is one line in the table plus
   one line in `Engine::readParams`.
5. **Space is a landscape, not an effect** (after Rich). Depth comes from
   contrast between planes, width from time differences, focus from a mono
   low end; nothing is compressed.

## Signal path

```
MIDI / Cluster Brain
      │ note on/off + distance (0 = at the ear, 1 = infinite background)
      ▼
Voice (×16) = Strand (×1..6) = additive bank of ≤32 partials
      │  each partial: own phase, own slow amplitude drift ("Shimmer"),
      │  optionally phase-modulated by the feedback loop (h·θ per partial)
      │  strand: detune offset + slow pitch drift, pan around a wandering voice centre
      │  spectrum: h^-tilt · odd/even weight · brightness window · inharmonic stretch
      │  + Air: band-passed noise around a drifting multiple of f0
      │  → TPT state-variable low-pass (key track, env, drift, −2.5 oct per unit distance)
      │  → ADSR (seconds to minutes) · (1 − 0.5·distance)
      │  → interaural time difference from the centre pan (≤ 0.65 ms, the far ear later)
      │  → near gain cos(d·π/2) → NEAR bus ; far gain sin(d·π/2) → FAR bus
      ▼
NEAR: Ensemble → StereoDelay (asymmetric L/R, cross-feed, damping) → Delay 2 (in series)
      ─┬─ mix → + Near reverb (small room)
       ├─ "to far" (both delays) ─┐
       └─ Cloud send → GrainCloud (grains of the recent foreground, sprayed back in
          time, octave/fifth transposed, stereo-scattered) ──► FAR ◄──────┘
FAR:  (+ delay echoes) → Far reverb: 8-line FDN, 4 input all-passes, per-line   │
      damping, slow length modulation, right group 8 % longer + right output   ◄┘
      delayed ≤ 10 ms (asymmetry), tail low-pass, freeze; 100 % wet · level
      ▼
Mid/Side: side high-passed at Bass Mono (low end centred), broad +N dB bell at
3 kHz on the side, width → master gain → cubic soft clip (no compressor)
      │
      └─ Feedback: the mix before mid/side → low-pass, saturation, level throttle →
         next chunk: into the NEAR bus (To Bus) and/or into the partials (To Pitch)
```

Why additive: partials above Nyquist are simply not generated, so there is no
aliasing at any pitch; every partial can have its own life (that is the
"breathing" of a Rich drone); brightness and inharmonicity become continuous
parameters instead of waveform switches.

### The spatial model in numbers

* Distance `d` per note. Brain notes are bimodal: 40 % land at `Depth·0.15·u`
  (close), 60 % at `Depth·(0.55 + 0.45·u)` (deep). MIDI notes take *Keys Depth*.
* Per unit distance: cutoff −2.5 octaves, level −6 dB, dry→wet crossfade by
  cos/sin, so the far plane is only heard through the dark far reverb.
* Interaural time difference: `0.65 ms · Time Width · |centre pan|`, applied
  as a fractional delay on the far ear's channel; it glides, never jumps.
* Far reverb asymmetry: lines 4–7 stretched by `1 + 0.08·a`, right output
  delayed `10 ms·a`. Left and right therefore hear different reflections.
* Mid/side: second-order high-pass on the side at *Bass Mono* (default 150 Hz),
  side bell at 3 kHz, Q 0.6, up to +6 dB.
* Measured on the default patch, 180 s: stereo correlation 0.11 (was 0.36
  before the spatial model), no sample jump above 0.06, level −21 … −28 dBFS.

### Carving the planes inside the voice

Rich lays the depth in the sound design, not in the mix: the background is
dark (air absorption), the foreground carries a presence lift and stays
dry, wide pads give up the sub register to one mono bass, and a very slow
modulation lets the room breathe. All four live inside the voice, in the
additive domain where they cost nothing per sample:

* **Presence** (Space, 0–6 dB): a parabolic bell in log frequency centred
  on 3.2 kHz, zero at ±0.8 octave (about 1.8–5.6 kHz), multiplied into the
  partial targets and scaled by `1 − d`, so a note at the ear gets the full
  lift and a note on the far plane none. Measured with a 32-partial C4 at
  +6 dB: the 2–5 kHz band gains a factor > 1.8 against partials 1–4 on the
  near plane and is unchanged on the far plane.
* **Pad Low Cut** (Foundation, 0–300 Hz): partials below the cut fall by
  `(f/fc)²` (12 dB/oct). The pads leave the bottom to the Foundation sub,
  which is mono anyway. Measured: a C3 with a 300 Hz cut loses more than
  80 % of its fundamental relative to its third partial.
* **Breath** and **Breath Rate** (Space): every voice's distance wanders by
  up to ±0.35 (at Breath 1) on its own `Drifter` at Breath Rate (default
  0.03 Hz, half a minute per swing). Everything hanging on the distance
  moves with it: dry/wet balance, level, filter, presence. `noteDistance`
  reports the breathing value, so the picture in VR breathes too. Measured:
  spread > 0.1 over 15 s at 0.2 Hz, no step above 0.05 per 100 ms.
* **Source = Difference** (Foundation): see the Foundation section — the
  sub follows the combination tone instead of the root.

Every partial is a rotating phasor: a (cos, sin) pair turned once per
sample by its own rotation (cos, sin of 2π·f_h/sr, refreshed at control
rate), renormalised once per control block so it stays on the unit circle.
No table lookup and no phase accumulator in the inner loop, and the
partials are independent of each other, so the loop pipelines and
vectorises. Measured on one core of an i9-12900K, 48 kHz: the effect chain
alone costs 1 % of a core; `ambient_render --bench` (all 128 presets, held
chord plus brain, 256-sample blocks) went from a median of 22× realtime
(table lookups) to 39×, the slowest preset from 11× to 16×; five voices with
six strands of 32 partials run at 17× harmonic or inharmonic alike. Two
dead ends on the way, kept here so they are not tried again: caching the
control-rate spectrum shape and doubling the control block gained nothing
(the inner loop dominated), and a single angle-addition recurrence per
strand was latency-bound; four interleaved chains helped (median 30×) but
the independent phasors beat them and are simpler. The presence bell, the
pad low cut and the breathing distance (below) were added afterwards in
the control-rate part of the voice; a re-run of the bench on an idle
machine gave a median of 43× and a slowest preset of 27×, so they are
free.

## Tuning

* `FixedScale`: up to 64 ratios per period, period ratio (2 = octave, 3 =
  tritave), copied into the audio thread without allocation.
* Built-in scales: 12-TET, Ptolemy major, JI minor, 7-limit (11 notes),
  Pythagorean, JI pentatonic, harmonic series 8–16, subharmonic 16–8, slendro,
  Bohlen-Pierce, otonality 1-3-5-7-9-11.
* Scala `.scl` parser (ratios and cents), user slot persisted in plugin state.
* Keyboard mapping: *Snap* (12 keys per octave, each snapped to the nearest
  degree — the intuitive way to play a 7- or 11-note JI scale) or
  *Consecutive* (n keys per period, for EDOs and non-octave scales). Snap
  falls back to Consecutive automatically when the period is not an octave.
* `intervalConsonance(ratio)`: octave-reduce, find the simplest p/q within
  10 cents (q ≤ 32), score 1/(1+log2(p·q)). Unison 1.0, fifth 0.28, major
  third 0.19, semitone 0.11, anything unrecognised 0.05.

## Cluster Brain

Runs at control rate inside the engine. State: up to 12 slots (note, time
remaining), a root note, a timer.

* Event timing: exponentially distributed intervals around *Event Rate*
  (clamped 0.5 s … 4× rate). First event 0.5 s after switching on.
* When a slot's hold time expires the note is released (the voice's long
  release does the fade).
* If *Density* is reached, an event either retires the note ending soonest
  (50 %) or does nothing.
* *Wander*: with probability 0.35·wander the root moves to the note in range
  whose ratio to the old root is closest to 3/2, 4/3, 5/4, 6/5, 5/3 or 8/5.
  A held MIDI key pins the root instead.
* Note choice: every note in [Lowest, Highest] not already sounding gets
  weight consonance(ratio to root)^(3·Consonance) × register bell curve;
  octave doublings of sounding notes ×0.15; the root's pitch class ×3 when
  nothing sounds it, ×0.25 when something does; exact duplicate pitches
  (possible with snapped keys) are excluded. Hold time uniform in
  [Hold Min, Hold Max], velocity 0.5–0.9.
* *Arc*: a `Drifter` with period *Arc Period* (minutes) scaled by *Arc*
  shifts density by up to ±2 voices, brightness by ±25 % and depth by ±30 %,
  so an all-night run has tides instead of a flat sea.

### Stack and Rate Wander

* **Stack** (Oscillator): instead of detuned copies, the strands sit at
  pure ratios to the note — Octaves (1 2 ½ 4 ¼), Fifths (1 3/2 2 3 ½ 9/4),
  Major (1 3/2 5/4 2 5/2 ½), Minor (1 3/2 6/5 …), Seventh (… 7/4), the
  harmonic and the subharmonic series — ordered so that two strands already
  make root + fifth and three a triad. Detune and drift still apply on top,
  so Detune 0 makes the chord beat-free: one key, one just chord, and the
  partials of the strands lock into each other (the fifth's second partial
  is the root's third). Measured: three strands, Major, one partial each,
  A3 → 220, 330 and 275 Hz, nothing else within 20×.
* **Rate Wander** (Oscillator, default 0.3): one 100-second `Drifter` per
  voice scales the pitch-drift, pan, filter, air, shimmer and breath rates
  by 2^(±wander), the nested-LFO idea — the movement itself speeds up and
  slows down, so a stretch of five minutes never resembles the previous
  five.

## Foundation, Bloom, Hold, Macros

* **Foundation** (`Engine::renderChunk`, after the mid/side stage so the
  binaural offset survives Bass Mono): one sine/triangle voice per ear on
  the brain's root ÷ 2 or ÷ 4, frequency glides in the log domain with time
  constant *Glide*, level rises over 2 s, ears offset by ±*Binaural*/2 Hz.
  Measured: root A3 gives 110 Hz an octave below; with 6 Hz binaural the
  left ear sits at 107 Hz and the right at 113 Hz.
  *Source = Difference* makes the sub follow the **ghost tone** instead: in
  just intonation two voices produce a combination tone `f2 − f1` in the
  ear itself (a fifth 3:2 gives f/2, a fourth 4:3 gives f/3, a major third
  5:4 gives f/4). Rich reports these tones between 55 and 440 Hz carrying so
  much energy that he tames them in mastering; here the Foundation doubles
  the one the two lowest sounding voices make, folded into the octave the
  root mode would use (so the register never changes), gliding as usual.
  With fewer than two distinct pitches it falls back to the root. Measured:
  A3 + D4 (4:3, 220 + 293.3 Hz) put the sub at 146.7 Hz, one voice left
  returns it to 110 Hz.
* **Bloom** (per voice): brightness = brightness × (1 − bloom × (1 −
  smoothstep(t / bloomTime))). Measured: with bloom 1 the first second
  carries less than a fifth of the high-partial energy of the opened state.
* **Hold**: note-on on a latched key releases it; note-off is ignored;
  switching Hold off releases all latched keys.
* **Macros** are the gesture inputs Custom0..3 fed from four parameters, so
  the same mapping table serves hands and knobs. A mapping only starts to
  write once its input has moved from its rest value (also true for head
  yaw), which keeps presets intact until the performer touches a macro.

## Cosmos path (science fiction / deep space)

A parallel send off the near bus after the delay; the dry path is untouched
and *Return* / *To Far* mix the processed signal back into the foreground and
the background. Chain, in order:

1. **FreqShifter** — single-sideband shifter built from Niemitalo's
   90° all-pass pair (eight second-order all-passes); measured sideband
   rejection 44 dB. The right channel is shifted 3 % less than the left, so a
   200 Hz shift beats at 6 Hz between the ears: alien, but wide.
2. **CombResonator** — two combs (right 0.3 % longer) tuned to the brain's
   current root × *Res Pitch*, feedback to 0.97, output normalised by
   (1 − feedback) so the resonant peak stays near unity gain.
3. **VowelFilter** — three SVF band-passes (Q 8) on formant tables for
   a/e/i/o/u, position driven by a `Drifter` at *Vowel Rate*, log-interpolated
   between neighbouring vowels; right channel 1 % higher.
4. **Nebula** — STFT (2048/512, Hann) magnitude smoothing with random phases
   per frame: Paulstretch-style smearing. Update coefficient
   α = (1 − smear)², so *Smear* = 1 freezes what is in the buffer. Latency
   2048 samples on this path only. Own FFT (radix-2), no dependencies.
5. **Shimmer** (in the engine, not the chain) — the far reverb's output is
   pitch-shifted (granular two-head shifter, 80 ms window) and fed back into
   the far reverb's input on the next block, low-passed at 4 kHz. The loop is
   throttled by the reverb's own level (feedback → 0 at a mean level of
   0.12), so it blooms and then holds; measured: stable at 1.0 for 20 s with
   the resonator at 0.97 feedback in the same patch.

## Feedback loop (the sound feeds itself)

Rich's drones are not a chain but a circle: what comes out of the reverb
goes back in front of the filter or into the oscillators. `Engine::renderChunk`
keeps the previous chunk's output mix (before mid/side, sub and master) in a
ring, low-passed at *Tone*, driven into a gain-compensated rational tanh
(*Drive* 0 → unity, 1 → ×10 into the curve). The next chunk reads it back
two ways:

* **To Bus** adds it to the near bus after the voices, i.e. before
  ensemble, both delays, the cloud and cosmos sends and the near reverb —
  the "after the reverb, back before the filter" loop through the whole
  foreground and background chain. This path adds energy, so it is
  throttled by the mix's own mean level (feedback → 0 at 0.1, about
  −20 dBFS, 50 ms follower, ramped across the chunk).
* **To Pitch** phase-modulates every partial of every voice by h·θ, θ =
  3·amount·feedback radians. In the phasor bank that is one extra
  small-angle rotation per partial (tan clamped to ±0.4, then two Newton
  steps of 1/√ so the phasor stays on the unit circle to 1e-4 per sample);
  the clamp makes deep modulation saturate softly on the high partials
  instead of tearing. Phase modulation moves energy between partials
  without adding any, so this path is not throttled — a loud drone keeps
  its modulation. Only the FM path pays: a four-note Slow Chorus Field
  renders at 27× realtime without and 15× with it.

The loop is one chunk late (≤ 512 samples, ~10 ms), which is nothing in a
loop that runs through a 25-second reverb. Silence stays silence (nothing
in, nothing to feed back). Measured: To Bus 1.0 with Drive 1.0 on a held
A3 stays below a peak of 0.95 for 20 s and is louder than the dry note;
To Pitch 1.0 moves more than 30 % of a C4's energy away from its exact
harmonics. The throttle ceiling was first 0.25: Distant Storm then climbed
10 dB and collapsed to a stereo correlation of 0.6, because a loop near
unity gain circulates through the delay's cross-feed until both ears carry
the same thing. At 0.1 the loop thickens a drone without taking it over.

## Presets

`Core/src/Presets.cpp`: a preset is a name and a `key=value;…` string over the
parameter table (choices by name). The engine, the render tool (`--preset`)
and the plugin's program list all use the same table. 128 presets in ten
families; all 128 render finite with a held chord, levels −29 … −11 dBFS.
User presets are saved by the plugin as `.ambientsynth` XML files (full
state including a loaded Scala scale).

Two layers, loadable independently and combinable (`PresetScope`):
*Sound* = every parameter outside the Cosmos section, *Cosmos* = the Cosmos
section. Applying a preset in one scope resets only that scope's parameters
to their defaults and then to the preset's values; the other layer is
untouched. The 128 full presets serve as the Sound bank (their sound layer)
and as DAW programs (both layers); a separate 32-entry Cosmos bank ("Cosmos
Off", shifters, resonators, vowels, nebulae, shimmers, the sci-fi
combinations) serves the Cosmos box. All 160 render finite.

### GrainCloud

History ring of 4 s fed from the near bus × *Send*. Grains are spawned at
exponentially distributed intervals around *Density*; each has a Hann window
of *Grain* × (0.7 … 1.3), a start point up to *Spray* seconds back, a random
pan, and a playback rate of 1 ± 2 % or, with probability *Pitch*, ×2, ×½
(70 %) or ×1.5, ×4 (30 %). Up to 32 grains overlap; the output gain is
normalised by √(density · grain length) so density does not change loudness.
The cloud is added to the far bus only: it exists in the background and the
far reverb smears it.

## Morph (the performance control)

Two full parameter snapshots live in the engine (`slotA_`, `slotB_`, atomics
per parameter, writable from any thread). While *MorphActive* is on,
`Engine::effectiveParam(id)` replaces the live value with the blend at the
current position: floats interpolate in the skewed domain the knobs use
(`p = ((v−min)/span)^skew`, lerp p, invert), integers round, choices and
switches take A below 0.5 and B above. The position glides toward *MorphPos*
at `1/MorphGlide` per second (glide 0 = jump), so a controller or a hand can
jump while the sound follows over minutes. The Morph section is excluded
from every preset scope: loading presets never disturbs a running morph.
Slots are saved in the plugin state and in `.ambientsynth` files.

## Toward VR (the reason for the design)

The target is a 30-minute drone set built with nothing but slow movements
in a headset: hands and head change parameters, the picture is the sound.
What that already dictates here:

* the whole instrument is a framework-free library with atomics as its only
  control surface, so an OpenXR app can drive it directly;
* every control is continuous and glides, nothing steps;
* the morph is one scalar — the natural quantity for a hand to own;
* the engine exposes observers (`soundingNotes`, `noteDistance`, `arcValue`,
  `morphPosition`, `brainRoot`) for a synaesthetic visualisation to read;
* the distance model maps one-to-one onto placing sources in a 3D scene.
Both of these exist now (`ambient/Osc.h`, `ambient/Gesture.h`): the OSC
server (framework-free UDP, Winsock/BSD, own parser for messages and
bundles) writes parameters through an `OscSink` and hand/head data into the
`GestureLayer`; the gesture layer maps inputs to parameters with range,
smoothing, dead-zone and a clutch, and its mappings are a small text format
saved with the plugin state. Decision 2026-09-04: the headset app is native
(NDK, OpenXR + `XR_EXT_hand_tracking`, Vulkan/GLES, Oboe), starting from
Meta's native hand-tracking sample; the core already cross-compiles for
arm64-v8a. Details in `docs/quest-plan.md`.

## Plugin shell

* MIDI learn: right-click on a control arms it; the next controller message
  binds (one controller per parameter, one parameter per controller); the
  map lives in the plugin state. Controllers write the parameter through the
  host, so the DAW sees the movement.

* `AudioProcessorValueTreeState` built from the parameter table; the raw
  atomic values are copied into the engine at the top of every block.
* MIDI note on/off and all-notes-off; sample-accurate splitting is
  deliberately absent (nothing here is faster than a control block).
* Programs = presets; state = APVTS XML + Scala text + display name.
* Editor: sections flow-laid-out from the table (knobs, toggles, combo boxes),
  header with preset box, voice count, brain root, scale, arc value and a
  keyboard strip where near notes are bright and far notes dim; *Load Scala…*
  file chooser; resizable.

## Roadmap

1. **Sound** — done since v0.2: spectral freeze (Nebula), head-shadow
   low-pass on the far ear (20 kHz → 3 kHz at full lateral position, scaled
   by *Time Width*), user preset files, a second delay in series, the
   granular cloud, independent Sound/Cosmos preset layers. Open: per-preset
   random seeds, a "morph" between two full presets over minutes, MIDI
   learn for the standalone.
2. **Performance** — Chebyshev sine recurrence when inharmonicity is 0, SIMD
   across partials, voice rendering in parallel on desktop.
3. **Quest** — CMake toolchain for the Android NDK (arm64-v8a), Oboe for
   low-latency audio, OpenXR for hands and head; the visual layer is a
   separate concern and can reuse the Kaleidoscope engine's ideas (calm
   motion, no camera shake). `Core/` is expected to compile unchanged; the
   `Engine::soundingNotes` / `noteDistance` / `arcValue` observers already
   exist for a visualisation to read, and the distance model maps directly
   onto placing sound sources in a 3D scene.
