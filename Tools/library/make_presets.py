"""Generate the preset library: thousands of presets as runtime packs.

Each style in styles.py becomes one .ambientpack file (see Core/include/ambient/Presets.h for
the format). A preset is drawn from the style's parameter ranges, then a handful of optional
blocks -- z-plane filter, Cosmos, granular cloud, feedback, the two source slots, the sub, the
convolution room -- are switched on with the style's own probabilities.

    python Tools/library/make_presets.py --per-style 200

Descriptors and map positions are derived from the settings, not measured: measuring five
thousand presets means five thousand renders. The built-in presets keep their measured table
(Tools/preset_map.py); the generated ones use this estimate, which is good enough to cluster
the map and drive the browser filters. The whole run is deterministic -- same seed, same
library -- so texture and wavetable references can be written before those files exist.

Texture and wavetable references are taken from Library/Textures and Library/Wavetables when
they are there; presets whose sample is missing simply load nothing into that slot.
"""
import argparse
import glob
import math
import os
import random
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
from styles import STYLES  # noqa: E402

RENDER = os.path.join(ROOT, "build", "Tools", "render", "Release", "ambient_render.exe")

# Choice names, exactly as Core/src/Params.cpp, Sources.cpp and ZPlane.cpp spell them.
STACKS = ["Octaves", "Fifths", "Major", "Minor", "Seventh", "Harmonics", "Subharmonics"]
SOURCE_TYPES = ["Wavetable", "FM", "Texture", "Noise", "Noise", "Additive"]   # noise twice: an ambient synth lives in it
FILTER_MODELS = ["LP 6", "LP 24", "HP 12", "BP 12", "Notch", "Peak", "Ladder", "Comb", "Formant"]
STRIKE_TYPES = ["String", "String", "Wood", "Metal"]
TABLES = ["Classic", "Organ", "Vocal", "Glass", "Metal", "User"]
SLOT_RATIOS = ["1/1", "9/8", "6/5", "5/4", "4/3", "3/2", "8/5", "5/3", "7/4", "2/1"]
def _z_shapes():
    """Every shape the filter bank has, read from the bank itself. This used to be a hand-written
    list of the original sixteen, which meant the whole five-thousand-preset library reached for
    sixteen filters while the instrument had a hundred and fifty-five. Reading the generated
    header means the list can never fall behind again."""
    inc = os.path.join(ROOT, "Core", "src", "ZPlaneBank.inc")
    text = open(inc, encoding="utf-8").read()
    body = text[text.index("kZShapeNames[kZShapes] = {"):]
    body = body[:body.index("};")]
    return re.findall(r'"([^"]+)"', body)


Z_SHAPES = _z_shapes()
# The families whose shapes are ringing objects rather than filter curves: these are the ones
# worth putting into Modal mode (see Core/include/ambient/ZPlane.h).
Z_MODAL_SHAPES = [s for s in Z_SHAPES if s in (
    "Marimba", "Vibraphone", "Glockenspiel", "Tubular Bell", "Church Bell", "Gong", "Tam Tam",
    "Steel Plate", "Handpan", "Kalimba", "Timpani", "Frame Drum", "Tabla", "Violin Body",
    "Cello Body", "Guitar Box", "Harp Body", "Piano Board", "Sympathetic", "Open Pipe",
    "Closed Pipe", "Clarinet", "Flute", "Bottle", "Organ Pipe", "Small Room", "Cave",
    "Concrete Pipe", "Plate Reverb", "Tunnel", "Metal Bars", "Glass", "Strings", "Wood")]
SHIMMER_PITCH = ["+12", "+7", "+5", "+19", "-12", "+24"]
TAGS = ["Dark", "Bright", "Calm", "Moving", "Tonal", "Noisy", "Wide", "Bass", "Dense", "Sparse",
        "Keys", "Generative", "Cosmos", "Feedback", "Sources", "JustIntonation", "Sub", "Stack", "Air"]


# ---------------------------------------------------------------- the parameter table

def param_table():
    """key -> (section, min, max, default), read from the synth itself so the generator can
    never drift away from Core/src/Params.cpp."""
    out = subprocess.run([RENDER, "--list"], capture_output=True, text=True, encoding="utf-8").stdout
    table = {}
    for line in out.splitlines():
        m = re.match(r"^(\S+)\s+(\S+(?: \S+)*?)\s+\[(-?[\d.e+-]+) \.\. (-?[\d.e+-]+)\] default (-?[\d.e+-]+)", line)
        if m:
            table[m.group(1)] = (m.group(2).strip(), float(m.group(3)), float(m.group(4)), float(m.group(5)))
    if not table:
        raise SystemExit(f"could not read the parameter table -- build ambient_render first ({RENDER})")
    return table


PARAMS = {}


def fmt(key, value):
    """Clamp to the parameter's range and print it the way a preset string wants it."""
    section, lo, hi, _ = PARAMS[key]
    v = min(max(float(value), lo), hi)
    if abs(v - round(v)) < 1e-6:
        return str(int(round(v)))
    return f"{v:.4g}"


def draw(rng, spec):
    """One value from a range spec (see styles.py)."""
    if isinstance(spec, list):
        return spec[rng.randrange(len(spec))]
    if isinstance(spec, tuple):
        if spec[0] == "log":
            return math.exp(rng.uniform(math.log(spec[1]), math.log(spec[2])))
        if spec[0] == "int":
            return rng.randint(int(spec[1]), int(spec[2]))
        return rng.uniform(spec[0], spec[1])
    return spec


def u(rng, lo, hi):
    return rng.uniform(lo, hi)


def logu(rng, lo, hi):
    return math.exp(rng.uniform(math.log(lo), math.log(hi)))


# ---------------------------------------------------------------- shades
#
# Two hundred presets drawn from one set of ranges would be two hundred variations of the same
# preset. Each one is therefore pushed into one of eight shades: a nudge on a few parameters
# and on the chance that an optional block is on. The style stays recognisable, the presets
# inside it do not collapse into each other.

