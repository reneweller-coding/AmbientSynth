# AmbientSynth

A drone-ambient software synthesizer for slowly evolving polyphonic sound
clusters, in the spirit of Robert Rich's sleep concerts: just-intonation
tunings, additive partial banks that breathe, envelopes measured in minutes,
a generative "Cluster Brain" that conducts the piece on its own, and a spatial
model that treats depth as a physical landscape: dry, bright foreground voices
against a dark, wide, infinite background.

Runs as a **VST3 plugin** and as a **standalone application** (Windows now;
the DSP core is framework-free so it can move to Android/Quest later).

Licence: AGPL-3.0 (see `LICENSE`).

![AmbientSynth standalone](docs/screenshot.png)

## Layout

| Directory | What | Depends on |
|---|---|---|
| `Core/` | The whole synthesizer: parameters, tuning, voices, spatial routing, effects, cluster brain, presets, engine. Pure C++20, no allocations while rendering. | nothing |
| `Plugin/` | JUCE wrapper: VST3 + Standalone, generic parameter GUI, preset list, Scala loader, state save/restore. | JUCE 9 (fetched by CMake) |
| `Tools/render/` | `ambient_render`: offline renderer to WAV with per-second measurements. | Core |
| `Tools/analyze.py` | Measures a WAV (level, clicks, stereo correlation, spectral centroid, peaks). | numpy |
| `Tests/` | `ambient_selftest`: tuning, envelope, engine, brain, delay, mid/side, presets, space, determinism. | Core |
| `Quest/` | Native Meta Quest app: OpenXR + hand tracking → gesture layer → engine, Oboe audio, GLES scene, OSC bridge; Gradle-free APK build. Compiles and packages, not yet run on a headset. | NDK, OpenXR loader, Oboe (fetched by script) |
| `docs/concept.md` | Sound-design and architecture notes, roadmap to the Quest. | |

## Build (Windows, Visual Studio 2026)

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

Outputs:

* `build/Plugin/AmbientSynth_artefacts/Release/VST3/AmbientSynth.vst3` (copy the folder to `C:\Program Files\Common Files\VST3`)
* `build/Plugin/AmbientSynth_artefacts/Release/Standalone/AmbientSynth.exe`
* `build/Tools/render/Release/ambient_render.exe`
* `build/Tests/Release/ambient_selftest.exe`

The first configure downloads JUCE (tag set by `AMBIENT_JUCE_TAG`).
`-DAMBIENT_BUILD_PLUGIN=OFF` builds only the core and the tools, which needs no JUCE.

## Using it

Start the standalone: the Cluster Brain is on by default and begins a piece
within a few seconds. Presets come in two independent layers that combine
freely: the **Sound** box (136 presets: voices, space, delays, reverbs, brain,
tuning) and the **Cosmos** box (32 presets for the Cosmos section only).
Loading one layer never touches the other. In a DAW the 136 full presets are
the plugin's programs (both layers at once). *Save…* / *Load…* store the whole
state as an `.ambientsynth` file. Play MIDI notes to add your own voices;
they sit in the foreground (see *Keys Depth*) and the lowest held key becomes
the brain's root.

**Morph.** The Morph section holds two full snapshots, A and B: pick a preset
for each or capture the current state with *A ← now* / *B ← now*. Switch
*Active* on and the *Position* knob (or a mapped MIDI controller, later a hand
in VR) blends the whole instrument between the two worlds; *Glide* sets how
long the instrument takes to follow a new position, up to 15 minutes, so one
gesture can carry a piece across a quarter of an hour. Continuous parameters
interpolate in the knob's own perceptual curve, integers round, choices and
switches flip halfway. Morph settings are never part of a preset.

**MIDI learn.** Right-click any knob, switch or box: *MIDI Learn*, then move a
controller. The label shows the bound CC; right-click again to clear. The
mapping is saved with the plugin state and in `.ambientsynth` files.

**Browser and preset map.** *Browse* in the header opens the preset
browser in two views. *Columns* is the classic one: Family, Character
(dark, bright, tonal, noisy, wide, bass), Motion (calm, moving, dense,
sparse) and Features (keys, generative, cosmos, feedback, sources, just
intonation, sub, stack, air) as columns that narrow the list, with search,
sort by any descriptor and favourites. Every preset was measured by
rendering it, not tagged by hand. *Map* keeps the list and shows all
presets as points clustered by what they sound like; click a point to load
it, or switch on *Map blend* and drag the cursor: the synth glides to the
blend of the presets around the cursor, so the space between two presets
becomes playable. The cursor is a parameter, so hands, MIDI, OSC or
automation can wander the map. A **route** is a list of waypoints with
travel and hold times that the synth walks by itself: pick one of the
twelve route presets (20–40 minute sets) or build your own from the
cursor, press *Play route*, and the set plays; *Speed* and *Loop* as you
like. `ambient_render --route "Night Descent"` renders one offline.

