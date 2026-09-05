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
alone costs 1 % of a core; `ambient_render --bench` (all presets, held
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

### Purity, purity drift, freeze

*Purity* (Tuning) blends every note's frequency between 12-TET (0) and
the chosen scale (1) in the log domain: at 1 the partials of different
notes lock into each other and the beating is gone, at 0 they beat like a
piano, in between the beating slows as the intervals close in on their
ratios. *Purity Drift* lets the blend wander on a Drifter at *Drift Rate*
(default one swing per 100 s), so the lock-in comes and goes over minutes;
sounding voices follow with a one-second log-domain glide, no retrigger.
Measured: E4 over a C root is 327.03 Hz pure and 329.63 Hz tempered, 0.5
gives their geometric mean, and a held voice moves from one to the other
within five seconds. *Freeze* (Oscillator) stops the movement clock of
every voice — shimmer, pitch drift, breath, bloom — while envelopes,
filters and effects keep their own time: the spectrum stands still.
Measured: the third partial of a frozen voice varies less than a quarter as
much as with shimmer at 2 Hz.

### Ghost, portamento with gravity, inertia, tape, coherence

Five small mechanisms from a second round of suggestions, all inside the
existing structure:

* **Ghost** (Air → Mode): the Air noise goes through six sharp resonators
  on the note's harmonics 1 2 3 5 7 9 instead of one band-pass, Q from
  *Air Q* times eight — the harmony is filtered out of the chaos, Rich's
  string and pipe resonances. Measured: with the partial bank silent,
  harmonics 3 and 5 of an A3 carry eight times the energy of 4 and 6.
* **Portamento** and **Gravity** (Tuning): a new key slides in from the
  last one in the log domain over *Portamento* seconds; *Gravity* slows the
  slide near consonant ratios to the root (unison, octave and fifth to about
  15 % speed, `intervalConsonance`), so the glissando dwells on the harmonic
  nodes and hurries across the dissonant stretches — the microtonal
  portamento of a lap steel. Measured: half-way through an even 2-second
  glide from A3 to E4 the pitch sits between the two, at the end it has
  arrived; with gravity the pitch is still lower at the half-way mark.
* **Inertia** (Macros, performance state): every float parameter the
  engine reads glides to its value with this time constant in the skew
  domain, the analogue slew — a knob torn open still arrives slowly.
  0 (default) is bit-exact bypass.
* **Tape** (Feedback): in the loop an asymmetric saturation (an even-order
  term the DC blocker cleans up on the next pass), wow (an irregular
  Drifter, up to 3 ms) and flutter (6 Hz, 0.3 ms) as a fractional read
  position, and a noise floor that rises with the loop's level — every
  generation through the loop goes a little softer and less stable, like a
  forty-year-old tape. Measured: a full loop with tape stays bounded and
  DC-free for 12 s.
* **Coherence** (its own section): four Kuramoto oscillators with natural
  periods 23, 31, 41 and 53 s over *Rate*, coupled by *Coherence*
  (dθᵢ = ωᵢ + K/N Σ sin(θⱼ − θᵢ), K up to 0.6 rad/s times *Rate*, so the
  lock is the same at every tempo), their sines added by
  *Depth* to brightness, brain depth, pan and the z-plane point. Independent
  at 0, pulsing as one unit at 1, and everything between. Measured with the
  Kuramoto order parameter after 20 minutes of phase: above 0.9 when
  coupled, below without.

### Sleep and the rest zone

After two seconds with no voice and the output below −90 dBFS the engine
sleeps: the effect chain is skipped and zeros go out, so a silent Quest
costs nothing; the brain keeps ticking inside the render and a note (MIDI,
OSC, brain) wakes the engine in the same block. The gesture layer has a
**rest zone** (default 8 % of the calibrated height, `rest_zone=` in
`ambient.cfg`): both hands hanging low means nothing is written, so the
arms can drop without touching the sound; it only arms once real hand data
arrived, so knobs and OSC alone never "rest". On the Quest the map cursor
now uses both hands — left reach and height for the position, right height
for the blend radius (sharp low, blurred high).

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

### Filter models

`Core/include/ambient/Filter.h`. The voice filter is one of nine models
behind the same five knobs (*Model*, *Cutoff*, *Resonance*, *Env Amount*,
*Drift*, *Key Track*, plus *Drive*): LP 6 (one pole), LP 12 (the
state-variable low pass the instrument always had, bit-identical), LP 24 (two
stages, shared resonance), HP 12, BP 12 (unity at the cutoff), Notch, Peak (a
bell of up to +14 dB, narrower with resonance), Ladder (four one-poles with
the last fed back through a soft saturation; the corner sits 1.55× above the
knob so the -3 dB point lands near Cutoff; resonance squared, because the
interesting range is at the top) and Comb (a feedback comb tuned to Cutoff
with a low pass in the loop, output scaled by 1 - fb so the peaks stay at
unity and Resonance deepens the dips instead of raising the level -- on a
sustained cluster less a filter than a second resonating body). *Drive* is a
soft saturation ahead of the filter, level-compensated. Every model reports
its own magnitude response (`VoiceFilter::magnitude`, the same maths as the
audio path), which is what the filter display draws. A model change resets the
filter states, since they mean different things in different models.

The voice filter and the z-plane are two filters, each with its own switch
(*On* in the Filter section; the z-plane's *Mode*, where *Replace* is the
z-plane alone), and with both on the z-plane's *Route* puts them in **series**
(the z-plane hears the filter, *Mix* is its dry/wet) or in **parallel** (both
hear the dry sum and *Mix* balances them). The display combines the two
responses the same way; the parallel sum ignores the phase between the
branches, which is the one thing the picture cannot show. Switching the
filter back in starts it from rest.

### Z-plane filter

After the idea Dave Rossum built into the E-mu Morpheus: four filter
frames sit on the corners of a square, and a point (X, Y) inside it is a
filter whose poles *and zeros* are interpolated between the corners — move
the point and the whole resonant structure glides, always through stable
filters. The patent (US 5,170,369) expired long ago and the manuals
describe what the filters do, but the coefficient tables in the original
firmware are proprietary data; this bank is our own, built in the same
architecture. `Core/include/ambient/ZPlane.h`:

* A frame is up to **six cascaded two-pole/two-zero sections** — a 12-pole
  filter, the order the Morpheus used. Interpolation is bilinear in log
  centre frequency and log bandwidth on the *pole and zero parameters*,
  never on coefficients, so every point inside the square is stable by
  construction.
* Sections come in two flavours, and the difference matters: a **bell**
  (zero on the pole, wider) boosts its frequency and leaves the rest at
  unity, so six of them in series shape a spectrum; a **resonator** (no
  zero, or a zero elsewhere) passes only its band, so a few in series take
  the sound over completely. A first attempt made every cluster shape a
  cascade of narrow resonators — six of those cancel each other out, and
  the self test found them at −120 dBFS.
* Normalisation happens twice per control block, from one pass over a
  probe grid (16 fixed logarithmic points plus every pole and zero angle of
  the frame — a fixed grid alone walks straight past a needle-sharp
  resonance). Each section is scaled to its geometric mean over the grid,
  which is what keeps a bell's background at unity, capped at 30 dB of
  boost; then the finished cascade is scaled so its loudest point is unity.
  A shape can therefore be extremely resonant without being loud.

Sixteen shapes in six families: Vowel Morph, Choir, Nasal · Low Sweep,
High Sweep, Band Sweep · Phaser, Comb, Flanger, Notch Cluster · Strings,
Metal Bars, Wood, Glass · Peaks · Infinite (two poles a hair apart at the
stability limit). Per voice: *Mode* Series (after the state-variable
filter) or Replace (instead of it), *X* / *Y*, *Rate* and *Depth* (two
Drifters move the point — the "LFO", in this synth's continuous,
non-repeating form), *Resonance* (quarters or doubles the bandwidths),
*Key Track* (the frame follows the note), *Mix*. Twelve presets
(*Morphing Vowels* … *Endless Resonance*) show the families off. Measured:
all sixteen shapes stay finite, audible and below a peak of 2 at their four
corners and their centre; on a 32-partial A2 the Vowel corner a favours
700 Hz over 2300 Hz more than three times as strongly as corner i; the six
sections cost 14 % more render time than no filter at all on the heaviest
preset (23× → 21× realtime).

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

### Three sources

Every voice has three equal source slots. Source 1's *Type* defaults to
**Additive**, which is the strand bank described above (unison, detune,
stacks, bloom -- the classic Oscillator; its Octave, Ratio and Pan move the
whole bank); set to anything else the bank falls silent and the slot renders
in its place, so a voice can be three granular players or three FM pairs.
Additive in Source 2 or 3 is a single 32-partial bank inside the slot with
the same spectrum formula (partials, tilt, brightness window, odd/even,
inharmonic stretch, per-partial shimmer) -- one strand, the cost of a
wavetable slot. The type index for Additive sits last so the indices the
five thousand presets store for the other types did not move; the switch was
measured sound-neutral on 37 presets (identical descriptors, because the new
random streams are seeded from side streams and never touch the voice's).

Each slot has a type, level, octave, a just
ratio to the note (1/1 … 2/1, so a slot can sit a fifth or a seventh above Each slot has a type, level, octave, a just
ratio to the note (1/1 … 2/1, so a slot can sit a fifth or a seventh above
the key), and a pan; all of it goes through the voice's filter, envelope,
distance and ITD like the bank. `Core/include/ambient/Sources.h`.

* **Wavetable** — a table of *spectra*, not of samples: up to 64 frames of
  32 partial amplitudes. *Position* interpolates between frames, *Pos
  Drift* lets it wander on a 50-second curve, and the result is rendered by
  a rotating-phasor bank like the main oscillator. Alias-free, and
  presence, low cut and the feedback's phase modulation treat it like any
  other partials. Built-in tables are generated (Classic: sine → triangle
  → saw → square → pulse; Organ drawbars; Vocal formants a-e-i-o-u; Glass;
  Metal); *User* is analysed from a WAV with 2048-sample single-cycle
  frames (the Serum/Vital layout) — FFT per frame, bins 1–32, up to 64
  frames picked evenly. Measured: Classic at 0 is a sine (no second
  partial within 100×), at 0.5 a saw (second partial at 1/4 the power,
  third present); ratio 3/2 with octave +1 puts A3 at 660 Hz.
* **FM** — carrier at the slot pitch, modulator at *FM Ratio*, *FM Index*
  up to 8, shrinking above 3 kHz so high notes do not alias; *Pos Drift*
  wanders the index by up to a factor two. Measured: index 0 is a sine,
  index 3 puts more than a tenth of the carrier's energy on the sideband.
* **Texture** — a granular player over a loaded sample: up to 8 Hann grains
  of *Grain* ms at *Density* per second, around *Position* (wandering with
  *Pos Drift*, sprayed ±3 %), each grain with its own small pan. *Pitch* =
  Free plays the sample at its speed (octave and ratio become speed
  multipliers); Note pitches it to the key, assuming the sample was
  recorded at C4. The texture is double-buffered in the engine so a new
  file never touches the buffer the audio thread reads. Measured: a 440 Hz
  sample plays at 440 Hz in Free and at 370 Hz on A3 in Note; an empty
  slot is silent.

The plugin loads textures in any format JUCE reads (paths kept in the
state), the render tool takes `--texture file.wav [baseHz]` and
`--wavetable file.wav`, and the Quest app picks up `texture*.wav` and
`wavetable.wav` from its data folder. The core has its own WAV reader for
that (PCM 8–32 and float, any channel count mixed to mono). A note token
at the end of the file name (`bowed_metal_sao_123_A3.wav`, also `C#4`,
`Bb2`) sets the texture's base pitch; without one, C4 is assumed.

**Where textures come from: `Tools/TextureGen`.** A PySide6 program with a
prompt, model choice, length, steps, guidance, seed and a variation count;
the models run in a child process that stays loaded between jobs (torch
never inside a Qt thread). Models: Stable Audio Open 1.0 (44.1 kHz stereo,
≤ 47 s — the first choice for textures, field recordings, metal, air;
gated on Hugging Face), MusicGen Large/Medium/Small (32 kHz, tonal drones
and choirs), AudioLDM 2 Large (16 kHz, dark effects). Output is 32-bit
float WAV at −6 dBFS peak, plus a `.txt` with the settings; the worker
detects the base pitch by autocorrelation and appends the note to the
file name only when the clip is clearly periodic, so rain stays unpitched
and a bowed plate becomes `…_A3.wav`.

**Where user wavetables come from: `Tools/WavetableGen`.** Same
environment, own GUI. Three sources: *Audio* slices single cycles out of
any WAV (pitch-tracked with the TextureGen rules, each frame the average
of a few cycles, frames spread over the selection, phase-aligned by
circular cross-correlation so morphing does not click); *Prompt* asks a
TextureGen model for a sustained note and slices that; *Procedural* walks
spectral recipes over the table position (saw → square, tilt, formant
sweep, comb, glass, odd breathing, random walk) with optional per-partial
random walk and phase scatter, and can morph the current table into a
recipe. A frame/spectral-image view, a preview sweep at a chosen note, and
export in the 2048-frame layout. Measured: a 220 Hz saw-to-square sweep
comes back with the second partial at 0.5 in the first frame and gone in
the last; the MusicGen C2 drone yields a 32-frame table the render tool
loads and plays. A learned latent space (WaveSpace-style) was considered
and left out: the tables are spectra already, and the three sources cover
the space from sound, language and rules without a checkpoint.

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
* **Macros** are the gesture inputs Custom0..7 fed from eight parameters, so
  the same mapping table serves hands and knobs. A mapping only starts to
  write once its input has moved from its rest value (also true for head
  yaw), which keeps presets intact until the performer touches a macro.
  They carry Rich's vocabulary, not the engine's: *Space* (far level, decay,
  depth, size), *Alien* (cosmos send, nebula, shift), *Motion* (drift and
  shimmer rates, pan drift, ensemble), *Bloom* (brightness, air, cutoff,
  presence), *Density* (brain density, cloud, strands), *Distance* (depth,
  keys depth, cutoff down, far level up, presence down), *Evolution* (rate
  wander, source position drift, breath, arc), *Air* (air, air colour, side
  air, tail cut). The plugin's **Perform** page shows only these eight as
  large knobs plus the morph, for playing a set without the editor.

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

## Room (convolution reverb, optional and additional)

A third reverb next to the near room and the far FDN: `Convolver`
(`Core/include/ambient/Convolution.h`) plays an impulse response by
uniform partitioned convolution — input blocks of 512 samples, each
block's spectrum into a frequency-domain delay line, every output block
the sum over all partitions of input spectrum × impulse-partition
spectrum, one inverse FFT per block, overlap-add. Latency is one block,
which the far plane does not notice. Real input, so only bins 0…N/2 are
multiplied and the mirror is rebuilt before the inverse transform.
True-stereo impulses keep L and R apart, a mono impulse serves both
channels with their own inputs. Impulses are double-buffered like
textures, resampled to the engine rate and energy-normalised, so a room
never changes the level of what it reverberates; the maximum is 8 s.
Without a file, `generateDefault` builds a dark hall at `prepare` (three
noise bands with RT60 5 / 3 / 1.2 s, a 20 ms diffuse onset, eight early
reflections, independent noise per ear), so the Room works out of the box.

Parameters (section Room): *Level* (0 = off and no CPU; the convolver
keeps running for one impulse length after the level reaches zero so the
tail can finish), *Source* (Far = the far sends before the FDN, Near = the
finished foreground), *Pre-Delay*, *Tail Cut*. Files: the plugin's
*Impulse…* (mono or stereo, path in the state), `ambient_render --ir`,
`impulse.wav` on the Quest (applied once the engine is prepared). Measured:
a unit impulse comes back one block late at unity, a tap 1500 samples in
lands at the right place across partitions, the default hall decays by
more than 10 dB per two seconds with an ear correlation below 0.3, Room
level 1 leaves a tail after a note where level 0 leaves silence.

**Where impulses come from: `Tools/ImpulseGen`.** Design (per-band RT60,
size, pre-delay, width, tone, modulation; eight room presets from dark
cathedral to infinite plate), Recording (onset, trim, floor, tail
extension for cut-off renders), Prompt (a TextureGen model renders a clap
in a described room, which is then cut — text-to-audio models do not know
impulse responses but they know claps in cathedrals), Hybrid (a
recording's spectrum colours noise, the designed decay shapes it). GUI and
CLI, energy decay curve and a chord preview.

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

### Output DC blocker

One high pass at 4 Hz sits between the mid/side stage and the master gain, below the lowest
sub the Foundation can reach (at 20 Hz it costs 0.17 dB). Several paths can leave an offset
behind -- FM at an integer ratio, the tape stage's asymmetric term, a granular window over a
clip that carries one, the shimmer's pitch shifter -- and an offset costs headroom in the soft
clipper without ever being heard. The blockers inside the voice's FM slot and inside the
feedback loop stay: they exist to stop a loop locking onto a DC operating point, which a filter
at the end cannot do.


## Real-time budget

What the audio thread is allowed to do, and what was measured.

**No allocation, no lock.** Every buffer is sized in `prepare()`; a scan of `Core/src/*.cpp`
finds no `new`, `assign`, `resize` or lock inside `process`, `render`, `readParams`, `control`,
`routeStep` or the note calls. The plugin's `processBlock` takes one `ScopedTryLock` for the
recorder (never blocks) and calls `setSize` on its scratch buffer only if the host hands it a
block larger than `prepareToPlay` announced. Parameters are cached `std::atomic<float>*` taken
once in the constructor -- never a string lookup. Data that crosses threads (user scale, user
wavetable, texture, route) is double-buffered and published with an atomic version.

**Control rate.** Modulation runs once per `kControlBlock` = 64 samples (1.3 ms); levels that
must not step use per-sample `Smoother`s.

**No transcendental in a per-sample loop.** `sin01` is a table with linear interpolation, and
every oscillator, window and formant runs as a rotating phasor seeded by `phasorFrom` (Dsp.h),
renormalised with one Newton step so it stays on the unit circle. Two places had been missed and
were found by scanning for `std::sin|cos|exp|pow|log` inside sample loops: the GrainCloud's Hann
window (a `std::cos` per sample per grain, up to 32 sounding) and the Foundation's sub, whose
glide is a one-pole in the log domain and took a `std::exp` per sample -- the block's end point
is known in closed form, so two `exp` per block and a linear walk do the same job. Measured on a
cloud- and sub-heavy render, interleaved A/B over four runs: 1.00-1.10 s before, 0.86-0.94 s
after, about 15 % (19.6x to 23.0x realtime).

**Denormals.** Flush-to-zero and denormals-are-zero are set at the top of `Engine::process` and
restored at the end: `_mm_setcsr` on x86, and FPCR bit 24 on ARM64 -- which had been missing, so
the Quest ran every decaying reverb tail into denormals.

**Voices.** A voice is rendered only while its envelope is not idle; release ends at level 1e-4
(-80 dB). The allocator takes a free voice, else the quietest releasing one, else the oldest.

**Not done, deliberately.** No SIMD: the partial bank is already a flat `float` array per strand
(structure of arrays) and the compiler vectorises it, but nothing is hand-written against SSE or
NEON, and the core stays framework-free so `juce::FloatVectorOperations` is not available to it.
No fast-math: the offline render is the determinism oracle of the self test, and reassociation
makes it drift. LTO is an option (`AMBIENT_LTO`), off by default -- measured on MSVC it changed
nothing (1.05-1.16 s either way).

**Known hazard.** `processBlock` writes host parameters through `setValueNotifyingHost` for the
gesture layer, the route cursor and, on the block where the map is switched off, for every
parameter at once. JUCE allows this from the audio thread, but what a host does with it is the
host's business; the burst on map exit is the one place where it is more than a handful.

## Modulation

`Core/{include/ambient/Modulation.h, src/Modulation.cpp}`. Until this existed, every modulator in
the instrument was soldered to one destination and carried its own depth and rate: the pitch
drifter to pitch, the filter drifter to the cutoff, the Kuramoto ring to brightness, distance,
pan and the z-plane point. Adding a source always meant adding another pair of knobs. This is the
general form; the soldered drifters stay, because they are per-partial and per-strand and a
matrix row cannot reach in there.

**Eight LFOs**, shapes Sine, Triangle, Ramp Up/Down, Square, Random, Steps and Table. Rates from
one cycle in twenty minutes to 20 Hz. The square's edges are ramped over 7 % of a cycle and the
random shapes interpolate: the standing rule that a modulator may not step holds here too. `Table`
reads a frame of the loaded wavetable, so any of the generated tables -- and any curve drawn into
one -- is an LFO shape, without a second mechanism for drawable modulators.

**Six envelopes**, up to sixteen breakpoints each, a curve per segment, an optional sustain point
and an optional loop between two points. Their clock is a phrase clock: it restarts when a note
arrives into silence, not on every note of a cluster, or a shape spanning a minute would never
get anywhere.

**A matrix** of thirty-two rows, `source -> target x depth`, with an optional second source as the
amount and a unipolar flag. Depth is a fraction of the target's own range, so the same number
means the same thing on a cutoff in hertz and on a mix in 0..1. One source may appear in as many
rows as it likes -- that is the whole point, and what the soldered drifters could never do.
Performance state (morph, macros, the map cursor, the route) is never a target: modulating the
morph position from inside would fight the hand holding it.

The LFO settings and the envelope times are parameters, so a host automates them. The shapes and
the matrix rows are **data**, the way a Scala scale and the gesture mappings already are: a text
form that travels in the preset (fields 7 and 8 of a pack line), in `.ambientsynth` files and in
the plugin state. Thirty-two rows as four parameters each would put a hundred and twenty entries
into the automation list for very little gain.

Modulation is added after the inertia glide: a modulator moves at its own rate, it is not slewed
by the setting that exists to slow the performer's hand down.

### Measuring without a temporary file

The measurement line also carries a `hash=` of the audio itself. Descriptors are averages: two
renders can agree on every one of them and still not be the same sound, and a regression that
moves energy around without changing its statistics would pass unnoticed. The samples are
quantised to about -120 dB before hashing, so a real change in what is played trips it while the
last bit of a sum does not (the vectorised partial bank moves those, and moving them is not a
change). Verified: the same render twice gives the same hash, and a cutoff moved by one hertz
gives a different one.

`ambient_render --measure` renders and prints the descriptors as one line instead of writing a
WAV. The measurement pass over the library used to render each preset to a temporary file and
read it straight back: five thousand presets times twelve seconds of stereo float is twenty-three
gigabytes written and read for nothing, and all of it stayed in the file cache. On a machine with
64 GB that filled the standby list to 38 GB, at which point Windows began trimming the working
sets of the applications on screen and switching between them stuttered. The WAV reader also asks
for `FILE_FLAG_SEQUENTIAL_SCAN`, so the clips it reads are aged out of the cache instead of kept.
Measured afterwards: a full pass grows the standby list by well under a gigabyte instead of
tens of them.

## The Rich refinements

Ten small mechanisms, each a way the sound stops being a synthesizer and starts being a place.
Every one is off by default and was measured sound-neutral there (37 presets identical before
and after); each was measured effective on its own.

* **Phase Width / Phase Rate** (Space): two first-order all-passes per ear whose corner
  frequencies (300 and 1500 Hz) drift apart and back on one slow curve, the left ear up and the
  right ear down by up to 1.5 octaves. The phase relation between the ears changes, the level
  does not, so the ear reads a room changing size rather than a sound moving. Measured: width
  descriptor 0.73 to 0.99 on the default patch at full depth.
* **Doppler** (Space): the breathing distance has a velocity; a voice coming closer rises, one
  receding falls, up to 3 % (one plane unit taken as about twenty metres).
* **Blur** (Foreground, a second Nebula): the near bus through a spectral smear ahead of every
  effect, so an attack is wiped into texture and one note flows into the next. Mix and Smear;
  the blurred part is 43 ms late, the dry part is not.
* **Formant** (tenth filter model): three band passes on the formants of u-o-a-e-i, Cutoff
  morphs the vowel, Resonance narrows the formants, Drift and an LFO make it breathe.
* **Strike** (its own tab): a Karplus-Strong loop excited with a noise burst at note-on --
  String at the note, Wood two octaves up with heavy damping and a short decay, Metal with an
  all-pass in the loop -- on the near plane whatever the voice's distance. Fires for keys, or
  for the brain's notes too. The intimate impulse that makes the background behind it vast.
* **Drift** per source (Source 1..3): an independent slow pitch drift in cents. Three sources
  on just ratios each drifting on their own curve beat like an ensemble in a room whose
  temperature moves; nothing is symmetric, nothing cancels for long.
* **Absorb** (both delays): with Absorb up the feedback loop also loses its low end, and its
  high cut sinks as the feedback rises (at full feedback and full absorb the loop keeps
  320 Hz to 1 kHz), every repeat passing through the band again: echoes drown in a fog
  instead of merely getting quieter.
* **Tide / Tide Period** (Tuning): the whole instrument's pitch leans by up to 30 cents on a
  minute-scale curve; the sub follows, so the harmony stays.
* **Rotate** (Far Reverb): the far field's left and right rotate into each other on a slow
  curve; the background turns.
* **Golden-ratio LFO defaults**: the eight LFOs start at 0.03 Hz times the golden ratio to
  the n-th power, so their cycles share no common period and their extremes never line up.
  The coherence ring's natural periods were already primes (23 / 31 / 41 / 53 s).

Every new random source (the phase field's drifter, the sources' pitch drifters, the tide and
the rotation) seeds from a side stream rather than from the voice's or the engine's, so
switching one on never moves the brain's dice or a preset's random phases. Ten built-in
presets (168..177, "rich studies") show each one; the library generator draws them per style.

## The instrument as a place: body, unmasking, patina, externalisation

Four stages that are about the room the sound is in rather than the sound itself. All four are
off by default and were measured sound-neutral there.

* **Body** (`Core/include/ambient/Body.h`): twelve modes tuned to the brain's root, fed from the
  finished mix and returned to it -- the soundboard a pad sits on. Four materials, which are four
  sets of mode ratios: Wood (a plate's irregular low modes), Plate (the stretched series of flat
  metal), Bell (hum, prime, tierce, quint, nominal), String (harmonic with a little stiffness).
  Decay is the lowest mode's T60; the higher ones die away faster at a rate belonging to the
  material; Tone tilts the gains, Spread scatters the modes across the field. Two things had to
  be measured rather than assumed: a resonator normalised to unity at its peak passes almost
  nothing of a broadband signal (the Body knob moved the mix by 0.02 dB), so the level is
  normalised on the expected *power* instead, the way the Air band is; and modes scattered
  randomly around the centre are all driven by the same mono signal and therefore correlate the
  two channels (width 0.69 collapsed to 0.11), so neighbouring modes sit on opposite sides. Q is
  capped at 300, because a mode narrower than that is never excited by a drone that drifts.
* **Unmask** (Far Reverb): the background steps aside for the foreground band by band -- three
  bands, the near bus as the side chain, 50 ms to duck and 1.2 s to return. It is what a mixing
  engineer does by riding the reverb return, and what keeps a dense pad from swallowing its own
  notes.
* **Patina** (master): tape wow and flutter as a moving read point, the top end a worn machine
  has lost, a noise floor that rises a little with the signal (modulation noise, which is what
  makes a floor sound like tape rather than like dither), and a gentle saturation. Bypassed
  entirely at zero.
* **Externalise** (Space): the two cues a headphone image needs to sit outside the head -- the
  notch the pinna cuts into what arrives from the side, whose frequency moves with the voice's
  angle, and the reflection off the shoulder a quarter of a millisecond later. Brown and Duda's
  structural model, the parts of it that need no measured data.

## Playing it: expression, two conductors, the grid

* **Expression** (MPE, aftertouch, CC 74, bend): pressure pulls a voice towards the listener --
  the plane already decides brightness, level, dryness and presence, so one finger moves all of
  them the way leaning into a note does -- and can also open brightness and level directly; the
  sideways slide moves that voice's filter and its point in the z-plane; bend is per note. With
  MPE on, channels 2 to 16 each carry one note with its own three; without it, the wheel and
  channel pressure apply to every sounding voice. Everything is smoothed inside the voice.
* **Brain Quantize**: the conductor's decisions wait for the next note value of the clock, and
  all of the waiting time is handed over at the tick, so the mean rate is unchanged.
* **Brain 2**: a second conductor with its own register, pace, density and plane, on the first
  one's root plus an interval, with its own random stream. Two of them play a slow counterpoint
  neither would play alone.
* **Room Morph**: a second impulse response and a crossfade between the two rooms. The second
  convolution only runs while the morph is actually between them.
* **Level matching** (plugin): every preset's loudness was measured from a twelve-second render
  and stored in its metadata; while level matching is on, loading a preset trims the master gain
  towards a common target (at most 12 dB) so that auditioning a hundred presets is not a ride on
  the volume knob. A preset that was never measured is left alone.

## Stems, and a score

**Stems.** `Engine::setStemBuffers` takes eight pointers -- near, far, cosmos and room, left and
right -- and fills them alongside the mix; `ambient_render --stems <prefix>` writes the four
files. Each stem is what its plane contributes where it joins the output, so the four sum to the
mix *before* the master stage (the mid/side split, the output DC blocker and the soft clipper come
after them). Two details had to be measured rather than assumed: the Cosmos return is added into
the near bus, so it is subtracted from the near stem or it would be counted twice (it was, and the
four summed 1.6 dB loud); and what the Cosmos sends into the far plane cannot be separated at all,
because the reverb has already mixed it with everything else -- it belongs to the far stem, which
is where it is heard. The self test renders four hundred blocks and compares the mid channel of
the mix with the sum of the stems; measured, the difference sits below -30 dB, and what is left is
the master stage: it is in the side channel (-14.5 dB) and below 50 Hz (-16 dB), which is exactly
the Bass Mono high-pass and the DC blocker.

**A score** (`Core/include/ambient/Score.h`). The set timeline records what you did; the route
walks the map; neither lets you write a piece. A score is a text file of timed ramps:

```
0:00    cosmos_send 0
6:00    cosmos_send 0.45 over 8:00
34:00   far_decay 90 over 6:00
40:00   brain_on off
```

`<time> <parameter key> <value> [over <duration>]`, times as m:ss or h:mm:ss. Without *over* the
value is set at that moment; with it the parameter travels there from wherever it was when the
ramp began, in the parameter's own skewed domain -- the same one the morph and the map blend use,
so a logarithmic knob moves the way a hand would move it. Choices and switches change when their
ramp ends rather than halfway. `ambient_render --score <file>` plays one offline and takes its
length from the score; `docs/example.score` is a forty-minute piece to start from.

## Clock and sync

`Core/include/ambient/Clock.h`. Where the tempo comes from is one setting,
*Clock Source*: **Internal** (the *Tempo* parameter, counting beats while *Run*
is on -- the standalone's own clock), **Host** (the DAW's play head: tempo,
position in quarter notes, playing; the plugin hands it over once per block),
or **MIDI** (MIDI clock at the input, 24 ticks a quarter; the tempo settles
over a beat's worth of ticks so interface jitter does not wobble every synced
LFO; Start/Continue/Stop; two seconds without a tick and it falls back). Host
and MIDI fall back to the internal clock when nothing arrives. The clock's
parameters are performance state like the morph: no preset touches the tempo.

Every rate that wants the grid keeps its free knob and gains a *Sync* choice
(Free, 64 bars … 1 bar, 1/2 … 1/32 with dotted and triplet values); when set,
the division at the current tempo replaces the knob: the eight LFOs (one
cycle per division, and the phase follows the beat position, so a synced LFO
stays on the grid however long it runs and wherever the transport jumps), the
six envelopes (the whole shape spans one division), both delays' left and
right times, the ensemble rate, the cloud's grain rate, the brain's event
rate, the arc period, and each source slot's grain density. Measured: a delay
on 1/4 at 90 bpm renders identically to 0.6667 s typed in; an LFO on one bar
changes the render between 90 and 180 bpm; every preset (all Free) is
unchanged.

## Presets

`Core/src/Presets.cpp`: a preset is a name and a `key=value;…` string over the
parameter table (choices by name). The engine, the render tool (`--preset`)
and the plugin's program list all use the same table. 158 presets in thirteen
families; all render finite with a held chord, levels −29 … −11 dBFS.

The granular family (148..157) sets the Texture slot up and keeps the partial
bank sounding, so the preset makes sense before a clip is loaded and gains its
granular layer the moment one is. In the grain-forward ones the layer carries
the sound: measured against the same preset without a clip, it sits 0.9 dB
below the whole in *Grain Swarm* and 1.5 dB in *Pulverised*.
User presets are saved by the plugin as `.ambientsynth` XML files (full
state including a loaded Scala scale).

### Preset packs

`Core/src/PresetPacks.cpp`: a pack is a UTF-8 text file (`.ambientpack`) read
at start, so a library of thousands does not sit in the binary. One preset per
line, `name|settings|x y bright motion width noisy bass density tags|texture|
wavetable`; everything after the settings is optional, and the two file fields
are resolved against the pack's own folder. Packs come from `$AMBIENT_PACKS`
(one folder, or several separated by `;`), otherwise from
`Documents/AmbientSynth/Packs`, and on the Quest from `<externalDataPath>/Packs`.

The list the rest of the program sees is `builtinPresetCount()` compiled-in
presets followed by every loaded pack, and `numPresets`, `preset`, `presetMeta`,
`numPresetFamilies` and `presetFamilyName` dispatch across both, so the DAW
programs, the browser, the map and routes-by-name pick packs up without knowing
they exist. Each pack becomes one family after the built-in ones.
`PresetMap::warmup` rebuilds when the count changes; the host loads a pack
preset's own sample and wavetable when it applies it, through
`presetFilePath(index, 0|1)`.

`Library/` holds a generated library of 5000 presets in 25 packs, with 1200
texture clips and 608 wavetables (see `Library/README.md` and
`Tools/library/`). Its descriptors are estimated from the settings rather than
measured -- five thousand measurements means five thousand renders -- which is
the one place where the map's numbers are a prediction and not a measurement.

Two layers, loadable independently and combinable (`PresetScope`):
*Sound* = every parameter outside the Cosmos section, *Cosmos* = the Cosmos
section. Applying a preset in one scope resets only that scope's parameters
to their defaults and then to the preset's values; the other layer is
untouched. The 148 full presets serve as the Sound bank (their sound layer)
and as DAW programs (both layers); a separate 32-entry Cosmos bank ("Cosmos
Off", shifters, resonators, vowels, nebulae, shimmers, the sci-fi
combinations) serves the Cosmos box. All 160 render finite.

### Texture slot (granular)

Not a sample player: grains are spawned at exponentially distributed intervals around *Density*,
each with its own start point, playback rate, Hann window and pan, and they overlap freely.
*Grains* (1..64) is the ceiling on how many a slot may have sounding at once; *Spread* scatters
the start point around *Position*, from a 0.1 % window up to the whole clip. The window runs as
a rotating phasor rather than a `std::cos` per sample -- at 64 grains that call was the most
expensive thing in the voice.

Two things were wrong here and are worth writing down. A clip enters at its own level, while the
wavetable and FM slots normalise themselves to unity, so `Texture::measure()` now sets a reading
gain from the clip's RMS. And the overlap normalisation was `0.7/sqrt(N)`: a Hann-windowed stream
at overlap N has RMS `sqrt(N)*0.612*source`, so the constant that returns the source's own level
is `1/0.612 = 1.63`. Together those two were 22 dB: at the same *Level*, a Texture slot was
inaudible next to a Wavetable slot. All three source types now land within 0.8 dB of each other.

Cost, measured: eight voices with both slots granular at 60 grains/s and 800 ms grains --
1024 concurrent grains -- render at 6.5x realtime with *Grains* at 64, 8.6x at 32 and 16x at 8.

### Noise slot

The fourth source type. Ten colours: the textbook slopes (white, pink, brown, blue, violet) plus
grey (white with the ear's most sensitive region taken out, so it *sounds* flat rather than
measuring flat), a resonant Band that can track the played note, Wind (the same band with a
wandering centre and less damping), Crackle (sparse decaying impulses -- vinyl, embers, rain) and
Digital (sample-and-hold white). Position sets the band centre or the colour, Pos Drift lets it
wander, Density is the crackle rate or the hold rate, Noise Q the width.

Two things were measured rather than assumed. The three-pole short form of Kellet's pink filter
is 1.7 dB per octave too steep, so the full seven-term version is used: measured slopes are white
-0.1, pink -3.1, brown -6.0, blue +2.8, violet +5.5 dB per octave. And every colour is
level-matched against a Wavetable slot at the same Level (-22.0 dBFS with the voice filter open);
before that a violet slot sat eleven decibels above a pink one at the same setting.

### GrainCloud

History ring of 4 s fed from the near bus × *Send*. Grains are spawned at
exponentially distributed intervals around *Density*; each has a Hann window
of *Grain* × (0.7 … 1.3), a start point up to *Spray* seconds back, a random
pan, and a playback rate of 1 ± 2 % or, with probability *Pitch*, ×2, ×½
(70 %) or ×1.5, ×4 (30 %). Up to 32 grains overlap; the output gain is
normalised by √(density · grain length) so density does not change loudness.
The cloud is added to the far bus only: it exists in the background and the
far reverb smears it.

## Preset browser and the preset map

Every built-in preset is measured, not described: `Tools/preset_map.py`
renders each one for 12 s (held chord plus a busy brain) and takes the
spectral centroid, spectral flatness, spectral flux, stereo width, energy
below 150 Hz and the mean voice count from the last 8 s, plus flags read
from the parameters (keys or generative, cosmos, feedback, sources, just
intonation, sub, stack, air). The result is generated into
`Core/src/PresetMeta.cpp`: per preset a rank 0..1 for brightness, motion,
width, noisiness, bass and density, a family index (from the comment
blocks in `Presets.cpp`), tag bits (quantiles of the ranks plus the flags)
and a position in a plane — the first two principal components of the
standardised descriptors, with the flags weighted in so cosmos, feedback
and source presets form their own clusters, followed by a short repulsion
pass so no two points overlap. The stub tool mode writes an empty table so
the core compiles before the first measurement; the self test accepts the
stub and checks the measured table.

The **Browse** page of the plugin has two views. **Columns** is the
classic browser (Omnisphere / Absynth style): Family | Character (dark,
bright, tonal, noisy, wide, bass) | Motion (calm, moving, dense, sparse) |
Features (keys, generative, cosmos, feedback, sources, just intonation,
sub, stack, air), each row with the number of presets it leaves; rows in
one column combine with OR (Ctrl-click), columns with AND, "All" clears;
below, the result list with family and descriptor bars. **Map** keeps the
list with tag toggles and shows the plane. Both share text search, sort by
any descriptor, favourites (a star per row, an "only favourites" filter,
kept in the plugin state), a click loads, → A / → B fill the morph slots.
The **map**: points coloured by family,
sized by density, filtered points bright and the rest dimmed, a hover
shows name, family and tags, a click loads. With *Map blend* on, the
cursor (MapX / MapY) is dragged across the plane and the engine plays the
blend of the presets around it: `PresetMap::neighbours` takes the six
nearest points with Gaussian weights of the distance (*Radius* = sigma,
default 0.08 of the plane), `PresetMap::blend` mixes floats in the skew
domain, rounds ints and takes choices and switches from the strongest
neighbour, and `Engine::updateBlend` glides every parameter toward that
blend with the Morph Glide time constant (95 % after *Glide* seconds). On
a point the blend is that preset; between points it is a sound nobody
saved. Switching the map off copies the gliding values into the live
parameters, so the sound stays where the map left it. The map is a
parameter set (MapActive, MapX, MapY, MapRadius, never part of a preset),
so MIDI, OSC, automation and the gesture layer can steer it; on the Quest
the hand menu has MAP ON/OFF and the left hand's reach and height move the
cursor. `ambient_render --map x y [radius]` renders any cursor position
offline and prints the neighbours and weights. Measured: on Sleep
Concert's point the blend reproduces its parameters to 0.1 %; half-way
between two presets a float lies between their values; the engine glides
to Distant Storm's far decay within the glide time and reports the value
through `blendValue`.

## Route (the map plays itself)

`Route` (`Core/include/ambient/Route.h`) is a list of up to 32 waypoints
on the map — position, blend radius, a travel time to reach the point and
a hold time to stay — walked by the engine: `routeStep` moves the map
cursor with smoothstep travel between points (no jumps), holds, and either
loops or stops at the last point and switches itself off, leaving the
cursor where it is. A route always plays through the map blend, so a whole
set becomes a path: parameters *Route Play*, *Speed* (0.25–4×) and *Loop*
are performance state like the map itself. The text form
`Preset Name|travel|hold[|radius]` (or `x,y|travel|hold[|radius]`) names
presets rather than coordinates, so routes survive a re-measurement of the
map; twelve **route presets** (`Core/src/Route.cpp`) are 20–40 minute
sets — Night Descent from Dawn Drift into Deep Sleep Sub, Glass to Storm,
Cosmos Crossing, Ninety Minute Arc and so on. The plugin's map view has the
route strip (route preset, play, loop, speed, *+ point* appends the cursor
as a waypoint, *Route…* edits the text) and draws the route with numbered
points and the segment being walked; the Quest hand menu has ROUTE
PLAY/STOP with the route from `ambient.cfg`; `ambient_render --route
"Night Descent" 40` renders a set at 40× speed. Measured: every route
preset parses and round-trips through text, a two-point route reaches the
smoothstep midpoint half-way through its travel, and at speed 2 the engine
walks a 12-second route in 6 seconds and switches itself off.

## Sets as timelines, and the automatic sound test

**Set recording.** `SetTimeline` (`Core/include/ambient/Timeline.h`) holds
every parameter change and every note with its time. The plugin's Perform
page has *Record set*: the starting state goes in at t = 0, then every
block logs the parameters that changed (hands, knobs, OSC, MIDI, macros,
routes — all of it ends up as parameter changes) and the notes; *Stop*
saves a `.ambientset` text file (`12.500 param far_decay 42`, `13.02 on 57
0.8`, `40 off 57`). *Play set…* replays one through the host parameters,
and `ambient_render --set-file set.ambientset` renders it again offline
at any sample rate, with the set's length plus 20 s of tail by default. A
good evening becomes reproducible, and can be rendered in higher quality
than it was played.

**Sound test.** `Tools/preset_check.py` renders every preset for 10 s
(held chord plus a busy brain) and fails a preset that is louder than
−12 dBFS in its second half, peaks above 0.98, jumps more than 0.3 between
samples, carries more than 0.02 DC or goes non-finite; exit code 1 when
anything fails, `--json` for a report. Its first run found four: Alto
Voices too loud (−11.4 dBFS, master trimmed) and three presets with DC of
0.03–0.11 — every preset that uses the feedback's phase modulation. A
partial that modulates its own phase carries a DC term (J1 of the index,
the same mechanism as an FM pair at ratio 1), so the voice now blocks DC
after the bank whenever the FM path is on; and a saturating feedback loop
whose DC gain exceeds one locks onto a DC operating point (Feedback Hiss
sat at +0.11), so the loop now blocks everything below 10 Hz before the
saturation. The measurement caught what listening had not.

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
  file chooser. The page is laid out once in a fixed design space and scaled
  as a whole, so dragging the corner zooms and never reflows. Everything is
  in sight at once, nothing scrolls: two columns (voice and morph on the
  left; effects, cosmos and conductor on the right), and rows whose sections
  are of a kind -- the three sources, the two filters, the effect pairs, the
  conductor's tables, morph and macros -- page through tabs, each tabbed row
  as tall as its tallest page so switching moves nothing else. The room a
  row's knobs leave goes to a live display drawn from the engine's numbers:
  the oscillator's cycle and partials, the source slot's table, grain window
  or noise colour, the two filters' response, the conductor's notes as a
  scrolling roll. Along the bottom the modulation strip: one card per source
  (dragged onto a knob it becomes a route; right-click lists and removes its
  routes), and tabs with the eight LFO editors, the six envelopes and the
  matrix as text. A modulated knob wears a thin ring in its source's colour
  and a second arc from its value to where the modulation is pushing it this
  instant; its right-click menu lists what drives it, each route removable.
  The editor is four translation units (`EditorCommon.h` holds what they share): the frame and
  the pages, the modulation strip, the in-grid displays, the browse and perform pages. Each
  section's title carries a die: a click draws that section's parameters again (in the
  parameter's own skewed domain, inside the middle 70 % of its range), shift nudges them.
  Undo, redo and an A/B compare work on whole parameter snapshots. The header carries the
  output's own spectrum with its peak level, and a Compact switch that wraps the widest rows
  into two: the page goes from 1.9 : 1 to 1.6 : 1, still without scrolling. (Wrapping every
  wide row, rather than only those above ten cells, gave 1.27 : 1 -- worse than the shape it
  started from, which is why the threshold is where it is.)
  Help (`Core/include/ambient/Help.h`): one or two sentences for every
  parameter (`paramHelp`, families share their text so the three slots and
  eight LFOs cannot drift apart; the self test insists every parameter has
  one) and the manual by topic (`helpTopicText`). The plugin shows them as
  tooltips, as the header line that follows the mouse (name, value, text,
  how many routes drive it), and as the Help page (button or F1): topics on
  the left, the text at a readable line length, and to its right the
  pictures -- a drawn signal-flow diagram for the overview, and for the other
  topics snapshots of the topic's sections taken from the panel that moment
  (the tab is switched in for the picture and back) plus a second, live copy
  of the unit's display. Texts live in the core so every shell tells the
  same story.

## Roadmap

1. **Sound** — done since v0.2: spectral freeze (Nebula), head-shadow
   low-pass on the far ear (20 kHz → 3 kHz at full lateral position, scaled
   by *Time Width*), user preset files, a second delay in series, the
   granular cloud, independent Sound/Cosmos preset layers. Open: per-preset
   random seeds, a "morph" between two full presets over minutes, MIDI
   learn for the standalone.
2. **Performance** — SIMD across partials is done (`Core/include/ambient/Simd.h`,
   AVX2 with a scalar path): the median preset went from about 39× to 52×
   realtime on one core, the slowest from 17× to 17×.

   **Voice rendering in parallel was measured and dropped, deliberately.** The
   voices are rendered per control block of 64 samples with the conductor
   interleaved between blocks; the work per block is a few microseconds, which
   is the same order as the cost of synchronising two threads, so splitting the
   voices there would buy nothing. Making it worthwhile would mean rendering a
   whole chunk per voice group, which moves the conductor's decisions from a
   1.3 ms grid to a 10 ms one and changes every render. At 52× realtime in the
   median and 17× in the worst case, on one core, there is nothing to buy: the
   instrument is not short of time. If a future feature changes that (many more
   sources per voice, say), this is the note that says what to do and what it
   would cost.
3. **Quest** — CMake toolchain for the Android NDK (arm64-v8a), Oboe for
   low-latency audio, OpenXR for hands and head; the visual layer is a
   separate concern and can reuse the Kaleidoscope engine's ideas (calm
   motion, no camera shake). `Core/` is expected to compile unchanged; the
   `Engine::soundingNotes` / `noteDistance` / `arcValue` observers already
   exist for a visualisation to read, and the distance model maps directly
   onto placing sound sources in a 3D scene.