SHADES = [
    ("deep",   {"brightness": ("add", -0.18), "cutoff": ("mul", 0.45), "tilt": ("add", 0.45),
                "far_highcut": ("mul", 0.55), "sub_level": ("add", 0.12), "brain_low": ("add", -5)},
               {"sub": 0.2}),
    ("lit",    {"brightness": ("add", 0.18), "cutoff": ("mul", 2.0), "tilt": ("add", -0.3),
                "far_highcut": ("mul", 1.7), "air": ("add", 0.1), "shimmer": ("add", 0.12)},
               {"cosmos": 0.1}),
    ("still",  {"shimmer_rate": ("mul", 0.4), "drift_rate": ("mul", 0.45), "brain_rate": ("mul", 2.2),
                "ens_depth": ("add", -0.2), "filter_drift": ("add", -0.2), "attack": ("mul", 1.6),
                "release": ("mul", 1.5), "src2_spread": ("mul", 0.35), "src3_spread": ("mul", 0.35)},
               {"zplane": -0.15, "cloud": -0.1, "coherence": -0.1}),
    ("astir",  {"shimmer_rate": ("mul", 2.2), "brain_rate": ("mul", 0.5), "ens_depth": ("add", 0.2),
                "filter_drift": ("add", 0.2), "pan_drift": ("add", 0.15), "rate_wander": ("add", 0.2),
                "src2_spread": ("mul", 2.2), "src3_spread": ("mul", 2.2)},
               {"zplane": 0.2, "coherence": 0.2, "delay2": 0.1}),
    ("sparse", {"brain_density": ("add", -2), "partials": ("add", -6), "strands": ("add", -1),
                "ens_mix": ("add", -0.15), "brain_rate": ("mul", 1.6),
                "src2_grains": ("mul", 0.7), "src3_grains": ("mul", 0.7)},
               {"src2": -0.25, "src3": -0.2, "cloud": -0.12, "stack": -0.15}),
    ("massed", {"brain_density": ("add", 2), "partials": ("add", 6), "strands": ("add", 1),
                "ens_mix": ("add", 0.15), "detune": ("mul", 1.4),
                "src2_grains": ("mul", 1.6), "src3_grains": ("mul", 1.6)},
               {"src2": 0.25, "src3": 0.15, "stack": 0.25}),
    ("rough",  {"inharmonic": ("add", 0.2), "fb_drive": ("add", 0.2), "resonance": ("add", 0.12),
                "air": ("add", 0.08), "src2_spread": ("mul", 1.8), "src3_spread": ("mul", 1.8)},
               {"feedback": 0.3, "cloud": 0.2, "texture": 0.2}),
    ("clean",  {"inharmonic": ("add", -0.15), "purity": ("add", 0.06), "detune": ("mul", 0.6),
                "far_damp": ("add", -0.1)},
               {"feedback": -0.35, "cloud": -0.15, "texture": -0.1}),
]


def apply_shade_granular(p, shade):
    """The granular parameters are set when a slot is filled, which happens after the shade pass,
    so the shade's nudges to them are applied here instead."""
    _, nudges, _ = shade
    for key in ("src2_spread", "src3_spread", "src2_grains", "src3_grains"):
        if key not in p or key not in nudges:
            continue
        how, amount = nudges[key]
        v = float(p[key]) * amount if how == "mul" else float(p[key]) + amount
        _, lo, hi, _d = PARAMS[key]
        v = min(max(v, lo), hi)
        p[key] = int(round(v)) if key.endswith("grains") else v


def apply_shade(p, mod, shade):
    _, nudges, mods = shade
    for key, (how, amount) in nudges.items():
        if key not in p or isinstance(p[key], str):
            continue
        v = float(p[key])
        p[key] = v * amount if how == "mul" else v + amount
        _, lo, hi, _d = PARAMS[key]
        p[key] = min(max(p[key], lo), hi)
        if key in ("partials", "strands", "brain_density", "brain_low", "brain_high",
                   "src2_grains", "src3_grains"):
            p[key] = int(round(p[key]))
    out = dict(mod)
    for key, delta in mods.items():
        out[key] = min(1.0, max(0.0, out.get(key, 0.0) + delta))
    return out



# ---------------------------------------------------------------- modulation
#
# Every preset gets a small matrix. The depths are fractions of the target's own range, so they
# have to be read per target: 0.3 on a mix is a third of it, 0.3 on the cutoff is 5.4 kHz. The
# rates are drone rates -- a cycle between eight seconds and forty minutes, not the tenths of a
# second an LFO usually lives at.

# target -> (depth lo, depth hi, needs which module or None)
MOD_TARGETS = [
    ("cutoff",        0.04, 0.14, None),
    ("brightness",    0.08, 0.28, None),
    ("shimmer",       0.10, 0.35, None),
    ("air",           0.08, 0.25, None),
    ("detune",        0.05, 0.25, None),
    ("purity",        0.04, 0.16, None),
    ("depth",         0.06, 0.22, None),
    ("pan_drift",     0.08, 0.30, None),
    ("width",         0.05, 0.18, None),
    ("ens_depth",     0.10, 0.35, None),
    ("near_mix",      0.06, 0.20, None),
    ("far_decay",     0.05, 0.20, None),
    ("far_highcut",   0.05, 0.18, None),
    ("far_size",      0.06, 0.22, None),
    ("dly_feedback",  0.04, 0.14, None),
    ("dly_mix",       0.05, 0.18, None),
    ("resonance",     0.06, 0.22, None),
    ("z_x",           0.15, 0.45, "zplane"),
    ("z_y",           0.15, 0.45, "zplane"),
    ("z_res",         0.08, 0.25, "zplane"),
    ("cosmos_shift",  0.04, 0.16, "cosmos"),
    ("cosmos_smear",  0.10, 0.30, "cosmos"),
    ("cosmos_nebula", 0.10, 0.30, "cosmos"),
    ("cloud_density", 0.10, 0.30, "cloud"),
    ("cloud_size",    0.10, 0.30, "cloud"),
    ("cloud_pitch",   0.08, 0.25, "cloud"),
    ("fb_tone",       0.06, 0.20, "feedback"),
    ("room_level",    0.05, 0.18, "room"),
]
# Targets that only make sense for a slot that is actually running, keyed by the slot's type.
MOD_SLOT_TARGETS = {
    "Wavetable": [("{p}pos", 0.10, 0.40), ("{p}level", 0.08, 0.25)],
    "FM":        [("{p}fm_index", 0.08, 0.30), ("{p}level", 0.08, 0.25)],
    "Texture":   [("{p}pos", 0.10, 0.40), ("{p}density", 0.08, 0.25), ("{p}spread", 0.10, 0.35),
                  ("{p}level", 0.08, 0.25)],
    "Noise":     [("{p}pos", 0.12, 0.45), ("{p}level", 0.08, 0.25), ("{p}noise_q", 0.10, 0.35)],
}
LFO_SHAPES = ["Sine", "Sine", "Sine", "Triangle", "Random", "Random", "Steps", "Table", "Ramp Up"]


