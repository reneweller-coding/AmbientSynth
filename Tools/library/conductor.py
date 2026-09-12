"""Rene's parameter ranges for what the conductor PLAYS, per artist.

`conductor_ranges.json` is his document -- seven families, 56 artists, each artist's ranges already
resolved against its family -- and this reads it and draws from it. Nothing here decides anything:
the file is the authority, and where a range is missing the generator's own value stands.

Two things are added on top of the file, both of them from rules in the rule book that the table
simply has no column for: a breath period for a conductor whose rate breathes (R5.1 says six to
twenty minutes), a length for a planned silence (R5.6 says ten to forty seconds), and a density
slew, because the Arc moves density and Anti 9 says it may not move by more than a voice a minute.

The "astir" rule from the file's own header is applied to half of each pack, so a pack holds a
still and a moving half rather than one temperament throughout.
"""

import json
import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(HERE, "conductor_ranges.json"), encoding="utf-8") as fh:
    SPEC = json.load(fh)

# The names the document uses for the scales, against the synth's own list.
SCALES = {
    "12-TET": "12-TET",
    "Ptolemy major": "JI Major (Ptolemy)",
    "just minor": "JI Minor",
    "7-limit": "JI 7-limit",
    "Pythagorean": "Pythagorean",
    "just pentatonic": "JI Pentatonic",
    "harmonic 8-16": "Harmonic 8-16",
    "subharmonic 16-8": "Subharmonic 16-8",
    "slendro": "Slendro (JI)",
    "Bohlen-Pierce": "Bohlen-Pierce (JI)",
    "otonality 1-3-5-7-9-11": "Otonality 1-11",
}
# Brain 2 stands a degree over the first conductor's root, and the synth says that in semitones.
INTERVALS = {"Root": 0, "Fifth": 7, "Fourth": 5, "Octave": 12, "Seventh": 10}
# What has to come out whole.
INTS = {"brain_density", "brain_low", "brain_high", "brain_loop", "brain_third_floor",
        "brain2_density", "brain2_low", "brain2_high", "auto_lead", "brain2_interval"}
# Which of the document's keys are switches rather than numbers.
FLAGS = {"brain2_on", "brain2_golden", "brain_onset_guard", "tuning_hold_sounding"}

# The built-in families are not artists, so each is given the family whose temperament it shares.
BUILTIN_FAMILY = {
    "Just Drones": "sleep", "Glass and Bells": "luminous", "Choirs and Vowels": "ritual",
    "Deep and Sub": "deep", "Bowed Strings": "brit", "Organs and Reeds": "ritual",
    "Played Keys": "luminous", "Generative Chords": "luminous", "Cosmos": "space",
    "Clouds and Memory": "cold", "Weather and Noise": "cold", "Metal and Feedback": "deep",
    "Space and Motion": "space", "Slow Worlds": "sleep", "Microtonal": "space",
    "Strike and Modal": "ritual",
}


def ranges_for(name):
    """The resolved ranges for an artist, or for a built-in family's stand-in. None if neither."""
    art = SPEC["artists"].get(name)
    if art is not None:
        return art["ranges"]
    fam = BUILTIN_FAMILY.get(name)
    return SPEC["families"][fam]["ranges"] if fam else None


def family_of(name):
    """An artist's family, or a built-in family's stand-in. None if neither."""
    art = SPEC["artists"].get(name)
    return art["family"] if art is not None else BUILTIN_FAMILY.get(name)


