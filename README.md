<img src="docs/logo-128.png" width="96" align="left" alt="AmbientSynth" />

# AmbientSynth

A drone-ambient synthesizer for slowly evolving polyphonic clusters, in the
spirit of Robert Rich's sleep concerts: just-intonation tunings, additive
partial banks that breathe, envelopes measured in minutes, a generative
**Cluster Brain** that conducts the piece on its own, and a spatial model that
treats depth as a landscape -- dry, bright voices in the foreground against a
dark, wide, infinite background.

**VST3 plugin and standalone application** for Windows (x64). The DSP core is
framework-free C++20; a native Meta Quest app (OpenXR, hand tracking) builds
against it but has not been run on a headset yet. Licence: AGPL-3.0.

<br clear="left" />

![AmbientSynth -- the main page](docs/screenshot.png)

## Download

**[AmbientSynth-1.7.0-Setup.exe](https://github.com/reneweller-coding/AmbientSynth/releases/download/v1.7.0/AmbientSynth-1.7.0-Setup.exe)**
(14 MB) -- installs the standalone, the VST3, the preset library and, with your
consent, downloads the 6 GB sample library. Nothing else has to be installed:
the runtime is linked in. **[Portable zip](https://github.com/reneweller-coding/AmbientSynth/releases/download/v1.7.0/AmbientSynth-1.7.0-portable.zip)**
(15 MB) for anyone who would rather not run an installer, and the
**[manual](https://github.com/reneweller-coding/AmbientSynth/releases/download/v1.7.0/AmbientSynth-Manual.pdf)**
(PDF, 158 pages) -- every tab of the panel as a picture, with what it does and
what its knobs mean, and seven Design chapters on why the instrument is built
the way it is, with the mathematics and the references. Everything is on the
[releases page](https://github.com/reneweller-coding/AmbientSynth/releases).

Requirements: Windows 10 or 11, a 64-bit processor with AVX2 (every x86-64
since 2013), a VST3 host if you want the plugin.

## What is in it

* **Four equal source slots** per voice -- additive bank, wavetable (a table of
  spectra), two-operator FM, granular texture, a Paulstretch-style spectral
  stretch that turns a field recording into weather, ten noise colours -- and
  a **Vector** that reads the four as the corners of one square. Wavetables
  morph by **optimal transport** when asked: a formant slides through the
  partials instead of fading across them, and the additive bank's partials can
  be **spread across the field one by one**. A pure fourth, fifth or octave can
  be put on everything at once, gliding, never jumping (**Transpose**).
* **Two filters**: ten models with a wavefolder, and a **Z-plane** morphing
  filter after the E-mu Morpheus with 155 shapes in twelve families, a cube
  rather than a square, and a modal mode that turns it into a struck body.
* **A spatial model** in which every note has a distance: brightness, level,
  dryness and presence all follow from that one number, with a true interaural
  time difference, a breathing distance, Doppler, and externalisation for
  headphones, and a **vertical axis**: each plane has a height, heard through
  the pinna's elevation notch, so the background can be the sky. Three reverb
  tiers -- near room, far reverb, convolution room -- with the background
  narrowed as it goes back, an **Envelop** that lifts the low lateral energy
  the sense of being inside a room is made of, and a **Depth Law** that makes
  the depth knob linear in heard distance rather than computed distance. A
  **Near Field** for voices within reach (the low-frequency level difference a
  far-field head never makes), **Comodulate** to let the whole background
  breathe as one so the foreground is heard through it, and a **Rotating**
  reverb whose lossless feedback matrix turns while every delay line stands
  still.
* **The Cosmos**: a parallel path of frequency shifter, tuned resonators, a
  vowel filter, a spectral nebula and a self-regulating shimmer loop.
* **A conductor** that plays all night: the Cluster Brain chooses notes from
  the scale, places them on the planes and holds them for minutes; an autoplay
  that exchanges one voice at a time; a second conductor for the background;
  purity drift and the **BEAT** source, the instrument listening to how far out
  of tune it is. It judges a chord four ways — by its intervals, by the
  roughness of the partials actually sounding, by whether the whole set implies
  **one root**, and by how evenly it is spread — and it **finds the key** it has
  drifted into from what has been sounding and for how long, rather than being
  told one. **Blend** lets a chord arrive as one object inside the ear's fusion
  window instead of voice by voice, **Guard** keeps drifting beats out of the
  2–8 Hz band where they read as wobble, and **Arc Harmony** lets the hour-scale
  arc loosen and tighten the harmony as it already does density and brightness.
  **Cascade** makes its clock a Hawkes process -- events that cause events,
  clusters and silences instead of a steady average -- **Surprise** and
  **Homeostat** hold the entropy of its choices to a target, and **Adaptive**
  tunes each arriving note pure against what is sounding while a shared comma
  offset walks the ensemble home at three cents a minute. A **Deja Vu** ring
  brings figures back and lets them mutate (after Marbles), and **Spread** and
  **Bias** shape the conductor's draws from grey average to soft-or-loud,
  short-or-long.
* **Thirteen tunings**: twelve just and historical tables, Scala files, and one
  that is not a table at all. **Timbre (Sethares)** sweeps the instrument's own
  roughness curve across the octave while it plays and puts a degree wherever
  the curve dips — just intonation for a harmonic spectrum, to within two cents,
  and something quite else once *Inharmonic* is up. Turn a knob and the tuning
  follows the sound. **Match** runs the other way: it bends the partials onto
  the degrees of whatever scale is chosen, so a tempered chord stops beating.
* **Modulation** everywhere: eight LFOs, six hand-drawn envelopes, eight
  macros, four coupled Kuramoto oscillators, a **Lenia** field -- a continuous
  cellular automaton whose blobs drift, split and die by their neighbours'
  doing, read at four points -- the **Lorenz** and **Roessler** attractors on a
  scale of minutes (chaos: a shape and a memory, never a cycle), aftertouch,
  wheel and slide, smoothed if you like by the **one-euro filter**, all through one
  matrix onto any knob -- including the modulators' own.
* **6800 presets in 34 packs**, each written in the spirit of an artist of
  the genre, every one rendered, measured and gain-matched; 1700 samples, 608
  wavetables and 240 impulse responses (forty of them struck objects cut from
  the field recordings, for convolving a pad with a piece of the world). A
  browser that filters by measured character, a **preset map** that zooms and
  pans -- names appear as you close in, and "more like this" narrows the list to
  a preset's measured neighbours -- whose empty space between presets is
  playable, and routes that walk it by themselves.
* **No compressor anywhere.** A BS.1770 loudness meter instead, and a mono
  guard: everything here is built to widen, and it is measured to survive a
  mono sum.
* Continuous by construction: every movement in the instrument is a rate or
  an amplitude, never a step. A click is a bug.

![The preset map](docs/map.png)

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
(14 MB) and a portable **`.zip`** (15 MB) for anyone who would rather not run an installer.

Nothing has to be installed first. The runtime is linked in
(`-DAMBIENT_STATIC_RUNTIME=ON`), so there is no Visual C++ redistributable to chase — the script
checks that with `dumpbin` and refuses to package a binary that still asks for one. The build is
AVX2 (52x realtime against 39x without it, both measured); every x86-64 processor since 2013 has
it, and the setup asks the processor before installing rather than letting an older one die on an
illegal instruction with no explanation.

The setup installs, each with its own checkbox:

* the **standalone** (always) into Program Files, with a Start-menu entry,
* the **VST3** into `Common Files\VST3`,
* the **preset library** (34 packs, 6800 presets) into `ProgramData\AmbientSynth\Packs`,
* the **sample library** — 1848 samples, wavetables and impulse responses, 6.1 GB in six
  archives (467 of them seamless field recordings for the Stretch type, 40 struck objects for the
  convolution room), downloaded from the release and checked against its hash — into the same
  folder,

all four on by default.

If the download is declined or fails, the setup says so and installs everything else; the
archives can then be unpacked by hand into `ProgramData\AmbientSynth` (or
`LocalAppData\AmbientSynth` for a per-user install), beside the `Packs` folder. A preset whose
sample is missing still loads and leaves that slot empty.

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

* the **Sound** box in the header — 196 built-in presets (voices, space,
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
library of 6800 presets in 34 packs with the samples, wavetables and
impulse responses they play. *Save…* / *Load…* store the whole
state as an `.ambientsynth` file. Play MIDI notes to add your own voices;
they sit in the foreground (see *Keys Depth*) and the lowest held key becomes
the brain's root.

**Four sources.** Every voice has four equal source slots. Each is one of
Additive (a 32-partial bank shaped by tilt, brightness, odd/even, inharmonic
stretch and shimmer -- in Source 1 the classic strand bank with unison,
detune and stacks), Wavetable (a table of spectra), FM, Texture (granular,
up to 64 grains), **Stretch**, **Bow**, **Spectral** or Noise (ten colours);
so four additive banks, four granular players or any mix are a matter of four
choices.

**Bow** is a bowed string: two digital waveguides meeting under the bow, and
at their junction the friction characteristic that decides, sample by sample,
whether hair and string are stuck together or slipping. That alternation is
the Helmholtz motion, and it sustains for as long as *Bow Speed* is above
zero. *Bow Force* against *Bow Speed* is the whole gesture -- light and fast
is breath, heavy and slow is tone. Measured: 222 Hz for an A3, at the level of
a wavetable slot to a tenth of a decibel, and a bow that stops moving damps
the string 73 dB in a tenth of a second, because the hair is still on it.

**Spectral** does not play the clip at all; it rebuilds it. On loading, the
recording is measured once into 32 bands on the ear's own frequency scale, and
what is kept per frame is how loud each band is, where in it the strongest
partial sits, and how far above the local noise floor that content stands.
Playback makes each band again from an oscillator and a band of noise, in that
proportion -- the deterministic-plus-stochastic decomposition of Serra and
Smith, taken band by band, so no fundamental has to be found and a bell, rain
and a voice all work. Nothing is a sample any more, so the note sets the pitch
and *Rate* sets the speed and neither touches the other; at *Rate* 0 the read
head stands still and one moment of a recording is held for as long as the
note lasts. *Breath* tilts the balance to the partials alone or to the noise
alone: rain becomes the chord hiding inside it, a struck bell becomes the wind
that has its shape.

**Stretch** is the same clip read as a continuum instead of as grains: a
spectral time stretch after Paulstretch — a window of the recording is
transformed, its magnitudes kept, its phases drawn afresh and the result
overlap-added, while the read position crawls through the clip at one
*Stretch*-th of its speed, from 1 to 1000. No grain rhythm and no transient
left standing: twenty seconds of rain become an evening of it. Pitch (*Pitch*
= Note or Free, with octave and ratio) is applied by resampling *before* the
stretch, so a chromatic sample plays across the keyboard without the high
notes getting shorter; *Grain* is the spectral window; the loop's seam is
crossfaded by *Loop Fade*, or not at all for a clip whose name carries
`_loop`, which marks it seamless. Measured against the granular player on the
same recording: the same level within a decibel, a lower flux (smoother), a
sample-to-sample jump under 0.011 against a limit of 0.3, and the pitch
identical to the last decimal in both Follow modes. Each slot has its
own clip, so four Texture slots can play four different recordings; a pack
preset names them as up to four paths separated by `;`, and a preset that
names one file still puts it in every slot, as it always did. **Nine filter
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
a voice out of the background towards you -- and **aftertouch, the mod wheel and the slide are
sources in the modulation matrix** as well, so one hand can open the filter, move the wavetable
and lift the reverb at once. A **second conductor** for the background, the first
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
every voice as a dot (left-right by pan, near-far by plane) and the three
planes as lines you can drag, the Cosmos return's spectrum, the conductor's
notes as a roll, the timbre's own roughness curve with the scale's degrees in
its dips and the key the conductor has found, the Kuramoto ring, the Lenia
field and the attractors. The strip along the bottom holds the modulators:
drag a card onto a knob to route it, right-click a card or a knob to see and
remove routes, and the LFO / ENVELOPES / MATRIX tabs edit the sources. A knob
shows what is moving it: the matrix as a ring in its source's colour, the
morph or the map's blend as a neutral arc from the knob's value to the live
one, and the arc and the tide as a dot on the outer ring that says where in
their slow swing they are -- with Arc Clock on, the Arc knob wears a dial of the
day. The header names the key the conductor has found and glows with each event
its cascade breeds; the notes roll shows the deja-vu ring, the cascade's glow and
the homeostat's needle; the attractors draw their orbits. Every section title carries a die (click to redraw that
section, shift to nudge it); Undo, Redo and A|B work on whole snapshots; the header shows the
output's spectrum. The page is laid out from the parameters, never from the window: a source
strip holds only the knobs its type uses, and a section whose switch is off -- a slot on Off, a
level at zero, a Morph or a second conductor that is not active -- closes to its title and its
switch; choose a type or throw the switch and it opens. Three layouts, one button: Normal (one
page, tabs; 2269 x 1260 at design size), Compact (the widest rows wrap, for a narrower window)
and Expanded (a page of its own, no tabs, three columns -- the voice and its room; the
shaping, the effects, the background and the small sections side by side; morph, macros and
the conductor -- 2269 x 958, about 2.4 : 1, which a 1080-line screen shows larger than life).

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

**Loudness.** In the header, where the small spectrum used to be (the strip
below does that better now): integrated and short-term loudness to ITU-R
BS.1770-4, the loudness range, the true peak between the samples, the crest
factor and the loudness in **sones**, with the −18 to −24 LUFS window ambient
masters live in marked on the bar. The sone figure answers a different
question. LUFS is energy through one fixed weighting curve; loudness grows
with bandwidth once a sound is wider than a critical band, and no energy meter
will ever say so. On this instrument's own presets, *Sleep Concert* and
*Subharmonic Deep* are 0.18 LUFS apart and 27 % apart in sones. For a piece
that has to sit at a comfortable level for an hour, the second number is the
useful one. It is Zwicker's model (ISO 532-1): a third-octave analysis, the
excitation pattern with the two slopes of a masking curve, the specific
loudness against the threshold in quiet, integrated over 24 Bark. Full scale
is taken as 100 dB SPL. Click it to start measuring again. It is in the engine rather than in the
interface, because the integrated figure is a number about the whole piece and
a meter that only sees what the interface asked for has holes in it. The
K-weighting is derived from the standard's analogue prototypes and reproduces
the published 48 kHz coefficients to sixteen digits, so it is right at every
sample rate; `ambient_render --loudness` prints the same numbers for a render.
The point is not to get louder. A brickwall limiter takes the finest amplitude
movement out of a reverb tail and leaves it grainy and flat, and the sense of
an enormous room comes from the distance between the quietest texture and the
loudest swell.

**Vector.** A page beside the source slots: a point in a square whose four
corners are the four sources, after the Prophet VS and the Wavestation. *Amount* at 0 does nothing at all, and the
centre of the square is neutral by construction — every factor is exactly 1 —
so it can be turned up on a patch you like and changes nothing until you move.
*Wander* lets the point drift on its own, on two curves whose rates share no
simple ratio.

**The filter funnel.** Each of the three reverb returns has a *Low Cut* beside
its high cut (12 dB/oct, off at 20 Hz). Dense tails pile up between 200 and
450 Hz, which is exactly where a background stops sitting behind the music and
starts covering it. Measured on the far return alone: at 800 Hz it takes 27.8 dB
out below 150 Hz and 1.4 dB off the top. And *Subsonic* in the Master section is
a 24 dB/oct high-pass for the energy under hearing, off by default — this
instrument's Foundation reaches to about 16 Hz, lower than a mastering engineer
would cut, so where to cut has to be the player's decision.

**Output spectrum.** Along the bottom of the left column, as wide as the page:
what is actually coming out, with the filter's own response laid over it on the
same decibel scale. The window is 16384 samples — 2.9 Hz at 48 kHz — which is
what it takes to show a low drone's partials as separate lines instead of one
hump (measured: the trough between a 40 Hz tone and its octave is 14 dB down at
4096 samples and 58 dB down at 16384). So a fifth sitting exactly on the third
partial is visible, and so is it coming apart as *Purity Drift* loosens the
tuning. The bars are the moment, the faint line above them is the loudest each
band has been in the last few seconds, the ticks along the bottom are the
fundamentals of the notes sounding now; point at it to read a frequency, its
nearest note and that band's level.

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
* **Filter** — ten models (one- to four-pole low-pass, high-pass, band-pass,
  notch, peak, saturating ladder, tuned comb, formant) with key tracking,
  envelope amount, slow drift and drive, and a *Fold*: a wavefolder after both
  filters that mirrors the wave instead of flattening it, for the metallic edge
  no saturation reaches.
* **Z-Plane** — a Morpheus-style morphing filter: four frames of up to six
  cascaded pole/zero sections sit on the corners of a square, a point
  between them is a filter interpolated from all four, and the point
  wanders on its own (*Rate*, *Depth*), and a third axis *Z* makes the square
  a cube. 155 shapes in twelve families — vowel morphs, choirs, sweeps,
  phasers, combs, notch clusters, strings, metal bars, wood, glass, peaks,
  and the families generated from acoustic ratios — in series after the
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
* **Ensemble** — *Chorus*, three modulated taps; *Microshift*, the two
  channels detuned a few cents in opposite directions and delayed by different
  amounts, with nothing moving; or *Velvet*, a sparse sequence of signed
  impulses per channel. A drone widened either of the latter two ways survives
  a mono sum, where a deep chorus at 13–22 ms is a comb filter waiting to be
  summed. Measured on a sustained tone, velvet and chorus decorrelate the two
  channels about equally, and velvet colours them by 1.32 dB where the chorus
  colours them by 5.41.
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
* **Haas** (in Space) — the 1.2–4 kHz band of the centre, delayed and put into
  the side channel: width where the ear takes its direction from level rather
  than from time. What is added on one side comes off the other, so the mono
  sum is exactly the picture it was.
* **Binaural** (in Space) — a headphone mode: the pan becomes an angle round
  the head, the interaural delay follows Woodworth's head model, a source
  behind you gets the lower pinna notch, and with a headset or an OSC head
  tracker (`/ambient/head`) the whole field turns against your head, so voices
  stay where they are in the room while you look round. Off on speakers.
* **Far Reverb** — 8-line FDN with decay up to 90 s, size, damping, pre-delay,
  *Asymmetry* (right-hand lines longer, right output later), *Tail Cut*
  (low-pass on the tail: distance darkens), *Low Cut*, *Freeze*, and *Width*:
  the background's own stereo width before it joins the foreground. A mix in
  which everything is spread as far as it will go is a flat wall; pulling the
  far plane towards the centre while the foreground stays wide is the funnel
  the ear reads as distance. *Mode*: the classic network, *Scattering* with
  an all-pass inside every line's loop (a denser tail at the same decay), or
  *Colourless*, the same network with its eight line lengths searched rather
  than chosen — 0.314 dB of spectral spread in the tail against the classic
  set's 0.474. *Unmask* lets the background step aside for the foreground band
  by band, and *Spread* gives it the ear's upward spread of masking.
* **Early Room** — the fourth reverb stage, off by default, and the only one
  that is about WHERE rather than about how long. A scattering delay network
  with one node at the centre of each wall of a shoebox: the source's sound
  reaches each wall after the real distance, the walls scatter into one another
  after the real distances between them, and each wall's answer arrives at the
  listener from its own direction. So the first reflections carry the room's
  size and the source's place in it, and they MOVE when the source moves —
  measured, 1.60 dB of side balance follows a source across a ten-metre room,
  where a diffuser gives 0.02. *Early* is the level, *Size* the room's longest
  wall in metres, *Absorb* how much each surface takes and how dark it returns,
  *Width* how far apart the walls are placed in the picture.
* **Room** — a third reverb, optional and additional: a convolution reverb
  on the far plane playing an impulse response (*Impulse…* loads a mono or
  stereo file, a built-in dark hall plays without one). The library's 240
  impulses include forty struck objects cut from the field recordings — a
  chain on concrete, a knocked pipe — so a pad can be played on a piece of
  the world rather than in a room. *Level*, *Source*
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
  end stays centred), *Side Air* (broad upper-mid lift on the sides), *Width*,
  *Subsonic* (a steep high-pass on the finished output). No compressor:
  dynamics are left alone. The loudness meter under it reads the output to
  BS.1770 — integrated, short term, range, true peak and crest — with the
  −24…−16 LUFS window a dark ambient master is asked to land in marked on the
  bar and the −14 LUFS streaming line drawn across it.
* **Cluster Brain** — density, event rate, hold time range, register range,
  *Consonance* (1 = only simple ratios to the root, 0 = clusters), *Wander*,
  and *Timbre*: the conductor judging intervals by the roughness of the partials
  it actually plays (Sethares) rather than by the ratio alone, so an inharmonic
  patch is conducted in the intervals it is consonant at.
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
  hurry across the dissonant stretches. *Stretch* widens the octave by a few
  cents, the way listeners prefer it and pianos are tuned, as a slope about
  the reference pitch; 0 is the exact 2:1.

## Measuring instead of listening

```bash
build/Tools/render/Release/ambient_render.exe --seconds 180 --out drone.wav --stats
build/Tools/render/Release/ambient_render.exe --seconds 90 --preset "Sleep Concert" --out sleep.wav
build/Tools/render/Release/ambient_render.exe --seconds 30 --set brain_on=off --notes 48,55,60,64,67 --set scale="JI Major (Ptolemy)" --out chord.wav
python Tools/analyze.py drone.wav --window 15
```

`--list` prints every parameter key with its range, `--list-presets` the
preset names. Renders are deterministic for a given seed.
