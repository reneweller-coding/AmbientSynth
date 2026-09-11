"""Tidy up after a generation round: the right folder for every clip, one index instead of 7000
little files.

Two jobs, both of which have to happen after clips are generated and before presets are built.

*The folders.* TextureGen writes the detected pitch into the file name when it finds a stable one,
and that is exactly the property the preset generator needs: only a clip with a note may be
transposed to the note being played. So the note in the name is the dividing line, and the two
folders then mean what they say -- Textures is tonal, FieldRecordings is not -- rather than being
told apart by whichever prompt happened to make them.

Since the library was measured (Tools/library/clip_tonality.py -> Library/tonality.json) the line
is drawn tighter: a clip stays in Textures only if it also measures tonal -- periodic enough, and
with its weight below 2.5 kHz. A referee's whistle at F6 has a note and is not what a tonal slot
should be built on; it goes to the beds. The rule is the same one make_presets.py applies when it
fills the slots, so the folder and the shelf agree. When clips move, their references in the packs
(../Textures/x.wav) move with them, so nothing needs re-measuring.

*The index.* The worker drops a small `.txt` beside every clip with the model, the prompt, the seed
and the detected pitch. Nothing in the build reads them, they never ship (the content pack copies
only what a pack references), and after a few rounds there were 6900 of them -- so a folder of
clips could not be counted by looking at it. They are folded into one `clips.json` per folder and
removed. The provenance is kept, the folder shows clips.

    python Tools/library/sort_clips.py [--dry-run] [--no-fold]
"""
import argparse
import glob
import json
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TEX = os.path.join(ROOT, "Library", "Textures")
FLD = os.path.join(ROOT, "Library", "FieldRecordings")
# Folded as well, but never sorted: nothing moves between these and anywhere else.
ALSO = [os.path.join(ROOT, "Library", d) for d in ("Wavetables", "Impulses")]
PITCHED = re.compile(r"_[A-G]#?-?[0-9]\.wav$")
INDEX = "clips.json"
# rejected.txt is read by make_presets.py (which clips are duds). CREDITS.txt is a licence notice --
# Library/Impulses holds the AIR database's MIT text -- and has no clip beside it, which is what
# fold() takes for an orphaned note and deletes.
KEEP = {"rejected.txt", "CREDITS.txt"}
PACKS = os.path.join(ROOT, "Library", "Packs")
TONALITY = os.path.join(ROOT, "Library", "tonality.json")

_MEASURE = {}


def measure_of(name):
    """The clip's tonality measurement by file name, whichever folder it was measured in."""
    if not _MEASURE:
        _MEASURE["_"] = {}
        if os.path.exists(TONALITY):
            try:
                for rel, m in json.load(open(TONALITY, encoding="utf-8")).get("clips", {}).items():
                    _MEASURE["_"][os.path.basename(rel)] = m
            except (ValueError, OSError):
                pass
    return _MEASURE["_"].get(name)


def belongs_in_textures(name):
    """A note in the name, and -- where a measurement exists -- tonal by measure. The thresholds
    are make_presets.py's, imported so the two cannot drift apart."""
    if not PITCHED.search(name):
        return False
    m = measure_of(name)
    if m is None:
        return True
    from make_presets import TONAL_HARM_MIN, TONAL_CENTROID_MAX
    return m.get("harm", 1.0) >= TONAL_HARM_MIN and m.get("centroid", 0.0) <= TONAL_CENTROID_MAX