**Hold, macros, recording.** *Hold* (Tuning) latches keys: a note stays
until its key is pressed again, and switching Hold off releases everything.
The eight **macros** (Space, Alien, Motion, Bloom, Density, Distance,
Evolution, Air) each move several parameters at once through the gesture
layer; they are automatable, MIDI-learnable and never part of a preset, and
a macro only acts once it has been moved, so loading presets leaves
everything intact. **Perform** in the header switches to a page with only
these eight as large knobs and the morph slider, for playing a set without
the editor in the way. *Rec* in the header records the output to a 32-bit
float WAV, for capturing a whole set.

**OSC and gestures.** The plugin listens on UDP port 9000 (header shows the
message count). Namespace: `/ambient/param/<key> f`, `/ambient/paramn/<key> f`
(normalised), `/ambient/morph f`, `/ambient/note i i`, `/ambient/preset s|i`,
`/ambient/sound s|i`, `/ambient/cosmos s|i`, and the hand vocabulary
`/ambient/hand/L|R x y z pinch tilt` (metres, OpenXR axes) and
`/ambient/head yaw pitch roll` (degrees). Hand data feeds the gesture layer,
which maps hand distance, heights, reach, palm tilt and head yaw onto
parameters with range, smoothing, dead-zone and a clutch: by default nothing
moves unless the right hand pinches. `Tools/osc_hand_sim.py` streams a slow
two-hand choreography for testing. *Calibrate* (header, or
`/ambient/calibrate f` over OSC) watches the hands for six seconds, together
and apart, low and high, near and far, and sets the ranges from what was
explored; the Quest app runs it on first start and stores the result. The same gesture layer will sit behind
OpenXR hand tracking on the Quest (see `docs/quest-plan.md`); the core already
builds for Android arm64 with the NDK.

**Layout.** The sections are grouped the way the signal flows: VOICE,
FOREGROUND and BACKGROUND on the left, CONDUCTOR (brain, tuning), COSMOS and
MORPH on the right, Master mid/side in the header. The header draws the
routing map: voices into the foreground chain, the parallel Cosmos return, the
cloud and far sends into the background reverb, and both planes into the
mid/side output.

Preset families: sleep and night pieces, cathedral and glass, deep and sub,
breath and voice, exotic tunings, shimmer and delay, thirty Cosmos
science-fiction textures, playable keyboard patches (brain off), long-form
night arcs, storms and clusters.

Sections of the GUI (all parameters are automatable in a DAW):

* **Oscillator** — additive bank per strand: partials, spectral tilt,
  brightness, odd/even balance, inharmonicity, *Shimmer* (each partial's level
  drifts on its own), strands (unison), detune, pitch drift, stereo spread.
  *Bloom* opens each note's spectrum from dark to its brightness over *Bloom
  Time* (up to five minutes), a slow flowering per voice. *Stack* puts the
  strands at pure ratios instead of detuning them (octaves, fifths, a just
  major or minor triad, sevenths, harmonics, subharmonics): one key, one
  beat-free chord. *Rate Wander* lets every movement rate itself wander
  slowly, so the drone never repeats its own pace.
* **Source 2 / Source 3** — two more sources per voice next to the partial
  bank, each with level, octave, a just ratio to the note and pan, sharing
  the voice's filter, envelope and distance. *Wavetable* is a table of
  spectra rendered by the same alias-free partial bank (Classic, Organ,
  Vocal, Glass, Metal, or a *User* table from a 2048-frame WAV), *Position*
  morphs and drifts; *FM* is a two-operator pair; *Texture* granulates a
  loaded sample (field recording, flute air, metal) around a wandering
  position, free-running or pitched to the key. `Tools/TextureGen` makes
  such samples from a text prompt (Stable Audio Open, MusicGen, AudioLDM 2)
  and writes the detected base pitch into the file name; `Tools/WavetableGen`
  builds *User* wavetables from any audio, from a prompt, or from spectral
  recipes, with a preview and export in the common 2048-frame layout.
* **Foundation** — a dry sub voice that follows the brain's root one or two
  octaves down and glides between roots (*Glide*); *Binaural* runs the two
  ears a few Hz apart for a slow beat in the delta/theta range; *Tone* blends
  sine into triangle. *Source = Difference* lets the sub follow the ghost
  tone instead — the combination tone `f2 − f1` that just intonation makes
  in the ear from the two lowest sounding voices. *Pad Low Cut* rolls the
  pads' partials off below the sub's register (12 dB/oct), so the bottom
  stays with one mono bass.
* **Air** — a band-passed noise layer per voice around a drifting multiple of
  the fundamental: breath, flute air, glass hiss.
