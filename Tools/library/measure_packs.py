"""Render every preset of a pack library, then rewrite the packs from what was measured.

make_presets.py estimates a preset's descriptors from its settings, because it has to name
files before they exist. That estimate is good enough to lay a map out, but it is a prediction.
This tool replaces it with the real thing, the same way Tools/preset_map.py does for the
built-in presets -- five thousand renders is about twenty minutes on six threads, so there is
no reason to guess.

    python Tools/library/measure_packs.py --packs Library/Packs --jobs 6

Two things are written back into the pack files:

* the six descriptors and the map position, from the audio
* a corrected `master_gain`, so every preset lands inside the target loudness window. A preset
  that came out at -10 dBFS gets turned down by exactly the excess; one at -34 gets turned up.
  This is what stops a library of thousands from having a few dozen presets that jump out.

Nothing else in the settings is touched, and the run is idempotent: measuring an already
measured library changes the gains by fractions of a dB.
"""
import argparse
import concurrent.futures
import math
import os
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
from make_presets import TAGS, layout, rank  # noqa: E402

RENDER = os.path.join(ROOT, "build", "Tools", "render", "Release", "ambient_render.exe")

# The window every preset is brought into. Drones live around -20..-28 dBFS; preset_check.py
# fails above -12, so the ceiling keeps a margin for a busier chord than the test's three notes.
TARGET_HI = -17.0
TARGET_LO = -30.0
GAIN_MIN, GAIN_MAX = -40.0, 12.0


# Every field of a pack line, in order. Reading a short list and writing it back is how the
# impulse, the matrix and the envelope shapes were silently dropped from all 5000 presets: the
# measurement pass rewrote each line with five fields instead of eight.
FIELDS = ["name", "settings", "meta", "texture", "wavetable", "impulse", "mod", "envs"]


def read_pack(path):
    head, rows = [], []
    for line in open(path, encoding="utf-8"):
        t = line.rstrip("\n")
        if not t.strip() or t.lstrip().startswith("#") or t.lstrip().startswith("pack "):
            head.append(t)
            continue
        f = t.split("|")
        while len(f) < len(FIELDS):
            f.append("")
        rows.append({k: f[i] for i, k in enumerate(FIELDS)})
    return head, rows


def write_pack(path, head, rows):
    with open(path, "w", encoding="utf-8") as f:
        for h in head:
            f.write(h + "\n")
        for r in rows:
            f.write("|".join(r[k] for k in FIELDS) + "\n")


MEASURE = re.compile(r"^measure: (.*)$", re.M)


# Renders run below normal priority. Five of them at full speed on a 24-thread machine still
# made the desktop stutter, and a batch that takes twenty minutes must not cost the machine.
LOW_PRIORITY = {"creationflags": subprocess.BELOW_NORMAL_PRIORITY_CLASS} if os.name == "nt" else {}


def render(name, packs, seconds):
    """The synth measures its own render and prints one line. It used to write a twelve-second
    stereo WAV to a temporary file and read it straight back -- five thousand presets is
    twenty-three gigabytes written and read for nothing, and every byte stayed in the file cache
    afterwards, which is what made the machine unusable."""
    res = subprocess.run([RENDER, "--packs", packs, "--preset", name, "--seconds", str(seconds),
                          "--notes", "45,52,59", "--set", "brain_rate=6", "--measure"],
                         capture_output=True, text=True, encoding="utf-8", errors="replace",
                         **LOW_PRIORITY)
    if res.returncode != 0:
        return None
    m = MEASURE.search(res.stdout or "")
    if not m:
        return None
    d = {}
    for tok in m.group(1).split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            try:
                d[k] = float(v)
            except ValueError:
                return None
    if not {"rms", "centroid", "flatness", "flux", "bass", "width", "voices"} <= set(d):
        return None
    d["rms_db"] = d.pop("rms")
    return d



def set_gain(settings, gain_db):
    parts = [p for p in settings.split(";") if p and not p.startswith("master_gain=")]
    g = min(max(gain_db, GAIN_MIN), GAIN_MAX)
    return ";".join([f"master_gain={g:.4g}"] + parts)


def get_gain(settings):
    for p in settings.split(";"):
        if p.startswith("master_gain="):
            try:
                return float(p.split("=", 1)[1])
            except ValueError:
                return -6.0
    return -6.0


