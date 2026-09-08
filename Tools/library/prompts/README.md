# The prompt lists

What the sample library is made of. Each line is one idea for a clip; the generator runs each
line a few times with different seeds and keeps what comes out.

| file | for | goes to |
|---|---|---|
| `tonal.txt` | material with a pitch in it: struck objects, bowed strings, voices, machines that hum | `Library/Textures` |
| `tonal-3.txt` | a second pass of the same kind, written against the first so nothing repeats | `Library/Textures` |
| `atonal.txt` | environments and noise: weather, rooms, places, machines that roar | `Library/FieldRecordings` |

The split matters to the synth, not just to the shelf. Only a clip with a detected pitch may be
transposed to the note being played, so the tonal granular voice draws from `Textures` and never
from `FieldRecordings`; a swamp pitched to a D is not granular synthesis, it is a mistake with a
fundamental. The generator writes the detected note into the file name and
`Tools/library/sort_clips.py` moves anything filed in the wrong folder, so the two folders end up
meaning what they say rather than recording which prompt list a clip came from.

About half of the tonal prompts produce a clip the pitch detector accepts. That is the reason the
lists are long and repeat nothing: more distinct ideas is the only honest way to more tonal
material, since loosening the detector would buy clips that are transposed to a note they do not
actually have.

Running one list:

    Tools/TextureGen/.venv/Scripts/python Tools/TextureGen/texturegen_worker.py \
        --batch Tools/library/prompts/tonal.txt --count 4 --out-dir Library/Textures \
        --resume --nice

`--resume` skips every clip already on disk, so a run can be stopped and picked up later.
`--nice` keeps the machine usable while it works.