def modulation_for(p, style, rng, shade_name):
    """Builds the matrix rows, the envelope shapes and the LFO parameters for one preset.
    Returns (matrix text, env text). Writes the LFO and envelope parameters into `p`."""
    # Which targets are available depends on what this preset actually switched on.
    on = lambda key: {
        "zplane":  p.get("z_mode", "Off") in ("Series", "Replace"),
        "cosmos":  float(p.get("cosmos_send", 0) or 0) > 0.05,
        "cloud":   float(p.get("cloud_send", 0) or 0) > 0.05,
        "feedback": float(p.get("fb_bus", 0) or 0) > 0.02,
        "room":    float(p.get("room_level", 0) or 0) > 0.02,
    }.get(key, True)
    pool = [(t, lo, hi) for t, lo, hi, need in MOD_TARGETS if need is None or on(need)]
    for n in (2, 3):
        kind = p.get(f"src{n}_type", "Off")
        for tpl, lo, hi in MOD_SLOT_TARGETS.get(kind, []):
            pool.append((tpl.format(p=f"src{n}_"), lo, hi))

    # A still preset gets fewer and slower routes, an astir one more and faster.
    count = {"still": (1, 3), "sparse": (1, 3), "clean": (2, 4), "deep": (2, 4),
             "lit": (2, 5), "massed": (3, 5), "rough": (3, 6), "astir": (4, 7)}.get(shade_name, (2, 5))
    n_routes = rng.randint(*count)
    rate_mul = {"still": 0.4, "sparse": 0.6, "astir": 2.6, "rough": 1.8}.get(shade_name, 1.0)

    rng.shuffle(pool)
    rows, used_lfo = [], []
    # The periods sit on a golden ladder: the first is drawn, the rest are it times phi to the
    # power of how many have gone before. Rates in a simple ratio come back into step and the
    # drone falls into a pattern a listener can hear coming; powers of phi never do.
    base_period = math.exp(u(rng, math.log(8.0), math.log(900.0))) / rate_mul
    PHI = 1.6180339887
    for i in range(min(n_routes, len(pool), 8)):
        target, lo, hi = pool[i]
        lfo = i % 8 + 1
        # Now and then the source is not an LFO at all: BEAT is the chord listening to how far
        # out of tune it is, and the coherence ring is four oscillators that pull on each other.
        # Both make the modulation come from the instrument rather than from a clock.
        other = None
        if rng.random() < 0.12:
            other = "beat"
        elif rng.random() < 0.10:
            other = "kura%d" % (rng.randrange(4) + 1)
        if other is not None:
            depth = u(rng, lo, hi) * (1.0 if rng.random() < 0.65 else -1.0)
            rows.append(f"{other}>{target}:{depth:.3f}")
            continue
        if lfo not in used_lfo:
            used_lfo.append(lfo)
            period = base_period * (PHI ** len(used_lfo))
            p[f"lfo{lfo}_rate"] = 1.0 / period
            p[f"lfo{lfo}_shape"] = LFO_SHAPES[rng.randrange(len(LFO_SHAPES))]
            p[f"lfo{lfo}_phase"] = round(u(rng, 0.0, 1.0), 3)
            p[f"lfo{lfo}_depth"] = round(u(rng, 0.6, 1.0), 3)
            if p[f"lfo{lfo}_shape"] == "Table":
                p[f"lfo{lfo}_table"] = rng.randrange(0, 32)
        depth = u(rng, lo, hi) * (1.0 if rng.random() < 0.65 else -1.0)
        row = f"lfo{lfo}>{target}:{depth:.3f}"
        # Now and then a macro decides how much of the route gets through. Sparingly: a macro is
        # performance state and starts at zero, so such a route is silent until a hand opens it.
        if rng.random() < 0.07:
            row += ":macro_" + "abcdefgh"[rng.randrange(8)]
        elif rng.random() < 0.15:
            row += ":none:u"
        rows.append(row)

    # Envelopes: slow shape generators, not attacks.
    #
    # This used to be "one or two, three to six points, never a sustain point", and measured over
    # the finished library that is exactly what came out: envelopes 1 and 2 only, at most six of
    # the sixteen breakpoints, and Sustain Loop -- one of the three modes -- used by none of six
    # thousand presets. The ceiling was the generator's, not the instrument's. The ranges below
    # are the instrument's own: up to six envelopes, up to sixteen points, and a sustain point on
    # the ones that are worth holding.
    #
    # The times sit on a golden ladder from the first envelope's, so a preset with five of them
    # running has five periods that never come back into step.
    envs = ["", "", "", "", "", ""]
    n_env = rng.choices([0, 1, 2, 3, 4, 5, 6], weights=[18, 26, 24, 14, 9, 6, 3])[0]
    base_time = math.exp(u(rng, math.log(2.0), math.log(20.0)))
    for e in range(n_env):
        pts, t = [], 0.0
        # Mostly short shapes, because a shape you can follow is worth more than a long one you
        # cannot; but a fifth of them go long, and those are the ones that use the whole editor.
        n_pts = rng.randint(3, 7) if rng.random() < 0.8 else rng.randint(8, 16)
        for k in range(n_pts):
            v = 0.0 if k in (0, n_pts - 1) else round(u(rng, -1.0, 1.0), 3)
            pts.append(f"{t:.3g}:{v:g}:{round(u(rng, -0.6, 0.6), 2):g}")
            t += u(rng, 0.6, 3.0)
        text = "/".join(pts)
        mode = "One Shot"
        if rng.random() < 0.55 and n_pts >= 4:
            text += f"!l0-{n_pts - 2}"          # loop everything but the tail
            mode = "Loop"
        elif rng.random() < 0.35 and n_pts >= 5:
            # Sustain Loop: rise, wait on the sustain point for as long as anything is sounding,
            # then play the tail from there. The point is picked in the middle of the shape so
            # there is something to rise through and something left to run out.
            text += f"!s{rng.randint(1, n_pts - 3)}"
            mode = "Sustain Loop"
        envs[e] = text
        # Golden ladder, so a preset's envelopes never line up with each other.
        p[f"env{e+1}_time"] = round(base_time * PHI ** e, 3)
        p[f"env{e+1}_mode"] = mode
        p[f"env{e+1}_depth"] = round(u(rng, 0.5, 1.0), 3)
        if pool:
            target, lo, hi = pool[rng.randrange(len(pool))]
            rows.append(f"env{e+1}>{target}:{u(rng, lo, hi) * (1.0 if rng.random() < 0.7 else -1.0):.3f}")

    return ";".join(rows), "~".join(envs).rstrip("~")

# ---------------------------------------------------------------- one preset

