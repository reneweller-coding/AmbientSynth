"""Field recordings for the Stretch and Texture types: prompts, generation, and the seam.

Text-to-audio for environments rather than instruments -- rain on different roofs, a forest at
four in the morning, a harbour in fog, a cave with water in it, a power station heard through a
wall. Twelve categories, each with a handful of prompt seeds, crossed with weather, distance and
time-of-day modifiers so five hundred clips are five hundred places and not fifty repeated.

    python Tools/library/make_field_recordings.py --jobs                 # write the batch
    python Tools/library/make_field_recordings.py --run --max-minutes 150 # generate (Stable Audio Open)
    python Tools/library/make_field_recordings.py --finish               # make every clip seamless

Two decisions the Stretch type makes for this script.

Short clips. A clip read at forty times slower than life is forty times longer than it is, so
twenty-four seconds is a quarter of an hour of weather and a minute would be a gigabyte the
package does not need. 26 s from the model, 24.5 after the seam.

Every clip is seamless, by construction. The last 1.5 s are faded into the first 1.5 s
(equal power) and the overlap trimmed, so the end of the file runs into its start with no
discontinuity, and the file is named `_loop` -- which is the mark the Stretch type reads to skip
its own crossfade. Made offline this costs nothing and is exact; the alternative, hoping the
model produces a loop, does not exist. The seam is measured afterwards, not assumed: the largest
sample-to-sample step across the wrap is reported, and a clip whose step is bigger than what the
clip's own interior shows is listed rather than shipped.

The files land in Library/Textures with the prefix `field_recordings_`, which is the slug the
preset generator uses to find a style's own clips, so the Field Recordings style picks them up
with nothing else to configure.
"""
import argparse
import glob
import json
import os
import random
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORKER = os.path.join(ROOT, "Tools", "TextureGen", "texturegen_worker.py")
PYTHON = os.path.join(ROOT, "Tools", "TextureGen", ".venv", "Scripts", "python.exe")
OUT = os.path.join(ROOT, "Library", "Textures")
STYLE_SLUG = "field_recordings"      # what the preset generator looks for
SECONDS = 26.0
SEAM_SECONDS = 1.5

# ------------------------------------------------------------------ the places
# Each category: a list of prompt seeds. Concrete nouns, a distance, a surface -- the models
# answer to specifics, and "ambient nature sound" gives you the same clip every time.
CATEGORIES = {
    "rain": [
        "steady rain on a corrugated tin roof, heard from inside a shed",
        "light rain on broad leaves in a tropical garden, distant thunder rolling",
        "rain on the canvas of a tent at night, drops sliding off",
        "heavy rain on a city street at night, tyres hissing past far away",
        "drizzle on a still lake, tiny drops everywhere, no wind",
        "rain running down a drainpipe into a barrel, dripping and gurgling",
    ],
    "wind": [
        "wind over a high moor, grass hissing, nothing else for miles",
        "wind moaning through a gap in an old window frame",
        "wind through dry reeds by a marsh, rattling stalks",
        "strong wind at the top of a mountain pass, gusting and fading",
        "wind singing in power lines across a plain, a low hum rising and falling",
        "wind in tall pines, a deep rushing that swells and settles",
    ],
    "water": [
        "a slow river over stones, close and continuous, no birds",
        "small waves lapping against the wooden hull of a moored boat",
        "a mountain stream in a rocky gorge, water everywhere, echoing",
        "the sea at night on a shingle beach, waves dragging the stones back",
        "dripping water in a flooded cellar, drops falling into deep water with echo",
        "a fountain in a stone courtyard at night, steady splashing",
    ],
    "forest": [
        "a dense forest at dawn, distant birds, leaves, a faint breeze",
        "a pine forest at night, creaking trunks, an owl far away",
        "a rainforest canopy, insects droning, a distant frog",
        "a beech wood in autumn, dry leaves stirring, crows far off",
        "deep jungle after rain, dripping, insect chorus",
        "a birch grove in winter, ice creaking, nearly silent",
    ],
    "cave": [
        "inside a limestone cave, water dripping into a pool with long echoes",
        "a deep mine shaft, air moving, distant rumble of the earth",
        "a sea cave with waves booming inside, resonant and hollow",
        "a huge underground cistern, every drip ringing for seconds",
        "an ice cave, cracking and groaning, wind at the mouth",
        "a stone crypt with a stream running under the floor",
    ],
    "city_far": [
        "a city heard from a rooftop at four in the morning, traffic as a distant sea",
        "a motorway heard from a hill a mile away, a constant hush",
        "a harbour in fog at night, foghorns, water, distant engines",
        "a train yard at night from far away, couplings and a long horn",
        "an airport heard from a field, jets rising and fading slowly",
        "a distant thunderstorm over a city, rolling thunder and faint rain",
    ],
    "industrial": [
        "a power station hum heard through a concrete wall, deep and steady",
        "an empty factory hall with a ventilation system running, huge space",
        "a ship's engine room from a corridor, throbbing low and constant",
        "a substation buzzing in a field at night, insects around it",
        "an old refrigerator warehouse, compressors cycling, fluorescent hum",
        "a wind turbine close up, blades sweeping slowly, gearbox whine",
    ],
    "interior": [
        "a large empty church at night, a heating system ticking, faint traffic outside",
        "an old library, a clock ticking, pages, distant footsteps on stone",
        "a greenhouse in the rain, glass roof, warm humid stillness",
        "an attic in a storm, wind pulling at the roof, timbers creaking",
        "an empty swimming pool hall, water lapping, huge reverberation",
        "a basement boiler room, pipes knocking, a steady burner",
    ],
    "desert": [
        "a desert at night, wind over sand, absolute stillness between gusts",
        "a dry salt flat at noon, heat shimmer, a single insect, nothing else",
        "a canyon with wind funnelled through it, a low howl",
        "a dust storm approaching across a plain, rising roar",
        "a desert oasis at dusk, palms rustling, a trickle of water, frogs",
        "sand dunes singing, a deep resonant boom as sand slides",
    ],
    "arctic": [
        "pack ice groaning and cracking, distant deep booms",
        "an arctic wind over a frozen sea, ice crystals hissing",
        "a glacier calving far away, then silence, then meltwater",
        "a frozen lake at night, ice singing and pinging",
        "a blizzard against a wooden cabin, muffled and relentless",
        "an aurora night, total stillness, a far dog, snow settling",
    ],
    "night": [
        "a summer meadow at night, crickets, a far-off owl, no wind",
        "a swamp at night, frogs, insects, bubbles rising from the mud",
        "a village at night, a dog, a distant bell, wind in a hedge",
        "a coast at night, surf far below, gulls asleep, wind",
        "a savannah at night, insects, distant animals calling",
        "a mountain hut at night, snow sliding off the roof, a stove ticking",
    ],
    "machines_soft": [
        "an old analogue tape machine idling, motor hum, transport ticking",
        "a mechanical clock shop, dozens of clocks ticking out of phase",
        "a sewing machine room, several machines whirring, muffled",
        "a lighthouse machinery room, the lamp mechanism turning slowly",
        "a mill wheel turning, water and wood, creaking rhythm blurred",
        "a radio between stations, static and faint voices from far away",
    ],
}

