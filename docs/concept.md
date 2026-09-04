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
   block, filter cutoff and reverb lengths glide. A drone must never click.
3. **Measure, don't only listen.** `ambient_render` renders deterministically
   and prints per-second RMS, peak, voice count and root; `analyze.py` reports
   clicks, spectral centroid and the strongest peaks. The self test checks the
   tuning maths to 1e-9.
4. **One parameter table.** `Core/include/ambient/Params.h` lists every
   parameter with range, default, skew, unit and section. The engine, the
   JUCE parameter layout, the GUI and the command-line renderer all iterate
   that table. Adding a parameter is one line in the table plus one line in
   `Engine::readParams`.

## Signal path

```
MIDI / Cluster Brain
      │ note on/off
      ▼
Voice (×16)  = Strand (×1..6) = additive bank of ≤32 partials
      │  each partial: own phase, own slow amplitude drift ("Shimmer")
      │  strand: detune offset + slow pitch drift, own pan position
      │  spectrum: h^-tilt · odd/even weight · brightness window · inharmonic stretch
      │  → TPT state-variable low-pass (key track, env amount, drift)
      │  → ADSR (seconds to minutes)
      ▼
stereo bus → Ensemble (3 modulated taps) → Reverb (8-line FDN, Householder feedback,
             4 input all-passes, per-line damping, slow length modulation, freeze)
          → width (mid/side) → master gain → cubic soft clip
```

Why additive: partials above Nyquist are simply not generated, so there is no
aliasing at any pitch; every partial can have its own life (that is the
"breathing" of a Rich drone); brightness and inharmonicity become continuous
parameters instead of waveform switches. Cost at the defaults is ~5 % of one
core (180 s render in 8.4 s); the worst case (16 voices × 6 strands × 32
partials) is about ten times that and would need the Chebyshev recurrence or
SIMD, both of which are straightforward later.

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

At the defaults this yields 2–6 voices, a root that moves every few minutes,
and a level that stays within a few dB (measured: −22 … −27 dBFS over 3 min,
no sample jump above 0.03).

## Plugin shell

* `AudioProcessorValueTreeState` built from the parameter table; the raw
  atomic values are copied into the engine at the top of every block.
* MIDI note on/off and all-notes-off; sample-accurate splitting is
  deliberately absent (nothing here is faster than a control block).
* State = APVTS XML + Scala text + display name.
* Editor: sections flow-laid-out from the table (knobs, toggles, combo boxes),
  header with voice count, brain root, scale name and a keyboard strip showing
  sounding notes and the root; *Load Scala…* file chooser; resizable.

## Roadmap

1. **Sound** — presets (a preset = a list of key/value pairs; the render tool
   already parses `--set`), a second spectral layer (noise/air with formant
   drift), spectral freeze, binaural panning per strand, an "arc" over hours
   (very slow macro drift of density/brightness).
2. **Performance** — Chebyshev sine recurrence when inharmonicity is 0, SIMD
   across partials, voice rendering in parallel on desktop.
3. **Quest** — CMake toolchain for the Android NDK (arm64-v8a), Oboe for
   low-latency audio, OpenXR for hands and head; the visual layer is a
   separate concern and can reuse the Kaleidoscope engine's ideas (calm
   motion, no camera shake). `Core/` is expected to compile unchanged; the
   `Engine::soundingNotes` / `activeVoices` observers already exist for a
   visualisation to read.
