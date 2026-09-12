"""Let the Foundation follow the chord in every pack preset that has one.

Measured over 1306 pack presets with `ambient_render --tonal`, which plays the same two-note chord
a tritone apart and asks how much of the sound moved with it:

    built-in presets (the yardstick)   median 16 % did not move
    pack presets, Foundation off       median 28 %
    pack presets, Foundation on Root   median 77 %

The Foundation stands on the conductor's root, which moves about once a quarter of an hour, so in
three quarters of the library the loudest thing in the sound was the one thing that did not answer
the keyboard. Play a chord, play another, hear the same drone: what a listener calls "no tonal
movement at all", and why only the additive presets -- whose own slot is loud enough to be heard
over it -- sounded tonal.

`sub_source=Lowest` puts it under the lowest sounding voice instead, folded by octaves into the
register it was already in, so the pitch class follows the chord and the weight stays where it
was. Measured on two of them, everything else untouched:

    Interior Bloom   47 % -> 5 %
    Sphere Signal    85 % -> 9 %

which is where the built-ins already were. No level is changed: the bass is exactly as loud as it
was, it simply belongs to the music now.

    python Tools/library/sub_follows_chord.py --dry-run
    python Tools/library/sub_follows_chord.py
"""
import argparse, glob, os, sys


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", default="Library/Packs")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    files = sorted(glob.glob(os.path.join(a.packs, "*.ambientpack")))
    if not files:
        sys.exit(f"no packs in {a.packs}")

    total = changed = no_sub = already = 0
    was = {}
    for path in files:
        lines = open(path, encoding="utf-8").read().split("\n")
        out, dirty = [], False
        for line in lines:
            if line.startswith("#") or "|" not in line:
                out.append(line)
                continue
            fields = line.split("|")
            pairs = [tok.partition("=") for tok in fields[1].split(";") if tok]
            total += 1
            level = 0.0
            for k, _, v in pairs:
                if k == "sub_level":
                    try:
                        level = float(v)
                    except ValueError:
                        level = 0.0
            if level <= 0.0:
                no_sub += 1
                out.append(line)
                continue
            before = next((v for k, _, v in pairs if k == "sub_source"), "Root")
            if before == "Lowest":
                already += 1
                out.append(line)
                continue
            was[before] = was.get(before, 0) + 1
            # In place if it is named, otherwise right after the level it belongs to, so the line
            # keeps reading as one section rather than growing a tail.
            rebuilt, placed = [], False
            for k, eq, v in pairs:
                if k == "sub_source":
                    rebuilt.append("sub_source=Lowest")
                    placed = True
                else:
                    rebuilt.append(k if not eq else f"{k}={v}")
                    if k == "sub_level" and not placed:
                        rebuilt.append("sub_source=Lowest")
                        placed = True
            fields[1] = ";".join(rebuilt)
            out.append("|".join(fields))
            changed += 1
            dirty = True
        if dirty and not a.dry_run:
            open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out))

    print(f"presets read        {total}")
    print(f"  no Foundation     {no_sub}")
    print(f"  already Lowest    {already}")
    print(f"  changed           {changed}   (from " + ", ".join(f"{k}: {v}" for k, v in sorted(was.items())) + ")")
    print("\n(dry run -- nothing written)" if a.dry_run
          else "\nMeasure the library again: the balance is part of every descriptor and of the loudness matching.")



if __name__ == "__main__":
    main()
