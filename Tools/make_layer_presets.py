"""AmbientSynth -- generate the Cosmos and Strike preset banks (Core/src/*Presets.inc).

Both are layers: a bank of presets that touches only one section, so it lands on top of whatever
sound is loaded without disturbing it. The Cosmos bank is the feedback network; the Strike bank is
the Karplus-Strong pluck that fires on note-on.

    python Tools/make_layer_presets.py

Why generated. Two hundred and fifty-six presets written by hand are two hundred and fifty-six
chances to type 0.8 where 0.08 was meant, and the result of that is not a wrong note, it is a
preset that sounds broken and nobody can say why. Here each family is a small designed GRID --
two or three parameters, four or five values each, chosen so that every step is audible -- and
the arithmetic is done by a machine. What is not automated is which parameters a family varies
and over what range; that is the design, and it is written out one family at a time below.

Every generated preset is then rendered and measured by Tools/check_layer_presets.py, which is
what actually keeps a silent or a clipping one out of the bank.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))

COSMOS_OUT = os.path.join(ROOT, "Core", "src", "CosmosPresets.inc")
STRIKE_OUT = os.path.join(ROOT, "Core", "src", "StrikePresets.inc")


def fmt(**kw):
    """A settings string in the parameter table's own key order-independent form."""
    return ";".join("%s=%s" % (k, v if isinstance(v, str) else ("%g" % v)) for k, v in kw.items())


# ============================================================ Cosmos
# Sixteen families of sixteen. Each family names the two or three things that make it what it is
# and walks them over a grid; the rest of the section stays at whatever the family's base says.
COSMOS = []


def cfam(name, presets):
    assert len(presets) == 16, (name, len(presets))
    COSMOS.append((name, presets))


def grid(names, base, axes, fixed=None):
    """`axes` is a list of (key, [values]); the product must come to sixteen."""
    out, i = [], 0
    (k1, v1), (k2, v2) = axes
    for a in v1:
        for b in v2:
            s = dict(base)
            s[k1] = a
            s[k2] = b
            if fixed:
                s.update(fixed)
            out.append((names[i], fmt(**s)))
            i += 1
    return out


# ---- 1. Shift: the frequency shifter, which is what makes the network inharmonic
cfam("Shift", grid(
    ["Breath Shift", "Hair Shift", "Slow Slide", "Cold Slide",
     "Six Hertz", "Twelve", "Twenty Five", "Fifty",
     "Ninety", "Hundred Fifty", "Two Twenty", "Three Hundred",
     "Falling Six", "Falling Forty", "Falling Ninety", "Falling Two Twenty"],
    dict(cosmos_send=0.8, cosmos_shift_drift=0.4, cosmos_return=0.7, cosmos_to_far=0.5),
    [("cosmos_shift", [1.5, 6, 25, 90]), ("cosmos_shift_drift", [0.15, 0.45, 0.8, 1.0])]))

# ---- 2. Beating: shifts small enough that the ear hears interference, not pitch
cfam("Beating", grid(
    ["Half Hertz", "One Hertz", "Two Hertz", "Four Hertz",
     "Slow Wash", "Slow Roll", "Slow Turn", "Slow Fold",
     "Deep Beat", "Deep Sway", "Deep Pulse", "Deep Fold",
     "Near Still", "Barely Moving", "Almost Static", "Held Breath"],
    dict(cosmos_shift_drift=0.2, cosmos_smear=0.8, cosmos_to_far=0.6),
    [("cosmos_shift", [0.5, 1, 2, 4]), ("cosmos_send", [0.5, 0.7, 0.85, 1.0])],
    fixed=dict(cosmos_return=0.75)))

# ---- 3. Resonators: the tuned comb, at the ratios that mean something against the root
cfam("Resonators", grid(
    ["Root", "Root Deep", "Root Long", "Root Endless",
     "Fifth", "Fifth Deep", "Fifth Long", "Fifth Endless",
     "Octave", "Octave Deep", "Octave Long", "Octave Endless",
     "Twelfth", "Twelfth Deep", "Twelfth Long", "Twelfth Endless"],
    dict(cosmos_send=1.0, cosmos_res=0.7, cosmos_return=0.7, cosmos_to_far=0.4),
    [("cosmos_res_pitch", [1, 1.5, 2, 3]), ("cosmos_res_fb", [0.75, 0.86, 0.92, 0.96])]))