# Crossed with the seeds. Weather, distance and hour change the place without changing it into
# another category.
MODIFIERS = [
    "", "", "",
    "recorded from far away", "recorded very close, intimate", "at dawn", "at dusk", "at midnight",
    "in thick fog", "after heavy rain", "in the heat of the afternoon", "in freezing cold",
    "with a slow, steady character, nothing sudden", "very quiet, almost inaudible",
    "wide stereo field", "with a deep low rumble underneath", "long and unchanging",
]
NEGATIVE = ("music, melody, drums, beat, rhythm, speech, talking, singing, vocals, instruments, "
            "synthesizer, sudden loud events, clipping, distortion")

MODEL = "sao"        # Stable Audio Open 1.0: 44.1 kHz stereo, up to 47 s, the best of the three for places


def slugify(text, limit=32):
    s = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_")
    return (s[:limit] or "place").rstrip("_").lower()


def build(count, seed):
    """`count` jobs, round-robin over the categories so every place gets its share."""
    rng = random.Random(seed)
    cats = list(CATEGORIES.keys())
    jobs = []
    k = 0
    per_cat = {c: 0 for c in cats}
    while len(jobs) < count:
        cat = cats[k % len(cats)]
        seeds = CATEGORIES[cat]
        base = seeds[per_cat[cat] % len(seeds)]
        per_cat[cat] += 1
        mod = MODIFIERS[rng.randrange(len(MODIFIERS))]
        prompt = base if not mod else f"{base}, {mod}"
        jobs.append({
            "prompt": prompt,
            "negative": NEGATIVE,
            "model": MODEL,
            "seconds": SECONDS,
            "steps": 90,
            "guidance": round(rng.uniform(6.0, 8.0), 1),
            "seed": rng.randrange(1, 1000000),
            "name": f"{STYLE_SLUG}_{cat}_{slugify(base, 26)}_{len(jobs):03d}",
            "style": "Field Recordings",
            "category": cat,
        })
        k += 1
    return jobs


# ------------------------------------------------------------------ the seam

