"""What the library sounds like, according to a model that listened to it.

The nine descriptors say what a preset is LIKE and the mel-cepstral fingerprint says something of
what it IS, but neither knows that one of them is a goods yard and the other a beehive. CLAP does:
it is a contrastive model that puts audio and language in one space, so the distance between a
render and the sentence "a bell in a stone room" is a number.

Three things come out of it, from the twelve-second excerpts the measurement pass leaves behind
(measure_packs.py --taps):

    embedding    a 512-number fingerprint per preset, for the map's layout and its groups
    scores       this preset against every phrase in the vocabulary, so the caller can judge a
                 phrase against the rest of the library instead of on its raw score
    phrases      the best few by raw score, for reading the file by eye

CLAP cannot write a description; it can only choose from what we wrote, which is the honest half
of the bargain. The vocabulary lives in VOCAB below, and every line the browser shows can be
traced back to a sentence a person put there and a distance a model measured.

    python Tools/library/clap_embed.py --taps <dir> --out clap.json
"""
import argparse
import glob
import json
import os

import numpy as np

MODEL = "laion/clap-htsat-unfused"
SR = 48000

# The vocabulary, in groups. Written for this instrument: what a drone library actually contains,
# in the language a player would use looking for it. The groups are not decoration -- a preset's
# line takes at most one phrase from each, so it cannot come out as "dark and heavy, dark and
# weighted at the bottom", which is what a plain top-two does when a model is confident.
VOCAB = {
    "material": [
        "a bell struck once in a stone room", "a large bronze gong, the tail alone",
        "bowed metal, singing high harmonics", "piano strings brushed with a soft mallet",
        "glass rubbed until it sings", "a struck wooden block in a small room",
        "iron and rust, something industrial at rest", "a tuned metal bar left ringing",
        "a singing bowl held under the hand", "a cymbal bowed at the edge",
        "a length of steel pipe struck and left", "stone on stone, dry and hard",
        "a harp string ringing in an empty room", "a struck anvil, the ring after the blow",
    ],
    "voice": [
        "a choir humming one chord far away", "whispered voices in a corridor",
        "a single held vowel, no words", "throat singing with strong overtones",
        "breath through a flute with no note", "a distant crowd before a service",
        "a low male voice holding one note", "a high voice, thin and pure",
        "many voices slightly out of tune with one another", "a hummed note close to the ear",
    ],
    "place": [
        "wind over a frozen plateau", "rain on a roof at night, far away",
        "thunder rolling behind a mountain", "water moving under ice",
        "insects at dusk in a wide field", "a forest at dawn, birds a long way off",
        "waves on a shingle beach, slow", "a cave with water dripping",
        "a desert at noon, almost nothing moving", "a river heard from the bank",
        "a snowfield with no wind at all", "birds circling over a cliff",
        "a harbour at night, water against wood", "a moor in fog",
    ],
    "room": [
        "an enormous hall with a very long tail", "a small dry room, close and intimate",
        "a cathedral, the sound a long way up", "a concrete bunker, dark and boxy",
        "a plate reverb, metallic and even", "a stairwell, hard and ringing",
        "a wooden church, warm and short", "an empty swimming pool",
        "outdoors with nothing to reflect off", "a tunnel with a standing wave in it",
    ],
    "machine": [
        "a transformer humming in a cellar", "old machinery turning slowly",
        "a ventilation fan in a duct", "shortwave radio between stations",
        "a fluorescent tube buzzing", "tape hiss with music underneath",
        "a modular synthesiser self-oscillating", "a ship's engine heard through a wall",
        "a turbine spinning up a long way off", "a refrigerator in a quiet kitchen",
        "power lines in damp air", "a hard disk working in a quiet room",
        "an aircraft cabin at cruising height", "a distant motorway at night",
    ],
    "character": [
        "warm and consonant, nothing moving", "dark and heavy, weighted at the bottom",
        "bright and glassy, high and thin", "rough and beating, partials grinding",
        "smooth and fused, one single body of sound", "sparse and quiet, mostly silence",
        "dense and layered, many voices at once", "slowly evolving, never the same twice",
        "static and unchanging, a held drone", "wide and enveloping, all around the head",
        "narrow and central, one point in front", "very distant, almost out of earshot",
        "close enough to touch, breathing", "shimmering and unstable, a pitch that drifts",
        "granular, made of small pieces", "stretched beyond recognition, a smear of a sound",
        "pulsing slowly, a tide in and out", "clusters of events, then long silences",
        "a low sub under everything", "metallic resonance, clangorous and inharmonic",
        "organ-like, drawbars and air", "string-like, bowed and sustained",
        "choral, human and vowel-coloured", "noise-based, a coloured bed of air",
        "restless, always about to change", "hushed, as if heard through a wall",
        "harsh and forward, hard to ignore", "gentle and rounded, nothing sharp in it",
        "hollow, like a tube with air in it", "brittle, dry and glassy at the top",
    ],
}
PHRASES = [p for group in VOCAB.values() for p in group]
PHRASE_GROUP = [g for g, ps in VOCAB.items() for _ in ps]


