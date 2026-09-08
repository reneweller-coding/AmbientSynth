"""ImpulseGen command line: impulse responses for the Room, in batch.

  python impulsegen_cli.py procedural --name dark_hall [--seconds 5] [--rt60 5 3 1.2] [--size 1]
                                      [--predelay 0] [--width 1] [--tone 0] [--modulation 0] [--seeds 1]
  python impulsegen_cli.py audio <files or globs>... [--max-seconds 8] [--floor -60] [--extend] [--rt60-extend 3]
        each recording (a clap, a balloon, an AI render) becomes an impulse
  python impulsegen_cli.py hybrid <file> [--seconds 5] [--rt60 5 3 1.2]
  python impulsegen_cli.py prompt --prompts rooms.txt [--model sao] [--seconds 20] [--count 1] [--extend]
        asks a TextureGen model for "a single clap in <room>" per line and cuts the impulses
  python impulsegen_cli.py presets [--out-dir Impulses]
        a set of designed rooms: cathedral, chapel, cave, hangar, dark chamber, glass room, infinite
"""
import argparse
import glob
import json
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import impulsegen_core as ig  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TEXTURE_WORKER = os.path.normpath(os.path.join(HERE, "..", "TextureGen", "texturegen_worker.py"))
# Where a hand-run experiment lands. NOT Library/: what the three designers write while
# somebody is trying things out is not the shipping library, and three folders in the repo
# root called Textures, Wavetables and Impulses looked exactly like a second one.
DEFAULT_OUT = os.path.join(ROOT, "Scratch", "Impulses")

# name: (seconds, rt60 low/mid/high, size, predelay ms, width, tone, modulation)
ROOM_PRESETS = {
    "dark_cathedral":   (8.0, 7.0, 4.5, 1.5, 2.2, 30, 1.0, -0.5, 0.1),
    "stone_chapel":     (4.0, 3.2, 2.4, 1.2, 1.0, 12, 0.9,  0.0, 0.0),
    "deep_cave":        (7.0, 6.5, 3.0, 0.7, 1.8, 40, 1.0, -0.8, 0.3),
    "hangar":           (5.0, 4.0, 3.5, 2.2, 3.0, 25, 1.0,  0.2, 0.0),
    "dark_chamber":     (2.5, 2.4, 1.6, 0.6, 0.5,  5, 0.8, -0.6, 0.0),
    "glass_room":       (3.0, 1.5, 2.0, 2.8, 0.6,  8, 1.0,  0.8, 0.2),
    "infinite_plate":   (8.0, 6.0, 6.0, 4.0, 0.2,  0, 1.0,  0.3, 0.5),
    "night_field":      (6.0, 5.0, 2.5, 0.5, 3.0, 60, 1.0, -0.9, 0.2),
}


def cmd_procedural(a):
    os.makedirs(a.out_dir, exist_ok=True)
    for s in range(a.seeds):
        seed = a.seed + s
        ir = ig.procedural(a.sr, a.seconds, a.rt60[0], a.rt60[1], a.rt60[2], a.size, a.predelay, 25.0, a.width, 0.5, a.modulation, a.tone, seed)
        out = ig.save(os.path.join(a.out_dir, f"{ig.slugify(a.name)}_{seed}.wav"), ir, a.sr,
                      {"mode": "procedural", "rt60": a.rt60, "size": a.size, "predelay_ms": a.predelay, "width": a.width, "tone": a.tone, "modulation": a.modulation, "seed": seed})
        d = ig.describe(ir, a.sr)
        print(f"impulse {os.path.basename(out)}  {d['seconds']:.1f} s, RT60 ~{d['rt60_estimate']:.1f} s, corr {d['stereo_correlation']:.2f}")
    return 0


def cmd_presets(a):
    os.makedirs(a.out_dir, exist_ok=True)
    for name, (sec, lo, mid, hi, size, pre, width, tone, mod) in ROOM_PRESETS.items():
        ir = ig.procedural(a.sr, sec, lo, mid, hi, size, pre, 25.0, width, 0.5, mod, tone, 1)
        out = ig.save(os.path.join(a.out_dir, name + ".wav"), ir, a.sr, {"mode": "procedural preset", "rt60": [lo, mid, hi], "size": size})
        d = ig.describe(ir, a.sr)
        print(f"impulse {os.path.basename(out)}  {d['seconds']:.1f} s, RT60 ~{d['rt60_estimate']:.1f} s")
    return 0


def expand(patterns):
    files = []
    for p in patterns:
        hits = glob.glob(p)
        files += hits if hits else ([p] if os.path.isfile(p) else [])
    return files


