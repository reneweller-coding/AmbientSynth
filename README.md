<img src="docs/logo-128.png" width="96" align="left" alt="AmbientSynth" />

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

<br clear="left" />

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
| `Deploy/` | The installer: `build_release.ps1` (a self-contained build, staged, checked and zipped) and `AmbientSynth.iss` for Inno Setup. | Inno Setup 6+ |
| `Tools/make_manual.py` | Turns the in-app help into `docs/manual/AmbientSynth-Manual.pdf`: the app writes its own pictures, this makes the book. | Edge or Chrome |
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

## Installing (what other people get)

```powershell
powershell -File Deploy\build_release.ps1
```

Builds in its own tree and leaves two things in `Deploy/out/`: **`AmbientSynth-<version>-Setup.exe`**
(9 MB) and a portable **`.zip`** (10 MB) for anyone who would rather not run an installer.

Nothing has to be installed first. The runtime is linked in
(`-DAMBIENT_STATIC_RUNTIME=ON`), so there is no Visual C++ redistributable to chase — the script
checks that with `dumpbin` and refuses to package a binary that still asks for one. The build is
AVX2 (52x realtime against 39x without it, both measured); every x86-64 processor since 2013 has
it, and the setup asks the processor before installing rather than letting an older one die on an
illegal instruction with no explanation.

The setup installs, each with its own checkbox:

* the **standalone** (always) into Program Files, with a Start-menu entry,
* the **VST3** into `Common Files\VST3`,
* the **preset library** (30 packs, 6000 presets) into `ProgramData\AmbientSynth\Packs`,
* the **sample library** — 1284 samples, wavetables and impulse responses, 3.3 GB, downloaded
  from the release and checked against its hash — into the same folder,

all four on by default.

**While the repository is private the download returns 404**, and the setup says so and installs
everything else. The URL is already the one it will be; nothing has to change when the repository
is made public. Until then the archives can be unpacked by hand into
`ProgramData\AmbientSynth` (or `LocalAppData\AmbientSynth` for a per-user install), beside the
`Packs` folder.

The sample library is built by `Tools/make_content_pack.py`, which takes only what the packs
actually name (the library folder holds more) and converts the 32-bit float samples to 24-bit,
a quarter off the size for headroom they do not use: they all peak at exactly -6 dBFS, and the
error the conversion adds sits at -149 dBFS. Rendered against both, every measured descriptor of
a preset is identical — only the sample hash differs, as it must. Too big for git, so it lives on
the release; the archives' names, sizes and hashes are generated into `Deploy/content-files.iss`
for the installer to verify.

It runs for everybody on the machine by default and asks for administrator rights once; without
them, "just for me" (or `/CURRENTUSER`) installs into your own folders instead. The instrument
reads packs from the installer's folders *and* from your `Documents\AmbientSynth\Packs`, and a
pack that sits in two of them loads once. Uninstalling removes what the installer put there and
nothing else. The sample library the packs name is a separate download; a preset whose sample is
missing falls back to the built-in sources and still plays.

## Using it

The standalone **starts where you left off**: the whole state is written whenever it changes, so
a crash or a kill loses nothing, and the next start brings back the same sound, the same
modulation, the same tuning and the same preset name in the box. **Recall** in the header turns
that off and forgets what was stored. The plug-in does not do this on purpose: there the host
saves the state with the project, and a fresh instance quietly loading somebody else's last
session would be a bug rather than a feature.

Start the standalone: the Cluster Brain is on by default and begins a piece
within a few seconds. Presets come in four independent layers that combine
freely, and loading one never touches the others:

* the **Sound** box in the header — 191 built-in presets (voices, space,
  delays, reverbs, brain, tuning), plus everything the packs add. The only
  one on the header, because it is the only one about the whole instrument;
* **Preset** in the **Cosmos** section — 257 in sixteen families;
* **Preset** in the **Z-Plane** section — 156, one per filter shape, grouped
  by the same twelve families as the shapes;
* **Preset** in the **Strike** section — 41 for the Karplus-Strong pluck.

The three section banks sit in the sections they belong to, beside the
controls they move. Loading a *sound* preset clears the three names: a sound
preset brings its own filter and its own pluck with it, and a box still
naming the filter that has just been overwritten would be a lie.

**The manual.** *Help* (or F1) opens it inside the instrument, with pictures of
each section taken from the panel itself. The same thing exists as a **PDF** of
about fifty pages: the installer puts it next to the executable, and it is
attached to the release, so it can be read before installing anything. It is
generated rather than kept in the repository (it is a megabyte and a half that
changes with every build):

