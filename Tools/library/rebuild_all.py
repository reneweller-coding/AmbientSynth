"""The whole library, from the clips on disk to the tables the synth ships with.

Seven steps, each of which can be run on its own; this driver exists so the order and the arguments
are written down once instead of living in a session's scrollback. It never generates audio -- the
clip generators (make_textures.py, make_field_recordings.py, make_wavetables.py, make_impulses.py)
are run by hand because they take hours of GPU time and their output is committed.

    1  presets    Tools/library/make_presets.py     the packs themselves
    2  verify     Tools/library/verify_packs.py     every key and value actually exists
    3  measure    Tools/library/measure_packs.py    60 s per preset, numbers plus a 12 s excerpt
    4  builtins   Tools/library/map_all.py --dry-run  the same for the built-ins, so CLAP hears them too
    5  clap       Tools/library/clap_embed.py       what a model says the excerpts sound like
    6  map        Tools/library/map_all.py          one layout, the groups, the phrases, the tables
    7  build      cmake --build ... && the selftest

  python Tools/library/rebuild_all.py --work <dir> [--from measure] [--jobs 4] [--dry-run]

--work holds everything that is not committed: the measurement cache, the excerpts and the CLAP
file. Steps are skipped when their output is already there, so an interrupted run continues where
it stopped; --force redoes them anyway.
"""
import argparse
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PACKS = os.path.join(ROOT, "Library", "Packs")
STEPS = ["presets", "verify", "measure", "builtins", "clap", "map", "build"]


def run(cmd, dry, cwd=ROOT):
    print("\n$ " + " ".join(str(c) for c in cmd), flush=True)
    if dry:
        return 0
    t = time.time()
    r = subprocess.run(cmd, cwd=cwd)
    print(f"   [{time.time() - t:.0f} s, exit {r.returncode}]", flush=True)
    return r.returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--work", required=True, help="folder for the cache, the excerpts and the CLAP file")
    ap.add_argument("--from", dest="start", default="presets", choices=STEPS)
    ap.add_argument("--to", dest="stop", default="build", choices=STEPS)
    ap.add_argument("--jobs", type=int, default=4, help="renders at a time; the machine stays usable at 4")
    ap.add_argument("--clusters", type=int, default=14)
    ap.add_argument("--clap-python", default="", help="interpreter with torch and transformers "
                    "(default: Tools/TextureGen/.venv, the one the clip generator uses)")
    ap.add_argument("--force", action="store_true", help="redo steps whose output is already there")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    work = os.path.abspath(a.work)
    os.makedirs(work, exist_ok=True)
    cache = os.path.join(work, "packs.json")
    bcache = os.path.join(work, "builtins.json")
    taps = os.path.join(work, "taps")
    clap = os.path.join(work, "clap.json")
    py = sys.executable
    lo = STEPS.index(a.start)
    hi = STEPS.index(a.stop)

    def want(step, output=None):
        i = STEPS.index(step)
        if i < lo or i > hi:
            print(f"-- {step}: not in range")
            return False
        if output and os.path.exists(output) and not a.force:
            print(f"-- {step}: {os.path.basename(output)} is already there (--force to redo)")
            return False
        return True

    if want("presets"):
        if run([py, os.path.join(HERE, "make_presets.py")], a.dry_run):
            return 1
        # A new library invalidates every measurement of the old one: same names, other sounds.
        for f in (cache, bcache, clap):
            if os.path.exists(f) and not a.dry_run:
                os.remove(f)
    if want("verify"):
        if run([py, os.path.join(HERE, "verify_packs.py")], a.dry_run):
            return 1
    if want("measure", cache):
        if run([py, os.path.join(HERE, "measure_packs.py"), "--packs", PACKS, "--cache", cache,
                "--taps", taps, "--jobs", str(a.jobs), "--no-write"], a.dry_run):
            return 1
    if want("builtins", bcache):
        # The layout is thrown away here; what is kept is the built-ins' measurements and their
        # excerpts, which have to exist before CLAP listens or the 196 would have no line.
        if run([py, os.path.join(HERE, "map_all.py"), "--pack-cache", cache, "--builtin-cache", bcache,
                "--taps", taps, "--jobs", str(a.jobs), "--dry-run"], a.dry_run):
            return 1
    if want("clap", clap):
        # CLAP needs torch, and the interpreter that runs the rest of this does not have it. The
        # clip generator's environment does, so that is the default rather than a second install.
        cpy = a.clap_python or os.path.join(ROOT, "Tools", "TextureGen", ".venv", "Scripts", "python.exe")
        if not os.path.exists(cpy):
            print(f"-- clap: no interpreter at {cpy}; pass --clap-python")
            return 1
        if run([cpy, os.path.join(HERE, "clap_embed.py"), "--taps", taps, "--out", clap], a.dry_run):
            return 1
    if want("map"):
        if run([py, os.path.join(HERE, "map_all.py"), "--pack-cache", cache, "--builtin-cache", bcache,
                "--taps", taps, "--clap", clap, "--clusters", str(a.clusters), "--jobs", str(a.jobs)],
               a.dry_run):
            return 1
    if want("build"):
        if run(["cmake", "--build", "build", "--config", "Release"], a.dry_run):
            return 1
        env = dict(os.environ, AMBIENT_MUTE="1", AMBIENT_PACKS=PACKS)
        print("\n$ ambient_selftest", flush=True)
        if not a.dry_run:
            r = subprocess.run([os.path.join(ROOT, "build", "Tests", "Release", "ambient_selftest.exe")],
                               cwd=ROOT, env=env)
            if r.returncode:
                print("selftest failed")
                return 1
    print("\ndone")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
