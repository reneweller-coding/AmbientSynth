"""The measured descriptors in absolute terms, packs against the built-ins.

The pack files carry ranks (0..1 across the library), and a rank cannot say whether the library
got more tonal: half of it is always above the median. The measurement caches hold the raw
numbers -- spectral flatness, centroid, roughness, RMS -- and the 256 built-ins, measured the same
way, are a yardstick that does not move when the packs do.

    python Tools/library/absolute_census.py <work dir with packs.json and builtins.json>
"""
import glob
import io
import json
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
work = sys.argv[1]
packs = json.load(open(os.path.join(work, "packs.json"), encoding="utf-8"))
built = json.load(open(os.path.join(work, "builtins.json"), encoding="utf-8"))
if isinstance(built, dict) and "presets" in built:
    built = built["presets"]
if isinstance(built, list):
    built = {b.get("name", str(i)): (b.get("m") or b.get("measure") or b) for i, b in enumerate(built)}

# which preset is which: primary type and what its slots carry, from the pack files
info = {}
for pack in glob.glob(os.path.join(ROOT, "Library", "Packs", "*.ambientpack")):
    for line in io.open(pack, encoding="utf-8"):
        if line.startswith("#") or line.startswith("pack ") or line.count("|") < 3:
            continue
        cols = line.rstrip("\n").split("|")
        kv = dict(t.split("=", 1) for t in cols[1].split(";") if "=" in t and ">" not in t)
        folders = set()
        for part in cols[3].split(";"):
            if "Textures/" in part:
                folders.add("T")
            elif "FieldRecordings/" in part:
                folders.add("F")
        info[cols[0]] = (kv.get("src1_type", "Additive"), "".join(sorted(folders)) or "-",
                         kv.get("z_mode", "off") if "z_shape" in kv else "off",
                         float(kv.get("src1_grain", "0")) if kv.get("src1_type") == "Texture" else 0.0)

KEYS = ["flatness", "centroid", "rough", "rms_db", "width"]


def table(label, rows):
    if not rows:
        return
    out = [f"  {label:28s} {len(rows):5d}"]
    for k in KEYS:
        v = np.array([r[k] for r in rows if k in r and r[k] is not None and np.isfinite(r[k])])
        if len(v) == 0:
            out.append(f"  {k}: -")
            continue
        out.append(f"  {k} p50 {np.median(v):8.3f} p90 {np.percentile(v, 90):8.3f}")
    print("".join(out))


def first(d):
    return d.get("m") or d.get("measure") or d


packrows = {n: first(m) for n, m in packs.items() if first(m)}
print(f"{len(packrows)} pack presets, {len(built)} built-ins")
print("  group                            n  " + "  ".join(f"{k:>22s}" for k in KEYS))
table("BUILT-IN (yardstick)", [first(m) for m in built.values() if first(m)])
table("all packs", list(packrows.values()))
for prim in ("Additive", "Wavetable", "Texture", "Stretch", "Spectral", "FM", "Noise", "Bow"):
    table(f"primary {prim}", [m for n, m in packrows.items() if n in info and info[n][0] == prim])
for fold, label in (("-", "no sample"), ("T", "Textures only"), ("F", "FieldRecordings only"), ("FT", "both")):
    table(f"clips: {label}", [m for n, m in packrows.items() if n in info and info[n][1] == fold])
for z in ("off", "Replace", "Series", "Modal"):
    table(f"z-plane {z}", [m for n, m in packrows.items() if n in info and info[n][2] == z])
for lo, hi in ((0.0, 120.0), (120.0, 300.0), (300.0, 900.0)):
    table(f"Texture grain {lo:.0f}-{hi:.0f} ms", [m for n, m in packrows.items()
                                                  if n in info and info[n][0] == "Texture" and lo <= info[n][3] < hi])
