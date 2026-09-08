"""Tonal material in Textures, environments in FieldRecordings -- by what the clip actually is.

TextureGen writes the detected pitch into the file name when it finds a stable one, and that is
exactly the property the preset generator needs: only a clip with a note may be transposed to the
note being played. So the note in the name is the dividing line, and the two folders then mean
what they say -- rather than being told apart by whichever prompt happened to make them.

    python Tools/library/sort_clips.py [--dry-run]
"""
import argparse
import os
import re
import shutil

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TEX = os.path.join(ROOT, "Library", "Textures")
FLD = os.path.join(ROOT, "Library", "FieldRecordings")
PITCHED = re.compile(r"_[A-G]#?-?[0-9]\.wav$")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true")
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
            if not a.dry_run:
                target = os.path.join(dst, name)
                if os.path.exists(target):
                    os.remove(os.path.join(src, name))
                else:
                    shutil.move(os.path.join(src, name), target)
    print("Textures -> FieldRecordings: %d (no detected pitch)" % moved[0])
    print("FieldRecordings -> Textures: %d (a pitch was detected)" % moved[1])
    for d in (TEX, FLD):
        n = len([f for f in os.listdir(d) if f.endswith(".wav")])
        print("%-32s %5d clips" % (d, n))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