```powershell
$env:AMBIENT_PRESET = "Three Voices, One Key"   # a patch with everything in use
$env:AMBIENT_MANUAL = "docs\manual"             # the app plays a chord, waits, exports, quits
build\Plugin\AmbientSynth_artefacts\Release\Standalone\AmbientSynth.exe
python Tools\make_manual.py                     # -> AmbientSynth-Manual.html and .pdf
```

`Deploy\build_release.ps1` does all of that on its own. The manual is not
written twice: the text is the help text, and the pictures are snapshots the
running instrument takes of its own panel, which is why they cannot drift out
of date the way a drawing would.

In a DAW the full presets are the plugin's programs (every layer at once).
Each of the three section banks is generated and then measured: every one is
rendered and checked for being silent, clipping, doing nothing, or sounding
the same as another (`Tools/check_layer_presets.py`).

**Preset packs.** Beyond the built-in presets the synth loads packs at
runtime: plain text files, one preset per line, that may also name a sample
and a wavetable of their own. They appear everywhere the built-in presets do
-- programs, browser, map, routes -- each pack as its own family. Drop
`*.ambientpack` files into `Documents/AmbientSynth/Packs`, or point
`AMBIENT_PACKS` at a folder. [`Library/`](Library/README.md) is a generated
library of 6000 presets in 30 packs with the 1284 samples, wavetables and
impulse responses they play. *Save…* / *Load…* store the whole
state as an `.ambientsynth` file. Play MIDI notes to add your own voices;
they sit in the foreground (see *Keys Depth*) and the lowest held key becomes
the brain's root.

**Three sources.** Every voice has three equal source slots. Each is one of
Additive (a 32-partial bank shaped by tilt, brightness, odd/even, inharmonic
stretch and shimmer -- in Source 1 the classic strand bank with unison,
detune and stacks), Wavetable (a table of spectra), FM, Texture (granular,
up to 64 grains) or Noise (ten colours); so three additive banks, three
granular players or any mix are a matter of three choices. **Nine filter
models** sit behind the same knobs: LP 6/12/24, HP, BP, notch, peak, a
saturating ladder and a tuned comb, with Drive ahead of them; the filter and
the z-plane each switch on and off and run in series or in parallel. **Clock.**
Every rate that wants the grid -- the eight LFOs, the six envelopes, both
delays, ensemble, cloud, brain, arc, grain density -- has a Sync choice from
64 bars to 1/32 next to its free knob. The tempo comes from the DAW's play
head, from MIDI clock at the input, or from the synth's own clock (Tempo /
Run in the Conductor's CLOCK tab), which is what the standalone runs on.

**The Rich refinements.** A binaural phase field per voice (Phase Width: the
room seems to change size rather than the sound to move), Doppler on the
breathing distance, a Blur that wipes attacks into texture on the near bus, a
Formant filter model, a Strike layer (plucked string, wooden knock, struck
metal at note-on, always in the foreground), an independent pitch drift per
source, Absorb in the delay feedback (echoes that drown), a pitch Tide over
minutes, a turning far field, and golden-ratio LFO defaults. Ten built-in
"rich studies" presets show them; the library uses them per style.

**A place, not a patch.** A resonating **Body** under everything (twelve modes on the root:
wood, plate, bell or string), an **Unmask** that lets the background step aside for the
foreground band by band, a **Patina** on the master (tape wow, lost highs, a noise floor,
gentle saturation), and **Externalise** for headphones (pinna notch and shoulder reflection).
**Expression**: pressure, slide and per-note bend, over MPE or a plain keyboard; pressure pulls
a voice out of the background towards you. A **second conductor** for the background, the first
one's decisions optionally **on the clock's grid**, and a **Room Morph** between two impulse
responses.

**Stems and scores.** `ambient_render --stems <prefix>` writes the near, far, cosmos and room
planes as four stereo files alongside the mix, so a piece can be balanced afterwards. A **score**
is a text file of timed ramps -- `6:00 cosmos_send 0.45 over 8:00` -- that plays a written piece
instead of a recorded one; `--score <file>` renders one offline, and `docs/example.score` is a
forty-minute example.

**The editor.** One page, nothing scrolls, the corner zooms: two columns of
sections, and rows of a kind -- the three sources, the filters, the effect
pairs, the conductor's tables, morph and macros -- page through tabs. Each
row's spare room is a live display drawn from the engine's numbers: the
source's bank as partials and cycle, the grains reading the clip, the chosen
filter model's response, the amp envelope with the live level, the stage with
every voice as a dot (left-right by pan, near-far by plane), the Cosmos
return's spectrum, the conductor's notes as a roll. The strip along the
bottom holds the modulators: drag a card onto a knob to route it, right-click
a card or a knob to see and remove routes, and the LFO / ENVELOPES / MATRIX
tabs edit the sources. A modulated knob wears a ring in its source's colour
and shows the modulation moving it. Every section title carries a die (click to redraw that
section, shift to nudge it); Undo, Redo and A|B work on whole snapshots; the header shows the
output's spectrum; Compact wraps the widest rows for a narrower window.

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

