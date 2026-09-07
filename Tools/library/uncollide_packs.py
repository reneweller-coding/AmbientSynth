"""Move a new pack's dots off the ones the rest of the library already occupies.

Every preset is a dot on the browser's map, at a position measured from its own sound. Two presets
on the same dot are two dots drawn on top of each other, and one of them can never be clicked.

The generator already avoids that WITHIN a run, and measure_packs lays out a run's presets on a
grid so nothing inside it hides underneath anything else. Neither of them knows about the packs
that were written before, and that is exactly the gap this closes: adding two packs to a library of
thirty-two put eleven of their presets on a dot another pack was already sitting on -- invisible in
the browser, and nothing said so until verify_packs counted them.

Only the named packs are moved. Everything else keeps the position it was measured at, so a library
that has already been published does not shift under its users.

    python Tools/library/uncollide_packs.py Rosin-and-Wire Held-Moment
    python Tools/library/uncollide_packs.py --check
"""
import argparse
import collections
import glob
import io
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PACKS = os.path.join(ROOT, "Library", "Packs")


def rows_of(path):
    """(line index, name, x, y) for every preset line in a pack."""
    lines = io.open(path, encoding="utf-8", newline="").read().split("\n")
    out = []
    for i, line in enumerate(lines):
        if line.startswith("#") or "|" not in line:
            continue
        cols = line.split("|")
        if len(cols) < 3:
            continue
        meta = cols[2].split()
        if len(meta) < 2:
            continue
        try:
            out.append((i, cols[0], float(meta[0]), float(meta[1])))
        except ValueError:
            continue
    return lines, out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("packs", nargs="*", help="file name stems of the packs that may be moved")
    ap.add_argument("--dir", default=PACKS)
    ap.add_argument("--check", action="store_true", help="only count, move nothing")
    a = ap.parse_args(argv)

    files = sorted(glob.glob(os.path.join(a.dir, "*.ambientpack")))
    movable = {os.path.splitext(os.path.basename(f))[0] for f in files} if not a.packs else set(a.packs)

    taken = collections.Counter()
    for f in files:
        _, rr = rows_of(f)
        for _i, _n, x, y in rr:
            taken[(round(x, 3), round(y, 3))] += 1

    stacked = sum(n - 1 for n in taken.values() if n > 1)
    print("%d presets in %d packs, %d sharing a dot" % (sum(taken.values()), len(files), stacked))
    if a.check or not stacked:
        return 0 if not stacked else 1

    moved = 0
    for f in files:
        stem = os.path.splitext(os.path.basename(f))[0]
        if stem not in movable:
            continue
        lines, rr = rows_of(f)
        changed = False
        for i, name, x, y in rr:
            key = (round(x, 3), round(y, 3))
            if taken[key] <= 1:
                continue
            # The golden angle, so successive steps never land on one another, and a fixed
            # step size, so the dot stays where the sound put it to within a few thousandths.
            nx, ny = x, y
            for step in range(1, 128):
                ang = 2.39996 * step
                nx = min(1.0, max(0.0, x + 0.004 * step * math.cos(ang)))
                ny = min(1.0, max(0.0, y + 0.004 * step * math.sin(ang)))
                if taken[(round(nx, 3), round(ny, 3))] == 0:
                    break
            taken[key] -= 1
            taken[(round(nx, 3), round(ny, 3))] += 1
            cols = lines[i].split("|")
            meta = cols[2].split()
            meta[0], meta[1] = "%.3f" % nx, "%.3f" % ny
            cols[2] = " ".join(meta)
            lines[i] = "|".join(cols)
            changed = True
            moved += 1
            print("  %-28s %-32s %.3f %.3f -> %.3f %.3f" % (stem, name, x, y, nx, ny))
        if changed:
            io.open(f, "w", encoding="utf-8", newline="").write("\n".join(lines))
    print("moved %d presets" % moved)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
