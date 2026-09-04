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
      │  each partial: own phase, own slow amplitude drift ("Shimmer")
      │  strand: detune offset + slow pitch drift, pan around a wandering voice centre
      │  spectrum: h^-tilt · odd/even weight · brightness window · inharmonic stretch
      │  + Air: band-passed noise around a drifting multiple of f0
      │  → TPT state-variable low-pass (key track, env, drift, −2.5 oct per unit distance)
      │  → ADSR (seconds to minutes) · (1 − 0.5·distance)
      │  → interaural time difference from the centre pan (≤ 0.65 ms, the far ear later)
      │  → near gain cos(d·π/2) → NEAR bus ; far gain sin(d·π/2) → FAR bus
      ▼
NEAR: Ensemble → StereoDelay (asymmetric L/R, cross-feed, damping) ─┬─ mix → + Near reverb (small room)
                                                                     └─ "to far" ─┐
FAR:  (+ delay echoes) → Far reverb: 8-line FDN, 4 input all-passes, per-line   │
      damping, slow length modulation, right group 8 % longer + right output   ◄┘
      delayed ≤ 10 ms (asymmetry), tail low-pass, freeze; 100 % wet · level
      ▼
Mid/Side: side high-passed at Bass Mono (low end centred), broad +N dB bell at
3 kHz on the side, width → master gain → cubic soft clip (no compressor)
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

Cost at the defaults is ~3 % of one core (180 s in 5.8 s); "Distant Storm"
(8 voices, 5 strands) 5 %; the worst case (16 voices × 6 strands × 32
partials) is about ten times that and would need the Chebyshev recurrence or
SIMD, both straightforward later.

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

## Presets

`Core/src/Presets.cpp`: a preset is a name and a `key=value;…` string over the
parameter table (choices by name). The engine, the render tool (`--preset`)
and the plugin's program list all use the same table. 128 presets in ten
families; all 128 render finite with a held chord, levels −29 … −11 dBFS.
User presets are saved by the plugin as `.ambientsynth` XML files (full
state including a loaded Scala scale).

## Plugin shell

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
   by *Time Width*), user preset files. Open: a second delay in series, a
   granular cloud on the far plane, per-preset random seeds.
2. **Performance** — Chebyshev sine recurrence when inharmonicity is 0, SIMD
   across partials, voice rendering in parallel on desktop.
3. **Quest** — CMake toolchain for the Android NDK (arm64-v8a), Oboe for
   low-latency audio, OpenXR for hands and head; the visual layer is a
   separate concern and can reuse the Kaleidoscope engine's ideas (calm
   motion, no camera shake). `Core/` is expected to compile unchanged; the
   `Engine::soundingNotes` / `noteDistance` / `arcValue` observers already
   exist for a visualisation to read, and the distance model maps directly
   onto placing sound sources in a 3D scene.