def _tensor(x):
    """transformers 5 hands back an output object here where 4 returned the tensor itself."""
    for attr in ("pooler_output", "last_hidden_state", "text_embeds", "audio_embeds"):
        if hasattr(x, attr):
            v = getattr(x, attr)
            if v is not None:
                return v
    return x


def load_model(device):
    from transformers import ClapModel, ClapProcessor
    model = ClapModel.from_pretrained(MODEL).to(device).eval()
    proc = ClapProcessor.from_pretrained(MODEL)
    return model, proc


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--taps", required=True, help="folder of 12 s mono excerpts")
    ap.add_argument("--out", required=True, help="JSON: per preset an embedding, its scores and its phrases")
    ap.add_argument("--batch", type=int, default=8)
    ap.add_argument("--top", type=int, default=3)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--resume", action="store_true",
                    help="keep what the output file already holds and only do the rest")
    ap.add_argument("--device", default="auto", choices=("auto", "cuda", "cpu"),
                    help="cpu is several times slower and cannot take the machine down with it")
    ap.add_argument("--half", action="store_true",
                    help="fp16 on the card: 323 MB of weights instead of 589, and a batch of 8 "
                         "peaks at 427 MB instead of 807. Measured, not estimated. It is free for "
                         "what this is used for -- a cosine distance between unit vectors")
    a = ap.parse_args()

    import torch
    import soundfile as sf
    device = a.device if a.device != "auto" else ("cuda" if torch.cuda.is_available() else "cpu")
    model, proc = load_model(device)
    if a.half and device == "cuda":
        model = model.half()
    print(f"CLAP on {device}{' (fp16)' if a.half and device == 'cuda' else ''}: "
          f"{len(PHRASES)} phrases in {len(VOCAB)} groups", flush=True)

    with torch.no_grad():
        t = proc(text=PHRASES, return_tensors="pt", padding=True).to(device)
        textEmb = _tensor(model.get_text_features(**t))
        textEmb = torch.nn.functional.normalize(textEmb, dim=-1)

    files = sorted(glob.glob(os.path.join(a.taps, "*.wav")))
    if a.limit:
        files = files[:a.limit]
    # Written as it goes, and resumable. Eight thousand excerpts is a long enough job that losing
    # it whole to one interruption has already cost an afternoon twice.
    out = {}
    if a.resume and os.path.exists(a.out):
        try:
            out = json.load(open(a.out, encoding="utf-8")).get("presets", {})
        except (ValueError, OSError):
            out = {}
    done_names = set(out)
    files = [f for f in files if os.path.splitext(os.path.basename(f))[0] not in done_names]
    print(f"{len(files)} excerpts to do" + (f", {len(out)} already there" if out else ""), flush=True)

    def flush():
        tmp = a.out + ".part"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump({"phrases": PHRASES, "groups": PHRASE_GROUP, "presets": out}, f)
        os.replace(tmp, a.out)
    for start in range(0, len(files), a.batch):
        chunk = files[start:start + a.batch]
        waves = []
        for f in chunk:
            x, sr = sf.read(f, dtype="float32", always_2d=False)
            if x.ndim > 1:
                x = x.mean(axis=1)
            # CLAP wants 48 kHz; the excerpts are 24 kHz, so every sample is doubled -- linear
            # interpolation would be kinder but the model's own front end is a mel filterbank.
            if sr != SR:
                idx = np.minimum((np.arange(int(len(x) * SR / sr)) * sr / SR).astype(np.int64), len(x) - 1)
                x = x[idx]
            waves.append(x)
        with torch.no_grad():
            # transformers 5 renamed the argument; 4 only knew the old name.
            try:
                inp = proc(audio=waves, sampling_rate=SR, return_tensors="pt", padding=True).to(device)
            except (TypeError, ValueError):
                inp = proc(audios=waves, sampling_rate=SR, return_tensors="pt", padding=True).to(device)
            if a.half and device == "cuda":
                inp = {k: (v.half() if hasattr(v, "is_floating_point") and v.is_floating_point() else v)
                       for k, v in inp.items()}
            emb = _tensor(model.get_audio_features(**inp)).float()
            emb = torch.nn.functional.normalize(emb, dim=-1)
            sim = emb @ textEmb.T
            best = sim.topk(a.top, dim=-1)
        for i, f in enumerate(chunk):
            name = os.path.splitext(os.path.basename(f))[0]
            out[name] = {
                "embedding": [round(float(v), 5) for v in emb[i].tolist()],
                "scores": [round(float(v), 5) for v in sim[i].tolist()],
                "phrases": [[PHRASES[int(j)], round(float(s), 4)]
                            for j, s in zip(best.indices[i].tolist(), best.values[i].tolist())],
            }
        if (start // a.batch) % 20 == 0:
            print(f"  {start + len(chunk)}/{len(files)}", flush=True)
            flush()
    flush()
    print(f"wrote {a.out}: {len(out)} presets", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
