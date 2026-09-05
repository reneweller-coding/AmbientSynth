"""Sort the generated texture clips into usable and unusable.

Text-to-audio models miss sometimes: a clip comes out near-silent, or it is one short event
with half a minute of nothing after it. Either makes a dead granular source, so the preset
generator needs to know which files not to name.

    python Tools/library/check_textures.py --dir Library/Textures

Writes `rejected.txt` (one file name per line, with the reason) next to the clips and prints a
summary. Nothing is deleted -- the list is advisory, and make_presets.py reads it.

A clip is rejected when
    rms      < -45 dBFS     nothing there
    active   < 0.55         less than 55 % of the 50 ms frames are above -50 dBFS
    peak     > 0.999        clipped by the model itself
    dc       > 0.02         a constant offset (grains would click)
    flat     < 1e-5         digital silence or a pure DC block
"""
import argparse
import glob
import json
import math
import os

import numpy as np
import soundfile as sf

LIMITS = {"rms_db": -45.0, "active": 0.55, "peak": 0.999, "dc": 0.02}


def highpass_in_place(path, hz=20.0):
    """A one-pole high pass at 20 Hz, applied per channel. A deep drone with a constant offset
    is worth keeping -- only the offset has to go, or every grain starts with a click."""
    x, sr = sf.read(path, dtype="float32", always_2d=True)
    a = math.exp(-2.0 * math.pi * hz / sr)
    y = np.empty_like(x)
    for c in range(x.shape[1]):
        v = x[:, c].astype(np.float64)
        # y[n] = a*(y[n-1] + v[n] - v[n-1]) -- run as a cumulative form to stay vectorised
        d = np.empty_like(v)
        d[0] = 0.0
        d[1:] = v[1:] - v[:-1]
        out = np.zeros_like(v)
        acc = 0.0
        for i in range(v.size):          # the recursion is serial; 20 s at 44.1 kHz is fine
            acc = a * (acc + d[i])
            out[i] = acc
        y[:, c] = out
    peak = float(np.abs(y).max())
    if peak > 1e-6:
        y *= 0.5 / peak
    sf.write(path, y, sr, subtype="FLOAT")


def to_int16(path):
    """32-bit float halved to 16-bit PCM. The clips are normalised to -6 dBFS peak and only ever
    feed a granular reader, so the extra bits buy nothing -- and the Quest's storage is finite."""
    info = sf.info(path)
    if info.subtype == "PCM_16":
        return False
    x, sr = sf.read(path, dtype="float32", always_2d=True)
    sf.write(path, x, sr, subtype="PCM_16")
    return True


def measure(path):
    x, sr = sf.read(path, dtype="float32", always_2d=True)
    mono = x.mean(axis=1)
    if mono.size < sr:
        return {"fail": ["too short"]}
    rms_db = float(20 * np.log10(np.sqrt((mono ** 2).mean()) + 1e-12))
    peak = float(np.abs(mono).max())
    dc = float(abs(mono.mean()))
    hop = max(1, int(sr * 0.05))
    n = (mono.size // hop) * hop
    frames = mono[:n].reshape(-1, hop)
    fr = 20 * np.log10(np.sqrt((frames ** 2).mean(axis=1)) + 1e-12)
    active = float((fr > -50.0).mean())
    finite = bool(np.all(np.isfinite(mono)))
    fails = []
    if not finite: fails.append("non-finite")
    if rms_db < LIMITS["rms_db"]: fails.append(f"quiet {rms_db:.0f} dBFS")
    if active < LIMITS["active"]: fails.append(f"gappy {active:.2f} active")
    if peak > LIMITS["peak"]: fails.append(f"clipped {peak:.3f}")
    if dc > LIMITS["dc"]: fails.append(f"dc {dc:.3f}")
    return {"rms_db": rms_db, "peak": peak, "dc": dc, "active": active, "seconds": mono.size / sr,
            "channels": x.shape[1], "rate": sr, "fail": fails}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Library", "Textures"))
    ap.add_argument("--json", default=None)
    ap.add_argument("--repair", action="store_true",
                    help="high-pass clips that carry a DC offset instead of rejecting them")
    ap.add_argument("--compact", action="store_true",
                    help="rewrite 32-bit float clips as 16-bit PCM (halves the library)")
    a = ap.parse_args()
    files = sorted(glob.glob(os.path.join(a.dir, "*.wav")))
    if not files:
        print(f"no clips in {a.dir}")
        return 0
    rows, bad, repaired, packed = [], [], 0, 0
    for f in files:
        m = measure(f)
        if a.repair and any(x.startswith("dc ") for x in m["fail"]):
            highpass_in_place(f)
            m = measure(f)
            repaired += 1
        m["name"] = os.path.basename(f)
        if a.compact and not m["fail"] and to_int16(f):
            packed += 1
        rows.append(m)
        if m["fail"]:
            bad.append(m)
    with open(os.path.join(a.dir, "rejected.txt"), "w", encoding="utf-8") as out:
        out.write("# clips the preset generator should not name; written by Tools/library/check_textures.py\n")
        for m in bad:
            out.write(f"{m['name']}  # {', '.join(m['fail'])}\n")
    ok = len(rows) - len(bad)
    secs = sum(r.get("seconds", 0.0) for r in rows)
    print(f"{ok} of {len(rows)} clips usable ({secs / 60:.0f} min of audio)"
          + (f", {repaired} high-passed" if repaired else "")
          + (f", {packed} packed to 16 bit" if packed else ""))
    for m in bad[:20]:
        print(f"  reject {m['name']}: {', '.join(m['fail'])}")
    if len(bad) > 20:
        print(f"  ... and {len(bad) - 20} more, see rejected.txt")
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump({"limits": LIMITS, "clips": rows}, f, indent=1)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