# Where the foreground stands against the background (12.09.2026, after the third reading of the
# day). Three things the instrument has had for rounds and the library never used: the far reverb
# stepping aside under a sounding voice (far_unmask -- at nought in all 14336 presets), the
# presence bell on the near plane (presence -- at nought in 79 %), and a far tail that is kept
# out of the bell's own band where a preset has one. Measured before this: one preset in five with
# its voice under a bed that is the sub at its floor. By family, because the dark and the cold
# profiles want nothing near, and drawn from a stream seeded by the preset's name, so that a
# preset already generated moves in nothing else.
DEPTH = {
    "sleep":    {"presence": (2.0, 3.5), "unmask": (0.30, 0.45), "cap": 2800.0},
    "luminous": {"presence": (2.0, 4.0), "unmask": (0.30, 0.45), "cap": 3000.0},
    "space":    {"presence": (1.5, 3.0), "unmask": (0.25, 0.40), "cap": 3200.0},
    "ritual":   {"presence": (1.0, 2.5), "unmask": (0.25, 0.40), "cap": 2800.0},
    "cold":     {"presence": (0.0, 1.0), "unmask": (0.20, 0.35), "cap": None},
    "brit":     {"presence": (0.0, 1.0), "unmask": (0.20, 0.35), "cap": None},
    "deep":     {"presence": (0.0, 0.0), "unmask": (0.25, 0.40), "cap": None},
}


def depth_cues(artist, preset_name, params):
    """The keys to add to a preset for its foreground: far_unmask (+spread), presence, and the far
    tail cap. `artist` is the pack's artist or the built-in family; `params` is what the preset
    already holds (far_highcut is read from it). Empty if the family is unknown."""
    fam = family_of(artist)
    if fam is None:
        return {}
    d = DEPTH[fam]
    rnd = random.Random(preset_name + "|depth")
    out = {"far_unmask": round(rnd.uniform(*d["unmask"]), 3),
           "far_unmask_spread": round(rnd.uniform(0.3, 0.5), 3)}
    lo, hi = d["presence"]
    presence = rnd.uniform(lo, hi) if hi > 0.0 else 0.0
    if presence >= 0.3:
        out["presence"] = round(presence, 2)
    elif "presence" in params:
        out["presence"] = 0.0          # the family wants nothing near: the style's own draw yields
    cap = d["cap"]
    if cap and presence >= 1.5:
        try:
            cut = float(params.get("far_highcut", 3500.0))
        except (TypeError, ValueError):
            cut = 3500.0
        if cut > cap:
            out["far_highcut"] = round(cap, 1)
    return out


def _one(spec, rnd):
    kind = spec["type"]
    if kind == "fixed":
        return spec["value"]
    if kind == "choice":
        opts = list(spec["options"].items())
        total = sum(w for _, w in opts) or 1.0
        r = rnd.random() * total
        for value, w in opts:
            r -= w
            if r <= 0.0:
                return value
        return opts[-1][0]
    lo, hi = float(spec["min"]), float(spec["max"])
    if hi <= lo:
        return lo
    if kind == "log-uniform":
        return math.exp(rnd.uniform(math.log(max(lo, 1e-6)), math.log(hi)))
    return rnd.uniform(lo, hi)


def _clamp(v, lo, hi):
    return max(lo, min(hi, v))