def read_wav(path):
    """(frames x channels float32, rate). Plain RIFF, 16/24/32-bit PCM or 32-bit float -- no
    soundfile, so the finish step runs on whatever Python is at hand, not only the worker's."""
    import struct
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a WAV: " + path)
    pos, fmt = 12, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if tag == 0xFFFE and len(body) >= 40:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, ch, rate, bits)
        elif cid == b"data" and fmt is not None:
            tag, ch, rate, bits = fmt
            if tag == 3 and bits == 32:
                x = np.frombuffer(body[:len(body) // 4 * 4], dtype="<f4").astype(np.float32)
            elif tag == 1 and bits == 16:
                x = np.frombuffer(body[:len(body) // 2 * 2], dtype="<i2").astype(np.float32) / 32768.0
            elif tag == 1 and bits == 24:
                b = np.frombuffer(body[:len(body) // 3 * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
                v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
                v = np.where(v >= 1 << 23, v - (1 << 24), v)
                x = v.astype(np.float32) / 8388608.0
            elif tag == 1 and bits == 32:
                x = np.frombuffer(body[:len(body) // 4 * 4], dtype="<i4").astype(np.float32) / 2147483648.0
            else:
                raise ValueError("unsupported WAV format in " + path)
            return x.reshape(-1, ch), rate
        pos += 8 + size + (size & 1)
    raise ValueError("no data chunk in " + path)


def write_wav(path, audio, sr):
    """32-bit float, which is what the worker writes and what the content packer converts."""
    import struct
    audio = np.ascontiguousarray(audio.astype("<f4"))
    ch = audio.shape[1]
    payload = audio.tobytes()
    header = (b"RIFF" + struct.pack("<I", 36 + len(payload)) + b"WAVEfmt "
              + struct.pack("<IHHIIHH", 16, 3, ch, sr, sr * ch * 4, ch * 4, 32)
              + b"data" + struct.pack("<I", len(payload)))
    with open(path, "wb") as f:
        f.write(header + payload)


def make_seamless(audio, sr, seconds=SEAM_SECONDS):
    """Fade the tail into the head (equal power) and drop the overlap. Returns the new clip."""
    n = int(seconds * sr)
    if audio.shape[0] < 4 * n:
        return None
    head = audio[:n]
    tail = audio[-n:]
    t = np.linspace(0.0, 1.0, n, endpoint=False, dtype=np.float32)[:, None]
    # equal power: the two halves keep the level constant through the fade
    blended = tail * np.cos(0.5 * np.pi * t) + head * np.sin(0.5 * np.pi * t)
    # the loop body: blended seam, then everything between head and tail
    return np.concatenate([blended, audio[n:-n]], axis=0)


def seam_step(audio):
    """The largest sample-to-sample step across the wrap, against the clip's own interior."""
    wrap = float(np.max(np.abs(audio[0] - audio[-1])))
    interior = float(np.max(np.abs(np.diff(audio, axis=0))))
    return wrap, interior


def finish(out_dir, report):
    files = sorted(f for f in glob.glob(os.path.join(out_dir, STYLE_SLUG + "_*.wav")) if "_loop" not in os.path.basename(f))
    done, bad, small = 0, [], []
    for path in files:
        audio, sr = read_wav(path)
        looped = make_seamless(audio, sr)
        if looped is None:
            small.append(os.path.basename(path))
            continue
        wrap, interior = seam_step(looped)
        stem = os.path.splitext(path)[0]
        # The engine reads the clip's pitch from the LAST name token ("_A3"), so a tonal clip
        # that the worker has named keeps that token last: the loop mark goes in front of it.
        m = re.search(r"_[A-G]#?-?\d+$", stem)
        newstem = (stem[:m.start()] + "_loop" + stem[m.start():]) if m else stem + "_loop"
        new = newstem + ".wav"
        write_wav(new, looped, sr)
        # the worker's sidecar text travels with the clip
        if os.path.isfile(stem + ".txt"):
            os.replace(stem + ".txt", newstem + ".txt")
        os.remove(path)
        if wrap > interior * 1.05:
            bad.append((os.path.basename(new), wrap, interior))
        done += 1
    print("seamless: %d clips; too short: %d; seam larger than the clip's own steps: %d" % (done, len(small), len(bad)))
    for name, w, i in bad[:10]:
        print("   %s  wrap %.4f  interior %.4f" % (name, w, i))
    if report:
        with open(report, "w", encoding="utf-8") as f:
            json.dump({"seamless": done, "too_short": small, "bad_seam": bad}, f, indent=1)


# ------------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", action="store_true", help="write the batch file")
    ap.add_argument("--run", action="store_true", help="run the batch through TextureGen's worker")
    ap.add_argument("--finish", action="store_true", help="make every generated clip seamless")
    ap.add_argument("--count", type=int, default=500)
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--out-dir", default=OUT)
    ap.add_argument("--batch", default=os.path.join(HERE, "field_jobs.json"))
    ap.add_argument("--max-minutes", type=float, default=0.0)
    ap.add_argument("--report", default=os.path.join(HERE, "field_report.json"))
    a = ap.parse_args()

    if a.jobs:
        jobs = build(a.count, a.seed)
        for j in jobs:
            j["out_dir"] = a.out_dir
        with open(a.batch, "w", encoding="utf-8") as f:
            json.dump(jobs, f, indent=1)
        cats = {}
        for j in jobs:
            cats[j["category"]] = cats.get(j["category"], 0) + 1
        print("%d jobs -> %s" % (len(jobs), a.batch))
        for c, n in sorted(cats.items()):
            print("   %-14s %d" % (c, n))
    if a.run:
        cmd = [PYTHON, WORKER, "--batch", a.batch, "--out-dir", a.out_dir, "--resume", "--nice"]
        if a.max_minutes > 0:
            cmd += ["--max-minutes", str(a.max_minutes)]
        print("running:", " ".join(cmd), flush=True)
        sys.exit(subprocess.call(cmd, cwd=ROOT))
    if a.finish:
        finish(a.out_dir, a.report)


if __name__ == "__main__":
    main()