def rewrite_references(moved, dry):
    """Every pack line's texture column names its clips as ../Folder/name.wav; a clip that moved
    is renamed there. tonality.json and affinity.json carry Folder/name.wav keys and move too."""
    changed_lines = 0
    for pack in sorted(glob.glob(os.path.join(PACKS, "*.ambientpack"))):
        out, touched = [], False
        with open(pack, encoding="utf-8") as f:
            for line in f:
                if line.startswith("#") or line.startswith("pack ") or line.count("|") < 3:
                    out.append(line)
                    continue
                cols = line.rstrip("\n").split("|")
                parts = cols[3].split(";")
                new = []
                for part in parts:
                    base = os.path.basename(part)
                    if base in moved:
                        part = "../" + os.path.basename(moved[base]) + "/" + base
                    new.append(part)
                if new != parts:
                    cols[3] = ";".join(new)
                    touched = True
                    changed_lines += 1
                out.append("|".join(cols) + "\n")
        if touched and not dry:
            with open(pack, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(out)
    for path in (TONALITY, os.path.join(ROOT, "Library", "affinity.json")):
        if not os.path.exists(path):
            continue
        text = open(path, encoding="utf-8").read()
        for base, dst in moved.items():
            src_folder = "FieldRecordings" if os.path.basename(dst) == "Textures" else "Textures"
            text = text.replace(f'"{src_folder}/{base}"', f'"{os.path.basename(dst)}/{base}"')
        if not dry:
            open(path, "w", encoding="utf-8").write(text)
    return changed_lines


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
    # Only the two clip folders exchange anything; Wavetables and Impulses have no partner.
    other = FLD if folder == TEX else (TEX if folder == FLD else None)
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".txt") or name in KEEP:
            continue
        wav = os.path.splitext(name)[0] + ".wav"
        # The file can be gone by now (a generator writing into the same folder, a scanner) --
        # the listing was taken a moment ago. A note that is not there is not worth an abort.
        try:
            meta = json.load(open(os.path.join(folder, name), encoding="utf-8"))
        except ValueError:
            try:
                meta = {"raw": open(os.path.join(folder, name), encoding="utf-8", errors="replace").read()}
            except OSError:
                continue
        except OSError:
            continue
        if os.path.exists(os.path.join(folder, wav)):
            index[wav] = meta
        elif other is not None and os.path.exists(os.path.join(other, wav)):
            handed[wav] = meta
        else:
            orphans += 1                 # the clip was rejected or deleted; its note goes with it
        folded += 1
        if not dry:
            try:
                os.remove(os.path.join(folder, name))
            except OSError:
                pass
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
    # `keep` is what a clip in this folder must be: Textures keeps the ones with a note in the
    # name, FieldRecordings the ones without. This pair was the wrong way round once, and because
    # a wrong pair swaps the two folders on every run rather than failing, it stayed unnoticed
    # until a count came back as "3325 clips, 0 with a note". Hence the check at the end.
    moved = [0, 0]
    moved_to = {}                  # name -> the folder it now lives in
    for src, dst, keep in ((TEX, FLD, True), (FLD, TEX, False)):
        for name in sorted(os.listdir(src)):
            if not name.endswith(".wav"):
                continue
            if belongs_in_textures(name) == keep:
                continue
            moved[0 if src == TEX else 1] += 1
            moved_to[name] = dst
            move_pair(src, dst, name, a.dry_run)
    print("Textures -> FieldRecordings: %d (no detected pitch, or not tonal by measure)" % moved[0])
    print("FieldRecordings -> Textures: %d (a pitch was detected and it measures tonal)" % moved[1])
    if moved_to:
        n = rewrite_references(moved_to, a.dry_run)
        print("pack lines whose clip references follow the move: %d%s" % (n, " (dry run)" if a.dry_run else ""))
        # The index entries travel too: fold() below only knows about sidecars.
        if not a.dry_run:
            idx = {}
            for folder in (TEX, FLD):
                path = os.path.join(folder, INDEX)
                try:
                    idx[folder] = json.load(open(path, encoding="utf-8"))
                except (ValueError, OSError):
                    idx[folder] = {"clips": {}}
            for name, dst in moved_to.items():
                src = FLD if dst == TEX else TEX
                meta = idx[src].get("clips", {}).pop(name, None)
                if meta is not None:
                    idx[dst].setdefault("clips", {})[name] = meta
            for folder, blob in idx.items():
                json.dump(blob, open(os.path.join(folder, INDEX), "w", encoding="utf-8"), indent=0, sort_keys=True)

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

        for folder in ALSO:
            if not os.path.isdir(folder):
                continue
            folded, orphans, _ = fold(folder, a.dry_run)
            print("%-32s %5d notes folded in, %d without a clip" % (os.path.basename(folder), folded, orphans))

    for d in [TEX, FLD] + [x for x in ALSO if os.path.isdir(x)]:
        files = os.listdir(d)
        n = len([f for f in files if f.endswith(".wav")])
        rest = len(files) - n
        print("%-32s %5d clips, %d other files" % (d, n, rest))

    # What the two folders MEAN, checked rather than assumed. Everything downstream rests on it:
    # only a clip with a note may be transposed to the note being played.
    wrong = 0
    for folder, want in ((TEX, True), (FLD, False)):
        bad = [f for f in os.listdir(folder) if f.endswith(".wav") and belongs_in_textures(f) != want]
        if bad:
            wrong += len(bad)
            print("WRONG: %d clips in %s are on the wrong shelf, e.g. %s"
                  % (len(bad), os.path.basename(folder), bad[0]))
    if wrong and not a.dry_run:
        return 1
    print("check: Textures is tonal, FieldRecordings is not" if not wrong else "check FAILED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
