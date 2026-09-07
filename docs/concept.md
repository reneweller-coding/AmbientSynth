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

### Autoplay: Free and Chords

The conductor above is *Free*, and Free stays the default: notes come and go
on their own timers, which makes the cluster breathe but never really move --
after ten minutes it is the same harmony, differently arranged.

*Chords* keeps the cluster **full** and exchanges exactly **one voice at a
time**. Every *Every* seconds (or on the clock, via *Sync*, or the moment
somebody presses *Step* / the panel's button / a mapped controller) the voice
that has been sounding longest leaves and one note takes its place. The
candidate is scored for three things at once:

* **How it sits against the voices that stay** -- the mean consonance with
  each of them and with the root, not just with the root. That is what makes
  the result a chord rather than a heap. *Tension* is the exponent on that
  term: at 0 only notes that fit the whole chord are considered, turned up the
  progression is allowed to lean.
* **How far that voice has to travel** -- *Voice Lead*, in semitones, is both
  a hard limit and a preference within it. Small is voice leading: the note
  that leaves is replaced by a near neighbour, and the ear hears the chord
  shift rather than one note being cut and another started. Large lets the
  harmony jump.
* **Doubling** -- an octave of a pitch class already sounding scores ×0.2, the
  root's own class ×0.5, so an exchange brings a new colour instead of
  thickening one that is already there.

* **What left recently** -- the last eight notes to leave carry a penalty that fades with age
  (×0.12 the moment they go, back to full after eight exchanges). Without it the harmony keeps
  picking up the note it has just put down: it is the nearest candidate and it fitted a moment
  ago, so it wins again. Nothing is banned, only postponed.

*Root Move* decides how often the exchange moves the root as well (the same
`wanderRoot` the Free mode uses); without it the harmony circles one centre
for ever, with it the piece travels. A small random factor (0.85–1.15) sits on
the final score so the same chord does not always resolve the same way.

The note being replaced is removed from the cluster **before** the candidates
are scored, so it does not vote on its own successor -- and is itself excluded
from the running, because at zero distance it would always win and the chord
would never move. That was a real bug; the self test caught it, and the
descriptor oracle never would have, because both modes render sound that
measures the same.

Filling an empty cluster adds one note per tick rather than all of them at
once, so the first minutes are an entrance and not a chord.

**Does it actually form chains?** Measured over an hour of simulated time, per
setting: how many exchanges happen, how many of the resulting chords are ones
it has never been in, how often it swings back to the chord two exchanges ago
(that would be a pendulum), and how far the mean pitch of the chord travels.

| Setting | Exchanges | New chords | Pendulum | Drift |
| --- | --- | --- | --- | --- |
| Lead 3, Tension 0.15, Root Move 0.25 | 56 | 56 | 0 | 6.4 st |
| Lead 7, Tension 0.5, Root Move 0.8 | 98 | 98 | 0 | 5.7 st |
| Lead 12, Tension 0.7, Root Move 0.5 | 116 | 116 | 0 | 6.0 st |
| Lead 5, Tension 0.25, Root Move 0.35 | 86 | 83 | 0 | 3.6 st |
| Lead 2, Tension 0, Root Move 0 | 116 | 35 | 0 | 2.6 st |

The last row is the corner where everything is set to its tightest: the root
is pinned, a voice may move two semitones, and only notes that fit the whole
chord are allowed. There the harmonic field really is finite, and circling it
is the correct answer rather than a fault -- *Root Move* is what opens it, and
its default is 0.2, not 0. (Before the recent-notes penalty that row managed
35 chords in name only: 18, with 98 revisits.) The self test holds the
property for the default range: over an hour, dozens of exchanges, nearly all
of them into a chord it has not been in, never one two steps back, and a chord
that has moved in pitch.

### Filter models

`Core/include/ambient/Filter.h`. The voice filter is one of ten models
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

After the idea Dave Rossum built into the E-mu Morpheus: filter frames sit
on the corners of a **cube**, and a point (X, Y, Transform) inside it is a
filter whose poles *and zeros* are interpolated between them — move the
point and the whole resonant structure glides, always through stable
filters. The patent (US 5,170,369) expired long ago and the manuals
describe what the filters do, but the coefficient tables in the original
firmware are proprietary data; this bank is our own, built in the same
architecture, with its numbers from published acoustics.

**155 shapes in twelve families** (voice, sweeps, combs and phasers,
strings and bodies, bars and bells, membranes, tubes and pipes, rooms,
series and exotic, extremes, instruments, EQ and speakers) — the original
shipped 197 cubes, and this is the same order of thing. They are generated
by `Tools/make_zplane_bank.py` rather than typed, because almost every one
of them is a set of frequency *ratios* — the mode series of a bar, a
membrane, a pipe, a bell — times a base frequency, and ninety-six shapes
times eight corners times six frequencies is an invitation to type 2.756
as 2.576 and never find out. The ratios are written down once, with a note
saying where each comes from (Fletcher & Rossing for the bars and bells,
the sung-vowel formant tables for the voices, `c/2L` for the room modes),
and a machine does the multiplication.

The first sixteen shapes are the original hand-written bank, carried over
frequency for frequency and frozen: the library's 6000 presets name their
shape as text (`z_shape=Glass`), so those names and their order can never
change. New shapes are appended, never inserted.

**The third axis** is what makes it a cube rather than a square, and the
original's name for its filters says so. Writing eight corners for every
shape would be twice the data for a face that is usually the same idea
again, so each shape instead names a *rule* for how its far face differs —
sharper, damped, peaks turned into notches, an octave up, the partials
fanned out. Transform's default is 0, which is exactly the square that was
there before the axis existed, so every preset ever saved sounds as it did.

`Core/include/ambient/ZPlane.h`:

* A frame is up to **six cascaded two-pole/two-zero sections** — a 12-pole
  filter, the order the Morpheus used. Interpolation is trilinear in log
  centre frequency and log bandwidth on the *pole and zero parameters*,
  never on coefficients, so every point inside the cube is stable by
  construction. The self test checks that claim rather than repeating it:
  all 155 shapes at three corners each, every section tested for stability,
  every cascade for a finite and sane level, and every shape for whether
  moving the point actually changes the sound — a shape whose corners agree
  is not a filter you can morph, it is a filter with three dead knobs.
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

### Four sources

Every voice has four equal source slots (three until the Vector arrived; the
fourth exists so the square's four corners are four sources rather than three
and the three together). Source 1's *Type* defaults to
**Additive**, which is the strand bank described above (unison, detune,
stacks, bloom -- the classic Oscillator; its Octave, Ratio and Pan move the
whole bank); set to anything else the bank falls silent and the slot renders
in its place, so a voice can be four granular players or four FM pairs.

Each slot has its own clip. The engine used to hold one texture for the whole
instrument, which meant two Texture slots always played the same recording;
now `setTexture(slot, ...)` fills one of four double-buffered clips, the
slotless call fills all four (what a preset naming a single file always
meant), and a pack preset's texture field may carry up to four paths
separated by `;`, an empty one meaning that slot has none. Adding the fourth
slot had one trap worth writing down: the slots fork the voice's random
stream in order, and a fourth fork advanced that stream by one draw and moved
every random decision after it -- the oracle reported all 39 presets changed.
Slot 4 seeds from a side stream instead, and the oracle is back to 39
identical.

### Stretch: a recording as a continuum

The Texture type reads a clip as grains; Stretch reads the same clip as a
continuum. It is Paulstretch inside a voice: a window of the recording
(Grain, up to a third of a second) is transformed, its magnitudes are kept
and its phases thrown away and drawn afresh, and the frame is overlap-added
at a quarter of the window -- the Cosmos's Nebula, pointed at a clip rather
than at the mix. Every frame is a plausible slice of the clip's spectrum with
no memory of where its transients were, so the analysis position can crawl
through the recording at a thousandth of its speed and what comes out has no
grain rhythm and no attack left standing. This is the tool the ambient
literature describes for Rich's *Perpetual*: field material stretched by
factors up to a thousand until only the overtone weave remains.

Two decisions shape it. Pitch is applied when the window is *read*, as a
resampling step through the clip, and the stretch is applied to how far the
read moves between frames; the two do not know about each other, so
Follow = Note plays a chromatic sample across the keyboard without a high
note ending sooner than a low one -- measured, the pitch comes out identical
to the granular player's in both modes (110.7 Hz Free, 220.3 Hz at A3). And
the loop's seam is crossfaded over the last part of the clip into its first,
so the wrap lands on the sample the fade has been arriving at, unless the
file name carries `_loop`: the generator marks clips it has made seamless,
and those wrap straight round.

Cost: a 16384-point transform per hop per voice per slot. Nine voices with
four Stretch slots each render at 6x realtime on the development machine
against 20x for four Texture slots; one slot in a few voices, which is what a
patch actually does, is not a number anyone will notice. The transforms are
shared across all slots of all voices, built once in `prepare()`, read-only
after; the buffers are sized once for the longest window and never touched
by an allocation in `render()`.

**The field recordings.** Five hundred places for it, from the same
text-to-audio worker that made the textures, pointed at environments instead
of instruments: twelve categories of six prompt seeds each -- rain on a tin
roof, a harbour in fog, pack ice, a power station through a wall -- crossed
with weather, distance and hour (`Tools/library/make_field_recordings.py`).
Stable Audio Open 1.0 only: 44.1 kHz stereo and the best of the three models
at places; AudioLDM 2 is 16 kHz and has no air. Twenty-six seconds each,
because at forty times slower than life that is a quarter of an hour and a
minute per clip would be a gigabyte the package does not need.

Every one of them is seamless by construction rather than by luck: the last
1.5 s are faded into the first (equal power), the overlap is trimmed, the
seam is *measured* -- the largest sample-to-sample step across the wrap
against the largest inside the clip -- and the file is marked `_loop`, which
is what the Stretch type reads to skip its own crossfade. The mark goes in
front of a trailing pitch token, not after it, because the engine reads the
clip's pitch from the last token of the name and a `_loop` after `_A3` would
have hidden every tonal recording's pitch. They land in `Library/Textures`
under the `field_recordings_` prefix, which is the slug the preset generator
uses to find a style's own clips, and the *Field Recordings* pack is built
from them with the Stretch type in every slot it fills, each slot drawing its
own clip: a preset can be four landscapes in the Vector's four corners.

Three measurements that had to be made rather than assumed: the type makes
sound (a silent new source type is the easiest thing in the world to ship),
a different stretch factor is a different render (the factor moves the read,
and a render blind to it would mean the read was not moving), and the
seamless mark is read from the name. The first draft of the pitch test failed
at 327 Hz for a 110 Hz clip -- not the stretch, the Air band, on by default at
three times the note and counted by a zero-crossing detector. With Air off:
110.7.
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
* **Autoplay**: the conductor in *Chords* mode keeps the cluster full and exchanges one voice at
  a time, on a timer, on the clock or by hand (see above). *Free* -- the original conductor -- is
  the default and is not going anywhere.
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

### BEAT: the instrument listening to its own tuning

The Foundation's ghost tone already takes the two lowest sounding voices and
sings their frequency difference as a bass note. That difference is only half
the story. What the ear reacts to in a held chord is not the combination tone
but whether the interval is *in tune*: two voices a fifth apart beat at
|2f₂ − 3f₁|, which is silent when the fifth is just and quicker the further it
has drifted. **BEAT** is a modulation source whose rate is exactly that.

It finds the simplest just ratio near the interval the two lowest voices make
(from a short list — small numbers only, because those are the ones whose
harmonics are close enough together to beat audibly) and runs at the difference
between the harmonics that would coincide if the interval were exact. For a
mistuned unison that is |f₂ − f₁|, the difference tone itself. Below a fiftieth
of a hertz the phase simply holds: a chord in tune should leave whatever it is
driving exactly where it is, not creep.

So the sound breathes at the rate of its own mistuning. Purity Drift is what
sets it moving; route BEAT at a filter, at the Nebula's smear, at anything.

The test compares two intervals in one tuning rather than one interval in two
tunings, and the difference matters. The first version played a fifth with the
scale set to just and again in equal temperament, expecting near-zero and about
half a hertz. Equal temperament gave 0.469 Hz, which is textbook — two cents
narrow at that pitch. "Just" gave 5.8 Hz, because it was measuring the tuning
system and the per-voice pitch drift rather than this source. An octave is
exactly 2:1 in every temperament there is, so the test now uses that as its
zero: octave 0.000 Hz, tempered fifth 0.469 Hz.

### Six small things, and what measuring them cost

Six changes that each sound like a one-line tweak. Four were; two were not, and
the two that were not are the interesting ones.

**Gravity is now a magnet, not a mood.** The portamento's pull towards
consonant ratios braked in proportion to `intervalConsonance()` of the ratio
the slide happened to be passing through -- a quantity that rises and falls
smoothly across the whole glide, so the pull was everywhere and nowhere, more
like wading than like a magnet. It is now the distance, in cents, to the
nearest just ratio, with the strength `1 / (1 + (d/30)^2)`: half strength at
thirty cents, eight per cent at a semitone, nothing at all in between the
nodes. A magnet has almost no reach and then all of it.

**The Kuramoto ring is asymmetric.** Every pair pulled on the other equally,
so a high Coherence settled into exact synchrony and stayed there -- four
oscillators behaving as one, which is the opposite of what the section is for.
Each is now pulled a little harder by the oscillator behind it in the ring than
by the one in front (`w = 1 + 0.22 sin(2*pi*(j-i)/4)`). An antisymmetric
perturbation has no synchronous fixed point, so the bank locks in frequency and
keeps a slowly turning spread of phase, which is what a ring of coupled
biological oscillators does.

**The air ahead of the far reverb saturates.** A very gentle asymmetric shaper
between the diffuser and the reverb's feedback network, riding on Diffuse so
there is no new knob: a dense cluster fired into the hall comes back thickened
rather than reflected.

**A mono safety net in the master.** Everything upstream is built to widen, and
a drone that is gigantic in stereo can vanish when a phone sums it. The side is
measured against the mid over about a second and a half, and if the side is
half again the power of the mid the width is eased back -- by a quarter at
most, at two per cent a second, never touching the middle of the mix. It is a
parameter (*Mono Safe*, on) because someone may prefer the width and their own
ears.

That threshold cost a measurement. At "side louder than mid" the guard engaged,
minutely, on thirty of the thirty-seven reference presets -- by an amount too
small to move any descriptor, but it engaged, and a safety net that is always
slightly on is not a safety net, it is a change to the sound. At half again it
leaves the median preset bit for bit identical and pulls back the few that were
genuinely collapsing: the worst case gains 0.44 dB of level and loses 0.45 dB
of mono loss, which is the trade the feature exists to make.

**The delay ducks.** While the input is loud the high cut inside the feedback
loop drops, so a fresh attack does not fight the brightness of the last one's
tail. Three things had to be got right, and each of the first two produced a
confident wrong answer first:

* The trigger. A level follower simply darkens any loud passage, which is a
  tone control with extra steps. It is now the fast envelope against the slow
  one -- how far the input stands above its own average -- so a drone ducks
  nothing and an attack ducks hard.
* The release. Ten milliseconds, which meant the loop was dark for the first
  echo and open for the rest; the measured effect was five per cent and looked
  like nothing. A second lets a whole train of echoes stay out of the way.
* The measurement. The first version compared high-frequency energy *relative
  to total* energy and reported that ducking makes the loop brighter -- a
  darker feedback loop builds up less of everything, so the ratio rises while
  the sound plainly darkens. And it measured during the plucks, where the wet
  output is the unfiltered delay read and the first repeat is as bright as the
  attack by design. Measured absolutely, in the echoes, it is fifteen per cent
  of the high end.

One claim is deliberately not asserted anywhere: that the loop "opens again
once the note has gone". The envelope does release, but it cannot be shown in
the audio, because a darkened feedback loop also loses energy faster -- by the
time the filter has opened there is almost no tail left to be brighter. The
test says so in a comment rather than asserting something the signal does not
do.

### Modal mode: the same bank, read as objects

The cascade shapes; a modal bank rings. That is the whole difference, and it is
larger than it sounds. Six biquads in series take away what is not wanted and
leave what is: stop the input and the filter stops. Six two-pole resonators in
*parallel*, each with its own decay time, keep sounding after the input has
gone -- which is what a bar, a bell, a membrane or a room actually does. It is
modal synthesis (Smith, *Physical Audio Signal Processing*; Bilbao, *Numerical
Sound Synthesis*), and it costs almost nothing here because **the data was
already in the building**: the 155 shapes are mode series of struck and blown
objects taken from the acoustics literature, and the cascade was using them as
filter frequencies. Modal mode uses them as what they are.

`z_mode = Modal` (a fourth value, appended, so no preset that names Series or
Replace is touched) with two parameters of its own: *Decay*, the T60 of the
lowest mode, up to forty seconds; and *Damping*, how much shorter the higher
modes ring -- 0 for everything holding equally, which no real object does, 1
for decay inversely proportional to frequency, which is roughly what wood,
metal and skin do.

Every resonator is normalised to unity gain at its own frequency, so a steady
tone at a mode cannot make the bank run away however long the decay is set, and
the bank as a whole is normalised on expected power rather than on the sum of
the peaks -- the modes are at different frequencies and almost never in phase,
so adding peaks would leave a bank of six far quieter than a bank of two.

The self test measures the two claims rather than repeating them: an impulse
in, and the tail has to be clearly present at half the stated decay and roughly
sixty decibels down at the decay itself, for 0.5, 2 and 8 seconds; and damping
has to leave less energy in the tail than no damping. The 44 modal presets --
one for every shape that is a physical object -- are rendered and measured with
the rest.

**And the descriptors were not enough this time.** Five modal presets came out
as identical twins: a bank of narrow resonators fed with noise measures almost
the same whatever its modes are. Their rendered audio hashes were all
different. The twin test now needs both -- the same descriptors *and* the same
hash -- which is the third time in this instrument that a measurement had to be
sharpened before it meant anything, and the third time the symptom was a
confident report that everything was the same.

### The ZDF question

The first thing in the literature on modulating filters is Zavalishin's
topology-preserving transform: solve the zero-delay feedback rather than let a
unit delay sit in the loop, and a filter stops detuning and clicking when it is
swept. That has been in this instrument since the beginning -- `Svf` in
`Dsp.h` is exactly that structure (`g = tan(pi f / sr)`, the trapezoidal
integrators, the feedback resolved algebraically), and the low pass, high pass,
band pass, notch, peak and formant models are all built on it.

The one place that is *not* zero-delay is the four-pole Ladder, which keeps a
sample of delay in its feedback path on purpose: that delay is part of what the
model sounds like, and every preset that uses it was voiced with it. A
zero-delay ladder would be a different filter and belongs beside it as a new
model rather than in place of it.

### Envelopes, and a control that was only a picture

The voice's amplitude envelope has always been a plain ADSR -- Attack, Decay,
Sustain, Release, with times up to a minute for the attack and two for the
release, which is what a drone wants. The six modulation envelopes have always
been more than that: up to sixteen breakpoints, a curve on every segment, an
optional sustain point and an optional loop.

What they did not have was any way to reach them. The four knobs under each
curve set the mode, the time scale, the depth and the sync; the *shape* could
only arrive from a preset or from the text form. So the curve on the panel was
a drawing of something the user could not touch, and the honest impression it
gave was that the instrument could not manage an ADSR -- which it could, twice
over, in two places nobody could get at.

The curves are edited on the curves now: drag a breakpoint, double-click the
line to add one or a point to remove it, right-click for the sustain point, the
loop, the curvature of a segment, and ten shapes to start from with ADSR at the
top of the list. The shapes live in the core rather than in the editor so that
the self test can check that each of them parses, starts at zero, keeps its
times in order and survives being written and read back -- a shape string with
a typo in it does not fail loudly, it simply does nothing when the menu item is
picked, and the only symptom is somebody clicking ADSR and watching nothing
happen.

One thing worth remembering from building it. The first version of the hit test
had its own copy of the row's geometry -- 8 and 16 and 0.34 against the
drawing's 5 and 10 and 0.36 -- and left the Depth knob out of the vertical
entirely. It compiled, it ran, and it would have grabbed points several pixels
away from where they were drawn, harder to explain than a crash. The drawing
and the mouse now ask one function for that geometry, which is the only way two
of them can never disagree.

**And then nobody used it.** Counted across all 6191 presets afterwards: 3986
carried an envelope shape, every one of them routed, every one curved rather
than linear, and 45 % looping -- so the feature was being used, in the library.
But the ceiling was nowhere near: envelopes 1 and 2 only, at most six of the
sixteen breakpoints, and *Sustain Loop*, one of the three modes, used by none of
six thousand. And not one of the built-in presets carried a shape at all, so
somebody clicking through the Sound box would never have met the thing.

Both of those were the generator's ceiling rather than the instrument's
(`Tools/library/make_presets.py` drew "nought to two envelopes, three to six
points" and never a sustain point), and both are lifted: up to six envelopes on
a golden ladder of times, up to sixteen points, and a sustain point on a third
of the shapes that are long enough to have something to rise through and
something left to run out. Five built-in presets show the range -- *Long Arc*,
*Held Breath*, *Six Hands*, *Sixteen Points*, *Dwelling Curve*.

Building *Held Breath* found that the sustain point did not actually work.
The clock the envelopes run on is a phrase clock, restarted when a note arrives
into silence; while a note was held the shape correctly waited at the sustain
point, but at the moment the last voice let go the release found that clock far
past the end of the shape and **snapped** to its final value. Not a glide, a
jump -- the one thing this instrument is not allowed to do. Each envelope now
has its own clock, and on that transition the clock of a Sustain Loop envelope
is put exactly on the sustain point, so the tail plays from the value the hold
ended on. Nothing that existed changed: no preset used the mode, and the 37-
preset sound oracle reports 39 identical, 0 different. The self test drives the
engine through hold and release and fails if the value moves by more than 0.02
across the release -- which it did, before the fix, by half the shape.

The wider lesson repeats one from the presets round. *Sixteen Points* was built
first with the envelope pointed at a formant filter over a bright thin drone,
and measured, its envelope did nothing at all: the centroid moved 3.0 % with the
envelope on and 3.1 % with it off. Two reasons, both invisible from the settings.
The *Air* band, at 0.24, is broadband noise sitting on top of the spectrum and
pins it -- with it in, sweeping the cutoff from 400 Hz to 4 kHz moved the
centroid by 8 %. And the conductor changing notes every few seconds moves the
centroid far more than any filter does, so the thing being demonstrated was
buried under the thing that was not. Rebuilt sparse, long-held and nearly
airless, the same envelope moves it from 3.9 % to 11.1 %. A demonstration has to
put the thing being demonstrated in front of the microphone, which is the same
sentence as three of the traps in this file.

### Loudness, and why an ambient synth measures it

The literature on this music is unanimous that loudness is the enemy: a
brickwall limiter takes the finest amplitude movement out of a reverb tail and
leaves it grainy and flat, and the impression of an enormous room comes from
the distance between the quietest texture and the loudest swell rather than
from the average level. Targets quoted for the genre are -18 to -24 LUFS
integrated, a crest factor above 14 dB and a true peak at -1 dBTP, against
-9 LUFS and 6 to 9 dB for commercial pop.

So the instrument measures itself, to ITU-R BS.1770-4: integrated (both gates),
short-term, momentary, loudness range, true peak between the samples, and the
crest factor. `Core/src/Loudness.cpp`, framework-free like the rest of the core,
fed from the engine after the master stage -- the integrated figure is a number
about the whole piece, and a meter that only sees what the interface happened to
ask for has holes in it.

Two things were worth getting right rather than approximately right. The
K-weighting is **not** the RBJ cookbook: given the standard's own f0, Q and gain,
the cookbook's shelf comes out about two per cent away from the coefficients
BS.1770 prints for 48 kHz -- close enough to look correct and wrong enough to be
wrong. The formulation used here reproduces the published numbers to sixteen
digits and is derived from the analogue prototype, so it holds at 44.1 and 96 kHz
too. And the whole meter was checked against an independent implementation of
the published coefficients over the same sixty-second render: -23.62 against
-23.62 integrated, -21.62 against -21.62 short-term. The self test pins a
constant that came from that reference, not from this code: a 997 Hz sine at
-20 dBFS RMS in both channels reads -16.99 LUFS.

### Four things the panel got wrong, and what it says now

Four observations from the same round, all right, and worth writing down as
reasoning rather than as a changelog.

*Strands had a tab of its own.* It is not a source: it is the strand bank of
Source 1's additive type -- unison, detune, stack, bloom -- and it is greyed
out the moment Source 1 is anything else. A tab suggested a fifth thing beside
the four sources. The ten controls now sit under Source 1's own display, in the
display's column, wrapped to its width (a page may name a section to place
*under* its display, `TabRow::under`); Source 1 is narrower for it (nine cells
instead of twelve), the additive-bank picture is half the height it was, and
the row lost a tab that only ever meant "Source 1".

*Strike sat among the sources.* It is a sound source, but not one of the four
oscillators: it fires at note-on, independent of what the slots do, exactly
the way the Cosmos is independent of them. It now shares the Cosmos group as a
tab -- COSMOS | STRIKE -- and the sources row is the four sources and the
Vector.

*There was no button for the main page.* Perform, Browse and Help each had a
toggle; the panel was where you landed by switching them off, a rule nobody
should have to learn. **Main** is a button now, lit while the panel shows;
Help sits last, where a manual belongs; and Calibrate and Gestures -- the two
controls only a headset needs -- live under one **VR** button as a menu
instead of taking two places on the toolbar of an instrument that mostly
plays on a desk.

*The Matrix tab was a text box.* One route per line, "lfo1>cutoff:0.4", and an
Apply button: exact, scriptable, and the wrong thing to put in front of a
musician, who opens a tab called Matrix expecting to see routes rather than
their spelling. A grid of every source against every parameter is not the
answer either -- thirty-odd sources by four hundred targets is a wall with a
dozen live cells in it. It is a table now (`EditorMatrix.cpp`): one row per
route, each a source, a target grouped by section, a depth you can drag, an
optional via source that scales it, and whether the source is read as 0..1 --
with "+ route" and a remove on every row. It hands the whole matrix back to
the engine as the same text the box used, so the parser, the presets and the
drag-a-card-onto-a-knob path see nothing new.

### The manual

The help page inside the instrument is the manual, and it exists as a PDF as
well, so that somebody can read it before installing anything. It is not
written twice. `AMBIENT_MANUAL=<folder>` makes the standalone export its own
help -- every topic's text, and its pictures -- and `Tools/make_manual.py`
turns that folder into an HTML book and prints it.

The pictures are the point. They are snapshots of the panel itself, taken from
a running instrument as each help topic is opened: the real sections with their
real values, and the live displays with something actually in them. A drawing
of a section goes stale the day the section changes and nobody notices for a
year; these cannot, because they are made again every time the manual is.

Three things had to be learned to make that work, and all three are the kind
that produce a plausible-looking wrong answer rather than an error:

* The export waits five seconds before taking its pictures. The displays of a
  running instrument -- its spectrum, its stage, its note roll -- are empty
  until it has been playing for a while, and a manual illustrated with empty
  boxes is worse than one with no pictures at all.
* It runs with a preset in which all three sources and the effects are in use.
  The pictures are of the panel as it stands, so the first draft illustrated
  its Sources chapter with a section greyed out because Source 3 was off.
* `juce::String::formatted` is wide-character, so `%s` handed a `const char*`
  writes the bytes as UTF-16. The first run produced a file called
  `topic-00-汦睧.png`.

And a fourth, in the printing: Edge's old `--headless` flag exits with status
zero and writes nothing at all on version 152. `--headless=new` prints in a
second. The tool tries the new flag first and falls back, so an older browser
still works, and if there is no browser at all the HTML is still written --
the manual is not held hostage by one.

### The manual, second draft: every block, every type, every tab

The first manual was a book of chapter texts with two or three section
pictures each and an appendix of every parameter. Read by somebody who did
not have the instrument in front of them it was thin in exactly the places a
manual is for: what does the *Feedback + Room* tab contain, what do its knobs
do, what does the *Stretch* type look like when it is playing. The second
draft answers those questions mechanically, so they cannot go unanswered
again:

* **Every tab of the panel is a picture**, photographed as the tab (its bar,
  its sections, its display, and on Source 1 the strand bank under the
  display), captioned with the name it wears on its bar. Twenty-four tabs,
  the three tabs of the modulation strip, the Perform and Browse pages, the
  sections that have no tab (Space, Foundation, Strands, the Master in its
  corner of the header). The in-app help page does not draw these -- its
  picture column holds two or three -- so they are a second list
  (`tabPics`) that only the export reads.
* **Under every picture a paragraph says what the block is for**, and under
  that every parameter of the sections in the picture with its help text.
  The paragraphs are a table in `Help.cpp` (`tabHelp`) keyed on the tab's
  name; the parameter lists come from the same `paramHelp` the tooltips use,
  exported structured (`params` in `manual.json`) rather than as the text
  blob the appendix is. Sources 2, 3 and 4 are the same twenty-six knobs
  three times over, so they are printed once under Source 2 and the other
  two pages say so.
* **The gallery of source types.** A slot's picture shows whatever type the
  preset happens to use. For the manual the export sets Source 2 to each type
  in turn -- Additive, Wavetable, FM, Texture, Stretch, Noise -- loads a field
  recording for the two that need a clip (`AMBIENT_MANUAL_CLIP`), waits for
  the display and the greyed-out knobs to follow, and photographs the tab.
  That is why `exportManual` became a list of steps a third of a second apart
  rather than one function: a parameter set from the message thread reaches
  the display on the next timer tick, not in the same call.
* **The presets chapter explains the groups**: the twelve families of the
  built-in presets and all thirty-four packs, each in a sentence -- who it is
  written in the spirit of and what corner of the repertoire it covers --
  rather than the list of numbers it was.

Two pictures were wrong in the first draft and both for the same reason. The
Master section is painted in the header, so its bounds are the editor's, not
the content's; photographed from the content it came out as a strip of the
panel's top-left corner, which is what a reader saw under "MIDI, OSC, files".
And the signal-flow diagram is a fixed canvas scaled to fit its component, so
one of the two dimensions is always left over; photographed whole it was a
diagram with a field of black under it. Both now photograph what is drawn
(`drawn()`), from the component that draws it.

The diagram itself was redrawn: it still showed three sources, nine filter
models and no Vector, Strike, Blur, Body or Patina, and it drew the master
chain in an order the code does not run it in. It now shows the four slots,
the Vector and the Strike, both filters with the fold, the foreground with
the Haas band and the microshift, the background with its own width, and the
output chain in its real order -- body, mid/side, sub, patina, subsonic,
master, clip.

And the printing broke a second time, differently. Edge with the default
profile -- or with any profile the script had used before -- takes the
command over and exits at once, without a file; a reused folder printed
nothing in twenty seconds, a fresh one in two. The launcher also returns
before the child that prints has written anything. `make_manual.py` now
makes a new profile folder per print, removes it afterwards, and waits for
the file to appear and stop growing.

### The mixing desk

The dark-ambient production literature -- a second paper after the ambient
sound-design one -- is mostly about mixing: stereo width that survives mono,
depth as a funnel, a wavefolder where a saturator would be, aftertouch on
three things at once. Measured against the instrument, most of it was there
(bass mono, side air, three reverb tiers with pre-delay and low cuts, air
absorption with distance, free-running LFOs, comb filters, Paulstretch, the
LUFS meter). Five things were not, and all five are now parameters that are
neutral at their default, so six thousand finished presets sound as they did:

* **Aftertouch, the wheel and the slide as modulation sources**
  (`pressure`, `wheel`, `slide`). Pressure and slide already reached the
  sound through fixed routes; the wheel reached it only through MIDI learn,
  one knob per controller. Now all three are ordinary sources, read from the
  loudest voice (pressure, slide) or the instrument (wheel, smoothed over
  30 ms so a 7-bit controller never steps a cutoff). They rest at zero, which
  through the bipolar mapping is -1, so a route wanting "nothing until I move
  it" carries the 0..1 flag -- and that is what makes them safe to put into
  every preset in the library, which the retrofit did.
* **The background's own width** (`far_width`). A mix in which everything is
  spread as wide as it goes is a flat wall; the far plane pulled in towards
  the centre while the foreground stays wide is what the ear reads as
  distance. A mid/side stage on the far bus alone, before it joins the near
  bus. Measured: width 0 leaves no side at all, 1 leaves the reverb's own,
  1.5 is wider, and the mid never moves.
* **Microshift** (`ens_mode`). The two channels detuned a few cents against
  each other, at different base delays, with nothing modulated. The textbook
  construction -- two taps half a cycle apart under a Hann pair -- has both
  taps audible all the time at a fixed delay difference, which on a sustained
  tone is a comb filter: measured, it lost a fifth of the signal. The version
  that ships uses a long ramp (200 ms of travel) and a short hand-over
  (25 ms), so the two taps overlap for a thousandth of the cycle and the
  shifter is a plain delay line at a slowly changing delay. Measured by
  counting zero crossings of a 440 Hz sine: left 443.06 Hz, right 436.96 Hz,
  twelve cents each way to within half a hertz; no step at the wrap; no dip.
* **The band-limited Haas effect** (`haas`, `haas_time`). Delaying a whole
  channel widens it and destroys it in mono. The 1.2-4 kHz band of the centre
  is delayed and put into the side channel -- added on the left, taken off on
  the right -- so the edges open, the low end stays, and a mono sum is exactly
  the picture it was, to the sample. The first version cross-fed the delayed
  band symmetrically and produced, from a mono input, no width at all: the
  test that found it fed mono noise and measured zero side. What "hard to the
  opposite side" comes to once it is made symmetrical is the side channel.
* **The wavefolder** (`filter_fold`), after both filters. A clipper flattens
  what will not fit; a folder reflects it, and the mirrored wave grows a
  family of high partials no saturation makes. `sin` is the smooth version of
  that curve, divided by its own gain so small signals pass unchanged; the
  positive half is driven a third harder than the negative, which is where the
  even harmonics and the body come from; the amount both drives and mixes, so
  the knob leaves the identity continuously. The makeup gain is measured: 2.2
  holds a 0.3-amplitude sine within 1.3 dB across the knob, and a loud input
  loses about 9 dB at the top, which is not a fault -- past the first fold the
  fundamental itself is being folded away.

The sound oracle -- forty presets rendered and hashed before and after --
came back forty identical. Then the library was retrofitted on purpose
(`Tools/library/retrofit_presets.py`, `Tools/retrofit_builtins.py`): every
preset judged from its own settings, the hands almost everywhere, the funnel
where there is a background deep enough to matter, the Haas band where there
is a foreground and it is not already at the edges, the microshift only where
the chorus was slow and quiet enough to have been a widener, the fold only
where saturation was already asked for, and a quiet spectrally-stretched
field recording under presets in the packs that are about places. The
built-in presets get the same rules, each sound-changing addition rendered
first and kept only if the preset still sounds like itself. Measured on a
sample before and after: level median 0.00 dB, worst +3.4 dB (which the
measurement pass then corrected), width shifts modest, mono loss unchanged,
no clipping, no silence, no click.

One more piece of the literature came in on the content side: **convolution
with a struck object**. The Room is a convolution reverb, and an impulse
response does not have to be a room. `make_impulses.py` gained a family cut
from the field recordings -- the sharpest event in a recording of a
foundry, a cistern, a hangar, shaped into a decaying impulse of a tenth to
half a second -- and a pad convolved with one is played on that object.
Measured on the room stem: five to fourteen decibels RMS different from a
hall across the third-octave bands, peaks of up to 27 dB where the object
rings. Forty of them, three megabytes, in the library and in the
thirty-second pack, *Cryo Chamber*, which was written for all of the above.

### After the classics: four things the newer literature asked for

The design chapters of the manual lean on the classic psychoacoustics, and a
check of where the field has moved produced four changes, all neutral at their
defaults and all measured:

* **Stretch** (Tuning): the octave a few cents wider than 2:1 -- Ward's octave
  enlargement, the Railsback curve -- as a slope about the reference pitch, so
  A4 stays and every octave away from it is `s` cents wider. Applied after
  Purity, retunes held notes through the same glide. Measured 1212.00 cents at
  twelve, twice that over two octaves.
* **Far reverb Mode = Scattering** (Schlecht and Habets 2020): a Schroeder
  all-pass with a mutually prime length inside every line's loop. Echo density
  after 50 ms 0.60 -> 0.76 of Gaussian (Abel and Huang's measure), decay
  unchanged, late-tail flatness 0.487 -> 0.501 -- so the honest claim is
  density, not colour, and the help text says so.
* **Unmask Spread**: the upward spread of masking. A band's effective
  side-chain envelope gains half of the band below, a quarter of the one two
  below and a tenth of the one above. Measured: a bass note in front ducks the
  background's middle to less than half of what it did without the spread.
* **Binaural = Headphones** (Space) with head tracking: pan becomes azimuth,
  Woodworth's ITD, full head shadow, a lower pinna notch behind the head, and
  the head's yaw -- from the Quest's OpenXR pose or an OSC `/ambient/head`
  tracker, through a new `OscSink::setHeadYaw` -- turns the field the other way.
  Measured: a centred voice with the head turned ninety degrees has the
  interaural lag of a hard-panned voice with the head straight (34 samples:
  Woodworth's 31.5 plus the shadow filter's group delay); off, bit-identical.

Two measurement lessons from the round. The reverb's coloration barely moved
because a modulated eight-line network is already nearly free of fixed modes;
the first test asserted a ten per cent gain in flatness and was wrong to. And
the unmask's one-pole crossovers leak: a 6 dB/octave split lets a ducked low
band show up in a middle-band measurement, so the surviving claims are
relative ones. The manual's design chapters were corrected in three places
the newer literature contradicts the classics -- loudness adaptation is small
(Scharf), the darkening with distance is a recording convention rather than
air absorption (ISO 9613 gives half a decibel over twenty metres), and bass
mono is a production rule rather than a perceptual limit -- and gained the
harmonicity account of consonance (McDermott et al. 2010, 2016; Harrison and
Pearce 2020) beside the roughness one.

### The three section layers

A layer is a preset bank that touches one section and nothing else, so it
lands on top of whatever sound is loaded. There are three; each lives in the
section it belongs to rather than on the header, which keeps only the Sound
box — the one preset that is about the whole instrument. All three are
generated and then *measured*:

* **Cosmos** — 257 presets in sixteen families (shift, beating, resonators,
  deep, vowels, nebula, shimmer, metallic, glass, drift, wide, ghost, choir,
  machine, bloom, edge). Each family is a designed grid: two parameters over
  four values each, chosen so every step is audible.
* **Z-plane** — one preset per filter shape, 156 with the off entry, grouped
  by the same twelve families as the shapes. A bank of 155 filters with no
  way in is a bank nobody uses.
* **Strike** — 41 presets for the Karplus-Strong pluck in four families
  (strings, wood, metal, and *conducted*, where the pluck fires from the
  conductor as well as from the keys, which turns it from something you play
  into something the piece does on its own).

`Tools/check_layer_presets.py` renders and measures every one of them, and
the three banks each needed their own way of being measured before the
measurement meant anything:

* The **Strike** bank first measured 30 presets as identical to the carrier,
  because the pluck fires on note-on and the offline render plays no keys.
  With notes added, 34 measured as identical to *each other*, because
  `--measure` analyses the second half of the render and a pluck of a few
  hundred milliseconds is long gone by then. Measured with the drone
  silenced and the conductor firing plucks throughout, all 41 are distinct.
* The **Z-plane** bank measured four extreme shapes as identical, because a
  filter can only be heard where the source has energy and a dark drone has
  none at 9 kHz — and because the sub, the body and the effects do not pass
  through the voice filter at all, so even at full wet they keep the drone
  in the measurement. On bare white noise, all 156 are distinct.
* The **Cosmos** bank came out with one dead preset out of 257, and that one
  was a real bug rather than a bad preset. *Smear* sets the Nebula's
  magnitude smoothing as `alpha = (1 - smear)^2`, which at Smear = 1 is
  exactly zero: the smoother then never updates, every bin stays at the zero
  it started from, and the Nebula falls silent. The top of that knob was not
  "the smoothest setting", it was "off", with nothing anywhere to say so.
  The default is 0.7 and no preset had ever sat on the end stop, so it had
  been there unnoticed since the section was written; a generated grid put
  one preset exactly on the corner and the render found it in a minute.
  `alpha` now has a floor of 2e-4, which at this hop rate is a time constant
  of minutes — which is what the top of a smear control should be.

The pattern is the same one the autoplay work ran into. A measurement that
does not put the thing being measured in front of the microphone will
happily report that everything is fine, or that everything is identical, and
both answers are worthless.

## The research round

Late in the instrument's life the design was checked against the current
literature rather than against the classic accounts it was built on. Nine
things were found. Eight were built; each is neutral at its default, so every
preset written before them renders to the bit as it did. The ninth, a neural
sound model in the audio path, was read and deliberately not built: it would
put a hundred megabytes of weights and a hard real-time constraint into a core
that is meant to run on a headset, and it would make the instrument's sound
something nobody could read. Every other decision here can be checked by
reading a formula.

* **Velvet noise** as a third Ensemble mode: sparse signed impulses instead of
  a chorus. The same width, measured, with 1.32 dB of colouration against the
  chorus's 5.41.
* **A colourless far reverb**: the same network with its eight line lengths
  searched by `Tools/optimise_fdn.py` rather than chosen. 0.314 dB of spectral
  spread in the tail against the classic set's 0.474.
* **The spherical head shadow** of Brown and Duda, in the binaural mode.
* **Critical-band spacing** in the conductor: it can be told to avoid or to
  seek notes that fall inside one critical band of a sounding one.
* **The Early Room**, a scattering delay network with one node per wall — the
  only stage in the instrument whose purpose is that the sound moves when the
  source does.
* **Bow**, a waveguide bowed string, the first physical model here.
* **Spectral**, a clip measured into 32 bands and rebuilt from them, which
  finally separates pitch from speed.
* **Loudness in sones** beside LUFS, and
  `Tools/library/texture_statistics.py`, which makes new texture from the
  statistics of old.

The long version, with the mathematics and the references, is the Design
chapters of the manual — and, more usefully, the traps: a masking slope that
made the loudness model four times too quiet while every relative test still
passed, a friction curve that was silent rather than wrong, a scattering loop
two samples long that destroyed the very thing it existed to carry, and a
pitch estimator that reported a clean A3 as 440 Hz.

## The harmony round

The same treatment was then given to the note generators, and the gaps there
turned out to be older and closer to home than the acoustic ones.

* **Harmonic.** The conductor scored a chord as the mean consonance over all
  its pairs. For two tones that is the whole question; for five it inverts the
  ranking. Measured on the instrument's own function, the pairwise rule prefers
  a stack of fifths (4:6:9, 0.233) to a just major triad (4:5:6, 0.212) and puts
  a plain segment of the harmonic series last of all (8:9:10:11:12, 0.165) —
  neighbouring members of one series make complicated ratios two at a time,
  however perfectly the set fits together. *Harmonic* asks instead how strongly
  the whole set implies one virtual root.
* **Key.** Krumhansl and Kessler's probe-tone profiles, correlated against a
  pitch-class distribution weighted by how long each class has been sounding.
  Nothing sets the key; it is found, and the correlation is both the confidence
  and the weight. Over three minutes the conductor's key confidence goes from
  0.73 to 0.90 with all nine pitch classes still in play.
* **Even** and **Smooth**, after Tymoczko. Evenness is the property that lets a
  chord move rather than leap; *Smooth* picks which voice moves so the chord
  travels least. His other measure needed nothing: for an exchange of one voice
  the voice-leading distance is exactly that voice's leap, which is the number
  the conductor had been using all along.
* **Timbre (Sethares)**, the thirteenth tuning: the instrument's own dissonance
  curve, swept across the octave while it plays, with a degree wherever it dips.

Four measurement traps, in the same spirit as the acoustic round's:

1. **A circular measure.** "How well do the notes fit the key?" — where the key
   is found from those very notes. It reported that *Key* made things worse.
   What the parameter claims is that the music sits more clearly in *one* key,
   which is the correlation itself.
2. **A window shorter than the thing measured.** Twenty-one seconds against a
   three-minute memory said the key confidence *fell*. Over three minutes it
   rises.
3. **A wrong reason nearly written into the code.** The fix for (2) was first
   attributed to a change that, remeasured, does nothing for the headline
   number. It was kept — root and key should know about each other — but the
   comment now says so plainly.
4. **A flag assigned over.** The timbre scale was rebuilt correctly and the flag
   that retunes the sounding notes was set, and three lines further down that
   flag was overwritten. The table test passed throughout; only a test that asks
   what a key actually *sounds at* catches it.

## The third pass

Four smaller ones, each measured: *Match* (Sethares' other direction — partials
bent onto the chosen scale; 12-TET's eleven intervals go from roughness 0.1263
to 0.1191), *Arc Harmony* (Lerdahl & Krumhansl's tension model as the arc's
lean on Harmonic, Key and Consonance), *Blend* (Rasch and Bregman — the first
five onsets span 6.29 s one by one, 0.020 s fused) and the fluctuation *Guard*
(Fastl & Zwicker — beat time in the 2–8 Hz band 22 % → 18 %, an honest four
points). One trap worth its own line: the matched partial in the voice is a
*branch*, not a blend, because `(f*h)*s` and `f*(h*s)` differ in the last bit
and the oracle would have heard it on every preset.

## The last acoustic pass

Three more, each measured. *Height* — the pinna's elevation notch (Hebrank &
Wright 1974) and Blauert's 8 kHz "above" band, one height per plane; the
10 kHz-to-7 kHz energy ratio goes 0.509 below, 0.227 flat, 0.028 overhead.
*Envelop* — Bradley & Soulodre's low-frequency lateral energy, lifted on the far
bus between Bass Mono's corner and 500 Hz; +3.0 dB side, 0.00 dB mid on the
output. *Depth Law* — Zahorik's compressive exponent inverted, d^1.85 at full;
half depth is heard at 0.277 instead of 0.500, the horizon unmoved. One trap:
both engine-level tests placed their note before the first `process()`, when
Keys Depth had not yet been read, and measured a note standing on the wrong
plane — nought decibels of lift and nought distance, from code that worked.

## The sixth round

A survey of the state of the art, written by a research model against the
v1.2.0 manual, proposed eleven things; nine were built (`Core/`, each with a
selftest), two declined (port-Hamiltonian rewrites of the physical models --
a passivity guarantee, not a sound; and a biosignal loop, which needs hardware).

*Cascade* -- the conductor's clock as a Hawkes process (time-rescaling: the
gap is drawn as before and consumed faster while the excitation is up;
branching 0.65 at full). Gaps' CV 0.91 -> 1.41, 2.8x the events. *Surprise* /
*Homeostat* -- a fading histogram of chosen interval classes, its Shannon
entropy held to a target by flattening or sharpening the draw; 3.52 -> 2.92
bits downwards. The untouched conductor already sits at 98 % of the maximum
twelve classes allow, so upwards there is almost nothing. *Adaptive* -- a new
note tuned pure against the sounding set (nearest of twelve ratios per voice,
weighted by simplicity, capped at 30 cents) and a common comma offset that
returns the ensemble's centre at 3 cents/min while every interval stays pure:
third 0.00 cents off 5:4, fifth 0.00 off 3:2, centre -3.9 -> -0.9 cents after a
minute. Found on the way: `brain_blend` (round 3) was never read into `bp_`;
its test drove `ClusterBrain` directly and passed. Wired, with a test through
the engine (2 -> 5 voices).