def make_preset(style, rng, textures, wavetables, impulses, shade):
    p = {}
    for key, spec in style["params"].items():
        if key not in PARAMS:
            continue
        p[key] = draw(rng, spec)
    mod = apply_shade(p, style["modules"], shade)
    on = lambda k: rng.random() < mod.get(k, 0.0)
    texture_file = wavetable_file = ""
    # One clip per slot where the slots differ (the Stretch type draws its own per slot); the
    # pack's texture field then carries them ';'-separated, one per slot, empty for a slot
    # without one. A preset whose slots share one clip keeps the single-path form.
    slot_textures = {}

    # Foundation ------------------------------------------------------------------------
    if not on("sub"):
        p["sub_level"] = 0.0
    elif p.get("sub_level", 0.0) < 0.1:
        p["sub_level"] = u(rng, 0.15, 0.4)
    if p.get("sub_level", 0.0) > 0.05:
        p.setdefault("sub_glide", logu(rng, 2.0, 20.0))
        if rng.random() < 0.4:
            p["sub_binaural"] = u(rng, 1.5, 7.0)

    # Stack -----------------------------------------------------------------------------
    if on("stack"):
        p["stack"] = STACKS[rng.randrange(len(STACKS))]
        p["strands"] = rng.randint(3, 6)

    # Envelope shape: a few presets are played rather than generated -----------------------
    if on("keys"):
        p["brain_on"] = "off"
        p["attack"] = logu(rng, 0.5, 6.0)
        p["release"] = logu(rng, 4.0, 20.0)
        p["keys_depth"] = u(rng, 0.0, 0.4)

    # Source slots ----------------------------------------------------------------------
    def fill_slot(n):
        nonlocal texture_file, wavetable_file
        pre = f"src{n}_"
        want_tex = on("texture") and textures
        want_tab = on("usertable") and wavetables
        # Stretch: the style's clips read as a continuum. A style asks for it with a "stretch"
        # module weight; the field-recording style asks for it in every slot it fills.
        want_stretch = on("stretch") and textures
        if want_stretch:
            kind = "Stretch"
        elif want_tex and (not want_tab or rng.random() < 0.5):
            kind = "Texture"
        elif want_tab:
            kind = "Wavetable"
        else:
            kind = SOURCE_TYPES[rng.randrange(len(SOURCE_TYPES))]
            if kind == "Texture" and not textures:
                kind = "Wavetable"
        p[pre + "type"] = kind
        p[pre + "level"] = u(rng, 0.15, 0.55)
        if kind != "Noise" and rng.random() < 0.4:   # independent fine drift: sources that beat like an ensemble
            p[pre + "drift"] = logu(rng, 0.8, 10.0)
        p[pre + "octave"] = rng.choice([-2, -1, 0, 0, 0, 1])
        p[pre + "ratio"] = SLOT_RATIOS[rng.randrange(len(SLOT_RATIOS))]
        p[pre + "pan"] = u(rng, -0.8, 0.8)
        if kind == "Wavetable":
            if want_tab and not wavetable_file:
                p[pre + "table"] = "User"
                wavetable_file = wavetables[rng.randrange(len(wavetables))]
            else:
                p[pre + "table"] = TABLES[rng.randrange(len(TABLES) - 1)]
            p[pre + "pos"] = u(rng, 0.0, 1.0)
            p[pre + "pos_drift"] = u(rng, 0.05, 0.7)
        elif kind == "Noise":
            p[pre + "noise"] = style["noise"][rng.randrange(len(style["noise"]))]
            p[pre + "noise_q"] = u(rng, 0.15, 0.85)
            p[pre + "pos"] = u(rng, 0.05, 0.9)          # band centre / colour
            p[pre + "pos_drift"] = u(rng, 0.05, 0.8)
            p[pre + "level"] = u(rng, 0.12, 0.45)
            if p[pre + "noise"] == "Crackle":
                p[pre + "density"] = logu(rng, 1.5, 30.0)
            p[pre + "follow"] = "Note" if (p[pre + "noise"] in ("Band", "Wind") and rng.random() < 0.4) else "Free"
        elif kind == "FM":
            p[pre + "fm_ratio"] = rng.choice([0.5, 1.0, 1.5, 2.0, 2.0, 3.0, 4.0, 5.0, 7.0])
            p[pre + "fm_index"] = logu(rng, 0.3, 3.5)
        elif kind == "Additive":
            # a second (or third) additive bank on its own just ratio: the classic Rich stack, but
            # with its own spectrum and its own slow pitch drift
            p[pre + "partials"] = rng.randint(6, 28)
            p[pre + "tilt"] = u(rng, 0.8, 2.2)
            p[pre + "bright"] = u(rng, 0.3, 0.9)
            p[pre + "odd_even"] = u(rng, -0.5, 0.5) if rng.random() < 0.5 else 0.0
            if rng.random() < 0.3:
                p[pre + "inharmonic"] = u(rng, 0.05, 0.4)
            p[pre + "shimmer"] = u(rng, 0.2, 0.6)
            p[pre + "shimmer_rate"] = logu(rng, 0.03, 0.4)
            p[pre + "drift"] = logu(rng, 1.0, 8.0)
        elif kind == "Stretch":
            # The clip as a continuum. The window (Grain) sits where Paulstretch is smooth, the
            # factor is log-spread from "slowed" to "geological", and each Stretch slot draws its
            # own clip so four slots are four places. Free unless the clip carries a pitch, and
            # even then mostly Free: a field recording pitched to the note is a choice, not a rule.
            p[pre + "grain"] = logu(rng, 150.0, 340.0)
            p[pre + "stretch"] = logu(rng, 6.0, 300.0)
            p[pre + "xfade"] = u(rng, 0.05, 0.25)
            p[pre + "pos"] = u(rng, 0.0, 1.0)
            p[pre + "pos_drift"] = u(rng, 0.1, 0.6)
            # Higher than a Texture slot: a stretched recording has no attacks to carry it, and at
            # the Texture slot's range a four-slot field preset measured -47 dBFS.
            p[pre + "level"] = u(rng, 0.3, 0.7)
            if rng.random() < 0.5:
                p[pre + "drift"] = logu(rng, 0.5, 4.0)
            own = textures[rng.randrange(len(textures))]
            slot_textures[n] = own
            if not texture_file:
                texture_file = own
            pitched = bool(PITCHED.search(own))
            p[pre + "follow"] = "Note" if (pitched and rng.random() < 0.35) else "Free"
        else:                                            # Texture
            grain_ms = logu(rng, 60.0, 800.0)
            density = logu(rng, 3.0, 40.0)
            p[pre + "grain"] = grain_ms
            p[pre + "density"] = density
            # Grains: enough for the overlap the density and length ask for, plus headroom and the
            # style's bias. Too few and the slot drops grains, which made more density quieter
            # instead of denser -- the ceiling used to be eight for everyone.
            gran = style["granular"]
            overlap = max(1.0, density * grain_ms / 1000.0)
            p[pre + "grains"] = int(min(64, max(4, math.ceil(overlap * 1.8 * gran["grains"]) + 4)))
            # Spread: the window the start points are drawn from. Near zero the same fragment
            # repeats and the clip freezes into a drone; near one a grain may come from anywhere.
            p[pre + "spread"] = math.exp(u(rng, math.log(gran["spread"][0]), math.log(gran["spread"][1])))
            # A texture slot is now level-matched to the other two, so it needs less than before.
            p[pre + "level"] = u(rng, 0.12, 0.42)
            if not texture_file and textures:
                texture_file = textures[rng.randrange(len(textures))]
            slot_textures[n] = texture_file
            # Only a clip with a detected pitch (TextureGen puts the note in the name) can be
            # transposed to the played note; the rest are played free, as a bed.
            pitched = bool(texture_file) and bool(PITCHED.search(texture_file))
            p[pre + "follow"] = "Note" if (pitched and rng.random() < 0.75) else "Free"

    if on("src2"):
        fill_slot(2)
    if on("src3"):
        fill_slot(3)
    if on("src4"):
        fill_slot(4)
    apply_shade_granular(p, shade)

    # Z-plane morphing filter -------------------------------------------------------------
    if on("zplane"):
        # One in five z-planes is Modal: the same shape read as a bank of resonators that rings
        # rather than a cascade that shapes. Only the shapes that are physical objects -- a
        # resonator bank on a phaser is a curiosity, on a bell it is the point.
        if rng.random() < 0.2 and Z_MODAL_SHAPES:
            p["z_mode"] = "Modal"
            p["z_shape"] = Z_MODAL_SHAPES[rng.randrange(len(Z_MODAL_SHAPES))]
            p["z_decay"] = logu(rng, 0.4, 18.0)
            p["z_damp"] = u(rng, 0.3, 0.95)
        else:
            p["z_mode"] = "Replace" if rng.random() < 0.45 else "Series"
            p["z_shape"] = Z_SHAPES[rng.randrange(len(Z_SHAPES))]
        p["z_z"] = u(rng, 0.0, 0.7)      # the cube's third axis
        p["z_x"] = u(rng, 0.1, 0.9)
        p["z_y"] = u(rng, 0.1, 0.9)
        p["z_rate"] = logu(rng, 0.006, 0.25)
        p["z_depth"] = u(rng, 0.2, 0.9)
        p["z_res"] = u(rng, 0.25, 0.85)
        p["z_keytrack"] = u(rng, 0.0, 0.6) if rng.random() < 0.4 else 0.0
        p["z_mix"] = u(rng, 0.4, 1.0) if p["z_mode"] == "Replace" else u(rng, 0.3, 0.9)

    # The delay ducks its own loop on transients, so a fresh attack does not have to fight the
    # brightness of the last one's tail.
    if p.get("dly_mix", 0.0) and rng.random() < 0.35:
        p["dly_duck"] = u(rng, 0.25, 0.8)

    # Cosmos ------------------------------------------------------------------------------
    if on("cosmos"):
        p["cosmos_send"] = u(rng, 0.15, 0.6)
        p["cosmos_return"] = u(rng, 0.3, 0.8)
        p["cosmos_to_far"] = u(rng, 0.2, 0.8)
        which = rng.random()
        if which < 0.3:
            p["cosmos_shift"] = u(rng, -80.0, 80.0)
            p["cosmos_shift_drift"] = u(rng, 0.1, 0.7)
        elif which < 0.55:
            p["cosmos_res"] = u(rng, 0.2, 0.7)
            p["cosmos_res_pitch"] = rng.choice([0.5, 1.0, 1.5, 2.0, 3.0, 4.0])
            p["cosmos_res_fb"] = u(rng, 0.6, 0.92)
        elif which < 0.75:
            p["cosmos_nebula"] = u(rng, 0.2, 0.8)
            p["cosmos_smear"] = u(rng, 0.4, 0.95)
        else:
            p["cosmos_shimmer"] = u(rng, 0.2, 0.7)
            p["cosmos_shimmer_pitch"] = SHIMMER_PITCH[rng.randrange(len(SHIMMER_PITCH))]
        if rng.random() < 0.35:
            p["cosmos_vowel"] = u(rng, 0.2, 0.7)
            p["cosmos_vowel_rate"] = logu(rng, 0.008, 0.2)

    # Granular cloud ------------------------------------------------------------------------
    if on("cloud"):
        p["cloud_send"] = u(rng, 0.15, 0.6)
        p["cloud_density"] = logu(rng, 3.0, 40.0)
        p["cloud_size"] = logu(rng, 60.0, 600.0)
        p["cloud_pitch"] = u(rng, 0.0, 0.7)
        p["cloud_spray"] = logu(rng, 0.15, 1.8)
        p["cloud_level"] = u(rng, 0.4, 0.85)

    # Feedback loop. The ceiling stays low: past 0.25 the bus climbs and collapses to mono.
    if on("feedback"):
        p.setdefault("fb_bus", u(rng, 0.04, 0.18))
        p["fb_bus"] = min(float(p["fb_bus"]), 0.2)
        p.setdefault("fb_tone", logu(rng, 400.0, 4000.0))
        p.setdefault("fb_drive", u(rng, 0.3, 0.9))
        if rng.random() < 0.35:
            p["fb_fm"] = u(rng, 0.03, 0.18)
        if rng.random() < 0.5:
            p.setdefault("fb_tape", u(rng, 0.2, 0.8))
    else:
        p.pop("fb_bus", None)
        p.pop("fb_fm", None)

    # Second delay, room, coherence, portamento ---------------------------------------------
    if on("delay2"):
        p["dly2_mix"] = u(rng, 0.1, 0.35)
        p["dly2_time_l"] = logu(rng, 0.5, 3.5)
        p["dly2_time_r"] = logu(rng, 0.7, 4.0)
        p["dly2_feedback"] = u(rng, 0.3, 0.75)
        p["dly2_cross"] = u(rng, 0.2, 0.9)
        p["dly2_damp"] = u(rng, 0.4, 0.9)
    impulse_file = ""
    if on("room") and impulses:
        p["room_level"] = u(rng, 0.15, 0.6)
        p["room_source"] = "Far" if rng.random() < 0.6 else "Near"
        p["room_predelay"] = logu(rng, 5.0, 150.0)
        p["room_highcut"] = logu(rng, 1500.0, 9000.0)
        impulse_file = impulses[rng.randrange(len(impulses))]
    if on("coherence"):
        p["coherence"] = u(rng, 0.2, 0.8)
        p["coherence_depth"] = u(rng, 0.2, 0.8)
        p["coherence_rate"] = u(rng, 0.3, 3.0)
    if on("portamento"):
        p["portamento"] = logu(rng, 0.5, 12.0)
        p["porta_gravity"] = u(rng, 0.2, 0.9)
    if rng.random() < 0.12:
        p["air_mode"] = "Ghost"
        p["air"] = max(float(p.get("air", 0.2)), 0.2)
    if rng.random() < 0.25:
        p["purity_drift"] = u(rng, 0.1, 0.5)
        p["purity_rate"] = logu(rng, 0.003, 0.05)

    # The Rich refinements ------------------------------------------------------------------
    if on("phase"):                                  # the binaural phase field, slow
        p["phase_width"] = u(rng, 0.2, 0.8)
        p["phase_rate"] = logu(rng, 0.006, 0.08)
    if float(p.get("breath", 0.0)) > 0.05 and rng.random() < 0.4:
        p["doppler"] = u(rng, 0.2, 0.8)
    if on("blur"):                                   # attacks wiped into texture
        p["blur_mix"] = u(rng, 0.2, 0.6)
        p["blur_smear"] = u(rng, 0.3, 0.9)
    if on("filtermodel"):
        model = FILTER_MODELS[rng.randrange(len(FILTER_MODELS))]
        p["filter_model"] = model
        if model == "Comb":
            p["cutoff"] = logu(rng, 80.0, 700.0); p["resonance"] = u(rng, 0.4, 0.8); p["keytrack"] = u(rng, 0.5, 1.0)
        elif model == "Formant":
            p["cutoff"] = logu(rng, 150.0, 6000.0); p["resonance"] = u(rng, 0.3, 0.8); p["filter_drift"] = u(rng, 0.4, 1.0)
        elif model == "HP 12":
            p["cutoff"] = logu(rng, 60.0, 400.0)
        elif model in ("BP 12", "Notch", "Peak"):
            p["cutoff"] = logu(rng, 200.0, 3000.0); p["resonance"] = u(rng, 0.3, 0.8); p["filter_drift"] = u(rng, 0.3, 0.9)
        elif model == "Ladder":
            p["resonance"] = u(rng, 0.3, 0.85); p["filter_drive"] = u(rng, 0.0, 0.5)
    if on("strike") or (on("keys") and rng.random() < 0.5):   # the struck foreground against the vast background
        p["strike_level"] = u(rng, 0.2, 0.6)
        p["strike_type"] = STRIKE_TYPES[rng.randrange(len(STRIKE_TYPES))]
        p["strike_decay"] = logu(rng, 0.1, 1.8)
        p["strike_damp"] = u(rng, 0.15, 0.8)
        if rng.random() < 0.3:
            p["strike_who"] = "Keys + Brain"
    if on("absorb"):                                 # echoes that drown instead of merely fading
        p["dly_absorb"] = u(rng, 0.3, 1.0)
        if "dly2_mix" in p:
            p["dly2_absorb"] = u(rng, 0.3, 1.0)
    if on("tide"):                                   # the whole instrument leans over minutes
        p["tide"] = logu(rng, 2.0, 12.0)
        p["tide_period"] = logu(rng, 4.0, 30.0)
    if on("rotate"):                                 # the background turns
        p["far_rotate"] = u(rng, 0.2, 0.8)
    p["seed"] = rng.randrange(1, 9999)

    # brain_high must stay above brain_low, and hold_max above hold_min
    if "brain_low" in p and "brain_high" in p and p["brain_high"] <= p["brain_low"] + 6:
        p["brain_high"] = p["brain_low"] + 12
    if "brain_hold_min" in p and "brain_hold_max" in p and p["brain_hold_max"] < p["brain_hold_min"] * 1.5:
        p["brain_hold_max"] = p["brain_hold_min"] * 2.0
    matrix, envs = modulation_for(p, style, rng, shade[0])
    if len(set(slot_textures.values())) > 1:
        texture_file = ";".join(slot_textures.get(k, "") for k in range(1, 5))
    return p, texture_file, wavetable_file, impulse_file, matrix, envs


