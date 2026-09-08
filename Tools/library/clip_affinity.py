"""Which clips belong to which style, decided by listening rather than by file name.

The sample library used to be generated per style: every clip's name began with its style's slug,
and `texture_pool` in make_presets.py handed a style its own clips. Then the library grew from
prompt lists instead -- a thousand distinct ideas rather than forty variations of forty -- and the
names stopped saying anything about style, so every style fell back to drawing from one big shelf.
A Sleep Concert preset could be built on a pneumatic drill.

This puts it back, and better than the file name did: CLAP embeds every clip and every style's own
prompt sentences in one space, and each style keeps the clips that sit closest to what it says it
is. As with the preset phrases, the score is taken against the library rather than raw -- a clip
that scores high for every style says nothing about any of them -- so a style gets the clips that
are unusually its own.

    Tools/TextureGen/.venv/Scripts/python Tools/library/clip_affinity.py --out Library/affinity.json

Written once per library and read by make_presets.py; if the file is missing the generator falls
back to the whole shelf, which is what it did before.
"""
import argparse
import glob
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from styles import STYLES            # noqa: E402
from clap_embed import MODEL, SR, _tensor, load_model   # noqa: E402


def clips(folder, name):
    d = os.path.join(ROOT, "Library", folder)
    if not os.path.isdir(d):
        return []
    return sorted(name + "/" + os.path.basename(f) for f in glob.glob(os.path.join(d, "*.wav")))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(ROOT, "Library", "affinity.json"))
    ap.add_argument("--keep", type=int, default=200, help="clips per style and per folder")
    ap.add_argument("--batch", type=int, default=16)
    ap.add_argument("--seconds", type=float, default=10.0, help="how much of each clip CLAP hears")
    a = ap.parse_args()

    import torch
    import soundfile as sf
    device = "cuda" if torch.cuda.is_available() else "cpu"
    model, proc = load_model(device)
    print(f"CLAP ({MODEL}) on {device}", flush=True)

    # A style is described by its own prompt sentences, averaged: what it says it sounds like.
    names = [s["name"] for s in STYLES]
    texts, owner = [], []
    for i, s in enumerate(STYLES):
        for p in s.get("prompts", [])[:8]:
            texts.append(p)
            owner.append(i)
    with torch.no_grad():
        t = proc(text=texts, return_tensors="pt", padding=True).to(device)
        te = torch.nn.functional.normalize(_tensor(model.get_text_features(**t)), dim=-1)
    styleEmb = torch.zeros((len(names), te.shape[1]), device=device)
    for i, o in enumerate(owner):
        styleEmb[o] += te[i]
    styleEmb = torch.nn.functional.normalize(styleEmb, dim=-1)
    print(f"{len(names)} styles from {len(texts)} sentences", flush=True)

    out = {"styles": names, "byStyle": {}}
    for folder, label in (("Textures", "Textures"), ("FieldRecordings", "FieldRecordings")):
        files = clips(folder, label)
        if not files:
            continue
        print(f"{label}: {len(files)} clips", flush=True)
        sims = np.zeros((len(files), len(names)), dtype=np.float32)
        for start in range(0, len(files), a.batch):
            chunk = files[start:start + a.batch]
            waves = []
            for rel in chunk:
                path = os.path.join(ROOT, "Library", rel.replace("/", os.sep))
                x, sr = sf.read(path, dtype="float32", always_2d=False)
                if x.ndim > 1:
                    x = x.mean(axis=1)
                x = x[:int(sr * a.seconds)]
                if sr != SR:
                    idx = np.minimum((np.arange(int(len(x) * SR / sr)) * sr / SR).astype(np.int64),
                                     max(len(x) - 1, 0))
                    x = x[idx]
                waves.append(x if len(x) else np.zeros(SR, dtype="float32"))
            with torch.no_grad():
                try:
                    inp = proc(audio=waves, sampling_rate=SR, return_tensors="pt", padding=True).to(device)
                except (TypeError, ValueError):
                    inp = proc(audios=waves, sampling_rate=SR, return_tensors="pt", padding=True).to(device)
                emb = torch.nn.functional.normalize(_tensor(model.get_audio_features(**inp)), dim=-1)
                sims[start:start + len(chunk)] = (emb @ styleEmb.T).cpu().numpy()
            if (start // a.batch) % 25 == 0:
                print(f"  {start + len(chunk)}/{len(files)}", flush=True)
        # The lift over the library, per style: a clip that fits everything fits nothing.
        z = (sims - sims.mean(axis=0)) / (sims.std(axis=0) + 1e-9)
        for j, nm in enumerate(names):
            order = np.argsort(-z[:, j])[:a.keep]
            out["byStyle"].setdefault(nm, {})[label] = [files[int(i)] for i in order]
    json.dump(out, open(a.out, "w", encoding="utf-8"), indent=0)
    n = sum(len(v.get("Textures", [])) + len(v.get("FieldRecordings", [])) for v in out["byStyle"].values())
    print(f"wrote {a.out}: {len(out['byStyle'])} styles, {n} clip references", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