*Comodulate* -- one random envelope (a Drifter at 9 Hz, own RNG) on the far
bus: modulation index 0.15 -> 0.67, low/high envelope correlation 0.41 -> 0.98
(Hall, Haggard & Fernandes 1984). *Rotating* -- fourth far-reverb mode: fixed
lengths, eight Givens rotations (per-sample rotation recurrence, renormalised
every 4096) before the Householder; frozen it loses 0.4 dB more than the fixed
network over 4 s, i.e. nothing beyond the interpolated delays' own loss, and its
modes drift second to second as much as the wobbling lines' do (0.51 vs 0.55)
-- a match, not a win, and the manual says so. *Near Field* -- low shelf per
ear below 1 kHz (-18 dB far, +4 dB near) scaled by lateral x nearness^2; ILD
below 400 Hz -5 -> +10 dB with the 3-6 kHz shadow unchanged (Brungart &
Rabinowitz 1999).

*Transport* (slot field 32) -- the 1-D Wasserstein barycentre between wavetable
frames: masses walked by cumulative distribution, each slice put down at
(1-f) from + f to; halfway energy 0.50 -> 1.00, spread 5 partials -> 0.
*Pulse* -- raised-cosine AM of the Foundation at the Binaural rate (`sin01`
reads a table: the phase must be wrapped before the quarter-turn offset).
*Bias* -- the feedback shaper's input pushed off centre by the 5 Hz envelope of
its content below 60 Hz. *Lenia* -- 32x32 torus, ring kernel R=5, growth
2 exp(-(u-mu)^2/2 sigma^2)-1, dt 0.1, rows spread over blocks, stepped only
while a matrix route reads `lenia1..4`; 599 steps in 30 s at 20 Hz, one reseed,
readings never move more than 1 % per block.