def tag_bits(settings, d):
    p = {}
    for kv in settings.split(";"):
        if "=" in kv:
            k, v = kv.split("=", 1)
            p[k] = v
    num = lambda k: float(p.get(k, "0") or 0)
    t = set()
    t.add("Keys" if p.get("brain_on") == "off" else "Generative")
    if num("cosmos_send") > 0.05 or num("cosmos_shimmer") > 0.05: t.add("Cosmos")
    if num("fb_bus") > 0.02 or num("fb_fm") > 0.02: t.add("Feedback")
    if p.get("src2_type", "Off") != "Off" or p.get("src3_type", "Off") != "Off": t.add("Sources")
    if any(k in p.get("scale", "") for k in ("JI", "Harmonic", "Subharmonic", "Pythag", "Bohlen", "Otonal", "Slendro")):
        t.add("JustIntonation")
    if num("sub_level") > 0.05: t.add("Sub")
    if p.get("stack", "Detune") != "Detune": t.add("Stack")
    if num("air") >= 0.3: t.add("Air")
    if d["bright"] < 0.3: t.add("Dark")
    if d["bright"] > 0.7: t.add("Bright")
    if d["motion"] < 0.3: t.add("Calm")
    if d["motion"] > 0.7: t.add("Moving")
    if d["noisy"] < 0.4: t.add("Tonal")
    if d["noisy"] > 0.7: t.add("Noisy")
    if d["width"] > 0.7: t.add("Wide")
    if d["bass"] > 0.7: t.add("Bass")
    if d["density"] > 0.7: t.add("Dense")
    if d["density"] < 0.3: t.add("Sparse")
    return sum(1 << TAGS.index(x) for x in t)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", default=os.path.join(ROOT, "Library", "Packs"))
    ap.add_argument("--jobs", type=int, default=3,
                    help="renders in parallel; three leaves a 24-thread machine usable")
    ap.add_argument("--seconds", type=float, default=12.0)
    ap.add_argument("--limit", type=int, default=0, help="only the first N presets (a dry run)")
    a = ap.parse_args()

    files = sorted(f for f in os.listdir(a.packs) if f.endswith(".ambientpack"))
    packs = [(f, *read_pack(os.path.join(a.packs, f))) for f in files]
    rows = [r for _, _, rr in packs for r in rr]
    if a.limit:
        rows = rows[:a.limit]
    print(f"{len(rows)} presets in {len(packs)} packs, {a.jobs} renders in parallel")

    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        done = 0
        for r, m in zip(rows, ex.map(lambda r: render(r["name"], a.packs, a.seconds), rows)):
            r["m"] = m
            done += 1
            if done % 250 == 0:
                print(f"  {done}/{len(rows)}")
    bad = [r["name"] for r in rows if r["m"] is None]
    if bad:
        print(f"{len(bad)} presets failed to render: {', '.join(bad[:5])}")
    good = [r for r in rows if r["m"] is not None]
    if not good:
        raise SystemExit("nothing rendered")

    # Loudness: move each preset's master gain by exactly the distance to the window.
    moved = 0
    for r in good:
        rms = r["m"]["rms_db"]
        delta = 0.0
        if rms > TARGET_HI:
            delta = TARGET_HI - rms
        elif rms < TARGET_LO:
            delta = min(TARGET_LO - rms, 6.0)          # never shout a quiet preset awake
        if abs(delta) > 0.2:
            r["settings"] = set_gain(r["settings"], get_gain(r["settings"]) + delta)
            r["gain_delta"] = delta       # the meta line stores the loudness after this correction
            moved += 1
    rmsv = np.array([r["m"]["rms_db"] for r in good])
    print(f"loudness before: median {np.median(rmsv):.1f} dBFS, {int((rmsv > TARGET_HI).sum())} above "
          f"{TARGET_HI:.0f}, {int((rmsv < TARGET_LO).sum())} below {TARGET_LO:.0f}; {moved} gains corrected")

    # Descriptors: rank each measurement across the library, exactly as preset_map.py does.
    raw = np.array([[r["m"]["centroid"], r["m"]["flux"], r["m"]["width"], r["m"]["flatness"],
                     r["m"]["bass"], r["m"]["voices"]] for r in good], dtype=np.float64)
    raw[:, 0] = np.log(np.maximum(raw[:, 0], 1.0))     # brightness is heard logarithmically
    desc = np.zeros_like(raw)
    for c in range(raw.shape[1]):
        desc[:, c] = rank(raw[:, c])
    xy = layout(desc)
    for i, r in enumerate(good):
        d = {"bright": desc[i, 0], "motion": desc[i, 1], "width": desc[i, 2],
             "noisy": desc[i, 3], "bass": desc[i, 4], "density": desc[i, 5]}
        bits = tag_bits(r["settings"], d)
        r["meta"] = (" ".join(f"{v:.3f}" for v in (xy[i, 0], xy[i, 1], d["bright"], d["motion"],
                                                   d["width"], d["noisy"], d["bass"], d["density"]))
                     + f" {bits} {r['m']['rms_db'] + r.get('gain_delta', 0.0):.1f}")   # tenth token: the loudness it now plays at

    if a.limit:
        print("dry run (--limit): the pack files were not rewritten")
        return 0
    for name, head, rr in packs:
        head = [h for h in head if "estimated" not in h]
        if not any("measured" in h for h in head):
            head.insert(1, "# Descriptors and map positions measured by Tools/library/measure_packs.py "
                           "from a 12 s render of every preset.")
        write_pack(os.path.join(a.packs, name), head, rr)
    # Report what actually survived the rewrite: dropping a field silently is exactly how the
    # impulses, the matrix and the envelope shapes disappeared from all 5000 presets once.
    kept = {k: sum(1 for _, _, rr in packs for r in rr if r.get(k, "").strip("~ "))
            for k in ("texture", "wavetable", "impulse", "mod", "envs")}
    print(f"rewrote {len(packs)} packs; presets carrying "
          + ", ".join(f"{k} {v}" for k, v in kept.items()))
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
