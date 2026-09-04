"""Build the wavetable library for the generated presets.

Every recipe in Tools/WavetableGen is rendered several times with different partial counts,
spectral noise and phase scatter, so the library holds a spread of related tables instead of
one per recipe. Fast and deterministic: the same seed always writes the same files, which is
what lets the preset generator name a table before it exists.

    python Tools/library/make_wavetables.py --per-recipe 32 --out-dir Library/Wavetables

19 recipes x 32 variants = 608 tables of 32 frames, written as 16-bit PCM (about 130 kB each).
"""
import argparse
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "WavetableGen"))
from wavetablegen_core import RECIPES, save_table, table_procedural  # noqa: E402


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def build(per_recipe, frames, out_dir, seed, int16):
    os.makedirs(out_dir, exist_ok=True)
    names = sorted(RECIPES)
    written = []
    for ri, recipe in enumerate(names):
        rng = random.Random(seed * 7919 + ri)
        for k in range(per_recipe):
            partials = rng.choice([16, 24, 32, 48, 48, 64])
            noise = round(rng.choice([0.0, 0.0, 0.1, 0.25, 0.4, 0.6]), 2)
            scatter = round(rng.choice([0.0, 0.0, 0.3, 0.6, 1.0]), 2)
            tseed = rng.randrange(1, 100000)
            table = table_procedural(recipe, frames=frames, partials=partials, seed=tseed,
                                     noise=noise, phase_scatter=scatter)
            path = os.path.join(out_dir, f"{slug(recipe)}_{k:03d}.wav")
            save_table(path, table, {"recipe": recipe, "partials": partials, "noise": noise,
                                     "phase_scatter": scatter, "seed": tseed}, float32=not int16)
            written.append(path)
    return written, names


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--per-recipe", type=int, default=32)
    ap.add_argument("--frames", type=int, default=32)
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--out-dir", default=os.path.join(ROOT, "Library", "Wavetables"))
    ap.add_argument("--float32", action="store_true", help="32-bit float instead of 16-bit PCM (4x the size)")
    a = ap.parse_args()
    written, names = build(a.per_recipe, a.frames, a.out_dir, a.seed, not a.float32)
    print(f"{len(written)} wavetables from {len(names)} recipes -> {a.out_dir}")
    for n in names:
        print(f"  {n}")


if __name__ == "__main__":
    main()
