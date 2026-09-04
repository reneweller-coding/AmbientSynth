"""Automatic sound test: render every preset and flag the ones that are too loud, clip, click,
carry DC or go non-finite. The measurement half of "measure, don't only listen", as a check
that can run before a commit or after a preset round.

    python Tools/preset_check.py [--seconds 10] [--json report.json] [--only "name"]

Limits (a preset fails if any is exceeded):
    rms      > -12 dBFS     (drones live around -20 .. -28)
    peak     > 0.98         (the soft clipper is working hard)
    jump     > 0.30         (sample-to-sample: a click)
    dc       > 0.02         (mean of the mixed signal)
    silent   rms < -60 dBFS after the chord and brain had 10 s
    non-finite samples
Exit code 1 when anything fails.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
RENDER = os.path.join(ROOT, "build", "Tools", "render", "Release", "ambient_render.exe")
sys.path.insert(0, HERE)
from analyze import read_wav  # noqa: E402

LIMITS = {"rms_db": -12.0, "peak": 0.98, "jump": 0.30, "dc": 0.02, "silent_db": -60.0}


def check_preset(name, seconds):
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, "p.wav")
        res = subprocess.run([RENDER, "--preset", name, "--seconds", str(seconds), "--notes", "45,52,59", "--set", "brain_rate=6", "--out", wav],
                             capture_output=True, text=True, encoding="utf-8", errors="replace")
        if res.returncode != 0 or not os.path.isfile(wav):
            return {"name": name, "error": (res.stderr or res.stdout).strip()[-200:], "fail": ["render"]}
        m = re.search(r"non-finite (\d+)", res.stdout)
        nonfinite = int(m.group(1)) if m else 0
        x, sr = read_wav(wav)
    mono = x.mean(axis=1)
    tail = x[int(len(x) * 0.5):]   # judge the second half: the chord and brain have settled
    rms_db = float(20 * np.log10(np.sqrt((tail ** 2).mean()) + 1e-12))
    peak = float(np.abs(x).max())
    jump = float(np.abs(np.diff(mono)).max())
    dc = float(abs(mono[int(len(mono) * 0.5):].mean()))
    fails = []
    if rms_db > LIMITS["rms_db"]: fails.append(f"loud {rms_db:.1f} dBFS")
    if peak > LIMITS["peak"]: fails.append(f"peak {peak:.2f}")
    if jump > LIMITS["jump"]: fails.append(f"click {jump:.2f}")
    if dc > LIMITS["dc"]: fails.append(f"dc {dc:.3f}")
    if rms_db < LIMITS["silent_db"]: fails.append(f"silent {rms_db:.1f} dBFS")
    if nonfinite: fails.append(f"non-finite {nonfinite}")
    return {"name": name, "rms_db": rms_db, "peak": peak, "jump": jump, "dc": dc, "nonfinite": nonfinite, "fail": fails}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--json", default=None)
    ap.add_argument("--only", default=None, help="substring of preset names to check")
    a = ap.parse_args()
    names = [n for n in subprocess.run([RENDER, "--list-presets"], capture_output=True, text=True, encoding="utf-8").stdout.splitlines() if n.strip()]
    if a.only:
        names = [n for n in names if a.only.lower() in n.lower()]
    results = []
    failed = 0
    for i, name in enumerate(names):
        r = check_preset(name, a.seconds)
        results.append(r)
        status = "FAIL " + ", ".join(r["fail"]) if r["fail"] else "ok"
        if r["fail"]: failed += 1
        if "error" in r:
            print(f"{i:3d} {name:28s} {status}: {r['error']}")
        else:
            print(f"{i:3d} {name:28s} rms {r['rms_db']:6.1f}  peak {r['peak']:.2f}  jump {r['jump']:.3f}  dc {r['dc']:.4f}  {status}")
    print(f"\n{len(names) - failed} of {len(names)} presets pass" + (f", {failed} FAIL" if failed else ""))
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump({"limits": LIMITS, "results": results}, f, indent=1)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