Test traps of the round: an engine-level test that calls `noteOn` before the
first `process()` (Keys Depth unread, again); third-octave bands cannot see
modes move (0.92 vs 0.94 -- bin resolution can); Bass Mono folds the very lows a
near-field test is about; the engine sleeps without a voice, so a Foundation
test needs a silent note to stay awake; the feedback loop throttles itself off
above a mean level of 0.1, so a loop test must play quietly.

## The seventh round

A second survey, of instruments rather than papers (Osmose/EaganMatrix, SOMA
Terra, Waldorf Iridium, Madrona Sumu, Borderlands, Surge XT, Mutable Marbles,
RAVE, FluCoMa, Ambisonics). Six things built, each with a selftest; the rest
either already existed under another name (bandwidth-enhanced partials = the
Spectral source; MPE; Scala/KBM; gesture recording = Sets; a 2D surface = the
map; per-voice microfluctuation; `inertia`) or was declined (Lua in the audio
thread, a 16-channel HOA bus on a headphone instrument, corpus navigation as a
project of its own, and the neural/port-Hamiltonian proposals again).

*Deja Vu / Loop* -- Marbles' ring in the conductor (`viaDejaVu`): each choice
moves one place; with probability Deja Vu the note already there is replayed
and kept, else the fresh one overwrites it; a ring note still sounding is passed
over. Loop of four at full: a note equals the one four back 100 % of the time
(29 % without). *Spread / Bias* -- `BrainParams::shaped()` on every velocity
and hold draw: |2v|^k with k from 4 (gather) to 1/4 (push to the extremes),
then a power for bias; identity at the defaults, bit for bit. At full Spread
62 % of velocities sit more than 0.15 from the centre; Bias 0.8 moves the mean
0.70 -> 0.80. *lorenz_x/y/z, rossler_x/y/z* -- RK4 in natural time scaled by
`chaos_period`, substeps of 0.01, integrated only while a route reads one
(`chaosUsed_`), readings clamped to the attractors' extents. Over two minutes at
period 10 s: Lorenz best self-match at any 5-55 s lag 0.40, Roessler 0.95 --
the spiral is nearly a cycle, only its climb varies; documented as such.
*MPE Filter: One Euro* -- cutoff 0.6 + 30 |x - y| Hz per unit of range; the
speed is read from the distance left to travel because controllers send steps
and a derivative between messages is zero. *Transpose* -- a pure 4:3, 3:2 or
2:1 either way in `frequencyOf`, glided in log2 at an octave per two seconds,
`retune_` set while it moves; lands within 0.01 cent, the voice follows at its
own glide (8 s to within 3 cents). *Partial Spread* -- `phasorBankStepStereo`:
per-partial equal-power weights on a golden-angle pattern turning at 0.02 Hz;
a single strand's L/R correlation drops from ~0.96 to below 0.7 at unchanged
energy (+0.06 dB). Found on the way: Time Width at its default gives even a
centred single strand an interaural delay (render tool: width 0.88 vs 0.04).

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