# ---- 4. Deep resonators: under the root, where the network becomes a room rather than a note
cfam("Deep", grid(
    ["Hull", "Hull Long", "Keel", "Keel Long",
     "Cellar", "Cellar Long", "Vault", "Vault Long",
     "Under", "Under Long", "Below", "Below Long",
     "Foundation", "Foundation Long", "Bedrock", "Bedrock Long"],
    dict(cosmos_send=1.0, cosmos_res=0.85, cosmos_return=0.8, cosmos_to_far=0.3),
    [("cosmos_res_pitch", [0.25, 0.375, 0.5, 0.75]), ("cosmos_res_fb", [0.82, 0.90, 0.94, 0.965])]))

# ---- 5. Vowels: the formant bank inside the loop, from a held vowel to a talking one
cfam("Vowels", grid(
    ["Held Vowel", "Slow Vowel", "Turning Vowel", "Talking",
     "Held Choir", "Slow Choir", "Turning Choir", "Choir Talking",
     "Held Throat", "Slow Throat", "Turning Throat", "Throat Talking",
     "Held Whisper", "Slow Whisper", "Turning Whisper", "Whisper Talking"],
    dict(cosmos_send=0.85, cosmos_return=0.8, cosmos_to_far=0.5, cosmos_smear=0.6),
    [("cosmos_vowel", [0.45, 0.65, 0.85, 1.0]), ("cosmos_vowel_rate", [0.008, 0.03, 0.09, 0.28])]))

# ---- 6. Nebula: diffusion, where individual repeats stop being repeats
cfam("Nebula", grid(
    ["Thin Cloud", "Thin Drift", "Thin Wash", "Thin Fog",
     "Cloud", "Drift", "Wash", "Fog",
     "Deep Cloud", "Deep Drift", "Deep Wash", "Deep Fog",
     "Total Cloud", "Total Drift", "Total Wash", "Total Fog"],
    dict(cosmos_send=0.9, cosmos_return=0.75, cosmos_to_far=0.7),
    [("cosmos_nebula", [0.3, 0.55, 0.8, 1.0]), ("cosmos_smear", [0.3, 0.55, 0.8, 1.0])]))

# ---- 7. Shimmer: pitch-shifted feedback, the octave-up bloom
cfam("Shimmer", grid(
    ["Shimmer Up", "Shimmer Up Deep", "Shimmer Up Full", "Shimmer Up Total",
     "Shimmer Fifth", "Shimmer Fifth Deep", "Shimmer Fifth Full", "Shimmer Fifth Total",
     "Shimmer Down", "Shimmer Down Deep", "Shimmer Down Full", "Shimmer Down Total",
     "Shimmer Two Up", "Shimmer Two Up Deep", "Shimmer Two Up Full", "Shimmer Two Up Total"],
    dict(cosmos_send=0.9, cosmos_return=0.75, cosmos_to_far=0.6, cosmos_smear=0.5),
    [("cosmos_shimmer_pitch", ["+12", "+7", "-12", "+24"]),
     ("cosmos_shimmer", [0.3, 0.5, 0.75, 1.0])]))

# ---- 8. Metallic: a big shift with the resonator behind it, which is what makes it ring like metal
cfam("Metallic", grid(
    ["Sheet", "Sheet Long", "Plate", "Plate Long",
     "Girder", "Girder Long", "Rail", "Rail Long",
     "Wire", "Wire Long", "Cable", "Cable Long",
     "Foil", "Foil Long", "Blade", "Blade Long"],
    dict(cosmos_send=1.0, cosmos_res=0.55, cosmos_return=0.7, cosmos_to_far=0.6,
         cosmos_shift_drift=0.25),
    [("cosmos_shift", [60, 110, 180, 260]), ("cosmos_res_fb", [0.80, 0.88, 0.93, 0.96])]))

# ---- 9. Glass: high resonators and the shimmer up, nothing below the middle
cfam("Glass", grid(
    ["Pane", "Pane Bright", "Pane Long", "Pane Endless",
     "Bowl", "Bowl Bright", "Bowl Long", "Bowl Endless",
     "Flute Glass", "Flute Bright", "Flute Long", "Flute Endless",
     "Ice Glass", "Ice Bright", "Ice Long", "Ice Endless"],
    dict(cosmos_send=0.9, cosmos_res=0.5, cosmos_return=0.65, cosmos_to_far=0.7,
         cosmos_shimmer=0.4, cosmos_shimmer_pitch="+12"),
    [("cosmos_res_pitch", [3, 4, 6, 8]), ("cosmos_res_fb", [0.78, 0.86, 0.92, 0.955])]))

