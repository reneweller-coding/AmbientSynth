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
within a few seconds. Pick a preset from the box in the header. Play MIDI
notes to add your own voices; they sit in the foreground (see *Keys Depth*)
and the lowest held key becomes the brain's root.

Sections of the GUI (all parameters are automatable in a DAW):

* **Oscillator** — additive bank per strand: partials, spectral tilt,
  brightness, odd/even balance, inharmonicity, *Shimmer* (each partial's level
  drifts on its own), strands (unison), detune, pitch drift, stereo spread.
* **Air** — a band-passed noise layer per voice around a drifting multiple of
  the fundamental: breath, flute air, glass hiss.
* **Envelope** — attack up to 60 s, release up to 120 s.
* **Filter** — state-variable low-pass with key tracking, envelope amount and slow drift.
* **Space** — *Depth*: how far back the brain places its notes (most go deep,
  some stay intimately close); *Keys Depth*: the plane of MIDI notes; *Pan
  Drift*: slow wandering of each voice's centre; *Time Width*: interaural time
  difference rendered per voice (true time-based width, mono-compatible);
  *Arc*: an hour-scale drift of density, brightness and depth with its period.
* **Ensemble** — three-tap modulated chorus.
* **Delay** — stereo delay with independent L/R times (asymmetric by default),
  feedback, cross-feed, damping, mix, and *To Far*: how much of the echoes
  recede into the background reverb.
* **Near Reverb** — small room for the foreground plane.
* **Far Reverb** — 8-line FDN with decay up to 90 s, size, damping, pre-delay,
  *Asymmetry* (right-hand lines longer, right output later), *Tail Cut*
  (low-pass on the tail: distance darkens), *Freeze*.
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