`Library/` holds a generated library of 6800 presets in 34 packs, with 1700
clips (1200 textures and 500 seamless field recordings), 608 wavetables and
200 impulse responses (see
`Library/README.md` and `Tools/library/`). Its descriptors and map positions
are measured, not estimated: `Tools/library/measure_packs.py` renders all six
thousand and writes the result back into the pack files, and corrects each
preset's master gain to the loudness it actually came out at.

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

**Quality, as opposed to soundness.** `Tools/rate_presets.py` scores every
preset on four axes that can be measured: ALIVE (how far the descriptors
travel between an early window of a render and the whole of it), MOVING
(spectral flux), REACH (how many of the instrument's twelve families the
settings touch) and APART (distance to the nearest other preset in
descriptor space, on a bucketed grid so six thousand presets are a few
hundred thousand distances rather than eighteen million). It exists because
"make the presets better" needs something to aim at, and the first thing it
found was that the built-in bank reached a median of three families out of
twelve: most of those presets predate the modulation matrix, the second and
third source slots, the z-plane's 155 shapes and the BEAT source.

`Tools/enrich_presets.py` (built-ins) and `Tools/enrich_packs.py` (the
library) fill those gaps, and both work under the same two rules. Every rule
fires into an *empty slot only*, so a preset that already has a matrix, or a
z-plane, or three sources, keeps exactly what it had. And every preset is
rendered before and after: if the change moved its level by more than 1.5 dB,
its centroid by a quarter, or its bass or width by 0.12, the change is thrown
away and the preset is put back as it was. 190 of 191 built-ins kept (Init is
left blank deliberately), 5637 of 5969 library presets kept. REACH on the
built-ins went from 0.25 to 0.42; nothing else moved.