def draw(name, rnd, astir=False, params=None):
    """One preset's worth of conductor settings. `params` is the synth's own table, so a key the
    running binary does not have is dropped rather than written into a preset that cannot load."""
    spec = ranges_for(name)
    if spec is None:
        return {}
    out = {}
    for key, rng in spec.items():
        value = _one(rng, rnd)
        if key == "scale":
            out["scale"] = SCALES.get(value, value)
            continue
        if key == "brain2_interval":
            out["brain2_interval"] = INTERVALS.get(value, 0)
            continue
        if key in FLAGS:
            out[key] = "on" if value == "on" else "off"
            continue
        if isinstance(value, str):
            out[key] = value
            continue
        out[key] = float(value)

    # The moving half of a pack (the document's own rule, under "Still und bewegt") -- but never
    # against a value the artist's own row PINS. A range is a spread and may be leaned on; a fixed
    # value is a statement, and "nichts wandert" is the whole of Paul Bradley, of Francisco Lopez,
    # of Chris Watson and of the brit family. Without this the moving half of those packs wandered
    # at 0.1, which is exactly what their entry in the table says they do not do.
    def pinned(key):
        return spec.get(key, {}).get("type") == "fixed"

    if astir:
        for key, factor in (("brain_rate", 0.6), ("auto_rate", 0.6), ("auto_root_move", 1.5)):
            if key in out and not pinned(key):
                out[key] = float(out[key]) * factor
        for key, add in (("brain_wander", 0.1), ("brain_rate_breath", 0.2), ("brain_cascade", 0.1)):
            if key in out and not pinned(key):
                out[key] = float(out[key]) + add

    # Three the table has no column for, each from a rule that does.
    if float(out.get("brain_rate_breath", 0.0) or 0.0) > 0.0:
        out["brain_breath_period"] = math.exp(rnd.uniform(math.log(6.0), math.log(20.0)))
    if float(out.get("brain_silence", 0.0) or 0.0) > 0.0:
        out["brain_silence_len"] = rnd.uniform(12.0, 35.0)
    # The Arc leans on density by up to two voices; Anti 9 says that may not arrive at once.
    out["brain_density_slew"] = rnd.uniform(2.0, 6.0)
    # R8.3: two note-offs at least two seconds apart. The table has no column for it, and every
    # pack of the first 2.0 library left it at nought (measured: a quarter of the presets had
    # note-offs inside two seconds of each other). Drawn from a stream of its own, seeded by the
    # name, so that adding it moved nothing else in a preset already generated.
    out["brain_release_gap"] = random.Random(name + "|release_gap").uniform(2.0, 4.0)
    # The conductor's clock comes from the table, never from the tempo. brain_sync does not put the
    # events on a grid -- it REPLACES the event rate with a number of bars, and with it everything
    # the table says about how often this artist moves. Measured in the first generated library:
    # 1007 presets took their rate from the clock, 670 of them landed outside their own artist's
    # range, and Francisco Lopez -- whose entry reads 250 to 300 seconds, the conductor as absence
    # -- was playing an event every 10.7 seconds. The grid, where an artist wants one, is
    # brain_quantize: it holds a decision until the next bar line and leaves the rate alone.
    out["brain_sync"] = "Free"

    # Two guards from the document's own header. The hold is deliberately fixed in a few places --
    # the brit family, Paul Bradley -- so it is only the ORDER that is enforced here, never the
    # spread, and a conductor whose rate breathes keeps a mean it can breathe around.
    if "brain_hold_min" in out and "brain_hold_max" in out and out["brain_hold_max"] < out["brain_hold_min"]:
        out["brain_hold_min"], out["brain_hold_max"] = out["brain_hold_max"], out["brain_hold_min"]
    if "brain2_hold_min" in out and "brain2_hold_max" in out and out["brain2_hold_max"] < out["brain2_hold_min"]:
        out["brain2_hold_min"], out["brain2_hold_max"] = out["brain2_hold_max"], out["brain2_hold_min"]
    if "brain_low" in out and "brain_high" in out and out["brain_high"] < out["brain_low"] + 7:
        out["brain_high"] = out["brain_low"] + 7
    if "brain2_low" in out and "brain2_high" in out and out["brain2_high"] < out["brain2_low"] + 7:
        out["brain2_high"] = out["brain2_low"] + 7

    # Round what has to be whole, clamp everything to what the synth will accept, and drop what it
    # does not know -- a generator run against an older binary writes no unloadable keys.
    final = {}
    for key, value in out.items():
        if params is not None and key not in params:
            continue
        if isinstance(value, str):
            final[key] = value
            continue
        if key in INTS:
            value = int(round(value))
        if params is not None:
            _, lo, hi, _ = params[key]
            value = _clamp(value, lo, hi)
            if key in INTS:
                value = int(round(value))
        final[key] = value
    return final


def coverage(params):
    """Every key the document names, and whether this binary has it. For the run's own report."""
    keys = sorted({k for a in SPEC["artists"].values() for k in a["ranges"]})
    return [(k, params is None or k in params) for k in keys]