def settings_string(p):
    """Only what differs from the default: applyPreset resets everything first."""
    parts = []
    for key, value in p.items():
        section, lo, hi, default = PARAMS[key]
        if isinstance(value, str):
            parts.append(f"{key}={value}")
        elif abs(float(value) - default) > 1e-6:
            parts.append(f"{key}={fmt(key, value)}")
    return ";".join(parts)


# ---------------------------------------------------------------- descriptors

def norm(v, lo, hi):
    return min(max((v - lo) / (hi - lo), 0.0), 1.0)


def descriptors(p):
    """An estimate of what the preset will sound like, from its settings. Six numbers in 0..1,
    ranked against the whole library afterwards."""
    g = lambda k, d=None: float(p[k]) if k in p and not isinstance(p[k], str) else (
        PARAMS[k][3] if d is None else d)
    s = lambda k: p.get(k, "")

    cutoff = g("cutoff")
    bright = (0.35 * g("brightness")
              + 0.20 * norm(math.log(max(cutoff, 40.0)), math.log(200.0), math.log(9000.0))
              + 0.15 * norm(math.log(max(g("far_highcut"), 500.0)), math.log(1000.0), math.log(12000.0))
              + 0.12 * (1.0 - norm(g("tilt"), 0.4, 2.8))
              + 0.10 * g("air") + 0.08 * g("shimmer"))

    motion = (0.22 * norm(math.log(max(g("shimmer_rate"), 0.005)), math.log(0.02), math.log(1.2))
              + 0.15 * g("shimmer")
              + 0.15 * (g("z_depth") * norm(math.log(max(g("z_rate"), 0.004)), math.log(0.005), math.log(0.6))
                        if s("z_mode") in ("Series", "Replace") else 0.0)
              + 0.12 * g("ens_depth") * g("ens_mix")
              + 0.12 * (1.0 - norm(math.log(max(g("brain_rate"), 2.0)), math.log(8.0), math.log(250.0)))
              + 0.10 * g("filter_drift") + 0.08 * g("breath")
              + 0.06 * max(g("src2_pos_drift") if s("src2_type") == "Wavetable" else 0.0,
                           g("src3_pos_drift") if s("src3_type") == "Wavetable" else 0.0))

    width = (0.30 * g("spread") + 0.22 * g("itd") + 0.18 * norm(g("width"), 0.6, 1.9)
             + 0.15 * g("pan_drift") + 0.15 * g("ens_mix"))

    noisy = (0.28 * g("inharmonic") + 0.18 * g("air")
             + 0.14 * (1.0 if s("src2_type") == "Texture" or s("src3_type") == "Texture" else 0.0)
             + 0.14 * g("cloud_send") + 0.12 * g("fb_drive") * min(g("fb_bus") * 6.0, 1.0)
             + 0.08 * g("fb_tape") + 0.06 * min(g("src2_fm_index") / 6.0, 1.0))

    bass = (0.34 * g("sub_level") + 0.22 * (1.0 - norm(g("brain_low"), 24.0, 60.0))
            + 0.16 * (1.0 - norm(math.log(max(g("cutoff"), 40.0)), math.log(200.0), math.log(6000.0)))
            + 0.14 * norm(g("bass_mono"), 60.0, 300.0)
            + 0.14 * (1.0 - norm(g("pad_low_cut"), 0.0, 200.0)))
    if s("sub_octave") == "-2":
        bass = min(1.0, bass + 0.08)

    density = (0.26 * norm(g("brain_density"), 1.0, 10.0)
               + 0.16 * norm(g("strands"), 1.0, 6.0)
               + 0.14 * norm(g("partials"), 4.0, 32.0)
               + 0.14 * (1.0 - norm(math.log(max(g("brain_rate"), 2.0)), math.log(8.0), math.log(250.0)))
               + 0.12 * ((0.5 if s("src2_type") not in ("", "Off") else 0.0)
                         + (0.5 if s("src3_type") not in ("", "Off") else 0.0))
               + 0.10 * g("cloud_send") + 0.08 * norm(g("ens_mix"), 0.0, 1.0))
    if s("brain_on") == "off":
        density *= 0.55
    return [bright, motion, width, noisy, bass, density]


