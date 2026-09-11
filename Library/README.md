# The AmbientSynth preset library

## Now: the new material, not yet the new presets (11.09.2026)

The clips and tables were swapped for the new generation; the packs were not rebuilt yet, so they
still name the old files (7887 of 8400 presets lose their sample or table until then).

```
Library/
  Textures/*.flac          5748 tonal clips from Stable Audio 3 medium: <category>-<id>_<Note>.flac,
                           49 categories, 24-bit 44.1 kHz stereo, 13-135 s, peak -1 dBFS
  FieldRecordings/*.flac   2938 seamless loops: fr-<category>-<id>_loop.flac, 36 categories, 40-115 s
  Wavetables/*.wav + .json 1020 tables, flat: harmonic_* (380, HarmonicGen, the Harmonic type, 64 frames)
                           and akwf_* / wavedit_* (640, AKWF and WaveEdit Online, CC0, the classic
                           Wavetable type, clm chunk); every table's .json says where it came from,
                           and for the classic ones whether it is grounded or hollow (upper layers only)
  Wavetables/CREDITS-classic.md   the AKWF/WaveEdit credits (not .txt: sort_clips.py deletes those)
  Impulses/                curated 11.09. (Tools/library/curate_impulses.py), CREDITS.txt for AIR
```

Where it came from: the generator and its ledgers in `G:/Tools/VRAudio/StableAudio3` (`build/`,
`build_fr/`, prompt lists in `prompts/`); the category `tape-loop` was renamed `tapeloop` on the way in,
because a `-loop` in a name makes the engine treat a clip as a seamless loop
(`build/library_renames.json`). The old library is backed up on
`M:/Samples/AmbientSynth-Backup/2026-09-10` and `2026-09-11` (README.txt there).

Until the presets are rebuilt, and before rebuilding them:
- the library tools below still look for `.wav` clips only: make them read `.flac` first
  (make_presets, clip_tonality, clip_affinity, sort_clips, check_textures, audit_names, preset_stats,
  verify_packs, make_content_pack), then rebuild `tonality.json` and `affinity.json`;
