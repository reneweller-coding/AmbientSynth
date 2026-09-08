"""How tonal is each clip, measured rather than read off the file name.

The two folders are told apart by whether TextureGen's pitch detector found a stable note, and
that is a coarse line in both directions: a whistle and a struck plate have a note and are not
what anyone means by tonal material, and a ceremonial drone whose pitch wavers by a quarter tone
has none and is. So every clip gets two numbers from its own signal:

    harm  0..1  how periodic it is: the height of the autocorrelation peak in the 40 Hz .. 2 kHz
                lag range, averaged over frames of the middle ten seconds. A sine is 1, noise ~0.
    flat  0..1  spectral flatness (geometric / arithmetic mean of the power spectrum), averaged.
                Noise is near 1, a line spectrum near 0.

    Tools/TextureGen/.venv/Scripts/python Tools/library/clip_tonality.py [--jobs 12]

Written to Library/tonality.json and read by make_presets.py, which draws the tonal slots from
the clips that measure tonal and leaves the rest to the stretched beds.
"""
import argparse
import glob
import json
import os
import sys
from multiprocessing import Pool

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FRAME, HOP, SECONDS = 4096, 2048, 10.0


def measure(path):
    import soundfile as sf
    try:
        x, sr = sf.read(path, dtype="float32", always_2d=False)
    except Exception as e:                       # noqa: BLE001 -- one bad file is one bad file
        return os.path.basename(os.path.dirname(path)) + "/" + os.path.basename(path), None, str(e)
    if x.ndim > 1:
        x = x.mean(axis=1)
    n = len(x)
    take = int(sr * SECONDS)
    if n > take:
        start = (n - take) // 2
        x = x[start:start + take]
    if len(x) < FRAME * 2:
        return os.path.basename(os.path.dirname(path)) + "/" + os.path.basename(path), None, "too short"
    lo, hi = int(sr / 2000.0), int(sr / 40.0)    # lags for 2 kHz .. 40 Hz
    win = np.hanning(FRAME).astype(np.float32)
    harms, flats = [], []
    for s in range(0, len(x) - FRAME, HOP):
        fr = x[s:s + FRAME]
        if np.max(np.abs(fr)) < 1e-4:
            continue
        fr = (fr - fr.mean()) * win
        spec = np.fft.rfft(fr, n=FRAME * 2)
        power = (spec.real ** 2 + spec.imag ** 2)
        # autocorrelation via the power spectrum, normalised at lag 0
        ac = np.fft.irfft(power)[:FRAME]
        if ac[0] <= 0:
            continue
        ac = ac / ac[0]
        harms.append(float(np.max(ac[lo:hi])))
        p = power[:FRAME // 2 + 1] + 1e-12
        flats.append(float(np.exp(np.mean(np.log(p))) / np.mean(p)))
    if not harms:
        return os.path.basename(os.path.dirname(path)) + "/" + os.path.basename(path), None, "silent"
    rel = os.path.basename(os.path.dirname(path)) + "/" + os.path.basename(path)
    return rel, {"harm": round(float(np.mean(harms)), 4), "flat": round(float(np.mean(flats)), 4)}, None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(ROOT, "Library", "tonality.json"))
    ap.add_argument("--jobs", type=int, default=12)
    a = ap.parse_args()
    files = []
    for folder in ("Textures", "FieldRecordings"):
        files += sorted(glob.glob(os.path.join(ROOT, "Library", folder, "*.wav")))
    print(f"{len(files)} clips", flush=True)
    out, bad = {}, []
    with Pool(a.jobs) as pool:
        for i, (rel, m, err) in enumerate(pool.imap_unordered(measure, files, chunksize=8), 1):
            if m is None:
                bad.append((rel, err))
            else:
                out[rel] = m
            if i % 500 == 0:
                print(f"  {i}/{len(files)}", flush=True)
    json.dump({"clips": out}, open(a.out, "w", encoding="utf-8"), indent=0, sort_keys=True)
    print(f"wrote {a.out}: {len(out)} clips, {len(bad)} unreadable", flush=True)
    for rel, err in bad[:10]:
        print("  ", rel, err)
    # The census, so the numbers say something at once.
    for folder in ("Textures", "FieldRecordings"):
        h = np.array([m["harm"] for k, m in out.items() if k.startswith(folder + "/")])
        f = np.array([m["flat"] for k, m in out.items() if k.startswith(folder + "/")])
        if len(h) == 0:
            continue
        print(f"{folder}: {len(h)} clips  harm median {np.median(h):.2f}  (p10 {np.percentile(h, 10):.2f}, p90 {np.percentile(h, 90):.2f})"
              f"  flat median {np.median(f):.3f}")
        for th in (0.5, 0.6, 0.7, 0.8):
            print(f"   harm >= {th}: {int((h >= th).sum()):5d}  ({100.0 * (h >= th).mean():.0f} %)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
