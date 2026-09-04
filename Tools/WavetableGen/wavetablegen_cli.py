"""WavetableGen command line: wavetables in batch, without the GUI.

  python wavetablegen_cli.py audio  <files or globs>... [--frames 32] [--pitch HZ] [--start S] [--end S]
                                     [--cycles 4] [--out-dir Wavetables] [--min-voiced 0.3]
        one table per input file, named <file>_<note>.wav; files without a stable pitch are skipped
  python wavetablegen_cli.py procedural [--recipe all|"Saw to square"|...] [--seeds 1] [--seed 1]
                                     [--frames 32] [--partials 48] [--noise 0.2] [--scatter 0.3] [--out-dir ...]
        every recipe (or one) times `seeds` seeds
  python wavetablegen_cli.py prompt --prompts prompts.txt [--model sao] [--seconds 10] [--count 1] ...
        generates a sustained note per prompt through TextureGen's worker, then slices it
  python wavetablegen_cli.py preview <table.wav> [--note 45] [--seconds 5] [--out preview.wav]
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
import wavetablegen_core as wt  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TEXTURE_WORKER = os.path.normpath(os.path.join(HERE, "..", "TextureGen", "texturegen_worker.py"))


def expand(patterns):
    files = []
    for p in patterns:
        hits = glob.glob(p)
        files += hits if hits else ([p] if os.path.isfile(p) else [])
    return [f for f in files if f.lower().endswith((".wav", ".flac", ".aif", ".aiff", ".ogg"))]


def cmd_audio(a):
    files = expand(a.files)
    if not files:
        print("no input files"); return 1
    os.makedirs(a.out_dir, exist_ok=True)
    ok = 0
    for f in files:
        try:
            mono, sr = wt.read_wav_mono(f)
            table, info = wt.table_from_audio(mono, sr, frames=a.frames, pitch_hz=a.pitch, start=a.start, end=a.end,
                                              cycles_per_frame=a.cycles)
            if info["voiced"] < a.min_voiced and a.pitch is None:
                print(f"skip  {os.path.basename(f)}: only {info['voiced']:.0%} of the frames have a stable pitch"); continue
            base = wt.slugify(os.path.splitext(os.path.basename(f))[0], 60)
            if not base.endswith("_" + info["note"]):
                base += "_" + info["note"]
            info["file"] = f
            out = wt.save_table(os.path.join(a.out_dir, base + ".wav"), table, info, float32=not a.int16)
            print(f"table {os.path.basename(out)}  {info['pitch_hz']:.1f} Hz, voiced {info['voiced']:.0%}, {a.frames} frames")
            ok += 1
        except Exception as e:
            print(f"fail  {os.path.basename(f)}: {e}")
    print(f"{ok} of {len(files)} files -> {a.out_dir}")
    return 0


def cmd_procedural(a):
    os.makedirs(a.out_dir, exist_ok=True)
    recipes = list(wt.RECIPES) if a.recipe == "all" else [a.recipe]
    for r in recipes:
        if r not in wt.RECIPES:
            print(f"unknown recipe {r!r}; known: {', '.join(wt.RECIPES)}"); return 1
    n = 0
    for r in recipes:
        for s in range(a.seeds):
            seed = a.seed + s
            table = wt.table_procedural(r, frames=a.frames, partials=a.partials, seed=seed, noise=a.noise, phase_scatter=a.scatter)
            out = wt.save_table(os.path.join(a.out_dir, f"proc_{wt.slugify(r)}_{seed}.wav"), table,
                                {"recipe": r, "seed": seed, "noise": a.noise, "phase_scatter": a.scatter, "partials": a.partials}, float32=not a.int16)
            print("table", os.path.basename(out)); n += 1
    print(f"{n} tables -> {a.out_dir}")
    return 0


def cmd_prompt(a):
    tmp = os.path.join(a.out_dir, "_prompts")
    os.makedirs(tmp, exist_ok=True)
    args = [sys.executable, TEXTURE_WORKER, "--batch", a.prompts, "--model", a.model, "--seconds", str(a.seconds),
            "--steps", str(a.steps), "--seed", str(a.seed), "--count", str(a.count), "--out-dir", tmp]
    print("generating notes:", " ".join(args[2:]))
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8")
    made = []
    for line in proc.stdout:
        line = line.strip()
        if not line.startswith("{"):
            continue
        ev = json.loads(line)
        if ev.get("event") == "done":
            made.append(ev["path"]); print("note ", os.path.basename(ev["path"]))
        elif ev.get("event") == "error":
            print("ERROR", ev["text"])
        elif ev.get("event") == "status":
            print("  ", ev["text"])
    proc.wait()
    if not made:
        print("nothing generated"); return 1
    a.files = made
    a.pitch = None; a.start = 1.0; a.end = None; a.cycles = 4; a.min_voiced = 0.3
    return cmd_audio(a)


def cmd_preview(a):
    import soundfile as sf
    mono, sr = wt.read_wav_mono(a.table)
    frames = len(mono) // wt.FRAME
    table = mono[:frames * wt.FRAME].reshape(frames, wt.FRAME)
    hz = 440.0 * 2 ** ((a.note - 69) / 12)
    y = wt.render_preview(table, 48000, a.seconds, hz, sweep=True)
    out = a.out or os.path.splitext(a.table)[0] + "_preview.wav"
    sf.write(out, y, 48000)
    print(f"preview {out}: {frames} frames swept at {hz:.1f} Hz")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--frames", type=int, default=32)
        p.add_argument("--out-dir", default=os.path.join(ROOT, "Wavetables"))
        p.add_argument("--int16", action="store_true", help="16-bit PCM instead of 32-bit float")

    p = sub.add_parser("audio"); common(p)
    p.add_argument("files", nargs="+")
    p.add_argument("--pitch", type=float, default=None, help="force the fundamental in Hz (default: tracked)")
    p.add_argument("--start", type=float, default=0.0)
    p.add_argument("--end", type=float, default=None)
    p.add_argument("--cycles", type=int, default=4, help="cycles averaged per frame")
    p.add_argument("--min-voiced", type=float, default=0.3, help="skip files whose pitch is stable in fewer frames than this share")
    p.set_defaults(fn=cmd_audio)

    p = sub.add_parser("procedural"); common(p)
    p.add_argument("--recipe", default="all")
    p.add_argument("--seeds", type=int, default=1)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--partials", type=int, default=48)
    p.add_argument("--noise", type=float, default=0.2)
    p.add_argument("--scatter", type=float, default=0.3)
    p.set_defaults(fn=cmd_procedural)

    p = sub.add_parser("prompt"); common(p)
    p.add_argument("--prompts", required=True, help="prompts.txt, one per line (TextureGen batch format)")
    p.add_argument("--model", default="sao")
    p.add_argument("--seconds", type=float, default=10.0)
    p.add_argument("--steps", type=int, default=100)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--count", type=int, default=1)
    p.set_defaults(fn=cmd_prompt)

    p = sub.add_parser("preview")
    p.add_argument("table")
    p.add_argument("--note", type=int, default=45)
    p.add_argument("--seconds", type=float, default=5.0)
    p.add_argument("--out", default=None)
    p.set_defaults(fn=cmd_preview)

    a = ap.parse_args()
    sys.exit(a.fn(a))


if __name__ == "__main__":
    main()