Two things this measured that are worth keeping. The first is that the
enrichment must vary per preset: the first version gave all 191 the same
four LFO rates and the same four routes, which is the opposite of what APART
asks for -- the ladder is now anchored on each preset's own base period
between 34 s and 78 s, and the routes are drawn from a character-appropriate
pool without repeats, giving 190 distinct matrices. The second is that
deeper modulation does not make a drone more alive. Measured over three
minutes, the timescale these LFOs actually run at, forcing every added LFO
to full depth moves the median ALIVE from 0.425 to 0.429 and costs up to
3.7 dB of level. A drone's ALIVE is made of its own slow architecture -- the
arc, the bloom, the conductor -- not of a modulator going round.

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

* **Session recall** (standalone only). JUCE writes the whole state into the
  standalone's settings file when the window is closed and reads it back on the
  next start. Two things were wrong with that as a feature. It only happens on a
  clean exit, so a crash, a kill or a power cut loses the evening; and the state
  carried every knob but never the *name* of the preset they came from, so a
  restored session played the right sound under the label "Init" and looked
  like a feature that did not work. Now a timer writes the state whenever it
  has actually changed -- the block is hashed, which is cheaper and safer than
  deciding what counts as a change -- and the two preset names travel with it.
  The names, not the indices: a pack added between two sessions renumbers every
  preset behind it, and an index would then name a different sound. **Recall**
  in the header switches it off and drops what was stored; the switch lives in
  the same settings file, and is read before JUCE gets the chance to restore
  anything. The plug-in deliberately has none of this: there the host saves the
  state with the project, and a fresh instance quietly loading somebody else's
  last session would be a bug.

