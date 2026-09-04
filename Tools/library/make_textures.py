"""Build the texture jobs for the preset library and (optionally) run them.

Every style in styles.py carries a handful of prompt seeds. This crosses them with modifier
phrases so each style ends up with a spread of related but distinct clips, assigns a model and
a seed, and writes a jobs.json that TextureGen's worker understands.

    python Tools/library/make_textures.py --per-style 48 --out Tools/library/texture_jobs.json
    Tools/TextureGen/.venv/Scripts/python Tools/TextureGen/texturegen_worker.py \
        --batch Tools/library/texture_jobs.json --out-dir Textures --resume --nice --max-minutes 240

The batch is resumable: rerunning with --resume skips every clip already on disk, so it can be
stopped at any time and picked up later.
"""
import argparse
import json
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from styles import STYLES  # noqa: E402

# Crossed with each style's prompt seeds. They change the character without leaving the style.
MODIFIERS = [
    "", "", "",                                   # the plain prompt is the most common case
    "very slow and static", "with faint metallic overtones", "heavily filtered, dark",
    "with a slow breathing swell", "close and intimate", "far away and reverberant",
    "with soft tape hiss", "granular and grainy", "pure and clean, no noise",
    "with a deep sub bass underneath", "high and thin", "warm and analog",
    "with slowly beating partials", "dry, almost no reverb", "drenched in reverb",
    "with irregular low creaks", "wide stereo, no centre",
]
NEGATIVE = "drums, percussion, beat, rhythm, speech, vocals, singing, melody, music production, clipping"

# Model mix. Stable Audio Open is the best texture model here; MusicGen Large is better at held
# tonal chords; AudioLDM 2 is 16 kHz and suits dark, noisy material.
MIX = [("sao", 0.60), ("musicgen-large", 0.25), ("audioldm2", 0.15)]
SECONDS = {"sao": 20.0, "musicgen-large": 20.0, "audioldm2": 20.0}
STEPS = {"sao": 80, "musicgen-large": 0, "audioldm2": 60}


def slugify(text, limit=44):
    s = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_")
    return (s[:limit] or "texture").rstrip("_")


def pick_model(rng):
    r = rng.random()
    acc = 0.0
    for key, w in MIX:
        acc += w
        if r < acc:
            return key
    return MIX[0][0]


def build(per_style, seed):
    jobs = []
    for si, st in enumerate(STYLES):
        rng = random.Random(seed * 1009 + si)
        seeds = st["prompts"] or ["ambient drone"]
        for k in range(per_style):
            base = seeds[k % len(seeds)]
            mod = MODIFIERS[rng.randrange(len(MODIFIERS))]
            prompt = base if not mod else f"{base}, {mod}"
            model = pick_model(rng)
            name = f"{slugify(st['name'], 20).lower()}_{slugify(base, 26).lower()}_{k:03d}"
            jobs.append({
                "prompt": prompt,
                "negative": NEGATIVE,
                "model": model,
                "seconds": SECONDS[model],
                "steps": STEPS[model] or 100,
                "guidance": round(rng.uniform(5.5, 8.5), 1),
                "seed": rng.randrange(1, 100000),
                "name": name,
                "style": st["name"],
            })
    return jobs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--per-style", type=int, default=48, help="clips per style (25 styles)")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--out", default=os.path.join(HERE, "texture_jobs.json"))
    a = ap.parse_args()
    jobs = build(a.per_style, a.seed)
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(jobs, f, indent=1)
    per_model = {}
    for j in jobs:
        per_model[j["model"]] = per_model.get(j["model"], 0) + 1
    print(f"{len(jobs)} jobs over {len(STYLES)} styles -> {a.out}")
    for k, v in sorted(per_model.items()):
        print(f"  {k:16s} {v}")


if __name__ == "__main__":
    main()
