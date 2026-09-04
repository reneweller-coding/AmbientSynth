# WavetableGen — wavetables for the User table

A PySide6 program that builds wavetables (frames of 2048 samples, the
Serum/Vital layout) for AmbientSynth's Wavetable source (*Table = User*) and
for any other wavetable synth. Same Python environment as TextureGen:

```
..\TextureGen\.venv\Scripts\python wavetablegen.py
```

## Three sources

* **Audio** — slices single cycles out of any WAV: pitch-tracked (or a pitch
  you set), each frame the average of a few consecutive cycles (steadier than
  one), frames spread evenly across the selection, phase-aligned to their
  neighbours so morphing through the table does not click. A TextureGen
  drone, a recorded note, a bowed glass: anything with a pitch.
* **Prompt** — asks a TextureGen model (Stable Audio Open, MusicGen) for a
  sustained note and slices the result. "a single sustained note of a bowed
  glass harmonica, steady pitch" works; rhythmic or noisy prompts do not.
* **Procedural** — spectral recipes over the table position: saw → square,
  tilt walk, formant sweep, comb, glass thinning, odd breathing, random walk;
  *Random walk* adds a smooth per-partial wander, *Phase scatter* moves from
  the classic in-phase waveform look to random phases (smoother, wider).
  *Morph* cross-fades the current table into a recipe.

The view shows the selected frame (click or drag across) and all frames as a
spectral image; *Play* sweeps through the table at a chosen note. *Export*
writes `Wavetables/<name>.wav` plus a `.txt` with the settings. In the synth:
Source 2/3 → Type Wavetable → Table User → *Wavetable...* → the file;
`ambient_render --wavetable file.wav` and `wavetable.wav` on the Quest do the
same.

## Why not a learned latent space?

Models like WaveSpace learn a latent space of single cycles and let you walk
through it. AmbientSynth's tables are spectra, so a table is already a path
through a spectral space, and the three sources above cover that path from
real sound (audio), from language (prompt) and from rules (procedural) without
a training set, a checkpoint or a GPU. A learned model can be added as a
fourth source later if a public checkpoint turns out to be worth it.