def tag_bits(p, d):
    """d: the ranked descriptors."""
    bright, motion, width, noisy, bass, density = d
    t = set()
    t.add("Keys" if p.get("brain_on") == "off" else "Generative")
    if float(p.get("cosmos_send", 0) or 0) > 0.05: t.add("Cosmos")
    if float(p.get("fb_bus", 0) or 0) > 0.02 or float(p.get("fb_fm", 0) or 0) > 0.02: t.add("Feedback")
    if p.get("src2_type") or p.get("src3_type"): t.add("Sources")
    if any(k in str(p.get("scale", "")) for k in ("JI", "Harmonic", "Subharmonic", "Pythag", "Bohlen", "Otonal", "Slendro")):
        t.add("JustIntonation")
    if float(p.get("sub_level", 0) or 0) > 0.05: t.add("Sub")
    if p.get("stack"): t.add("Stack")
    if float(p.get("air", 0) or 0) >= 0.3: t.add("Air")
    if bright < 0.3: t.add("Dark")
    if bright > 0.7: t.add("Bright")
    if motion < 0.3: t.add("Calm")
    if motion > 0.7: t.add("Moving")
    if noisy < 0.4: t.add("Tonal")
    if noisy > 0.7: t.add("Noisy")
    if width > 0.7: t.add("Wide")
    if bass > 0.7: t.add("Bass")
    if density > 0.7: t.add("Dense")
    if density < 0.3: t.add("Sparse")
    return sum(1 << TAGS.index(x) for x in t)