**Sets.** *Record set* on the Perform page logs every knob, gesture,
macro, route step and note with its time; stop to save a `.ambientset`
file, *Play set…* replays it, and `ambient_render --set-file` renders it
again offline, so a good evening can be reproduced and rendered in higher
quality than it was played. `Tools/preset_check.py` is the automatic sound
test: every preset rendered and flagged for level, clipping, clicks, DC or
silence.

**Are the presets any good?** `preset_check.py` asks whether a preset is
broken. `Tools/rate_presets.py` asks the harder question, in four ways that
can be measured instead of argued about: ALIVE (do the descriptors move
between the first part of a render and the whole of it), MOVING (spectral
flux), REACH (how many of the instrument's twelve families the preset
actually touches) and APART (distance to its nearest neighbour in descriptor
space, so a bank of near-duplicates scores badly even when each one is fine).

It found the built-in presets reaching a median of three families out of
twelve, because most of them were written before the modulation matrix, the
three source slots, the z-plane's 155 shapes and the BEAT source existed.
`Tools/enrich_presets.py` gave them those parts -- four LFOs on a golden
ladder anchored on the preset's own base period, a matrix of four slow
routes drawn from a pool that suits its character, a z-plane where there was
none, a quiet second source -- and then proved it had not changed what they
sound like: every preset rendered before and after, and any whose level moved
more than 1.5 dB, centroid more than a quarter, or bass or width more than
0.12 was put back exactly as it was. 190 of 191 were kept (*Init* is left
blank on purpose), and REACH went from 0.25 to 0.42 with every other column
unmoved. `Tools/enrich_packs.py` does the same for the library, where the
gaps were different ones: the BEAT source, the filter's third axis, the
delay's duck, and LFO rates snapped onto a golden ladder so no two of a
preset's modulators come back into step. 5637 of 5969 kept.

`Tools/param_usage.py` asks the same question from the other end: for each of
the 386 parameters, how many of the 6191 presets move it off its default and
how many point a modulation route at it (`--sections` rolls it up per
section, `--unused` lists what nothing touches, `--json` writes
`docs/param-usage.json`). It is how you find out that a control shipped and
nobody ever used it.

What it did *not* do is worth saying too. Deeper modulation does not make a
preset more alive: measured over three minutes -- the timescale these LFOs
actually run at -- turning every added LFO to full depth moves the median
ALIVE from 0.425 to 0.429 and costs up to 3.7 dB of level. What a drone's
ALIVE score is made of is its own slow architecture, the arc, the bloom and
the conductor, not a modulator going round.

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
MORPH on the right, Master mid/side in the header. The header carries the
help line: point at any control and it names it, shows its value and says
what it does (the same text as its tooltip). *Help* (or F1) opens the manual:
thirteen topics with a signal-flow diagram, pictures of the sections as they
stand on the panel, the live displays, and a list of every parameter.

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
  the fundamental: breath, flute air, glass hiss. *Mode Ghost* sends the
  noise through six sharp resonators on the note's just harmonics instead:
  a harmony filtered out of chaos.
* **Coherence** — four slow coupled oscillators (the Kuramoto model of
  fireflies falling into step) move brightness, depth, pan and the Z-plane
  point; *Coherence* 0 leaves them independent, 1 locks them into one pulse.
