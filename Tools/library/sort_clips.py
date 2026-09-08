"""Tidy up after a generation round: the right folder for every clip, one index instead of 7000
little files.

Two jobs, both of which have to happen after clips are generated and before presets are built.

*The folders.* TextureGen writes the detected pitch into the file name when it finds a stable one,
and that is exactly the property the preset generator needs: only a clip with a note may be
transposed to the note being played. So the note in the name is the dividing line, and the two
folders then mean what they say -- Textures is tonal, FieldRecordings is not -- rather than being
told apart by whichever prompt happened to make them.

*The index.* The worker drops a small `.txt` beside every clip with the model, the prompt, the seed
and the detected pitch. Nothing in the build reads them, they never ship (the content pack copies
only what a pack references), and after a few rounds there were 6900 of them -- so a folder of
clips could not be counted by looking at it. They are folded into one `clips.json` per folder and
removed. The provenance is kept, the folder shows clips.

    python Tools/library/sort_clips.py [--dry-run] [--no-fold]
"""
import argparse
import json
import os
import re
import shutil

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TEX = os.path.join(ROOT, "Library", "Textures")
FLD = os.path.join(ROOT, "Library", "FieldRecordings")
PITCHED = re.compile(r"_[A-G]#?-?[0-9]\.wav$")
INDEX = "clips.json"
KEEP = {"rejected.txt"}          # read by make_presets.py: which clips are duds


def move_pair(src, dst, name, dry):
    """The clip and the little file beside it travel together, or the index loses track of it."""
    for a, b in ((name, name), (os.path.splitext(name)[0] + ".txt",) * 2):
        s = os.path.join(src, a)
        if not os.path.exists(s):
            continue
        if dry:
            continue
        t = os.path.join(dst, b)
        if os.path.exists(t):
            os.remove(s)
        else:
            shutil.move(s, t)


def fold(folder, dry):
    """Every sidecar in this folder into its clips.json, then away. An entry whose clip has since
    moved to the other folder is handed over rather than dropped."""
    index = {}
    path = os.path.join(folder, INDEX)
    if os.path.exists(path):
        try:
            index = json.load(open(path, encoding="utf-8")).get("clips", {})
        except (ValueError, OSError):
            index = {}
    handed, folded, orphans = {}, 0, 0
    other = FLD if folder == TEX else TEX
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".txt") or name in KEEP:
            continue
        wav = os.path.splitext(name)[0] + ".wav"
        try:
            meta = json.load(open(os.path.join(folder, name), encoding="utf-8"))
        except (ValueError, OSError):
            meta = {"raw": open(os.path.join(folder, name), encoding="utf-8", errors="replace").read()}
        if os.path.exists(os.path.join(folder, wav)):
            index[wav] = meta
        elif os.path.exists(os.path.join(other, wav)):
            handed[wav] = meta
        else:
            orphans += 1                 # the clip was rejected or deleted; its note goes with it
        folded += 1
        if not dry:
            os.remove(os.path.join(folder, name))
    # Anything in the index whose clip is gone goes too, so the file counts what is there.
    index = {k: v for k, v in index.items() if os.path.exists(os.path.join(folder, k))}
    if not dry:
        with open(path, "w", encoding="utf-8") as f:
            json.dump({"note": "How each clip in this folder was made. Written by "
                               "Tools/library/sort_clips.py from the generator's own notes.",
                       "clips": index}, f, indent=0, sort_keys=True)
    return folded, orphans, handed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--no-fold", action="store_true", help="only sort the folders, leave the sidecars")
    a = ap.parse_args()
    os.makedirs(TEX, exist_ok=True)
    os.makedirs(FLD, exist_ok=True)
    moved = [0, 0]
    for src, dst, want in ((TEX, FLD, False), (FLD, TEX, True)):
        for name in sorted(os.listdir(src)):
            if not name.endswith(".wav"):
                continue
            if bool(PITCHED.search(name)) == want:
                continue
            moved[0 if src == TEX else 1] += 1
            move_pair(src, dst, name, a.dry_run)
    print("Textures -> FieldRecordings: %d (no detected pitch)" % moved[0])
    print("FieldRecordings -> Textures: %d (a pitch was detected)" % moved[1])

    if not a.no_fold:
        # Twice round: the first pass hands over the notes of clips that moved, the second files
        # them where their clip now lives.
        pending = {TEX: {}, FLD: {}}
        for folder in (TEX, FLD):
            folded, orphans, handed = fold(folder, a.dry_run)
            pending[FLD if folder == TEX else TEX].update(handed)
            print("%-32s %5d notes folded in, %d without a clip" % (os.path.basename(folder), folded, orphans))
        for folder, extra in pending.items():
            if not extra or a.dry_run:
                continue
            path = os.path.join(folder, INDEX)
            blob = json.load(open(path, encoding="utf-8"))
            blob["clips"].update({k: v for k, v in extra.items()
                                  if os.path.exists(os.path.join(folder, k))})
            json.dump(blob, open(path, "w", encoding="utf-8"), indent=0, sort_keys=True)
            print("%-32s %5d notes taken over from the other folder" % (os.path.basename(folder), len(extra)))

    for d in (TEX, FLD):
        files = os.listdir(d)
        n = len([f for f in files if f.endswith(".wav")])
        rest = len(files) - n
        print("%-32s %5d clips, %d other files" % (d, n, rest))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