def rank(v):
    order = np.argsort(np.argsort(v))
    return order / max(len(v) - 1, 1)


def layout(desc):
    """Map positions: half the rank of the two strongest directions, half a principal-component
    projection, then every preset is pushed into its own cell of a fine grid so nothing hides
    underneath anything else."""
    x = desc.copy()
    x = (x - x.mean(axis=0)) / (x.std(axis=0) + 1e-9)
    u_, s_, vt = np.linalg.svd(x - x.mean(axis=0), full_matrices=False)
    pc = u_[:, :2] * s_[:2]
    xy = np.zeros((len(desc), 2))
    xy[:, 0] = 0.5 * rank(desc[:, 0]) + 0.5 * rank(pc[:, 0])     # brightness / first component
    xy[:, 1] = 0.5 * rank(desc[:, 4]) + 0.5 * rank(pc[:, 1])     # weight / second component

    # Grid de-collision: n presets, a grid with about 3n cells, spiral search for a free one.
    n = len(xy)
    side = int(math.ceil(math.sqrt(n * 3.0)))
    taken = {}
    order = np.argsort(-desc[:, 5])          # densest first, they get the best cells
    for i in order:
        cx = min(side - 1, max(0, int(xy[i, 0] * side)))
        cy = min(side - 1, max(0, int(xy[i, 1] * side)))
        if (cx, cy) not in taken:
            taken[(cx, cy)] = i
            continue
        placed = False
        for r in range(1, side):
            for dx in range(-r, r + 1):
                for dy in (-r, r) if abs(dx) < r else range(-r, r + 1):
                    a, b = cx + dx, cy + dy
                    if 0 <= a < side and 0 <= b < side and (a, b) not in taken:
                        taken[(a, b)] = i
                        placed = True
                        break
                if placed: break
            if placed: break
    out = np.zeros((n, 2))
    for (cx, cy), i in taken.items():
        out[i] = ((cx + 0.5) / side, (cy + 0.5) / side)
    return out


# ---------------------------------------------------------------- assets and names

PITCHED = re.compile(r"_[A-G]#?-?\d+\.wav$")


def rejected_clips(dirname):
    """Names Tools/library/check_textures.py has flagged as unusable."""
    path = os.path.join(dirname, "rejected.txt")
    if not os.path.isfile(path):
        return set()
    out = set()
    for line in open(path, encoding="utf-8"):
        line = line.split("#")[0].strip()
        if line:
            out.add(line)
    return out


def texture_pool(dirname, style_name, rejected):
    """Clips whose file name starts with this style's slug (make_textures.py names them that
    way), falling back to everything in the folder. Duds are left out."""
    slug = re.sub(r"[^a-z0-9]+", "_", style_name.lower()).strip("_")[:20]
    files = sorted(os.path.basename(f) for f in glob.glob(os.path.join(dirname, "*.wav"))
                   if os.path.basename(f) not in rejected)
    own = [f for f in files if f.startswith(slug + "_")]
    return own or files


def impulse_pool(dirname, style):
    """Impulses whose name starts with one of the style's families (see make_impulses.py)."""
    files = sorted(os.path.basename(f) for f in glob.glob(os.path.join(dirname, "*.wav")))
    own = [f for f in files if any(f.startswith(pre) for pre in style["impulses"])]
    return own or files


def wavetable_pool(dirname, style):
    slugs = [re.sub(r"[^a-z0-9]+", "_", t.lower()).strip("_") for t in style["tables"]]
    files = sorted(os.path.basename(f) for f in glob.glob(os.path.join(dirname, "*.wav")))
    own = [f for f in files if any(f.startswith(s + "_") for s in slugs)]
    return own or files


