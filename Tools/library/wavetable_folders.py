"""The three shelves of Library/Wavetables, and which one a table belongs on.

Until 11.09.2026 the 2191 tables lay flat in one folder, and the file dialog of a Wavetable slot
showed 551 tables built for the Harmonic type between the classic ones. They are sorted by what
they are for now:

  Harmonic/   harmonic_*           HarmonicGen: 64 frames of partial spectra, for the Harmonic type
  Classic/    akwf_*, wavedit_*    AKWF and WaveEdit Online (CC0): waveforms for the Wavetable type,
                                   with CREDITS-classic.md
  Ambient/    ambient_*            AmbientGen: 128 partials with fixed phases, for either type

Shelves inside Wavetables rather than a fifth library folder: the ambient tables belong to both
types, and the installer, the content archives and make_content_pack know exactly four kinds of
content by their folder names. A pack names a table as ../Wavetables/<shelf>/<name>.wav, which the
engine resolves against the pack's folder like any other relative path.

    python Tools/library/wavetable_folders.py [--root Library/Wavetables] [--apply]

sorts the tables lying loose in the folder onto their shelves, each with the .json beside it;
without --apply it only says what it would move. The library tools take tables() and shelf_for()
from here.
"""
import argparse
import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WAVETABLES = os.path.join(ROOT, "Library", "Wavetables")
SHELVES = (("Harmonic", ("harmonic_",)), ("Classic", ("akwf_", "wavedit_")), ("Ambient", ("ambient_",)))
TYPES = {"Harmonic": ("Harmonic",), "Classic": ("Wavetable",), "Ambient": ("Harmonic", "Wavetable")}
BESIDE = {"CREDITS-classic.md": "Classic"}      # files that belong to a shelf without a table's name
TABLE_EXTENSIONS = (".wav", ".flac", ".wt")


def shelf_for(name):
    """The shelf a table's file name puts it on, or None for a name no rule knows."""
    base = os.path.basename(name)
    if base in BESIDE:
        return BESIDE[base]
    for shelf, prefixes in SHELVES:
        if base.lower().startswith(prefixes):
            return shelf
    return None


def types_for(path):
    """The source types a table is made for: from the shelf it is on, or the one its name puts it on."""
    parts = path.replace("\\", "/").split("/")
    shelf = parts[-2] if len(parts) >= 2 and parts[-2] in TYPES else shelf_for(parts[-1])
    return TYPES.get(shelf, ("Harmonic", "Wavetable"))


def tables(root=WAVETABLES, shelves=None, extensions=(".wav",)):
    """Every table under root, as a path relative to it with '/' separators
    ("Harmonic/harmonic_organ_003.wav"), sorted. A table still lying loose in root comes back as its
    bare name; `shelves` keeps only the tables on those shelves."""
    out = []
    if not os.path.isdir(root):
        return out
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        rel = os.path.relpath(dirpath, root)
        for f in filenames:
            if not f.lower().endswith(extensions):
                continue
            path = f if rel == "." else os.path.join(rel, f).replace(os.sep, "/")
            if shelves is None or path.split("/")[0] in shelves:
                out.append(path)
    return sorted(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=WAVETABLES)
    ap.add_argument("--apply", action="store_true", help="move the files; without it, only say what would move")
    a = ap.parse_args()
    if not os.path.isdir(a.root):
        sys.exit(f"no folder {a.root}")
    moves, unknown, clashes = [], [], []
    for name in sorted(os.listdir(a.root)):
        src = os.path.join(a.root, name)
        if not os.path.isfile(src):
            continue
        if name not in BESIDE and not name.lower().endswith(TABLE_EXTENSIONS + (".json", ".txt")):
            continue
        shelf = shelf_for(name)
        if shelf is None:
            unknown.append(name)
            continue
        dst = os.path.join(a.root, shelf, name)
        if os.path.exists(dst):
            clashes.append(name)
            continue
        moves.append((src, dst, shelf))
    per_shelf = collections.Counter(s for src, _, s in moves if src.lower().endswith(TABLE_EXTENSIONS))
    print(f"{sum(per_shelf.values())} tables to move ({len(moves)} files with their .json): "
          + ", ".join(f"{s} {per_shelf[s]}" for s, _ in SHELVES))
    for label, names in (("no shelf for", unknown), ("already on its shelf, left where it is:", clashes)):
        if names:
            print(f"  {label} {len(names)}: {', '.join(names[:8])}{' ...' if len(names) > 8 else ''}")
    if a.apply:
        for shelf, _ in SHELVES:
            os.makedirs(os.path.join(a.root, shelf), exist_ok=True)
        for src, dst, _ in moves:
            os.replace(src, dst)
        print("moved")
    for shelf, _ in SHELVES:
        print(f"  {shelf:9s} {len(tables(a.root, (shelf,))):5d} tables")
    loose = [n for n in tables(a.root) if "/" not in n]
    print(f"  loose     {len(loose):5d} tables")
    return 1 if unknown or clashes else 0


if __name__ == "__main__":
    sys.exit(main())