def cmd_audio(a):
    files = expand(a.files)
    if not files:
        print("no input files"); return 1
    os.makedirs(a.out_dir, exist_ok=True)
    for f in files:
        try:
            x, sr = ig.read_wav(f)
            ir = ig.from_audio(x, sr, a.max_seconds, a.floor, a.extend, a.rt60_extend)
            out = ig.save(os.path.join(a.out_dir, ig.slugify(os.path.splitext(os.path.basename(f))[0]) + "_ir.wav"), ir, sr, {"mode": "from audio", "source": f, "extend": a.extend})
            d = ig.describe(ir, sr)
            print(f"impulse {os.path.basename(out)}  {d['seconds']:.1f} s, RT60 ~{d['rt60_estimate']:.1f} s, corr {d['stereo_correlation']:.2f}")
        except Exception as e:
            print(f"fail {os.path.basename(f)}: {e}")
    return 0


def cmd_hybrid(a):
    x, sr = ig.read_wav(a.file)
    ir = ig.hybrid(x, sr, a.seconds, a.rt60[0], a.rt60[1], a.rt60[2], a.seed)
    os.makedirs(a.out_dir, exist_ok=True)
    out = ig.save(os.path.join(a.out_dir, ig.slugify(os.path.splitext(os.path.basename(a.file))[0]) + "_hybrid.wav"), ir, sr, {"mode": "hybrid", "source": a.file, "rt60": a.rt60})
    print("impulse", os.path.basename(out))
    return 0


def cmd_prompt(a):
    tmp = os.path.join(a.out_dir, "_prompts")
    os.makedirs(tmp, exist_ok=True)
    # wrap every line as an impulse-like event unless it already says clap/impulse
    with open(a.prompts, encoding="utf-8") as f:
        lines = [l.strip() for l in f if l.strip() and not l.startswith("#")]
    wrapped = os.path.join(tmp, "_wrapped_prompts.txt")
    with open(wrapped, "w", encoding="utf-8") as f:
        for l in lines:
            text, opts = (l.rsplit("|", 1) + [""])[:2] if "|" in l else (l, "")
            text = text.strip()
            if not any(w in text.lower() for w in ("clap", "impulse", "balloon", "gunshot", "snap")):
                text = f"a single sharp hand clap in {text}, then only the room's reverb tail, no music, no voices"
            f.write(text + (" | " + opts.strip() if opts.strip() else "") + "\n")
    args = [sys.executable, TEXTURE_WORKER, "--batch", wrapped, "--model", a.model, "--seconds", str(a.seconds),
            "--steps", "100", "--seed", str(a.seed), "--count", str(a.count), "--out-dir", tmp]
    print("generating rooms:", " ".join(args[2:]))
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8")
    made = []
    for line in proc.stdout:
        line = line.strip()
        if not line.startswith("{"):
            continue
        ev = json.loads(line)
        if ev.get("event") == "done":
            made.append(ev["path"]); print("render", os.path.basename(ev["path"]))
        elif ev.get("event") == "error":
            print("ERROR", ev["text"])
    proc.wait()
    if not made:
        print("nothing generated"); return 1
    a.files = made; a.max_seconds = a.seconds; a.floor = -60.0; a.rt60_extend = 3.0
    return cmd_audio(a)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--out-dir", default=DEFAULT_OUT)
        p.add_argument("--sr", type=int, default=48000)

    p = sub.add_parser("procedural"); common(p)
    p.add_argument("--name", default="room")
    p.add_argument("--seconds", type=float, default=5.0)
    p.add_argument("--rt60", type=float, nargs=3, default=[5.0, 3.0, 1.2], metavar=("LOW", "MID", "HIGH"))
    p.add_argument("--size", type=float, default=1.0)
    p.add_argument("--predelay", type=float, default=0.0, help="ms")
    p.add_argument("--width", type=float, default=1.0)
    p.add_argument("--tone", type=float, default=0.0, help="-1 dark .. +1 bright")
    p.add_argument("--modulation", type=float, default=0.0)
    p.add_argument("--seeds", type=int, default=1)
    p.add_argument("--seed", type=int, default=1)
    p.set_defaults(fn=cmd_procedural)

    p = sub.add_parser("presets"); common(p)
    p.set_defaults(fn=cmd_presets)

    p = sub.add_parser("audio"); common(p)
    p.add_argument("files", nargs="+")
    p.add_argument("--max-seconds", type=float, default=8.0)
    p.add_argument("--floor", type=float, default=-60.0)
    p.add_argument("--extend", action="store_true")
    p.add_argument("--rt60-extend", type=float, default=3.0)
    p.set_defaults(fn=cmd_audio)

    p = sub.add_parser("hybrid"); common(p)
    p.add_argument("file")
    p.add_argument("--seconds", type=float, default=5.0)
    p.add_argument("--rt60", type=float, nargs=3, default=[5.0, 3.0, 1.2])
    p.add_argument("--seed", type=int, default=1)
    p.set_defaults(fn=cmd_hybrid)

    p = sub.add_parser("prompt"); common(p)
    p.add_argument("--prompts", required=True)
    p.add_argument("--model", default="sao")
    p.add_argument("--seconds", type=float, default=20.0)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--count", type=int, default=1)
    p.add_argument("--extend", action="store_true")
    p.set_defaults(fn=cmd_prompt)

    a = ap.parse_args()
    sys.exit(a.fn(a))


if __name__ == "__main__":
    main()
