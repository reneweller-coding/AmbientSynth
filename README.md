# AmbientSynth

A drone-ambient software synthesizer for slowly evolving polyphonic sound
clusters, in the spirit of Robert Rich's sleep concerts: just-intonation
tunings, additive partial banks that breathe, envelopes measured in minutes,
and a generative "Cluster Brain" that conducts the piece on its own.

Runs as a **VST3 plugin** and as a **standalone application** (Windows now;
the DSP core is framework-free so it can move to Android/Quest later).

## Layout

| Directory | What | Depends on |
|---|---|---|
| `Core/` | The whole synthesizer: parameters, tuning, voices, effects, cluster brain, engine. Pure C++20, no allocations while rendering. | nothing |
| `Plugin/` | JUCE wrapper: VST3 + Standalone, generic parameter GUI, Scala loader, state save/restore. | JUCE 9 (fetched by CMake) |
| `Tools/render/` | `ambient_render`: offline renderer to WAV with per-second measurements. | Core |
| `Tools/analyze.py` | Measures a WAV (level, clicks, spectral centroid, peaks) so a patch can be judged by numbers. | numpy |
| `Tests/` | `ambient_selftest`: tuning, envelope, engine, brain, determinism. | Core |
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
within a few seconds. Play MIDI notes to add your own voices; the lowest held
key becomes the brain's root.

Sections of the GUI (all parameters are automatable in a DAW):

* **Oscillator** — additive bank per strand: number of partials, spectral tilt,
  brightness, odd/even balance, inharmonicity, *Shimmer* (each partial's level
  drifts on its own), strands (unison), detune, pitch drift, stereo spread.
* **Envelope** — attack up to 60 s, release up to 120 s.
* **Filter** — state-variable low-pass with key tracking, envelope amount and slow drift.
* **Ensemble** — three-tap modulated chorus.
* **Reverb** — 8-line feedback delay network, decay up to 60 s, *Freeze*.
* **Cluster Brain** — density (how many notes), event rate, hold time range,
  register range, *Consonance* (1 = only simple ratios to the root, 0 = clusters),
  *Wander* (probability of the root moving by a fifth/fourth/third).
* **Tuning** — 11 built-in scales (12-TET, JI major/minor, 7-limit, Pythagorean,
  pentatonic, harmonic series, subharmonic, slendro, Bohlen-Pierce, otonality)
  plus a *Load Scala…* slot for any `.scl` file. *Keys*: "Snap to 12 keys" maps
  each of the 12 keys to the nearest scale degree (default); "Consecutive
  degrees" maps successive keys to successive degrees (for 19-EDO, 31-EDO,
  Bohlen-Pierce and friends). Root, A4 reference, random seed.

## Measuring instead of listening

```bash
build/Tools/render/Release/ambient_render.exe --seconds 180 --out drone.wav --stats
build/Tools/render/Release/ambient_render.exe --seconds 30 --set brain_on=off --notes 48,55,60,64,67 --set scale="JI Major (Ptolemy)" --out chord.wav
python Tools/analyze.py drone.wav --window 15
```

`--list` prints every parameter key with its range. Renders are deterministic
for a given seed, so a measurement can be repeated exactly.

## Licence note

JUCE 8/9 is available under AGPLv3 for open-source projects or under a
commercial licence. Releasing this project publicly therefore means AGPLv3
(or buying a JUCE licence). The `Core/` library has no JUCE dependency and can
be licensed independently.
