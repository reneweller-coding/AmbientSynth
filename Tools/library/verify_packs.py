"""Structural check of a pack library: does every line parse, and does it mean what it says.

The audio side is Tools/preset_check.py's job. This is the cheap pass that runs in a second and
catches the things a generator gets wrong in bulk: an unknown parameter key, a choice name the
synth does not know, a value outside its range, a duplicate preset name, a sample that is named
but not there, metadata outside 0..1, two presets sitting on the same spot of the map.

    python Tools/library/verify_packs.py --packs Library/Packs

Exit code 1 if anything failed.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RENDER = os.path.join(ROOT, "build", "Tools", "render", "Release", "ambient_render.exe")

# Choice values the table cannot tell us, keyed by parameter.
CHOICES = {
    "stack": ["Detune", "Octaves", "Fifths", "Major", "Minor", "Seventh", "Harmonics", "Subharmonics"],
    "src2_type": ["Off", "Wavetable", "FM", "Texture"], "src3_type": ["Off", "Wavetable", "FM", "Texture"],
    "src2_table": ["Classic", "Organ", "Vocal", "Glass", "Metal", "User"],
    "src3_table": ["Classic", "Organ", "Vocal", "Glass", "Metal", "User"],
    "src2_ratio": ["1/1", "9/8", "6/5", "5/4", "4/3", "3/2", "8/5", "5/3", "7/4", "2/1"],
    "src3_ratio": ["1/1", "9/8", "6/5", "5/4", "4/3", "3/2", "8/5", "5/3", "7/4", "2/1"],
    "src2_follow": ["Free", "Note"], "src3_follow": ["Free", "Note"],
    "z_mode": ["Off", "Series", "Replace"],
    "z_shape": ["Vowel Morph", "Choir", "Nasal", "Low Sweep", "High Sweep", "Band Sweep", "Phaser",
                "Comb", "Flanger", "Notch Cluster", "Strings", "Metal Bars", "Wood", "Glass",
                "Peaks", "Infinite"],
    "sub_octave": ["-1", "-2"], "sub_source": ["Root", "Difference"], "room_source": ["Far", "Near"],
    "air_mode": ["Band", "Ghost"], "keymap": ["Snap to 12 keys", "Consecutive degrees"],
    "cosmos_shimmer_pitch": ["+12", "+7", "+5", "+19", "-12", "+24"],
    "root": ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"],
    "scale": ["12-TET", "JI Major (Ptolemy)", "JI Minor", "JI 7-limit", "Pythagorean", "JI Pentatonic",
              "Harmonic 8-16", "Subharmonic 16-8", "Slendro (JI)", "Bohlen-Pierce (JI)",
              "Otonality 1-11", "User (Scala)"],
}
BOOLS = {"on", "off", "true", "false", "yes", "no"}
PERFORMANCE = ("morph", "macro_", "map_", "route_", "inertia")


def param_table():
    out = subprocess.run([RENDER, "--list"], capture_output=True, text=True, encoding="utf-8").stdout
    table = {}
    for line in out.splitlines():
        m = re.match(r"^(\S+)\s+(\S+(?: \S+)*?)\s+\[(-?[\d.e+-]+) \.\. (-?[\d.e+-]+)\] default (-?[\d.e+-]+)", line)
        if m:
            table[m.group(1)] = (float(m.group(3)), float(m.group(4)))
    if not table:
        raise SystemExit(f"build ambient_render first ({RENDER})")
    return table


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", default=os.path.join(ROOT, "Library", "Packs"))
    a = ap.parse_args()
    params = param_table()
    problems = []
    names = collections.Counter()
    cells = collections.Counter()
    total = 0
    files = sorted(f for f in os.listdir(a.packs) if f.endswith(".ambientpack"))
    if not files:
        raise SystemExit(f"no packs in {a.packs}")
    for fname in files:
        path = os.path.join(a.packs, fname)
        for ln, line in enumerate(open(path, encoding="utf-8"), 1):
            t = line.strip()
            if not t or t.startswith("#") or t.startswith("pack "):
                continue
            total += 1
            where = f"{fname}:{ln}"
            f = t.split("|")
            if len(f) < 2:
                problems.append(f"{where}: no settings field")
                continue
            name, settings = f[0].strip(), f[1].strip()
            if not name:
                problems.append(f"{where}: empty name")
            names[name] += 1
            for kv in settings.split(";"):
                if not kv:
                    continue
                if "=" not in kv:
                    problems.append(f"{where}: '{kv}' is not key=value")
                    continue
                k, v = kv.split("=", 1)
                if k not in params:
                    problems.append(f"{where}: unknown parameter '{k}'")
                    continue
                if any(k.startswith(p) for p in PERFORMANCE):
                    problems.append(f"{where}: '{k}' is performance state and must not be in a preset")
                if k in CHOICES:
                    if v not in CHOICES[k]:
                        problems.append(f"{where}: '{v}' is not a value of '{k}'")
                    continue
                if v in BOOLS:
                    continue
                try:
                    x = float(v)
                except ValueError:
                    problems.append(f"{where}: '{k}={v}' is not a number")
                    continue
                lo, hi = params[k]
                if x < lo - 1e-6 or x > hi + 1e-6:
                    problems.append(f"{where}: {k}={x} outside [{lo}, {hi}]")
            if len(f) > 2 and f[2].strip():
                m = f[2].split()
                if len(m) != 9:
                    problems.append(f"{where}: metadata has {len(m)} fields, expected 9")
                else:
                    try:
                        vals = [float(x) for x in m[:8]]
                        int(m[8], 0)
                    except ValueError:
                        problems.append(f"{where}: metadata is not numeric")
                    else:
                        if any(v < -1e-6 or v > 1 + 1e-6 for v in vals):
                            problems.append(f"{where}: metadata outside 0..1")
                        cells[(round(vals[0], 3), round(vals[1], 3))] += 1
            for which, field in (("texture", 3), ("wavetable", 4)):
                if len(f) > field and f[field].strip():
                    ref = os.path.normpath(os.path.join(a.packs, f[field].strip()))
                    if not os.path.isfile(ref):
                        problems.append(f"{where}: {which} not found: {f[field].strip()}")
    for name, n in names.items():
        if n > 1:
            problems.append(f"duplicate preset name '{name}' ({n} times)")
    stacked = sum(n - 1 for n in cells.values() if n > 1)
    if stacked:
        problems.append(f"{stacked} presets share a map position with another")
    print(f"{total} presets in {len(files)} packs")
    for p in problems[:40]:
        print(f"  {p}")
    if len(problems) > 40:
        print(f"  ... and {len(problems) - 40} more")
    print("verify: ok" if not problems else f"verify: {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
