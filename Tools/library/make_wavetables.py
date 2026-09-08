"""Build the wavetable library for the generated presets, from two sources.

*Recipes.* Every recipe in Tools/WavetableGen is rendered several times with different partial
counts, spectral noise and phase scatter. Fast and deterministic: the same seed always writes the
same files, which is what lets the preset generator name a table before it exists.

*Clips.* There are only nineteen recipes, so a shelf built from them alone is nineteen ideas with
variations -- thirty-two neighbours of one theme, which is what it sounds like. The tonal sample
library is thousands of different recorded sounds, and every clip with a stable pitch can be sliced
into single cycles and stacked into a table (Tools/WavetableGen, the same code its GUI uses). A
table made from a bowed cymbal and one made from a reed organ are not variations of anything.

    python Tools/library/make_wavetables.py --per-recipe 16 --from-clips 2

Both halves land in Library/Wavetables as 32-frame 16-bit tables of about 130 kB. --from-clips N
takes at most N clips per prompt idea, so the shelf spreads over ideas rather than over the four
seeds of whichever prompt happened to come first.

Slicing needs soundfile, which lives in the clip generator's environment:

    Tools/TextureGen/.venv/Scripts/python Tools/library/make_wavetables.py --from-clips 2
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


PITCHED = re.compile(r"_([A-G]#?-?[0-9])\.wav$")

# Not every pitched clip makes a wavetable worth having. What a drone wants out of a table is an
# inharmonic or resonant spectrum -- struck metal, bowed strings, voices, glass, reeds -- and what
# it does not want is a siren or a motor whose "pitch" is a single steady partial: sliced, that is
# a sine with extra steps. So the clips are chosen by what they are of, from the words the prompt
# lists are written in.
WANTED = re.compile("|".join([
    # struck and rubbed metal, and everything with an inharmonic ring
    "bell", "gong", "bowl", "chime", "carillon", "tubular", "crotale", "cymbal", "tam_tam",
    "anvil", "triangle", "tingsha", "almglocken", "handpan", "hang_drum", "steel_tongue",
    "metallophone", "lithophone", "glockenspiel", "vibraphone", "celesta", "marimba", "xylophone",
    "kalimba", "mbira", "sansula", "music_box", "tuning_fork", "spring", "saw_blade", "slinky",
    # bowed things: rich high noise over a warm body
    "bowed", "arco", "cello", "violin", "viola", "double_bass", "contrabass", "fiddle",
    "hurdy", "nyckelharpa", "erhu", "sarangi", "kemenche", "rebab", "gamba", "psaltery",
    "musical_saw", "morin", "dilruba", "gudok", "tromba", "crwth", "jouhikko", "talharpa",
    "gadulka", "lyra", "ghaychak", "hardanger", "lira", "viola_d_amore", "violoncello",
    # plucked and struck strings, and prepared ones
    "piano", "clavichord", "harpsichord", "fortepiano", "harp", "lyre", "zither", "koto",
    "guzheng", "guqin", "kora", "oud", "lute", "theorbo", "cittern", "dulcimer", "cimbalom",
    "santur", "santoor", "yangqin", "qanun", "kantele", "pipa", "shamisen", "biwa", "sitar",
    "tambura", "saz", "setar", "dutar", "rubab", "charango", "balalaika", "domra", "bandura",
    # voices and formants
    "choir", "chant", "voice", "vocal", "humming", "hummed", "singing", "vowel", "throat",
    "overtone", "countertenor", "contralto", "soprano", "falsetto", "keening", "muezzin", "monk",
    # air through a body: reeds, organs, flutes
    "organ", "harmonium", "accordion", "bandoneon", "concertina", "sheng", "shruti", "khaen",
    "melodica", "harmonica", "reed", "shawm", "crumhorn", "duduk", "zurna", "bagpipe", "chanter",
    "flute", "ney", "shakuhachi", "bansuri", "recorder", "ocarina", "panpipe", "whistle_organ",
    "clarinet", "oboe", "bassoon", "saxophone", "horn", "trumpet", "trombone", "tuba", "alphorn",
    "didgeridoo", "conch", "serpent", "euphonium", "flugelhorn", "cornet",
    # glass, stone, wood, and the electric instruments with a spectrum of their own
    "glass", "crystal", "armonica", "porcelain", "ceramic", "terracotta", "stone", "slate",
    "marble", "wooden", "wood_block", "log_drum", "slit", "bamboo", "udu", "ghatam",
    "mellotron", "chamberlin", "optigan", "rhodes", "wurlitzer", "clavinet", "pianet", "hammond",
    "leslie", "tonewheel", "ondes", "trautonium", "theremin", "ondioline", "telharmonium",
    "resonat", "e_bow", "ebow", "karplus", "vocoder", "formant", "talkbox",
]), re.I)


def clip_ideas(folder, per_idea, limit, any_clip=False):
    """Pitched clips, grouped by the prompt they came from. A clip's name is the prompt slug, the
    model, the seed and the note, so everything up to the model is the idea -- and four clips of
    one idea would make four tables that are neighbours again."""
    ideas = {}
    for f in sorted(os.listdir(folder)):
        if not PITCHED.search(f) or not (any_clip or WANTED.search(f)):
            continue
        key = re.sub(r"_(sao|musicgen-large|musicgen-medium|musicgen-small|audioldm2)_.*$", "", f)
        ideas.setdefault(key, []).append(os.path.join(folder, f))
    picked = []
    for key in sorted(ideas):
        picked.extend(ideas[key][:per_idea])
    if limit and len(picked) > limit:
        # Thin evenly across the ideas rather than cutting the alphabet off halfway.
        step = len(picked) / float(limit)
        picked = [picked[int(i * step)] for i in range(limit)]
    return picked, len(ideas)


def build_from_clips(files, frames, out_dir, int16):
    """One table per clip, sliced out of it. A clip whose pitch is not steady enough is skipped:
    a table built from frames that were never the same note is noise with a period."""
    sys.path.insert(0, os.path.join(ROOT, "Tools", "WavetableGen"))
    import wavetablegen_core as wt
    os.makedirs(out_dir, exist_ok=True)
    written, skipped, failed = [], 0, 0
    for f in files:
        try:
            mono, sr = wt.read_wav_mono(f)
            table, info = wt.table_from_audio(mono, sr, frames=frames)
            if info["voiced"] < 0.6:
                skipped += 1
                continue
            base = "clip_" + wt.slugify(os.path.splitext(os.path.basename(f))[0], 56)
            if not base.endswith("_" + info["note"]):
                base += "_" + info["note"]
            info["file"] = os.path.basename(f)
            written.append(save_table(os.path.join(out_dir, base + ".wav"), table, info, float32=not int16))
        except Exception as e:                        # a short or silent clip: not worth a stack trace
            failed += 1
            if failed <= 5:
                print(f"  skip {os.path.basename(f)}: {type(e).__name__}: {e}")
    return written, skipped, failed


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
            # save_table writes the table and a little note beside it. The note is expendable --
            # sort_clips folds them all into one index anyway -- so a scanner holding one open
            # must not take a thousand tables down with it.
            try:
                save_table(path, table, {"recipe": recipe, "partials": partials, "noise": noise,
                                         "phase_scatter": scatter, "seed": tseed}, float32=not int16)
            except OSError as e:
                print(f"  note not written for {os.path.basename(path)}: {e}")
                if not os.path.exists(path):
                    continue
            written.append(path)
    return written, names


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--per-recipe", type=int, default=32)
    ap.add_argument("--from-clips", type=int, default=0,
                    help="also slice a table out of the tonal clips: at most this many per prompt idea")
    ap.add_argument("--clips", default=os.path.join(ROOT, "Library", "Textures"))
    ap.add_argument("--any-clip", action="store_true",
                    help="do not filter by what the clip is of (see WANTED)")
    ap.add_argument("--max-clips", type=int, default=1600, help="ceiling on the sliced tables")
    ap.add_argument("--frames", type=int, default=32)
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--out-dir", default=os.path.join(ROOT, "Library", "Wavetables"))
    ap.add_argument("--float32", action="store_true", help="32-bit float instead of 16-bit PCM (4x the size)")
    a = ap.parse_args()
    written, names = build(a.per_recipe, a.frames, a.out_dir, a.seed, not a.float32)
    print(f"{len(written)} wavetables from {len(names)} recipes -> {a.out_dir}")
    for n in names:
        print(f"  {n}")
    if a.from_clips > 0:
        files, ideas = clip_ideas(a.clips, a.from_clips, a.max_clips, a.any_clip)
        print(f"clips: {len(files)} of {ideas} ideas in {a.clips}", flush=True)
        made, skipped, failed = build_from_clips(files, a.frames, a.out_dir, not a.float32)
        print(f"{len(made)} wavetables sliced from clips "
              f"({skipped} not steady enough, {failed} unreadable) -> {a.out_dir}")


if __name__ == "__main__":
    main()
