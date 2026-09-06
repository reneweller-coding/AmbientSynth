"""Fit what the instrument grew into the presets it already had.

A synthesiser that gains a feature gains it for the patches written afterwards; six thousand
finished presets go on sounding like the instrument of the day they were made. This pass walks
the library and gives each preset the new things that suit it -- and only those, judged from the
preset's own settings rather than from its style:

    hands       aftertouch, the wheel and the slide as routes in the matrix. Written with the
                0..1 flag, so a preset carrying them sounds exactly as it did until a hand moves.
                This one is free and goes almost everywhere.
    far width   the background pulled in towards the centre while the foreground stays wide --
                the funnel that reads as distance. Only where there IS a background and the
                preset is deep enough for the depth to matter.
    haas        the 1.2-4 kHz band delayed into the side channel: width where the ear takes its
                direction from level. Only where there is a foreground to widen, and never on a
                preset that is already at the edges.
    microshift  the Ensemble as a static detune instead of a chorus. Only where the chorus is
                slow and quiet enough that it was being used as a widener in the first place --
                a fast, deep chorus is an effect somebody chose to hear, and it stays.
    fold        the wavefolder, small, and only where the preset already asked for saturation.
    a place     a field recording, spectrally stretched, under a preset that has a source slot
                free and belongs to a style about somewhere rather than something. Quiet: it is
                a floor under the patch, not a new voice in it.

    python Tools/library/retrofit_presets.py            # what it would do
    python Tools/library/retrofit_presets.py --write    # do it

Deterministic: the seed is the preset's own name, so the same library always gets the same
treatment, and running it twice changes nothing the second time.

Afterwards the gains are wrong by a decibel or two, because the sound has changed. Run
    python Tools/library/measure_packs.py --jobs 6
    python Tools/library/verify_packs.py
which is what the library does after any change to what it sounds like.
"""
import argparse
import glob
import hashlib
import io
import os
import random
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

# Packs whose subject is a place rather than an instrument: these are the ones a field recording
# belongs under. The rest keep their sources.
PLACE_PACKS = {
    "Field-Absence", "Permafrost", "Deep-Earth", "Millstone", "Hull-Rumble", "Corridor",
    "Northern-Dark", "Void-Station", "Chamber-Grey", "Ghost-Signal", "Ritual-Machine",
    "Slow-Carousel", "Aldebaran", "Loop-Studio", "Tape-Saturation", "Desert-Ember",
}

HAND_TARGETS = {
    "pressure": [("cutoff", 0.20, 0.45), ("brightness", 0.15, 0.35), ("z_x", 0.15, 0.40),
                 ("resonance", 0.10, 0.30), ("far_level", 0.10, 0.25), ("shimmer", 0.15, 0.40)],
    "wheel":    [("cloud_send", 0.20, 0.50), ("cosmos_send", 0.20, 0.50), ("far_level", 0.15, 0.40),
                 ("dly_mix", 0.15, 0.40), ("z_y", 0.20, 0.50), ("air", 0.15, 0.40),
                 ("blur_mix", 0.20, 0.50), ("filter_fold", 0.20, 0.60)],
    "slide":    [("z_y", 0.15, 0.40), ("inharmonic", 0.15, 0.40), ("odd_even", 0.15, 0.40),
                 ("tilt", 0.15, 0.35), ("cosmos_vowel", 0.20, 0.50)],
}
# Targets every preset has, whether or not it names them in its settings.
ALWAYS = {"cutoff", "brightness", "air", "tilt", "shimmer", "resonance", "far_level", "odd_even",
          "inharmonic", "filter_fold"}


def parse_settings(text):
    """key=value;key=value -> an ordered dict. Values stay strings: they are written back."""
    out = {}
    for part in text.split(";"):
        part = part.strip()
        if not part or "=" not in part:
            continue
        k, v = part.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def num(settings, key, default):
    try:
        return float(settings[key])
    except (KeyError, ValueError):
        return default


def rng_for(name):
    """One stream per preset, keyed on its name: the run is reproducible and order-independent."""
    return random.Random(int(hashlib.sha1(name.encode("utf-8")).hexdigest()[:12], 16))


def field_clips(texture_dir):
    if not os.path.isdir(texture_dir):
        return []
    rejected = set()
    rej = os.path.join(texture_dir, "rejected.txt")
    if os.path.isfile(rej):
        rejected = {l.strip() for l in io.open(rej, encoding="utf-8") if l.strip()}
    return [f for f in sorted(os.listdir(texture_dir))
            if f.startswith("field_recordings_") and f.endswith(".wav") and f not in rejected]


def free_slot(settings):
    """The first source slot that is switched off, 2..4. Slot 1 is never touched: it is the
    preset's own voice, and on most of them it is the strand bank."""
    for k in (2, 3, 4):
        t = settings.get("src%d_type" % k, "Off")
        if t == "Off" or num(settings, "src%d_level" % k, 0.0) <= 0.0:
            return k
    return None