* **Envelope** — attack up to 60 s, release up to 120 s.
* **Filter** — state-variable low-pass with key tracking, envelope amount and slow drift.
* **Z-Plane** — a Morpheus-style filter: four resonant frames on the corners
  of a square, a point between them is a filter interpolated from all four,
  and the point wanders on its own (*Rate*, *Depth*). Shapes: Vowels, Metal,
  Bells, Comb, Dark Hall, Sweep. In series after the filter or instead of it.
* **Space** — *Depth*: how far back the brain places its notes (most go deep,
  some stay intimately close); *Keys Depth*: the plane of MIDI notes; *Pan
  Drift*: slow wandering of each voice's centre; *Time Width*: interaural time
  difference rendered per voice (true time-based width, mono-compatible);
  *Arc*: an hour-scale drift of density, brightness and depth with its period.
  *Presence*: a 2–5 kHz lift for notes on the near plane only (nothing on the
  far plane), the articulation that makes the room behind it feel deep.
  *Breath* and *Breath Rate*: each voice's distance wanders slowly, so a
  note drifts forward and sinks back on its own — the room breathes.
* **Ensemble** — three-tap modulated chorus.
* **Delay** — stereo delay with independent L/R times (asymmetric by default),
  feedback, cross-feed, damping, mix, and *To Far*: how much of the echoes
  recede into the background reverb.
* **Delay 2** — a second stereo delay in series after the first, so echoes of
  echoes form long, rhythm-free chains; same controls.
* **Cloud** — a granular cloud on the far plane: grains of the recent
  foreground (*Grain* length, *Density* per second) scattered back in time
  (*Spray*), optionally transposed by octaves and fifths (*Pitch*), spread
  across the stereo field and dropped into the far reverb.
* **Near Reverb** — small room for the foreground plane.
* **Far Reverb** — 8-line FDN with decay up to 90 s, size, damping, pre-delay,
  *Asymmetry* (right-hand lines longer, right output later), *Tail Cut*
  (low-pass on the tail: distance darkens), *Freeze*.
* **Room** — a third reverb, optional and additional: a convolution reverb
  on the far plane playing an impulse response (*Impulse…* loads a mono or
  stereo file, a built-in dark hall plays without one). *Level*, *Source*
  (far sends or the finished foreground), *Pre-Delay*, *Tail Cut*.
  `Tools/ImpulseGen` designs rooms from numbers, cuts impulses out of
  recordings or AI renders of a clap in a described room, and blends both.
* **Feedback** — the sound feeds itself: the output mix returns, low-passed
  (*Tone*) and softly saturated (*Drive*), either into the foreground bus
  before the effects (*To Bus*) or as phase modulation of every partial (*To
  Pitch*). Throttled by its own level, so a hot loop hisses and holds instead
  of running away.
* **Cosmos** — a parallel science-fiction path fed from the foreground bus
  (*Send*) and mixed back in (*Return* to the foreground, *To Far* into the
  background reverb), so the original sound and the alien one blend freely:
  *Shift* (single-sideband frequency shifter, ±300 Hz, with slow *Shift Drift*;
  the right ear is shifted 3 % less, so wide shifts beat slowly), *Resonator*
  (tuned comb filters that follow the brain's root × *Res Pitch*, hull-like
  ringing), *Vowel* (three formants morphing between a-e-i-o-u at *Vowel
  Rate*, alien choirs), *Nebula* (spectral smearing with random phases,
  *Smear* = 1 freezes the spectrum), *Shimmer* (pitch-shifted feedback around
  the far reverb at *Shimmer Pitch*, self-regulating so it blooms and holds).
* **Master** — mid/side stage: *Bass Mono* (side channel high-passed, the low
  end stays centred), *Side Air* (broad upper-mid lift on the sides), *Width*.
  No compressor: dynamics are left alone.
* **Cluster Brain** — density, event rate, hold time range, register range,
  *Consonance* (1 = only simple ratios to the root, 0 = clusters), *Wander*.
* **Tuning** — 11 built-in scales plus *Load Scala…*; *Keys*: "Snap to 12
  keys" (default) or "Consecutive degrees"; root, A4, random seed.

## Measuring instead of listening

```bash
build/Tools/render/Release/ambient_render.exe --seconds 180 --out drone.wav --stats
build/Tools/render/Release/ambient_render.exe --seconds 90 --preset "Sleep Concert" --out sleep.wav
build/Tools/render/Release/ambient_render.exe --seconds 30 --set brain_on=off --notes 48,55,60,64,67 --set scale="JI Major (Ptolemy)" --out chord.wav
python Tools/analyze.py drone.wav --window 15
```

`--list` prints every parameter key with its range, `--list-presets` the
preset names. Renders are deterministic for a given seed.
