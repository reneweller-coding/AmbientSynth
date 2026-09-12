"""Put the played note above the drone that is not played.

The Sub is a fundament: it follows the conductor's root, not the note a voice is playing
(`subHz = rootHz / 2` or `/ 4` in EngineRender). That is the design and it is right -- a pedal
under the cluster. What was wrong is how loud it stood against the material that does follow the
note. Measured over the library before this ran:

    primary slot   presets   slot level (median)   sub level (median)   sub within 6 dB of the slot
    Texture           2223           0.27                 0.20                  55 %
    Wavetable         2445           0.35                 0.21                  48 %
    Additive          2099           1.00                 0.20                   4 %

So in half the sample-based presets the one thing that does not change with the note was as loud
as, or louder than, everything that does. Play a chord and you hear the pedal; play a different
chord and you hear the same pedal. That is what "only the additive presets sound tonal" means, and
the additive ones were fine only because their slot happens to sit at 1.0.

This walks the packs and gives the pitched slot a margin over the sub. It raises the slot first,
as far as 1.0, because that leaves the sub its weight and only changes the balance; the sub is
lowered only for what the slot alone cannot cover. Nothing else is touched -- not the effects, not
the envelopes, not which clip is loaded. Presets whose primary source does not follow the note
(a Free texture, a field recording) are left alone: there the pedal IS the harmony.

    python Tools/library/rebalance_sub.py --dry-run
    python Tools/library/rebalance_sub.py --ratio 2.0

Afterwards the library has to be measured again: the balance is part of every descriptor the map
is laid out from, and of the per-preset loudness matching.
"""
import argparse, glob, math, os, statistics, sys

PITCHED_ALWAYS = {"Additive", "Wavetable", "FM", "Bow"}
FOLLOWS_NOTE = {"Texture", "Stretch", "Spectral"}
# Slot 1's level is the Oscillator level; the other three have their own.
SLOT_LEVEL = {"1": "osc_level", "2": "src2_level", "3": "src3_level", "4": "src4_level"}


def parse_settings(text):
    """Order matters: a pack line is read left to right, so the pairs are kept as a list."""
    out = []
    for tok in text.split(";"):
        if "=" in tok:
            k, _, v = tok.partition("=")
            out.append([k, v])
        elif tok:
            out.append([tok, None])
    return out


def write_settings(pairs):
    return ";".join(k if v is None else f"{k}={v}" for k, v in pairs)


def get(pairs, key, default):
    for k, v in pairs:
        if k == key:
            try:
                return float(v)
            except (TypeError, ValueError):
                return default
    return default


def get_str(pairs, key, default=""):
    for k, v in pairs:
        if k == key:
            return v if v is not None else default
    return default


def put(pairs, key, value):
    """Replace in place if present, else append -- so a preset that never named the sub keeps
    not naming it, and one that did keeps its position in the line."""
    text = f"{value:.4g}"
    for pair in pairs:
        if pair[0] == key:
            pair[1] = text
            return
    pairs.append([key, text])


def primary_slot(pairs):
    """The loudest slot that follows the note, or None. Slot 1 is the usual one, but a preset
    whose first slot is a Free bed and whose second plays the tune is placed by the second."""
    best, best_level = None, 0.0
    for s in "1234":
        t = get_str(pairs, f"src{s}_type", "Off" if s != "1" else "Additive")
        if t in ("Off", ""):
            continue
        follows = t in PITCHED_ALWAYS or (t in FOLLOWS_NOTE and get_str(pairs, f"src{s}_follow", "Free") == "Note")
        if not follows:
            continue
        level = get(pairs, SLOT_LEVEL[s], 1.0 if s == "1" else 0.0)
        if level > best_level:
            best, best_level = s, level
    return best


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", default="Library/Packs")
    ap.add_argument("--ratio", type=float, default=2.0,
                    help="how far the played material sits above the sub (2.0 = 6 dB)")
    ap.add_argument("--max-level", type=float, default=1.0, help="how far a slot may be raised")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    files = sorted(glob.glob(os.path.join(a.packs, "*.ambientpack")))
    if not files:
        sys.exit(f"no packs in {a.packs}")

    touched = raised = lowered = skipped = total = 0
    gain_db, cut_db = [], []
    for path in files:
        lines = open(path, encoding="utf-8").read().split("\n")
        out = []
        changed = False
        for line in lines:
            if line.startswith("#") or "|" not in line:
                out.append(line)
                continue
            fields = line.split("|")
            pairs = parse_settings(fields[1])
            total += 1
            slot = primary_slot(pairs)
            sub = get(pairs, "sub_level", 0.0)
            if slot is None or sub <= 0.0:
                skipped += 1
                out.append(line)
                continue
            key = SLOT_LEVEL[slot]
            level = get(pairs, key, 1.0 if slot == "1" else 0.0)
            want = a.ratio * sub
            if level >= want:
                out.append(line)
                continue
            # Raise the slot as far as it may go, then take the rest off the sub.
            new_level = min(a.max_level, want)
            if new_level > level:
                gain_db.append(20.0 * math.log10(new_level / max(level, 1e-6)))
                put(pairs, key, new_level)
                raised += 1
                level = new_level
            new_sub = min(sub, level / a.ratio)
            if new_sub < sub - 1e-6:
                cut_db.append(20.0 * math.log10(max(new_sub, 1e-6) / sub))
                put(pairs, "sub_level", new_sub)
                lowered += 1
            fields[1] = write_settings(pairs)
            out.append("|".join(fields))
            touched += 1
            changed = True
        if changed and not a.dry_run:
            open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out))

    print(f"presets read          {total}")
    print(f"  left alone          {skipped} (no slot follows the note, or no sub)")
    print(f"  already in balance  {total - skipped - touched}")
    print(f"  changed             {touched}")
    print(f"     slot raised      {raised}" + (f", median +{statistics.median(gain_db):.1f} dB" if gain_db else ""))
    print(f"     sub lowered      {lowered}" + (f", median {statistics.median(cut_db):.1f} dB" if cut_db else ""))
    if a.dry_run:
        print("\n(dry run -- nothing written)")
    else:
        print("\nThe library has to be measured again: this changes loudness and descriptors.")



if __name__ == "__main__":
    main()