def retrofit(name, settings, matrix, texture, pack, clips, counts):
    """Returns (settings, matrix, texture). Mutates counts."""
    r = rng_for(name)
    changed_texture = texture

    # ---- the hands: nearly free, and the reason the matrix exists
    rows = [x for x in matrix.split(";") if x] if matrix else []
    if len(rows) < 15 and r.random() < 0.85:
        used = {x.split(">")[1].split(":")[0] for x in rows if ">" in x}
        for source in r.sample(["pressure", "wheel", "slide"], k=r.choice([1, 1, 2])):
            if any(x.startswith(source + ">") for x in rows) or len(rows) >= 16:
                continue
            choices = [(t, lo, hi) for t, lo, hi in HAND_TARGETS[source]
                       if (t in settings or t in ALWAYS) and t not in used]
            if not choices:
                continue
            t, lo, hi = choices[r.randrange(len(choices))]
            rows.append("%s>%s:%.3f:u" % (source, t, r.uniform(lo, hi)))
            used.add(t)
            counts["hands"] += 1

    # ---- the funnel: a background narrower than the foreground
    far = num(settings, "far_level", 0.8)
    depth = num(settings, "depth", 0.5)
    if "far_width" not in settings and far > 0.35 and depth > 0.45 and r.random() < 0.7:
        settings["far_width"] = "%.3f" % (r.uniform(0.55, 0.9) - 0.15 * depth)
        counts["far_width"] += 1

    # ---- the Haas band, where there is a foreground to widen and it is not already at the edges
    width = num(settings, "width", 1.0)
    near = num(settings, "near_mix", 0.0)
    if "haas" not in settings and width < 1.25 and (near > 0.05 or num(settings, "dly_mix", 0.0) > 0.1) \
            and r.random() < 0.35:
        settings["haas"] = "%.3f" % r.uniform(0.12, 0.35)
        settings["haas_time"] = "%.1f" % r.uniform(11.0, 20.0)
        counts["haas"] += 1

    # ---- the microshift, where the chorus was a widener rather than an effect
    ens = num(settings, "ens_mix", 0.0)
    ens_rate = num(settings, "ens_rate", 0.2)
    ens_depth = num(settings, "ens_depth", 0.4)
    if "ens_mode" not in settings and 0.1 < ens < 0.6 and ens_rate < 0.35 and ens_depth < 0.7 \
            and r.random() < 0.5:
        settings["ens_mode"] = "Microshift"
        settings["ens_depth"] = "%.3f" % r.uniform(0.35, 1.0)
        settings["ens_rate"] = "%.4f" % (10 ** r.uniform(-1.7, -1.05))   # 0.02 .. 0.09 Hz: the knob's own floor
        counts["microshift"] += 1

    # ---- the folder, only where saturation was already wanted
    drive = max(num(settings, "filter_drive", 0.0), num(settings, "fb_drive", 0.0),
                num(settings, "patina", 0.0) * 0.5)
    if "filter_fold" not in settings and drive > 0.2 and r.random() < 0.45:
        settings["filter_fold"] = "%.3f" % r.uniform(0.08, 0.3)
        counts["fold"] += 1

    # ---- a place under the patch
    slot = free_slot(settings)
    if clips and pack in PLACE_PACKS and slot is not None and not texture and r.random() < 0.3:
        clip = clips[r.randrange(len(clips))]
        settings["src%d_type" % slot] = "Stretch"
        settings["src%d_level" % slot] = "%.3f" % r.uniform(0.12, 0.3)
        settings["src%d_grain" % slot] = "%.1f" % r.uniform(160.0, 320.0)
        settings["src%d_stretch" % slot] = "%.1f" % (10 ** r.uniform(1.0, 2.3))
        settings["src%d_pos" % slot] = "%.3f" % r.uniform(0.0, 0.8)
        settings["src%d_pos_drift" % slot] = "%.3f" % r.uniform(0.05, 0.4)
        settings["src%d_xfade" % slot] = "%.3f" % r.uniform(0.05, 0.25)
        settings["src%d_follow" % slot] = "Free"
        settings["src%d_pan" % slot] = "%.3f" % r.uniform(-0.5, 0.5)
        # One clip for every slot: these presets have one source of the kind, so the slotless
        # form is what they mean.
        changed_texture = "../Textures/" + clip
        counts["place"] += 1

    return settings, ";".join(rows), changed_texture


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", default=os.path.join(ROOT, "Library", "Packs"))
    ap.add_argument("--textures", default=os.path.join(ROOT, "Library", "Textures"))
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--only", default="", help="only packs whose file name contains this")
    a = ap.parse_args()

    clips = field_clips(a.textures)
    counts = {"hands": 0, "far_width": 0, "haas": 0, "microshift": 0, "fold": 0, "place": 0}
    files = sorted(glob.glob(os.path.join(a.packs, "*.ambientpack")))
    touched = 0
    for path in files:
        pack = os.path.splitext(os.path.basename(path))[0]
        if a.only and a.only not in pack:
            continue
        lines = io.open(path, encoding="utf-8").read().split("\n")
        out = []
        for line in lines:
            if line.startswith("#") or line.startswith("pack ") or "|" not in line:
                out.append(line)
                continue
            cols = line.split("|")
            if len(cols) < 7:
                out.append(line)
                continue
            name, settings_text = cols[0], cols[1]
            settings = parse_settings(settings_text)
            before = (dict(settings), cols[6], cols[3])
            settings, matrix, texture = retrofit(name, settings, cols[6], cols[3], pack, clips, counts)
            if (settings, matrix, texture) == before:
                out.append(line)
                continue
            touched += 1
            cols[1] = ";".join("%s=%s" % (k, v) for k, v in settings.items())
            cols[6] = matrix
            cols[3] = texture
            out.append("|".join(cols))
        if a.write:
            io.open(path, "w", encoding="utf-8", newline="").write("\n".join(out))
    print("%d presets touched in %d packs%s" % (touched, len(files), "" if a.write else "  (dry run)"))
    for k in ("hands", "far_width", "haas", "microshift", "fold", "place"):
        print("  %-11s %d" % (k, counts[k]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
