"""Which settings go with a harsh preset? Measured, not guessed.

Takes the raw measurements (flatness = how noise-like the spectrum is, centroid = where its weight
sits) from the measurement cache and the settings from the packs, and for every switch a preset
can have -- a module on, a source type, a z-plane mode or shape, a wavetable recipe, a noise
colour -- prints the median flatness of the presets that have it against the ones that do not.
The built-ins are the yardstick for what "not harsh" measures as.

    python Tools/library/harsh_census.py <work dir with packs.json and builtins.json>
"""
import collections
import glob
import io
import json
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
work = sys.argv[1]
MIN_GROUP = 80


def first(d):
    return d.get("m") or d.get("measure") or d


packs = {n: first(m) for n, m in json.load(open(os.path.join(work, "packs.json"), encoding="utf-8")).items() if first(m)}
built = json.load(open(os.path.join(work, "builtins.json"), encoding="utf-8"))
if isinstance(built, dict) and "presets" in built:
    built = built["presets"]
bvals = [first(b)["flatness"] for b in (built.values() if isinstance(built, dict) else built) if first(b) and "flatness" in first(b)]
print(f"built-ins: flatness p50 {np.median(bvals):.4f}  p90 {np.percentile(bvals, 90):.4f}")

rows = []
for pack in glob.glob(os.path.join(ROOT, "Library", "Packs", "*.ambientpack")):
    for line in io.open(pack, encoding="utf-8"):
        if line.startswith("#") or line.startswith("pack ") or line.count("|") < 3:
            continue
        cols = line.rstrip("\n").split("|")
        m = packs.get(cols[0])
        if not m or "flatness" not in m:
            continue
        kv = dict(t.split("=", 1) for t in cols[1].split(";") if "=" in t and ">" not in t)
        feats = set()
        for n in range(1, 5):
            t = kv.get(f"src{n}_type", "Additive" if n == 1 else "Off")
            if t != "Off":
                feats.add(f"slot type {t}")
                if n == 1:
                    feats.add(f"primary {t}")
            if t == "Wavetable":
                feats.add("table " + kv.get(f"src{n}_table", "Classic"))
            if t == "Noise":
                feats.add("noise " + kv.get(f"src{n}_noise", "?"))
        if "z_shape" in kv:
            feats.add("z-plane " + kv.get("z_mode", "?"))
            feats.add("z shape " + kv["z_shape"])
        else:
            feats.add("z-plane off")
        for key, label in (("cloud_level", "cloud"), ("fb_bus", "feedback bus"), ("cosmos_send", "cosmos"),
                           ("strike_level", "strike"), ("filter_fold", "filter fold"), ("room_level", "conv room"),
                           ("sub_level", "sub"), ("dly2_mix", "delay 2"), ("haas", "haas"), ("air", "air >= 0.3")):
            v = kv.get(key)
            try:
                on = v is not None and float(v) > (0.3 if key == "air" else 0.02)
            except ValueError:
                on = False
            feats.add(f"{label} {'on' if on else 'off'}")
        feats.add("ensemble " + kv.get("ens_mode", "off") if float(kv.get("ens_mix", "0") or 0) > 0.02 else "ensemble off")
        feats.add("filter " + kv.get("filter_model", "?"))
        feats.add("scale " + kv.get("scale", "?"))
        feats.add("pack " + os.path.basename(pack).replace(".ambientpack", ""))
        # the wavetable file's recipe, from its name
        wt = cols[4] if len(cols) > 4 else ""
        if wt:
            base = os.path.basename(wt)
            feats.add("recipe " + ("clip" if base.startswith("clip_") else base.rsplit("_", 1)[0]))
        rows.append((m["flatness"], m["centroid"], feats))

allf = np.array([r[0] for r in rows])
print(f"packs:     flatness p50 {np.median(allf):.4f}  p90 {np.percentile(allf, 90):.4f}   ({len(rows)} presets)\n")
byfeat = collections.defaultdict(list)
for f, c, feats in rows:
    for x in feats:
        byfeat[x].append((f, c))
out = []
for x, vals in byfeat.items():
    if len(vals) < MIN_GROUP or len(vals) > len(rows) - MIN_GROUP:
        continue
    have = np.array([v[0] for v in vals])
    ids = set()
    rest = np.array([r[0] for r in rows if x not in r[2]])
    out.append((np.median(have) / max(np.median(rest), 1e-9), x, len(vals), np.median(have), np.percentile(have, 90),
                np.median([v[1] for v in vals]), float((have > np.percentile(bvals, 90)).mean())))
out.sort(reverse=True)
print(f"  {'setting':34s} {'n':>5s}  {'flat p50':>8s} {'flat p90':>8s}  {'x rest':>6s}  {'centroid':>8s}  {'above built-in p90':>18s}")
for ratio, x, n, p50, p90, cen, share in out[:28]:
    print(f"  {x:34s} {n:5d}  {p50:8.4f} {p90:8.4f}  {ratio:6.2f}  {cen:8.0f}  {100 * share:17.0f} %")
print("  ...")
for ratio, x, n, p50, p90, cen, share in out[-12:]:
    print(f"  {x:34s} {n:5d}  {p50:8.4f} {p90:8.4f}  {ratio:6.2f}  {cen:8.0f}  {100 * share:17.0f} %")