# ---- 10. Drift: the shift wanders on its own curve, so nothing is ever quite where it was
cfam("Drift", grid(
    ["Tide", "Tide Wide", "Tide Deep", "Tide Total",
     "Current", "Current Wide", "Current Deep", "Current Total",
     "Weather", "Weather Wide", "Weather Deep", "Weather Total",
     "Season", "Season Wide", "Season Deep", "Season Total"],
    dict(cosmos_send=0.85, cosmos_shift_drift=1.0, cosmos_smear=0.7, cosmos_return=0.75),
    [("cosmos_shift", [4, 14, 40, 110]), ("cosmos_to_far", [0.3, 0.5, 0.75, 1.0])]))

# ---- 11. Wide: everything sent to the far plane, which is what puts the network behind the room
cfam("Wide", grid(
    ["Horizon", "Horizon Far", "Horizon Deep", "Horizon Total",
     "Distance", "Distance Far", "Distance Deep", "Distance Total",
     "Expanse", "Expanse Far", "Expanse Deep", "Expanse Total",
     "Beyond", "Beyond Far", "Beyond Deep", "Beyond Total"],
    dict(cosmos_send=0.8, cosmos_smear=0.85, cosmos_nebula=0.5, cosmos_shift=7,
         cosmos_shift_drift=0.6),
    [("cosmos_to_far", [0.55, 0.7, 0.85, 1.0]), ("cosmos_return", [0.25, 0.45, 0.65, 0.85])]))

# ---- 12. Ghost: barely there, which is where this network is at its best
cfam("Ghost", grid(
    ["Trace", "Trace Slow", "Trace Deep", "Trace Wide",
     "Rumour", "Rumour Slow", "Rumour Deep", "Rumour Wide",
     "Shadow", "Shadow Slow", "Shadow Deep", "Shadow Wide",
     "Whisper Back", "Whisper Slow", "Whisper Deep", "Whisper Wide"],
    dict(cosmos_shift=3, cosmos_shift_drift=0.7, cosmos_smear=0.9, cosmos_nebula=0.4),
    [("cosmos_send", [0.2, 0.32, 0.45, 0.6]), ("cosmos_return", [0.2, 0.35, 0.5, 0.65])],
    fixed=dict(cosmos_to_far=0.8)))

# ---- 13. Choir: the vowel bank and the resonator together, which is where voices come from
cfam("Choir", grid(
    ["Alien Choir", "Alien Deep", "Alien Long", "Alien Endless",
     "Far Choir", "Far Deep", "Far Long", "Far Endless",
     "Low Choir", "Low Deep", "Low Long", "Low Endless",
     "High Choir", "High Deep", "High Long", "High Endless"],
    dict(cosmos_send=0.9, cosmos_vowel=0.9, cosmos_vowel_rate=0.02, cosmos_res=0.5,
         cosmos_return=0.8, cosmos_to_far=0.55),
    [("cosmos_res_pitch", [1, 0.5, 2, 4]), ("cosmos_res_fb", [0.80, 0.88, 0.93, 0.96])]))

# ---- 14. Machine: shift, resonator and diffusion at once -- the network as an object, not a space
cfam("Machine", grid(
    ["Engine", "Engine Hot", "Engine Long", "Engine Total",
     "Turbine", "Turbine Hot", "Turbine Long", "Turbine Total",
     "Reactor", "Reactor Hot", "Reactor Long", "Reactor Total",
     "Generator", "Generator Hot", "Generator Long", "Generator Total"],
    dict(cosmos_send=1.0, cosmos_res=0.6, cosmos_nebula=0.55, cosmos_shift_drift=0.3,
         cosmos_return=0.7, cosmos_to_far=0.45),
    [("cosmos_shift", [18, 45, 95, 170]), ("cosmos_res_fb", [0.78, 0.86, 0.92, 0.955])]))

# ---- 15. Bloom: shimmer and nebula, the slow upward opening
cfam("Bloom", grid(
    ["Open", "Open Wide", "Open Deep", "Open Total",
     "Rise", "Rise Wide", "Rise Deep", "Rise Total",
     "Ascend", "Ascend Wide", "Ascend Deep", "Ascend Total",
     "Lift", "Lift Wide", "Lift Deep", "Lift Total"],
    dict(cosmos_send=0.9, cosmos_shimmer_pitch="+12", cosmos_smear=0.75,
         cosmos_return=0.7, cosmos_to_far=0.7),
    [("cosmos_shimmer", [0.35, 0.55, 0.75, 0.95]), ("cosmos_nebula", [0.2, 0.45, 0.7, 0.95])]))

