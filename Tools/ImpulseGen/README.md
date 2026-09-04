# ImpulseGen — impulse responses for the Room

The Room is AmbientSynth's third reverb: a convolution reverb on the far plane
that plays an impulse response (BACKGROUND → Room). Without a file it uses a
built-in dark hall; this tool makes files for it. Same Python environment as
TextureGen:

```
..\TextureGen\.venv\Scripts\python impulsegen.py
```

## Four ways to an impulse

* **Design** — a synthetic room from numbers: RT60 per band (low below
  300 Hz, mid, high above 3 kHz — Rich's rooms keep the low end ringing
  longest), *Size* (spacing of the early reflections), *Pre-delay*, *Width*,
  *Tone*, *Modulation* (a slow wobble in the tail, less static). Eight room
  presets: dark cathedral, stone chapel, deep cave, hangar, dark chamber,
  glass room, infinite plate, night field.
* **Recording** — any WAV becomes an impulse: onset detection, trim, cut
  where the tail falls below the floor, fades; *extend* continues a tail that
  the recording cut off, with the recording's own level as the starting point.
* **Prompt** — a TextureGen model (Stable Audio Open works best) renders
  "a single sharp hand clap in <your room>", and the result is cut like a
  recording. Text-to-audio models do not know what an impulse response is,
  but they know what a clap in a cathedral sounds like, and that is one.
* **Hybrid** — the average spectrum of a recording colours noise, the
  designed per-band decay shapes it: the character of a place with a decay
  you choose.

The view shows the impulse and its energy decay curve; *Play chord through
it* convolves a short dry chord for a quick impression. Export writes
`Impulses/<name>.wav` (stereo float) plus a `.txt` with the numbers. The synth
normalises every impulse's energy, so rooms do not change the level.

## Command line

```
..\TextureGen\.venv\Scripts\python impulsegen_cli.py presets
..\TextureGen\.venv\Scripts\python impulsegen_cli.py procedural --name cavern --seconds 7 --rt60 7 4 1 --size 2 --tone -0.7 --seeds 3
..\TextureGen\.venv\Scripts\python impulsegen_cli.py audio ..\..\Textures\*.wav --extend
..\TextureGen\.venv\Scripts\python impulsegen_cli.py prompt --prompts rooms.txt --model sao --seconds 20 --extend
..\TextureGen\.venv\Scripts\python impulsegen_cli.py hybrid recording.wav --rt60 6 3 1
```

`ambient_render --ir file.wav` loads an impulse offline; the Quest app picks
up `impulse.wav` from its data folder.