- do not run `make_wavetables.py` into `Library/Wavetables`, nor `check_textures.py --repair/--compact`
  (they write the old library's formats), nor `sort_clips.py` without `--no-fold`;
- do not measure, rebalance or map the old packs: every render would miss its clips;
- new packs need a `format 2` line and the right type per table (Harmonic for harmonic_*, Wavetable
  for akwf_*/wavedit_*); the plugin reads at most 120 s of a clip (85 textures are longer).

Everything below describes the library as it was built before this swap.

## Before the swap

Six thousand eight hundred presets in thirty-four packs, with the samples and wavetables they play.

The packs are text and live in the repository. The audio does not: 1200 texture clips are
about 8 GB and 608 wavetables about 80 MB, so both are generated locally and are ignored by
git. Everything is deterministic -- the same seeds always produce the same files with the same
names -- which is why a pack can name a sample that has not been rendered yet.

```
Library/
  Packs/*.ambientpack     34 files, 200 presets each   (in git)
  Textures/*.wav          1700 clips: 1200 textures and 500 field recordings   (generated)
  Wavetables/*.wav        608 wavetables               (generated)
  Impulses/*.wav          240 impulse responses        (generated)
```

A pack line carries eight fields: name, settings, metadata, and then the three files a preset
brings with it (texture, wavetable, impulse) plus its modulation -- the matrix rows and the six
envelope shapes. Everything after the settings is optional.

## Building it

Wavetables and impulses first; together about two minutes, CPU only.

```
Tools/TextureGen/.venv/Scripts/python Tools/library/make_wavetables.py --per-recipe 32
Tools/TextureGen/.venv/Scripts/python Tools/library/make_impulses.py
```

The impulses are not room simulations for their own sake -- a convolution reverb fed a drone is a
resonator you can shape. Nine families: designed rooms, tuned partial banks that ring in key,
inharmonic modal metal, reversed swells, combs and pipes, thinning scatter, octave-up shimmer,
spectral bands with different decay times -- and forty struck objects, cut from the field
recordings: the sharpest event in a recording of a foundry or a cistern, shaped into an impulse of
a tenth to half a second. Convolved with a pad, that object's resonance lands on the synthetic
wave; measured on the room stem, five to fourteen decibels different from a hall band by band.
The struck family needs the field recordings to exist first, so run it (or run the script again)
after `make_field_recordings.py`.

Then the textures. This is the long one: three text-to-audio models, roughly four hours on an
RTX 5090. It runs below normal priority, stops on its own after `--max-minutes`, and `--resume`
skips everything already on disk, so it can be interrupted and restarted at will.

```
Tools/TextureGen/.venv/Scripts/python Tools/library/make_textures.py --per-style 48
Tools/TextureGen/.venv/Scripts/python Tools/TextureGen/texturegen_worker.py \
    --batch Tools/library/texture_jobs.json --out-dir Library/Textures \
    --resume --nice --max-minutes 330
```

Most of the shelf comes from the prompt lists rather than from per-style generation, because a
thousand distinct ideas beat forty variations of forty. Those runs take a plain text file instead
of a jobs file (see `Tools/library/prompts/README.md`):

```
Tools/TextureGen/.venv/Scripts/python Tools/TextureGen/texturegen_worker.py \
    --batch Tools/library/prompts/tonal.txt --count 4 --out-dir Library/Textures --resume --nice
Tools/TextureGen/.venv/Scripts/python Tools/TextureGen/texturegen_worker.py \
    --batch Tools/library/prompts/atonal.txt --count 3 --out-dir Library/FieldRecordings --resume --nice
python Tools/library/sort_clips.py
```

`sort_clips.py` has the last word on which folder a clip belongs in: the generator writes the
detected note into the file name, and only a clip that has one may be transposed to the note being
played.

Audit the clips: text-to-audio misses sometimes, and a silent or gappy clip makes a dead
granular source. `--repair` high-passes clips with a DC offset instead of rejecting them, and
`--compact` halves the library by rewriting 32-bit float as 16-bit PCM, which is worth doing
before pushing anything to a headset.

```
python Tools/library/check_textures.py --repair
```

The field recordings, for the Stretch and Texture types: five hundred places -- rain on twelve
kinds of roof, caves, harbours in fog, power stations through a wall -- from Stable Audio Open,
made seamless afterwards (the last 1.5 s faded into the first and the overlap trimmed, the
seam measured, the file marked `_loop`, which is what the Stretch type reads to skip its own
crossfade). Short on purpose: at forty times slower than life, 24 seconds is a quarter of an
hour. They land in `Library/Textures` with the `field_recordings_` prefix the preset
generator uses to find a style's own clips.

```
python Tools/library/make_field_recordings.py --jobs
python Tools/library/make_field_recordings.py --run --max-minutes 180
python Tools/library/make_field_recordings.py --finish
```

### Texture from statistics

A recording can also be used as a description rather than as material. McDermott and Simoncelli
showed that sound texture -- rain, wind, fire, a crowd -- is recognised from a small set of
time-averaged statistics of the auditory periphery, not from the waveform: measure those on one
recording, impose them on noise, and a listener hears the same texture without hearing the same
sound. That is an endless bed with the character of the original and none of its repetition, which
is what an ambient instrument wants from a field recording.

```
python Tools/library/texture_statistics.py --selftest
python Tools/library/texture_statistics.py --in-dir Library/Textures --out-dir Library/Textures \
    --count 40 --seconds 30 --report stats.csv
```

Every run reports how close it got, as a relative error against the source's own statistics, and
how much of the source is literally in the result. On eight library recordings: the moments and the
cross-band correlations land within a few per cent, and the largest cross-correlation with any
source is 0.066, so nothing is copied. The modulation power is the loose figure and it separates
the material honestly -- heavy rain 0.12, a dust storm 0.25, a glacier 0.42, but a resonant bronze
bowl 0.92. That is the model's own boundary: a bell struck once is not a texture. Use it on beds;
the number will tell you when you have not.

Then the packs, which reference the files by name:

```
python Tools/library/make_presets.py --per-style 200
```

Finally the measurement pass. `make_presets.py` estimates each preset's descriptors from its
settings, because it has to name files before they exist. This replaces that estimate with the
real thing and corrects every preset's master gain to the measured loudness, which is what
keeps a library of thousands from having a few dozen presets that jump out. About half an
hour for six thousand renders.

```
python Tools/library/measure_packs.py --jobs 6
python Tools/library/verify_packs.py          # again: the pass rewrites every line
```

Verify **after** measuring as well as before. The measurement pass rewrites all six thousand
lines, and the first version of it read five fields and wrote five -- silently dropping the
impulse, the matrix and the envelope shapes from the whole library. It now names the fields once
and reports how many presets still carry each of them.

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

Five more were written later, for the parts of the instrument the twenty-five above predate --
the modal resonator bank, three source slots of the same kind, the conductor exchanging one
voice at a time, the 155 filter shapes, and a matrix fed by the instrument's own tuning:

| Pack | In the spirit of | Character |
| --- | --- | --- |
| Struck Bodies | Bernhard Guenter | the z-plane read as a resonator, struck and left to ring |
| Three Alike | Eliane Radigue | three source slots of the same kind, beating against each other |
| Turning Harmony | Pauline Oliveros | autoplay in Chords: one voice exchanged at a time |
| Filter Cubes | Alva Noto | the filter cube's third axis, cold and precise |
| Own Tuning | Catherine Christer Hennix | purity drift, difference tones, BEAT as a modulator |

And one for the field recordings, written after the Stretch type and the fourth source slot:

| Pack | In the spirit of | Character |
| --- | --- | --- |
| Field Recordings | Chris Watson | places, not instruments: seamless recordings read as a continuum by the Stretch type, up to four of them in the Vector's four corners, a quiet additive centre underneath |

And one for the mixing desk the instrument grew last:

| Pack | In the spirit of | Character |
| --- | --- | --- |
| Cryo Chamber | Atrium Carceri | the background narrowed as it goes back, the foreground opened by the Haas band, the Ensemble as a microshift, the wavefolder, and the Room loaded with a struck object rather than a hall |
| Rosin and Wire | the bowed-string tradition, from Tony Conrad to Ellen Fullman | the Bow type in most slots -- a real waveguide string under a real bow -- with the Early Room on, because a string is an object in a place |
| Held Moment | Eliane Radigue, Thomas Koener | the Spectral type nearly everywhere, and mostly with its read head stopped: one second of a recording, rebuilt from its bands and held for as long as the note lasts |

## Fitting new features into old presets

An instrument that gains a feature gains it for the presets written afterwards. `retrofit_presets.py`
walks the library and gives each preset the new things that suit it, judged from its own settings:
aftertouch, wheel and slide as matrix routes (neutral until moved, so nearly everywhere), the
background's own width where there is a deep background, the Haas band where there is a foreground,
the microshift only where the chorus was slow and quiet enough to have been a widener, the fold only
where saturation was already asked for, and a quiet stretched field recording under presets in the
packs that are about places. Deterministic on the preset's name; a second run changes nothing.
`Tools/retrofit_builtins.py` does the same for the compiled-in presets, rendering each sound-changing
addition first and keeping it only if the preset still sounds like itself. After either, run the
measurement pass: the gains are wrong by a decibel or two because the sound has changed.

```
python Tools/library/retrofit_presets.py --write
python Tools/library/measure_packs.py --jobs 6
python Tools/library/verify_packs.py
```

## Modulation in the library

Every preset carries a matrix -- about four and a half rows on average -- and most carry one or
two envelope shapes. Which targets a preset may use depends on what it actually switched on:
`z_x` only where the z-plane filter runs, `cloud_density` only where the cloud does.

Depths are read per target, because a depth is a fraction of the target's own range: 0.3 on a mix
is a third of it, 0.3 on the cutoff would be 5.4 kHz. The rates are drone rates, log-uniform
between one cycle in eight seconds and one in forty minutes, so the median route takes a bit over
two minutes to come round. That is deliberate and it means a short render will not show much:
the instrument is built for half-hour pieces.

The shade decides how much of it there is. *still* gets one to three slow rows, *astir* four to
seven at 2.6 times the rate.

## Metadata

Every pack line carries a map position, six descriptors (brightness, motion, width, noisiness,
weight, density) and tag bits, so the browser filters and the Absynth-style point map work on
the library the same way they work on the built-in presets.

Both halves of the library are measured, not described. `Tools/preset_map.py` renders the 191
built-in presets; `Tools/library/measure_packs.py` renders the generated ones and writes the
result back into the pack files. `make_presets.py` alone would only estimate the descriptors
from the settings, which is enough to lay a map out but is a prediction -- so a library that has
not been through the measurement pass says so in its own header line.

## Checking it

`Tools/library/verify_packs.py` is the cheap pass: it parses every line and fails on an unknown
parameter, a choice name the synth does not know, a value outside its range, a duplicate preset
name, a named sample that is not there, or two presets on the same spot of the map. A second
for six thousand presets.

```
python Tools/library/verify_packs.py
```

`Tools/preset_check.py` renders presets and fails the ones that are too loud, clip, click,
carry DC or come out silent. It takes the whole library:

```
python Tools/preset_check.py --packs Library/Packs --sample 300 --jobs 6
```