ROMAN = ["", " II", " III", " IV", " V", " VI", " VII", " VIII", " IX", " X"]


def name_for(style, rng, used):
    firsts, seconds = style["words"]
    for _ in range(60):
        n = f"{firsts[rng.randrange(len(firsts))]} {seconds[rng.randrange(len(seconds))]}"
        if n not in used:
            used.add(n)
            return n
    base = f"{firsts[rng.randrange(len(firsts))]} {seconds[rng.randrange(len(seconds))]}"
    for r in ROMAN[1:] + [f" {i}" for i in range(2, 400)]:
        if base + r not in used:
            used.add(base + r)
            return base + r
    return base


# ---------------------------------------------------------------- main

def main():
    global PARAMS
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--per-style", type=int, default=200)
    ap.add_argument("--styles", default="", help="comma-separated style names; default is all of them")
    ap.add_argument("--seed", type=int, default=23)
    ap.add_argument("--out-dir", default=os.path.join(ROOT, "Library", "Packs"))
    ap.add_argument("--textures", default=os.path.join(ROOT, "Library", "Textures"))
    ap.add_argument("--wavetables", default=os.path.join(ROOT, "Library", "Wavetables"))
    ap.add_argument("--impulses", default=os.path.join(ROOT, "Library", "Impulses"))
    a = ap.parse_args()
    PARAMS = param_table()
    os.makedirs(a.out_dir, exist_ok=True)

    rejects = rejected_clips(a.textures)
    if rejects:
        print(f"{len(rejects)} clips skipped (see {os.path.join(a.textures, 'rejected.txt')})")
    packs = []
    taken = set()          # map cells already used, across the whole library
    # Names stay unique across the whole synth, so "--preset <name>" and the browser search
    # always mean one preset -- the built-in ones included.
    # Pointed at the library, so the names already in it count as taken. Without this the render
    # tool only reports the built-ins, and a new pack happily reuses a name an existing pack has
    # -- which it did: forty-eight collisions with the older packs, all of them plausible pairs
    # like "Iron Reach" that two different word pools can both produce.
    # Names already taken: the packs in the output folder AND the library proper, when the output
    # is a folder of its own. Writing a new pack into a side folder and checking names only
    # against that folder produced 59 presets that collided with the library the moment the pack
    # was moved in -- "Ice Descent" twice, once in Permafrost and once in Field Recordings.
    library = os.path.join(ROOT, "Library", "Packs")
    folders = a.out_dir if os.path.normcase(os.path.abspath(a.out_dir)) == os.path.normcase(os.path.abspath(library)) \
              else a.out_dir + ";" + library
    env = dict(os.environ, AMBIENT_PACKS=folders)
    used_names = {n.strip() for n in subprocess.run([RENDER, "--list-presets"], capture_output=True,
                                                    text=True, encoding="utf-8", env=env).stdout.splitlines() if n.strip()}
    wanted = [w.strip().lower() for w in a.styles.split(",") if w.strip()]
    for si, st in enumerate(STYLES):
        # The seed is keyed on the style's index, not on how many are being written, so asking
        # for one pack gives exactly the pack that a full run would have given.
        if wanted and st["name"].lower() not in wanted:
            continue
        rng = random.Random(a.seed * 104729 + si)
        textures = texture_pool(a.textures, st["name"], rejects)
        tables = wavetable_pool(a.wavetables, st)
        impulses = impulse_pool(a.impulses, st)
        rows = []
        for k in range(a.per_style):
            p, tex, tab, imp, matrix, envs = make_preset(st, rng, textures, tables, impulses,
                                                        SHADES[k % len(SHADES)])
            rows.append({"name": name_for(st, rng, used_names), "params": p, "shade": SHADES[k % len(SHADES)][0],
                         "settings": settings_string(p),
                         # one path, or up to four ';'-separated (one per slot): each non-empty
                         # part gets the folder, an empty part stays empty and means "no clip"
                         "texture": ";".join(f"../Textures/{part}" if part else "" for part in tex.split(";")) if tex else "",
                         "wavetable": f"../Wavetables/{tab}" if tab else "",
                         "impulse": f"../Impulses/{imp}" if imp else "",
                         "mod": matrix, "envs": envs,
                         "desc": descriptors(p)})
        packs.append((st, rows))

    # Rank the descriptors across the whole library, then lay the map out over all of it.
    all_rows = [r for _, rows in packs for r in rows]
    desc = np.array([r["desc"] for r in all_rows], dtype=np.float64)
    for c in range(desc.shape[1]):
        desc[:, c] = rank(desc[:, c])
    xy = layout(desc)
    for i, r in enumerate(all_rows):
        r["desc"] = desc[i]
        r["xy"] = xy[i]
        r["tags"] = tag_bits(r["params"], desc[i])

    total = 0
    for st, rows in packs:
        path = os.path.join(a.out_dir, re.sub(r"[^A-Za-z0-9]+", "-", st["name"]) + ".ambientpack")
        with open(path, "w", encoding="utf-8") as f:
            f.write(f"# {st['name']} -- {len(rows)} presets for AmbientSynth, generated by Tools/library/make_presets.py\n")
            f.write(f"# In the spirit of {st['inspiration']}. Not affiliated with, sampled from or endorsed by anyone.\n")
            f.write("# Format: name|settings|x y bright motion width noisy bass density tags|"
                    "texture|wavetable|impulse|mod matrix|env shapes\n")
            f.write(f"pack {st['name']}\n")
            for r in rows:
                d = r["desc"]
                # Two presets on the same spot of the map are two dots the browser draws on top of
                # each other, and one of them can never be clicked. Nudged apart on a deterministic
                # spiral, so the same seed still gives the same library.
                x, y = r["xy"][0], r["xy"][1]
                step = 0
                while (round(x, 3), round(y, 3)) in taken and step < 64:
                    step += 1
                    ang = 2.39996 * step                     # the golden angle: no two land alike
                    x = min(1.0, max(0.0, r["xy"][0] + 0.004 * step * math.cos(ang)))
                    y = min(1.0, max(0.0, r["xy"][1] + 0.004 * step * math.sin(ang)))
                taken.add((round(x, 3), round(y, 3)))
                meta = " ".join(f"{v:.3f}" for v in (x, y, d[0], d[1], d[2], d[3], d[4], d[5]))
                f.write(f"{r['name']}|{r['settings']}|{meta} {r['tags']}|{r['texture']}|"
                        f"{r['wavetable']}|{r['impulse']}|{r['mod']}|{r['envs']}\n")
        total += len(rows)
        print(f"{len(rows):5d}  {path}")
    print(f"{total} presets in {len(packs)} packs -> {a.out_dir}")


if __name__ == "__main__":
    main()
