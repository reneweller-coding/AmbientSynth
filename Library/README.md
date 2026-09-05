# The AmbientSynth preset library

Five thousand presets in twenty-five packs, with the samples and wavetables they play.

The packs are text and live in the repository. The audio does not: 1200 texture clips are
about 8 GB and 608 wavetables about 80 MB, so both are generated locally and are ignored by
git. Everything is deterministic -- the same seeds always produce the same files with the same
names -- which is why a pack can name a sample that has not been rendered yet.

```
Library/
  Packs/*.ambientpack     25 files, 200 presets each   (in git)
  Textures/*.wav          1200 granular source clips   (generated)
  Wavetables/*.wav        608 wavetables               (generated)
```

## Building it

Wavetables first; they take about a minute and need nothing but numpy and soundfile.

```
Tools/TextureGen/.venv/Scripts/python Tools/library/make_wavetables.py --per-recipe 32
```

Then the textures. This is the long one: three text-to-audio models, roughly four hours on an
RTX 5090. It runs below normal priority, stops on its own after `--max-minutes`, and `--resume`
skips everything already on disk, so it can be interrupted and restarted at will.

```
Tools/TextureGen/.venv/Scripts/python Tools/library/make_textures.py --per-style 48
Tools/TextureGen/.venv/Scripts/python Tools/TextureGen/texturegen_worker.py \
    --batch Tools/library/texture_jobs.json --out-dir Library/Textures \
    --resume --nice --max-minutes 330
```

Audit the clips: text-to-audio misses sometimes, and a silent or gappy clip makes a dead
granular source. `--repair` high-passes clips with a DC offset instead of rejecting them, and
`--compact` halves the library by rewriting 32-bit float as 16-bit PCM, which is worth doing
before pushing anything to a headset.

```
python Tools/library/check_textures.py --repair
```

Then the packs, which reference the files by name:

```
python Tools/library/make_presets.py --per-style 200
```

Finally the measurement pass. `make_presets.py` estimates each preset's descriptors from its
settings, because it has to name files before they exist. This replaces that estimate with the
real thing and corrects every preset's master gain to the measured loudness, which is what
keeps a library of thousands from having a few dozen presets that jump out. About twenty
minutes for five thousand renders.

```
python Tools/library/measure_packs.py --jobs 6
```

## Installing it

The synth loads every `*.ambientpack` in `$AMBIENT_PACKS` (one folder, or several separated by
`;`), and otherwise in `Documents/AmbientSynth/Packs`. A pack names its samples relative to its
own folder, as `../Textures/...` and `../Wavetables/...`, so keep the three folders together:

```
Documents/AmbientSynth/Packs        <- Library/Packs
Documents/AmbientSynth/Textures     <- Library/Textures
Documents/AmbientSynth/Wavetables   <- Library/Wavetables
```

A preset whose sample is missing still loads; it just leaves that slot empty.

## What is in the packs

Each pack is one corner of the drone repertoire, written in the spirit of an artist who works
there. Nothing is sampled from or affiliated with any of them; the packs are ranges over this
synth's own parameters, chosen by ear.

| Pack | In the spirit of | Character |
| --- | --- | --- |
| Sleep Concert | Robert Rich | just intonation, binaural sub, hours-long holds |
| Deep Earth | Lustmord | subterranean, sub-heavy, almost no treble |
| Permafrost | Thomas Koener | filtered noise fields, nearly motionless |
| Field Absence | Francisco Lopez | granular field recordings, quiet, atonal |
| Aldebaran | Inade | ritual metal, Cosmos-heavy, ceremonial |
| Ritual Machine | Deutsch Nepal | saturated feedback loops, tape, grime |
| Planetary | Michael Stearns | harmonic series, wide, shimmering |
| Temple of Air | Ooephoi | pure, extremely slow, minute-long envelopes |
| Vast Chord | Mathias Grassow | dense just-intoned chord walls |
| Desert Ember | Steve Roach | warm analog, organic, slow pulses |
| Modular Nocturne | Ian Boddy | resonant filter movement, echoing sequences |
| Millstone | Jonathan Coleclough | acoustic, mechanical, granular |
| Slow Carousel | Mimir | warped loops under tape hiss |
| Glass Vitrine | Mirror | ghostly harmonium, spectral |
| Chamber Grey | In Camera | small dim rooms, close and quiet |
| Painted Field | Andrew Chalk | blurred warm washes |
| Loop Studio | Colin Potter | long tape delays, processed loops |
| Ghost Signal | Bass Communion | granular, wide, processed strings |
| Sustain | Paul Bradley | one long tone, minimal change |
| Hull Rumble | SleepResearch_Facility | machine hum, static, deep |
| Corridor | Kammarheit | dark reverberant rooms, sparse |
| Northern Dark | Gustaf Hildebrand | cinematic, sub-bass, wide |
| Void Station | Tholen | cold science fiction, slow z-plane sweeps |
| Strings at Rest | Stars of the Lid | consonant bowed swells |
| Tape Saturation | Tim Hecker | bright, distorted, damaged |

## Metadata

Every pack line carries a map position, six descriptors (brightness, motion, width, noisiness,
weight, density) and tag bits, so the browser filters and the Absynth-style point map work on
the library the same way they work on the built-in presets.

Both halves of the library are measured, not described. `Tools/preset_map.py` renders the 148
built-in presets; `Tools/library/measure_packs.py` renders the generated ones and writes the
result back into the pack files. `make_presets.py` alone would only estimate the descriptors
from the settings, which is enough to lay a map out but is a prediction -- so a library that has
not been through the measurement pass says so in its own header line.

## Checking it

`Tools/preset_check.py` renders presets and fails the ones that are too loud, clip, click,
carry DC or come out silent. It takes the whole library:

```
python Tools/preset_check.py --packs Library/Packs --sample 300 --jobs 6
```