# ---- 16. Edge: the top of the feedback range, where the network starts to sing on its own
cfam("Edge", grid(
    ["Held", "Held Bright", "Held Low", "Held Wide",
     "Singing", "Singing Bright", "Singing Low", "Singing Wide",
     "Ringing", "Ringing Bright", "Ringing Low", "Ringing Wide",
     "On The Edge", "Edge Bright", "Edge Low", "Edge Wide"],
    dict(cosmos_send=0.85, cosmos_res=0.9, cosmos_shift=2, cosmos_shift_drift=0.5,
         cosmos_smear=0.6, cosmos_to_far=0.5),
    [("cosmos_res_fb", [0.93, 0.95, 0.962, 0.97]), ("cosmos_res_pitch", [1, 2, 0.5, 3])],
    fixed=dict(cosmos_return=0.6)))


# ============================================================ Strike
# The Karplus-Strong pluck. Three excitations (a string, a dull wooden knock, a stretched metal
# string with an all-pass in its loop) and three controls, which is a small space -- so this bank
# is forty presets rather than two hundred, and every one of them is somewhere different in it.
STRIKE = [
    ("Strings", [
        ("Nylon", dict(strike_level=0.55, strike_type="String", strike_decay=0.35, strike_damp=0.70)),
        ("Steel", dict(strike_level=0.60, strike_type="String", strike_decay=0.60, strike_damp=0.35)),
        ("Harp", dict(strike_level=0.50, strike_type="String", strike_decay=1.10, strike_damp=0.45)),
        ("Long String", dict(strike_level=0.55, strike_type="String", strike_decay=2.20, strike_damp=0.30)),
        ("Muted", dict(strike_level=0.60, strike_type="String", strike_decay=0.12, strike_damp=0.80)),
        ("Damped Felt", dict(strike_level=0.45, strike_type="String", strike_decay=0.25, strike_damp=0.95)),
        ("Open String", dict(strike_level=0.65, strike_type="String", strike_decay=1.60, strike_damp=0.12)),
        ("Wire", dict(strike_level=0.70, strike_type="String", strike_decay=0.90, strike_damp=0.05)),
        ("Whisper Pluck", dict(strike_level=0.22, strike_type="String", strike_decay=0.80, strike_damp=0.55)),
        ("Cloud Pluck", dict(strike_level=0.35, strike_type="String", strike_decay=3.00, strike_damp=0.60)),
    ]),
    ("Wood", [
        ("Knock", dict(strike_level=0.55, strike_type="Wood", strike_decay=0.10, strike_damp=0.60)),
        ("Block", dict(strike_level=0.60, strike_type="Wood", strike_decay=0.20, strike_damp=0.45)),
        ("Marimba Hit", dict(strike_level=0.55, strike_type="Wood", strike_decay=0.45, strike_damp=0.35)),
        ("Log Drum", dict(strike_level=0.65, strike_type="Wood", strike_decay=0.80, strike_damp=0.30)),
        ("Dry Tap", dict(strike_level=0.40, strike_type="Wood", strike_decay=0.06, strike_damp=0.85)),
        ("Hollow Wood", dict(strike_level=0.60, strike_type="Wood", strike_decay=1.40, strike_damp=0.20)),
        ("Soft Mallet", dict(strike_level=0.35, strike_type="Wood", strike_decay=0.55, strike_damp=0.75)),
        ("Far Knock", dict(strike_level=0.20, strike_type="Wood", strike_decay=0.30, strike_damp=0.55)),
        ("Kalimba Tine", dict(strike_level=0.50, strike_type="Wood", strike_decay=0.90, strike_damp=0.15)),
        ("Bamboo", dict(strike_level=0.58, strike_type="Wood", strike_decay=0.14, strike_damp=0.72)),
    ]),
    ("Metal", [
        ("Bell Pluck", dict(strike_level=0.50, strike_type="Metal", strike_decay=1.20, strike_damp=0.35)),
        ("Clang", dict(strike_level=0.65, strike_type="Metal", strike_decay=0.60, strike_damp=0.20)),
        ("Gamelan", dict(strike_level=0.55, strike_type="Metal", strike_decay=2.00, strike_damp=0.40)),
        ("Anvil", dict(strike_level=0.70, strike_type="Metal", strike_decay=0.25, strike_damp=0.10)),
        ("Long Metal", dict(strike_level=0.50, strike_type="Metal", strike_decay=3.00, strike_damp=0.25)),
        ("Glass Tine", dict(strike_level=0.40, strike_type="Metal", strike_decay=1.60, strike_damp=0.55)),
        ("Struck Wire", dict(strike_level=0.60, strike_type="Metal", strike_decay=0.80, strike_damp=0.05)),
        ("Distant Bell", dict(strike_level=0.22, strike_type="Metal", strike_decay=2.40, strike_damp=0.45)),
        ("Sheet", dict(strike_level=0.62, strike_type="Metal", strike_decay=0.45, strike_damp=0.15)),
        ("Halo", dict(strike_level=0.32, strike_type="Metal", strike_decay=2.80, strike_damp=0.65)),
    ]),
    ("Conducted", [
        # The same excitations, but fired by the conductor as well as by the keys -- which turns
        # the pluck from something you play into something the piece does on its own.
        ("Rain On Strings", dict(strike_level=0.35, strike_type="String", strike_decay=1.20, strike_damp=0.55, strike_who="Keys + Brain")),
        ("Slow Harp", dict(strike_level=0.30, strike_type="String", strike_decay=2.60, strike_damp=0.40, strike_who="Keys + Brain")),
        ("Falling Wood", dict(strike_level=0.34, strike_type="Wood", strike_decay=0.50, strike_damp=0.55, strike_who="Keys + Brain")),
        ("Wind Chimes", dict(strike_level=0.28, strike_type="Metal", strike_decay=2.20, strike_damp=0.50, strike_who="Keys + Brain")),
        ("Temple", dict(strike_level=0.32, strike_type="Metal", strike_decay=3.00, strike_damp=0.30, strike_who="Keys + Brain")),
        ("Distant Work", dict(strike_level=0.20, strike_type="Wood", strike_decay=0.18, strike_damp=0.70, strike_who="Keys + Brain")),
        ("Ghost Pluck", dict(strike_level=0.16, strike_type="String", strike_decay=1.80, strike_damp=0.65, strike_who="Keys + Brain")),
        ("Gamelan Rain", dict(strike_level=0.30, strike_type="Metal", strike_decay=1.40, strike_damp=0.45, strike_who="Keys + Brain")),
        ("Loose Strings", dict(strike_level=0.40, strike_type="String", strike_decay=0.70, strike_damp=0.20, strike_who="Keys + Brain")),
        ("Slow Bells", dict(strike_level=0.26, strike_type="Metal", strike_decay=3.00, strike_damp=0.55, strike_who="Keys + Brain")),
    ]),
]