* **Envelope** — attack up to 60 s, release up to 120 s.
* **Filter** — state-variable low-pass with key tracking, envelope amount and slow drift.
* **Z-Plane** — a Morpheus-style morphing filter: four frames of up to six
  cascaded pole/zero sections sit on the corners of a square, a point
  between them is a filter interpolated from all four, and the point
  wanders on its own (*Rate*, *Depth*). Sixteen shapes — Vowel Morph,
  Choir, Nasal, Low/High/Band Sweep, Phaser, Comb, Flanger, Notch Cluster,
  Strings, Metal Bars, Wood, Glass, Peaks, Infinite — in series after the
  filter or instead of it, with resonance and key tracking.
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
  of running away. *Tape* adds asymmetric saturation, wow and flutter and a
  level-dependent noise floor inside the loop: every pass goes a little
  softer, like an old tape loop.
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
* **Z-Plane** — a morphing filter after Dave Rossum's idea from the E-mu
  Morpheus: filter frames on the corners of a **cube**, and a point inside
  it is a filter whose poles and zeros are interpolated between them.
  **155 shapes in twelve families** — voices and vowel spaces, sweeps, combs
  and phasers, string and instrument bodies, bars, plates and bells,
  membranes, pipes, rooms, harmonic and exotic series, extremes, instrument
  models, EQ curves and speaker cabinets. The original shipped 197 cubes;
  its coefficient tables are proprietary firmware and are not here, so the
  numbers in this bank come from published acoustics instead (bar and bell
  mode ratios from Fletcher & Rossing, the sung-vowel formant tables, `c/2L`
  for room modes) and are generated by `Tools/make_zplane_bank.py`, which
  writes down where each one comes from. *X*, *Y* and *Transform* move the
  point; *Resonance* narrows every band at once; *Key Track* moves the whole
  frame with the note. **Modal** is a fourth mode: the same shapes read as what
  they physically are -- a parallel bank of resonators that *rings* rather than
  a cascade that shapes, with its own *Decay* (up to 40 s) and *Damping*. 200
  presets, one per shape plus one per physical object ringing.
* **Strike** — the Karplus-Strong pluck at note-on, with 41 presets of its
  own in four families. *Fires* decides whether it answers the keys only or
  the conductor as well; the latter turns it from something you play into
  something the piece does on its own.
* **BEAT** — a modulation source that is the instrument listening to its own
  tuning: it turns at the beat between the two lowest voices and the nearest
  just ratio, so it stands still when the chord is in tune and quickens as
  Purity Drift loosens it. Route it at anything and the sound breathes in time
  with its own harmonic friction.
* **Envelopes** — the voice has a plain **ADSR** (Attack, Decay, Sustain,
  Release; up to a minute of attack and two of release, which is what a drone
  wants). On top of that, six **modulation envelopes** with up to sixteen
  breakpoints, a curve on every segment, an optional sustain point and an
  optional loop — edited on the curve itself: drag a point, double-click to add
  or remove one, right-click for the sustain point, the loop, the curvature and
  ten shapes to start from, ADSR first among them. Five presets exist to show
  them, because measured across all 6191 nothing did: *Long Arc* (twelve points
  over nine minutes, once), *Held Breath* (a sustain point: it rises while a key
  is down and runs out when you let go), *Six Hands* (all six envelopes at once,
  their times on a golden ladder so they never line up), *Sixteen Points* (the
  full ceiling, looping) and *Dwelling Curve* (the same four points twice, curved
  to dwell at the start of each segment and at the end — the one thing an ADSR
  cannot say).
* **Autoplay** — an optional second way for the conductor to work. *Free*
  (default) is the Cluster Brain as it always was: notes start and stop on
  their own timers. *Chords* keeps the cluster full and exchanges one voice at
  a time, so the harmony travels instead of churning: *Every* (seconds) or
  *Sync* (on the clock) sets the pace, *Step* and the panel's **Step now**
  button do it by hand, *Voice Lead* limits how far the exchanged voice may
  move (small = the chord shifts, large = it jumps), *Tension* how strictly
  the arriving note has to fit the ones that stay, *Root Move* how often the
  exchange takes the root with it. The last eight notes to leave carry a
  fading penalty, so the harmony does not keep picking up what it has just put
  down: measured over an hour at the default settings it reaches a chord it
  has never been in nearly every time and never swings back to the one two
  exchanges ago. Four presets use it: *Slow Progression*, *Turning Harmony*,
  *Chord Ladder* (on the clock) and *By Hand* (only when you press Step).
* **Tuning** — 11 built-in scales plus *Load Scala…*; *Keys*: "Snap to 12
  keys" (default) or "Consecutive degrees"; root, A4, random seed. *Purity*
  blends every note between 12-TET and the chosen scale, so you can hear the
  beating lock in; *Purity Drift* lets that come and go over minutes, with
  sounding notes gliding along. *Freeze* (Oscillator) holds every voice's
  spectrum still. *Portamento* slides a new key in from the last one;
  *Gravity* makes the slide linger on consonant intervals to the root and
  hurry across the dissonant stretches.

## Measuring instead of listening

```bash
build/Tools/render/Release/ambient_render.exe --seconds 180 --out drone.wav --stats
build/Tools/render/Release/ambient_render.exe --seconds 90 --preset "Sleep Concert" --out sleep.wav
build/Tools/render/Release/ambient_render.exe --seconds 30 --set brain_on=off --notes 48,55,60,64,67 --set scale="JI Major (Ptolemy)" --out chord.wav
python Tools/analyze.py drone.wav --window 15
```

`--list` prints every parameter key with its range, `--list-presets` the
preset names. Renders are deterministic for a given seed.