* **Deployment** (`Deploy/`). The build that other people get differs from the
  everyday one in three ways, each for a reason worth writing down. The MSVC
  runtime is linked in, so there is no redistributable to chase -- and the
  release script proves it with `dumpbin` rather than trusting the flag: it
  refuses to package a binary whose imports still name `VCRUNTIME`. It is built
  for AVX2 (52x realtime against 39x), which every x86-64 processor since 2013
  has; the setup asks the processor first, because a machine without it does
  not fail gracefully, it takes an illegal instruction and dies with nothing
  said. And it builds in its own tree, so the everyday one is left alone.
  The tests are run in that configuration, not in the developer's: a static
  runtime and a missing AVX2 are exactly the kind of change that is fine until
  it is not. The setup itself (Inno Setup) installs the standalone, the VST3
  and the preset packs, each with its own checkbox, for the machine or -- for
  anyone without administrator rights -- for one user, the same script either
  way because every path in it is an `{auto...}` one. The packs go into a
  folder of the installer's own rather than into Documents, so that removing
  them again can never take a pack the user put there themselves with it.

* **The sample library** (`Tools/make_content_pack.py`). The 6800 presets in
  the packs name 1584 samples, wavetables and impulse responses that are far
  too big for git -- so they are a downloaded package, and the setup fetches
  and unpacks it. Two things happen on the way in. Only what is referenced
  travels: the library folder holds more than the packs use, and shipping the
  rest would add gigabytes nobody's preset asks for. And the samples, which
  are generated as 32-bit float, become 24-bit PCM. That is a quarter off the
  size for headroom they do not have: measured, every one of them peaks at
  exactly 0.5, and the error the conversion adds sits at -149 dBFS RMS. The
  wavetables (16-bit) and impulse responses (24-bit) were already PCM and are
  copied untouched -- which is worth saying, because the first version of the
  script reported them as "would have clipped", a number that was simply
  false. The proof that it is inaudible is not the arithmetic but the render:
  four pack presets measured against both libraries agree in every descriptor
  to the last digit printed, and differ only in the sample hash, as they must.
  4.86 GB of source becomes 3.06 GB in three archives -- deflate at level 6,
  measured at 85 % where level 9 is also 85 % -- because a release asset may
  not exceed 2 GB. Names, sizes and SHA-256 are generated into an include the
  installer reads, since a hash that does not match what is on the release is
  a download that fails at the last possible moment.
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
  output's own spectrum with its peak level, and a layout button that cycles three shapes:
  Normal (one page, tabs), Compact (the widest rows wrap into two: 1.9 : 1 to 1.6 : 1, still
  without scrolling -- wrapping every wide row rather than only those above ten cells gave
  1.27 : 1, worse than the shape it started from, which is why the threshold is where it is)
  and Expanded (every page of every tab row laid out under one another with a title in the
  tab's place, no tabs, 2080 x 2604 at design size: the four sources, the two filters and
  the conductor's six tables all in sight, for a tall screen or for reading a preset through).
  The mode is kept in the state; `AMBIENT_EXPANDED=1` / `AMBIENT_COMPACT=1` set it for a run,
  and the manual's tab pictures are always taken in Normal.

  The GUI round (after a third report, on the interface): a knob shows everything that moves
  it -- the matrix as before, in its source's colour; the morph or the map's blend as a
  neutral arc from the knob's value to the live one (`effectiveParam` + `modAmount` against
  the raw value); and the arc and the tide, whose swing is too slow to see as motion, as a
  dot on the outer ring (`halo`) saying where in it they are. The Tuning page's display draws
  the timbre's roughness curve across the octave from the same `BrainSpectrum` the conductor
  judges with (computed every block now, a dozen pows; nothing reads it unless Timbre is up,
  so the sound is untouched: oracle 41/41), the scale's degrees as ticks -- they sit in the
  dips when the scale is the timbre's own -- the key the conductor found, the comma, the
  tide. The Coherence page's display: the Kuramoto ring, the Lenia field as a grey grid, the
  six attractor readings; asleep, they say so. The stage's three planes (conductor, keys,
  second conductor) are lines a hand can drag, through the host's parameter system, so the
  drag is automated and undone like a knob; the voices stay a picture, because the conductor
  put them where they are. A glyph where the loop closes (Feedback says it returns to the
  sources, Source 1 that it is fed), a sentence under a one-strand additive bank saying why it
  is one strand, and fixed-width figures in the knobs and the header line (shrunk to fit --
  the first draft cut "40 min" to "40 mi"). Declined from that report, with reasons in the
  manual: the three-zone rebuild, the isometric stage with grabbable voices, a 3D filter cube,
  a second preset explorer. `AMBIENT_SHOT=<png>` writes a picture of the whole editor after ten
  seconds of a chord, for looking at the panel without a screen grab (a screen grab takes
  whatever else is on the screen, and did). From the reply to that reply, four more small
  things: the header names the conductor's key and confidence and glows with the cascade's
  excitation; the notes roll shows the deja-vu ring (place lit), the cascade as a glow at its
  newest edge and the homeostat's lean as a needle; the Coherence display draws the attractors'
  x/y orbits from the last forty seconds; and with Arc Clock on the Arc knob's halo becomes a
  24-hour dial with marks at 4, 10, 16 and 22 (`clockHour` property). Declined from it: tying
  the Expanded layout to a pixel height (the editor scales as a whole, and a reflow at a
  threshold is exactly the surprise calm technology forbids -- it stays a mode), component
  encapsulation for its own sake, and the three-zone phase, whose dock, drawer and soundstage
  are the strip, its tabs and the Perform page under other names.

  The layout critique, after 1.5.0, was right about Expanded: at 2080 x 2604 it was 4 : 5, and a
  page that scales as a whole lands at 41 % on a 1080-line screen. The height was the greyed
  knobs -- every source strip the union of every type's cells, five rows for two or three
  live ones, and Off slots, Off filters, inactive morphs all fully open. Three changes, all
  under one principle, that the layout is a function of the parameter state and never of the
  window: (1) a cell the slot's type does not use is `unused` -- not laid out, not drawn -- so a
  strip is as tall as its type (`updateSourceCells` sets the flags and calls `rebuildLayout`
  when they change; the Texture and Wavetable buttons follow their types); (2) a section whose
  switch is off is `collapsed` to its title and the switch's own cells (`closers()`: the slots'
  Type, Z-Plane's Mode, Cosmos Send, Strike level, Morph Active, Brain 2 Active, Early Room,
  Body, Room, Cloud, Feedback's two paths, Patina), the manual export sets `openAll_` so its
  pictures are of open sections, and a closed page hides its display; (3) Expanded is three
  columns -- the voice; the room, the effects, the Cosmos and the spectrum; morph and the
  conductor -- with the last group of each column stretched so the three end level. Measured:
  Normal 2269 x 1260 (86 % at 1080 lines), Expanded 2269 x 1352 (80 %, from 41 %). A type
  change or a switch thrown relays out the page and resizes the window to the new ratio, on
  the player's own action -- never on the window's. `AMBIENT_LAYOUT=<0|1|2>` sets the mode for
  a run over the recalled one (session recall keeps it, which is why the `AMBIENT_EXPANDED` run
  left the next start expanded).
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