def write_bank(path, array, cat_array, off_name, off_settings, families, what):
    nl = chr(10)
    lines = ["// Generated by Tools/make_layer_presets.py -- do not edit.",
             "// %s" % what,
             "const Preset %s[] = {" % array,
             '    { "%s", "%s" },' % (off_name, off_settings)]
    cats = []
    names = set()
    for fi, (fam, presets) in enumerate(families):
        for name, settings in presets:
            assert name not in names, "duplicate preset name: " + name
            names.add(name)
            lines.append('    { "%s", "%s" },' % (name, settings))
            cats.append(fi)
    lines.append("};")
    lines.append("")
    lines.append("// The family each preset belongs to; 255 is the Off entry, which has none.")
    lines.append("const unsigned char %s[] = {" % cat_array)
    lines.append("    255,")
    for i in range(0, len(cats), 12):
        lines.append("    " + " ".join("%d," % c for c in cats[i:i + 12]))
    lines.append("};")
    lines.append("")
    lines.append("const char* const %sFamilyNames[] = {" % array)
    for fam, _ in families:
        lines.append('    "%s",' % fam)
    lines.append("};")
    lines.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write(nl.join(lines))
    return len(cats) + 1


def main():
    n = write_bank(COSMOS_OUT, "kCosmosPresets", "kCosmosPresetCategory", "Cosmos Off", "",
                   COSMOS, "%d presets in %d families, touching only the Cosmos section."
                   % (sum(len(p) for _, p in COSMOS) + 1, len(COSMOS)))
    print("wrote %s: %d presets in %d families" % (COSMOS_OUT, n, len(COSMOS)))
    fams = [(f, [(nm, fmt(**kw)) for nm, kw in ps]) for f, ps in STRIKE]
    n = write_bank(STRIKE_OUT, "kStrikePresets", "kStrikePresetCategory", "Strike Off",
                   "strike_level=0", fams,
                   "%d presets in %d families, touching only the Strike section."
                   % (sum(len(p) for _, p in fams) + 1, len(fams)))
    print("wrote %s: %d presets in %d families" % (STRIKE_OUT, n, len(fams)))


if __name__ == "__main__":
    main()
