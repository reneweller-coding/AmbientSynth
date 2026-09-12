"""The preset library's styles: one pack per artist, and the families of the built-in presets.

Fifty-six packs of 256 presets each, every one written in the spirit of one artist of the ambient and
drone repertoire (Tools/library/artists_research.md holds what that means and where it comes from),
and sixteen families of sixteen for the presets compiled into the instrument. Nothing here is sampled
from or affiliated with any of the artists; the pack names are descriptive, and the `inspiration`
line names whose sound world the settings aim at.

A style is what styles.py made it -- parameter ranges over Core/src/Params.cpp, module weights, the
two-part vocabulary of its preset names -- and what the new library made possible:

  material    which clips it plays. The prompt lists the library was generated from name the artists
              every clip was written for (clip_catalog.py); a style's own name is its first target,
              `kin` borrows other artists' clips at a weight, `cats`, `worlds` and `gestures` lean the
              draw (a weight of 1 doubles a clip's chance) and seed it for artists no clip names,
              `avoid` keeps categories out, `fr` is the share of the bed that is field recording.
  types       how likely each source type is, for the first slot (the voice) and for the layers.
  wavetables  table families by weight: harmonic:<family>, ambient:<family>, classic:akwf|wavedit, and
              builtin:<Classic|Organ|Vocal|Glass|Metal> for a table without a file.
  rooms       impulse families by weight: the generated rooms and the curated diffusion and real rooms.
  details     `strikes` (the Strike's types), `zshapes` (shapes worth ringing in Modal mode),
              `filters` (filter models), `field` (an environment: no tonal anchor), `sparse` (fewer
              layers), `builtin` (no files at all: compiled into the instrument).

Range specs as in styles.py: 3.5 a constant, (lo, hi) uniform, ("log", lo, hi), ("int", lo, hi),
["A", "B"] a choice.
"""
from styles import BASE, MODULES_BASE, COMMON_SECOND, GRANULAR_BASE, NOISE_BASE


def W(text):
    """'flute 1 steel-guitar 1.2' -> {'flute': 1.0, 'steel-guitar': 1.2}"""
    toks = text.split()
    return {toks[i]: float(toks[i + 1]) for i in range(0, len(toks) - 1, 2)}


def K(text):
    """'Steve Roach .4 | Michael Stearns .3' -> {'Steve Roach': 0.4, 'Michael Stearns': 0.3}"""
    out = {}
    for part in text.split("|"):
        part = part.strip()
        if part:
            name, w = part.rsplit(" ", 1)
            out[name.strip()] = float(w)
    return out


def T(lead, layer):
    return {"lead": W(lead), "layer": W(layer)}


def F(text):
    return text.split()


def P(*parts, **kw):
    out = {}
    for part in parts:
        out.update(part)
    out.update(kw)
    return out


def reg(lo1, lo2, hi1, hi2):
    return {"brain_low": ("int", lo1, lo2), "brain_high": ("int", hi1, hi2)}


def pace(rate, hold_min, hold_max):
    return {"brain_rate": ("log",) + tuple(rate), "brain_hold_min": ("log",) + tuple(hold_min),
            "brain_hold_max": ("log",) + tuple(hold_max)}


def env(attack, release):
    return {"attack": ("log",) + tuple(attack), "release": ("log",) + tuple(release)}


def M(**kw):
    return kw


# ---------------------------------------------------------------- shared ground

# What the instrument grew since styles.py was written, as weights every style starts from.
MODULES = dict(MODULES_BASE,
               memory=0.22,        # the Memory: long drifting lines, recall, reverse, half speed
               cloudplus=0.45,     # the Cloud's feedback, scatter, swarm and resonators, where a Cloud is on
               unison=0.30,        # detuned copies of a pitched slot
               hermite=0.55,       # a clip read on a curve instead of a straight line
               rootfloor=0.6,      # a floor under a table's fundamental where the table has little
               grainshimmer=0.2,   # the Cosmos shimmer on the old two-head shifter instead of the spectral one
               ownenv=0.25,        # a later source entering on a contour of its own
               keylock=0.85,       # the key and scale taken from the clip the voice is playing
               # What the conductor judges by. All of these existed and almost none of them were
               # used: the first 2.0 library set the chord-level harmonicity in 8 % of its presets
               # and the key in 23 %, and the 195 hand-written built-ins before it set them in
               # none at all. Measured on nine presets with a fast clock, the chord harmonicity
               # halves the roughness (median 0.0058 -> 0.0026), so it is worth having on.
               chordharm=0.5,      # judge a candidate against the WHOLE chord, not just the root
               cluster=0.3,        # seconds that crowd on purpose (negative Spacing)
               timbre=0.55,        # consonance heard through the preset's own spectrum
               keyfind=0.45,       # the key the music has drifted into
               evensmooth=0.45,    # even intervals, and steps rather than leaps
               brain2=0.35)        # the background plane gets a conductor of its own

TYPES_BASE = T("Additive 1 Harmonic 1.2 Wavetable .6 FM .3 Bow .3 Texture 1.2 Stretch .6 Spectral .8",
               "Additive .6 Harmonic .8 Wavetable .5 FM .3 Noise .35 Bow .25 Texture 1 Stretch 1 Spectral .7")
TABLES_BASE = W("harmonic:pad 1 harmonic:strings 1 harmonic:organ 1 ambient:consonant 1 ambient:bowed 1 "
                "ambient:sampled 1 ambient:otmorph 1 builtin:Classic .3 builtin:Organ .3 builtin:Vocal .3 "
                "builtin:Glass .3 builtin:Metal .2")
ROOMS_BASE = W("hall 2 cathedral 1 chamber 1 bloom .5 vast .5 plate .5")
MATERIAL_BASE = {"targets": [], "kin": {}, "cats": {}, "avoid": [], "worlds": {}, "gestures": {}, "fr": 0.4}
STRIKE_BASE = ["String", "String", "Wood", "Metal"]
INDUSTRIAL = ["factory", "engine-hull", "electrical-hum", "ventilation", "transport", "city"]
KEYS = ["piano", "electric-piano", "mallet", "tine", "zither-harp"]


def A(name, inspiration, params, modules=None, words=None, material=None, types=None, wavetables=None,
      rooms=None, granular=None, noise=None, strikes=None, zshapes=None, filters=None, builtin=False,
      field=False, sparse=False):
    mat = {k: (list(v) if isinstance(v, list) else (dict(v) if isinstance(v, dict) else v))
           for k, v in MATERIAL_BASE.items()}
    if material:
        mat.update(material)
    mat["targets"] = list(dict.fromkeys([inspiration] + list(mat.get("targets", []))))
    ty = types or TYPES_BASE
    st = {"name": name, "inspiration": inspiration, "params": dict(BASE), "modules": dict(MODULES),
          "words": words or (["Slow", "Long", "Deep"], COMMON_SECOND), "prompts": [], "tables": [],
          "granular": dict(GRANULAR_BASE), "impulses": [], "noise": noise or list(NOISE_BASE),
          "material": mat, "types": {"lead": dict(ty["lead"]), "layer": dict(ty["layer"])},
          "wavetables": dict(wavetables or TABLES_BASE), "rooms": dict(rooms or ROOMS_BASE),
          "strikes": strikes or list(STRIKE_BASE), "zshapes": zshapes or [], "filters": filters or [],
          "builtin": builtin, "field": field, "sparse": sparse}
    if granular:
        st["granular"].update(granular)
    st["params"].update(params)
    if modules:
        st["modules"].update(modules)
    return st


# ---------------------------------------------------------------- the fifty-six packs

ARTISTS = [

    A("Sleep Concert", "Robert Rich",
      P(reg(34, 44, 64, 80), pace((25, 90), (45, 150), (150, 420)), env((8, 30), (15, 50)),
        scale=["JI 7-limit", "Otonality 1-11", "Harmonic 8-16", "JI Major (Ptolemy)", "Slendro (JI)"],
        purity=(0.95, 1.0), brain_density=("int", 4, 7), bloom=(0.3, 0.8), bloom_time=("log", 60, 240),
        sub_level=(0.15, 0.4), sub_binaural=(2.0, 7.0), far_decay=("log", 20, 55), far_highcut=("log", 1800, 4500),
        brightness=(0.45, 0.75), arc=(0.3, 0.7), depth=(0.7, 0.95), air=(0.1, 0.3)),
      M(sub=0.85, zplane=0.3, cosmos=0.25, feedback=0.05, portamento=0.4, memory=0.15, cloud=0.2, adaptive=0.35,
        binaural=0.25, phase=0.6, tide=0.4, keys=0.05),
      words=(F("Sleep Somnia Lullaby Trance Night Dream Canopy Lattice Otonal Glide Rainfall Hypnagogic Moss Ratio "
               "Seventh Bamboo Steel Mist Firefly"),
             COMMON_SECOND + F("Concert Cycle Hours Chorus Mirage Grove Current Vapour")),
      material=dict(kin=K("Steve Roach .4 | Michael Stearns .3"),
                    cats=W("flute 1 steel-guitar 1.2 guitar .6 singing-bowl .6 just-chord .8 analog-pad .5 rainforest 1 insects .4 gamelan .4"),
                    worlds=W("warm .6 cosmic .4"), gestures=W("sustain .6 swell .5 melodic .3"), fr=0.35),
      types=T("Texture 1.3 Harmonic 1 Additive 1.2 Spectral .8 Stretch .4 Bow .2 Wavetable .3 FM .15",
              "Stretch 1 Texture .8 Additive .8 Harmonic .7 Spectral .6 Noise .2"),
      wavetables=W("ambient:consonant 3 ambient:sampled 1.5 harmonic:glass 1 harmonic:pure 1.5 harmonic:breath 1 ambient:otmorph 1 builtin:Glass .3"),
      rooms=W("cathedral 2 vast 1 bloom 1.5 cavern 1 hall 1"),
      granular={"spread": (0.004, 0.05), "grains": 1.3}, noise=["Pink", "Brown", "Pink", "Wind"]),

    A("Deep Earth", "Lustmord",
      P(reg(24, 34, 46, 62), pace((40, 160), (90, 240), (240, 600)), env((10, 40), (20, 80)),
        master_gain=(-13.0, -8.0), brightness=(0.1, 0.35), tilt=(1.6, 2.8), partials=("int", 6, 14),
        cutoff=("log", 250, 1200), far_highcut=("log", 900, 2200), far_decay=("log", 35, 85), far_size=(2.2, 3.0),
        far_damp=(0.5, 0.85), brain_density=("int", 3, 6), sub_level=(0.35, 0.7), sub_octave=["-1", "-2"],
        sub_tone=(0.0, 0.25), bass_mono=("log", 130, 260), air=(0.0, 0.12), shimmer=(0.05, 0.3),
        inharmonic=(0.1, 0.4), depth=(0.75, 1.0), pad_low_cut=("log", 20, 45),
        scale=["JI Minor", "Subharmonic 16-8", "JI 7-limit", "Pythagorean"]),
      M(sub=1.0, feedback=0.25, cloud=0.35, cosmos=0.45, zplane=0.3, memory=0.2, farfreeze=0.1, rotate=0.4, envelop=0.5),
      words=(F("Abyssal Mantle Tectonic Subterranean Obsidian Chthonic Bedrock Magma Catacomb Crypt Basalt Fumarole "
               "Seismic Nadir Umbral Sulphur Ossuary Mineshaft"),
             COMMON_SECOND + F("Chamber Pressure Weight Fault Shaft Well Vault Rift")),
      material=dict(kin=K("Arecibo .5 | Sleep Research Facility .3 | Raison d'être .2"),
                    cats=W("gong 1 chant .8 choir .5 brass .8 bowed-low-string .6 low-wind .6 engine-hull .5 cave 1.2 "
                           "seismic 1 geothermal .8 cave-wind .6 sub-drone .8"),
                    worlds=W("dark .8 ritual .4 cosmic .3"), gestures=W("sustain .6 swell .6 struck .2"),
                    avoid=KEYS + ["guitar", "digital-bell", "steel-guitar"], fr=0.5),
      types=T("Stretch 1.2 Spectral 1.2 Texture .8 Harmonic .7 Additive .5 Bow .4 Wavetable .2 Noise .2",
              "Stretch 1.3 Spectral .9 Noise .5 Texture .6 Harmonic .5 Additive .4"),
      wavetables=W("harmonic:sub 3 ambient:tube 2 ambient:sampled 1.5 ambient:vowel_bass 1.5 harmonic:metal 1 builtin:Metal .3"),
      rooms=W("cavern 3 vast 3 far 1.5 bloom 1 cathedral 1"),
      granular={"spread": (0.006, 0.08), "grains": 1.2}, noise=["Brown", "Pink", "Brown"]),

    A("Desert Ember", "Steve Roach",
      P(reg(33, 43, 60, 76), pace((20, 70), (40, 140), (140, 360)), env((6, 25), (15, 45)),
        scale=["JI Major (Ptolemy)", "JI 7-limit", "JI Pentatonic", "Pythagorean", "12-TET"],
        brightness=(0.4, 0.7), cutoff=("log", 900, 4500), ens_mix=(0.3, 0.7), dly_feedback=(0.4, 0.75),
        dly_mix=(0.12, 0.35), far_decay=("log", 20, 60), sub_level=(0.2, 0.45), arc=(0.3, 0.7)),
      M(delay2=0.5, sync=0.35, coherence=0.3, cloud=0.3, cosmos=0.3, zplane=0.4, filtermodel=0.5, memory=0.25,
        strike=0.15, quantize=0.2, phase=0.5),
      words=(F("Desert Ember Mesa Canyon Songline Dune Heat Arroyo Ochre Monsoon Sunstone Sage Spirit Basin "
               "Saguaro Outback Redrock Coyote"),
             COMMON_SECOND + F("Crossing Circle Night Walk Sky Fire Plain Glow")),
      material=dict(kin=K("Robert Rich .4 | Michael Stearns .3"),
                    cats=W("flute 1 analog-pad 1 modular-drone .8 drone-string .6 blown-vessel .5 rainforest 1 desert 1.2 wind .4 fire .4 just-chord .4"),
                    worlds=W("warm .8 cosmic .6 wild .4"), gestures=W("sustain .6 swell .4 chord .4"), fr=0.4),
      types=T("Texture 1.1 Harmonic 1 Wavetable .8 Additive .9 Spectral .6 Stretch .5 FM .2",
              "Stretch 1.1 Texture .8 Wavetable .6 Harmonic .6 Noise .3"),
      wavetables=W("harmonic:pad 2 harmonic:breath 1.5 harmonic:reed 1 ambient:sampled 1 classic:akwf 1.2 ambient:bowed .8 builtin:Classic .5"),
      rooms=W("hall 2 vast 1.5 bloom 1.5 drift 1 cavern .8"), filters=["Ladder", "Ladder", "LP 24", "LP 6"]),

    A("Ritual Machine", "Deutsch Nepal",
      P(reg(28, 40, 52, 66), pace((8, 40), (15, 60), (60, 180)), env((2, 12), (8, 30)),
        fb_bus=(0.05, 0.18), fb_drive=(0.4, 0.9), fb_tape=(0.3, 0.8), fb_tone=("log", 500, 2500),
        brightness=(0.25, 0.55), cutoff=("log", 500, 2500), brain_density=("int", 3, 6), far_decay=("log", 10, 35),
        near_mix=(0.2, 0.4), dly_feedback=(0.5, 0.8), dly_mix=(0.2, 0.4), sub_level=(0.2, 0.5),
        scale=["JI Minor", "Pythagorean", "12-TET", "Subharmonic 16-8"]),
      M(feedback=1.0, patina=0.6, strike=0.5, cascade=0.4, quantize=0.35, dejavu=0.5, memory=0.4, delay2=0.45,
        cloud=0.35, zplane=0.45, fold=0.3, banks=0.4),
      words=(F("Rust Iron Engine Bellows Anvil Chain Cog Furnace Rivet Piston Hammer Smoke Siren Crank Gear "
               "Soot Lever Boilerplate"),
             COMMON_SECOND + F("Mass Order Loop Yard Shift Drum Works Litany")),
      material=dict(kin=K("Moljebka Pvlse .3 | Tho-So-Aa .3"),
                    cats=W("bowed-metal 1 gong .8 chant .6 pipe-tank .8 feedback-drone .8 metal-creak 1 abandoned .6 factory .6 electrical-hum .4"),
                    worlds=W("industrial .8 ritual .6"), gestures=W("sustain .4 struck .6"),
                    avoid=["flute", "piano", "zither-harp", "crystal-bowl", "solo-voice"], fr=0.45),
      types=T("Texture 1.2 Stretch 1 Spectral .6 Harmonic .5 Wavetable .6 FM .5 Noise .3 Bow .3",
              "Stretch 1 Texture .9 Noise .6 FM .4 Wavetable .4"),
      wavetables=W("harmonic:metal 2 classic:wavedit 1.5 harmonic:resonator 1 ambient:tube 1 builtin:Metal .5"),
      rooms=W("chamber 2 plate 1.5 far 1 cavern 1 drift .5"),
      strikes=["Metal", "Metal", "Wood"], zshapes=["Steel Plate", "Metal Bars", "Tam Tam", "Gong", "Concrete Pipe"],
      noise=["Brown", "Crackle", "Band", "Digital"]),

    A("Arctic Loop", "Biosphere",
      P(reg(36, 46, 62, 78), pace((20, 90), (40, 160), (160, 420)), env((4, 20), (12, 40)),
        brightness=(0.35, 0.65), tilt=(1.2, 2.2), far_highcut=("log", 1500, 4500), far_decay=("log", 20, 60),
        sub_level=(0.2, 0.5), sub_binaural=(0.0, 3.0), brain_density=("int", 2, 5), air=(0.05, 0.25),
        dly_mix=(0.12, 0.3), dly_feedback=(0.4, 0.7), depth=(0.6, 0.95),
        scale=["JI Minor", "JI Pentatonic", "JI Major (Ptolemy)", "12-TET"]),
      M(memory=0.5, sync=0.3, pulse=0.35, cloud=0.3, delay2=0.45, absorb=0.6, dejavu=0.5, comod=0.4, farmode=0.4),
      words=(F("Arctic Fjord Glacier Aurora Polar Tundra Frost Snowfield Isobar Sonar Northern Boreal Cirrus Floe "
               "Radiosonde Fell Skerry Midwinter"),
             COMMON_SECOND + F("Loop Shelf Signal Light Season Coast Pole Beacon")),
      material=dict(kin=K("Thomas Köner .3 | Loscil .3 | Chris Watson .2"),
                    cats=W("digital-bell 1 string-ensemble .6 glass .8 choir .4 ice 1 low-wind .8 sea 1 ice-snow 1 wind .8 radio .5 polar-station .6"),
                    worlds=W("cold 1 luminous .4"), gestures=W("sustain .4 melodic .4 chord .3"), fr=0.45),
      types=T("Texture 1.2 Harmonic 1 Spectral .8 Stretch .6 Additive .6 Wavetable .6",
              "Stretch 1.2 Texture .8 Harmonic .5 Noise .4"),
      wavetables=W("harmonic:glass 2 harmonic:shimmer 1.5 ambient:consonant 1 classic:wavedit 1 ambient:sampled .8 builtin:Glass .4"),
      rooms=W("cavern 2 vast 1.5 hall 1.5 drift 1 far 1"), noise=["Wind", "Pink", "Band", "Crackle"]),

    A("Far Relay", "Martin Stürtzer",
      P(reg(30, 40, 56, 72), pace((20, 80), (40, 160), (160, 420)), env((6, 30), (15, 50)),
        brightness=(0.3, 0.6), far_decay=("log", 25, 70), far_size=(2.0, 3.0), sub_level=(0.25, 0.55),
        dly_mix=(0.12, 0.3), dly_feedback=(0.45, 0.75), scale=["JI Minor", "12-TET", "JI 7-limit", "Pythagorean"]),
      M(sync=0.45, pulse=0.45, cosmos=0.45, memory=0.3, delay2=0.5, farmode=0.4, quantize=0.2, zplane=0.35, filtermodel=0.4),
      words=(F("Orbit Parsec Quasar Starfield Helix Zenith Corona Ion Pulsar Nadir Andromeda Vela Carina Lyra "
               "Cepheid Hyades Altair Deneb"),
             COMMON_SECOND + F("Pulse Dock Relay Array Band Belt Transit Beacon")),
      material=dict(kin=K("Phelios .6"),
                    cats=W("analog-pad 1 modular-drone .8 digital-bell .6 electric-piano .5 brass .5 radio-space 1 seismic .5 sub-drone .5"),
                    worlds=W("cosmic 1 dark .4"), gestures=W("sustain .5 chord .4"), fr=0.3),
      types=T("Harmonic 1 Wavetable 1 Texture 1 Additive .6 Spectral .6 FM .3",
              "Stretch 1 Texture .8 Wavetable .6 Noise .3"),
      wavetables=W("harmonic:pad 2 classic:akwf 1.5 classic:wavedit 1 ambient:otmorph 1 harmonic:shimmer 1"),
      rooms=W("vast 2 hall 1.5 bloom 1.5 far 1")),

    A("Planetary", "Michael Stearns",
      P(reg(36, 46, 66, 84), pace((30, 120), (60, 200), (200, 500)), env((10, 40), (20, 60)),
        brightness=(0.5, 0.85), far_decay=("log", 30, 80), far_size=(2.4, 3.0), far_highcut=("log", 3000, 9000),
        shimmer=(0.3, 0.7), sub_level=(0.2, 0.45), width=(1.1, 1.6), spread=(0.7, 1.0),
        scale=["Harmonic 8-16", "JI Major (Ptolemy)", "Otonality 1-11", "JI 7-limit"]),
      M(stack=0.6, cosmos=0.55, zplane=0.35, memory=0.2, envelop=0.5, elev=0.4, farmode=0.4, rotate=0.5, cosmosswell=0.5),
      words=(F("Orbital Solstice Meridian Equinox Celestial Axis Aeon Gaia Zodiac Eclipse Corona Prism Harmonic "
               "Heliacal Precession Azimuth Firmament Stratos"),
             COMMON_SECOND + F("Rise Arc Circle Sphere Ocean Gate Light Beam")),
      material=dict(kin=K("Robert Rich .3 | Klaus Wiese .3"),
                    cats=W("singing-bowl .8 drone-string 1 overtone-voice .8 overtone-horn .8 just-chord .8 analog-pad .6 flute .5 long-string .6"),
                    worlds=W("cosmic 1 warm .4 ritual .3"), gestures=W("sustain .6 swell .5"), fr=0.2),
      types=T("Harmonic 1.1 Additive 1 Bow .8 Texture 1 Spectral .8 Wavetable .4",
              "Harmonic .7 Stretch .8 Texture .7 Bow .5 Spectral .6"),
      wavetables=W("harmonic:stack 2 ambient:overtone 2.5 harmonic:strings 1.5 ambient:bowed 1.5 harmonic:shimmer 1 ambient:consonant 1"),
      rooms=W("vast 3 cathedral 2 bloom 1.5 swell .8")),

    A("Gentle Systems", "Brian Eno",
      P(reg(40, 50, 64, 80), pace((6, 30), (8, 30), (30, 90)), env((1.5, 8), (6, 20)),
        master_gain=(-16.0, -10.0), brain_density=("int", 2, 4), brightness=(0.45, 0.8), far_decay=("log", 12, 35),
        dly_mix=(0.1, 0.28), brain_consonance=(0.7, 0.95), depth=(0.4, 0.8), sub_level=(0.0, 0.2),
        scale=["JI Pentatonic", "JI Major (Ptolemy)", "12-TET", "JI 7-limit"]),
      M(keys=0.25, dejavu=0.6, memory=0.5, delay2=0.5, strike=0.25, room=0.6, blend=0.4, sub=0.25, cloud=0.15,
        cascade=0.15, evensmooth=0.5),
      words=(F("Gentle Quiet Lantern Pale Drifting Soft Linen Window Evening Porcelain Lucid Still Mild Warm "
               "Paper Chalkline Weather Tidal"),
             COMMON_SECOND + F("System Loop Hour Piece Pattern Room Garden Tape")),
      material=dict(kin=K("Loscil .3 | Andrew Chalk .2 | Mirror .2"),
                    cats=W("mallet 1 reed-organ .8 piano 1 electric-piano .8 choir .6 solo-voice .5 zither-harp .8 analog-pad .6 flute .4 tine .6"),
                    worlds=W("luminous 1 warm .4"), gestures=W("melodic .8 struck .4 chord .3"), fr=0.15),
      types=T("Texture 1.3 Harmonic .9 Additive .9 FM .4 Wavetable .4 Spectral .5",
              "Texture .8 Harmonic .6 Stretch .5 Additive .6"),
      wavetables=W("harmonic:organ 1.5 harmonic:reed 1.2 harmonic:pluck 1.5 ambient:consonant 1.5 harmonic:choir 1 classic:akwf .8 builtin:Organ .5"),
      rooms=W("hall 3 chamber 1.5 bloom 1 plate .8")),

    A("Vast Chord", "Mathias Grassow",
      P(reg(28, 38, 52, 66), pace((60, 200), (120, 400), (300, 900)), env((20, 60), (30, 90)),
        brain_density=("int", 5, 9), purity=(0.95, 1.0), sub_source=["Difference", "Lowest"], sub_level=(0.3, 0.6),
        brightness=(0.25, 0.55), far_decay=("log", 40, 90),
        scale=["Subharmonic 16-8", "JI Minor", "JI 7-limit", "Harmonic 8-16", "Otonality 1-11"]),
      M(stack=0.9, sub=0.9, cosmos=0.3, cloud=0.2, memory=0.15, adaptive=0.5, zplane=0.2, farfreeze=0.12, envelop=0.4),
      words=(F("Mantra Sanctum Dhyana Prana Lotus Stupa Summit Tamboura Resin Incense Radiance Ashram Pilgrim "
               "Dharma Bodhi Samadhi Kailash Anahata"),
             COMMON_SECOND + F("Chord Wall Vow Breath Chant Pillar Offering Dawn")),
      material=dict(kin=K("Klaus Wiese .6 | Michael Stearns .2"),
                    cats=W("singing-bowl 1 overtone-voice 1 drone-string 1 chant .8 choir .6 drone-ensemble .8 just-chord .6 crystal-bowl .6"),
                    worlds=W("ritual .8 warm .4 dark .2"), gestures=W("sustain 1 swell .5"),
                    avoid=KEYS + ["guitar", "digital-bell"], fr=0.1),
      types=T("Harmonic 1.2 Additive 1.2 Spectral 1 Texture .9 Stretch .5 Bow .3",
              "Stretch .9 Spectral .8 Harmonic .7 Texture .6"),
      wavetables=W("ambient:overtone 3 ambient:vowel_bass 1.5 ambient:vowel_tenor 1 harmonic:choir 1.5 harmonic:stack 1.5 ambient:consonant 1.5"),
      rooms=W("cathedral 3 vast 2.5 bloom 1")),

    A("Ritual Stone", "Raison d'Être",
      P(reg(30, 40, 54, 70), pace((30, 120), (60, 200), (200, 500)), env((10, 40), (20, 60)),
        brightness=(0.25, 0.55), far_decay=("log", 30, 80), far_size=(2.2, 3.0), sub_level=(0.25, 0.55),
        near_mix=(0.1, 0.25), scale=["JI Minor", "Pythagorean", "12-TET", "Subharmonic 16-8"]),
      M(strike=0.5, room=0.85, roommorph=0.4, memory=0.35, cosmos=0.4, zplane=0.5, cascade=0.3, brain2=0.35,
        banks=0.5, patina=0.3, bodychar=0.6, elev=0.4),
      words=(F("Crypt Reliquary Cloister Abbey Vesper Candle Nave Censer Litany Relic Shroud Penance Lament "
               "Transept Chantry Apse Sepulchre Tallow"),
             COMMON_SECOND + F("Stone Hymn Prayer Choir Rite Bell Procession Crossing")),
      material=dict(kin=K("Lustmord .2"),
                    cats=W("chant 1.2 choir 1 bell 1 gong .6 pipe-organ .8 bowed-low-string .8 brass .5 cave .8 cave-wind .5 ritual-objects .5"),
                    worlds=W("ritual 1 dark .8"), gestures=W("sustain .6 swell .4 struck .3"),
                    avoid=["electric-piano", "digital-bell", "tine", "zither-harp", "steel-guitar"], fr=0.3),
      types=T("Spectral 1.2 Texture 1.1 Stretch .8 Harmonic .8 Additive .5 Bow .5",
              "Stretch 1.1 Texture .8 Spectral .7 Harmonic .5"),
      wavetables=W("harmonic:choir 2 harmonic:organ 1.5 ambient:vowel_bass 1.5 ambient:sampled 1.2 harmonic:metal 1 builtin:Vocal .4"),
      rooms=W("cathedral 3 vast 1.5 cavern 1.5 swell .6"),
      strikes=["Metal", "Metal", "String"], zshapes=["Church Bell", "Tubular Bell", "Gong", "Tam Tam", "Organ Pipe"]),

    A("Frozen Gong", "Thomas Köner",
      P(reg(24, 32, 42, 58), pace((60, 200), (90, 300), (240, 600)), env((15, 50), (30, 90)),
        master_gain=(-15.0, -10.0), brightness=(0.05, 0.3), tilt=(1.8, 3.0), partials=("int", 4, 10),
        cutoff=("log", 180, 800), far_highcut=("log", 700, 1600), far_decay=("log", 30, 70), shimmer=(0.0, 0.2),
        shimmer_rate=("log", 0.01, 0.08), brain_density=("int", 2, 4), sub_level=(0.3, 0.6), air=(0.0, 0.1),
        ens_mix=(0.0, 0.2), drift=(0.5, 3.0), detune=("log", 2.0, 8.0), depth=(0.85, 1.0)),
      M(sub=0.95, cloud=0.35, zplane=0.25, cosmos=0.25, memory=0.2, farfreeze=0.15, blur=0.35),
      words=(F("Glacial Frozen Icefall Crevasse Serac Firn Floe Whiteout Rime Hoarfrost Katabatic Polar Gong "
               "Iceberg Sastrugi Blizzard Nightfall Sealskin"),
             COMMON_SECOND + F("Plain Silence Night Shelf Sheet Well Depth Hold")),
      material=dict(kin=K("Biosphere .2 | Sleep Research Facility .2 | BJNilsen .2"),
                    cats=W("gong 1.5 bell .6 ice 1 water-tone 1 cave-wind .8 pure-tone .6 singing-bowl .5 ice-snow 1 wind .6 underwater .8"),
                    worlds=W("cold 1 dark .8"), gestures=W("sustain .8 struck .4"),
                    avoid=KEYS + ["guitar", "digital-bell", "choir", "solo-voice", "steel-guitar"], fr=0.45),
      types=T("Spectral 1.5 Stretch 1.3 Texture .5 Harmonic .4 Additive .4",
              "Stretch 1.3 Spectral 1 Noise .5 Texture .3"),
      wavetables=W("harmonic:metal 1.5 ambient:tube 1.5 harmonic:sub 1.5 harmonic:pure 1"),
      rooms=W("cavern 3 vast 2 far 1.5 drift 1"),
      granular={"spread": (0.2, 0.8), "grains": 1.4}, noise=["Brown", "Wind", "Pink"]),

    A("Harbour Rain", "Loscil",
      P(reg(38, 48, 60, 76), pace((10, 45), (20, 80), (80, 220)), env((2, 10), (8, 25)),
        master_gain=(-15.0, -10.0), brightness=(0.35, 0.65), cutoff=("log", 800, 3500), brain_density=("int", 2, 5),
        dly_feedback=(0.4, 0.75), dly_mix=(0.15, 0.35), dly_damp=(0.5, 0.85), sub_level=(0.15, 0.4),
        far_decay=("log", 15, 40), scale=["JI Minor", "JI Pentatonic", "12-TET", "JI Major (Ptolemy)"]),
      M(sync=0.6, pulse=0.5, delay2=0.55, memory=0.35, absorb=0.6, keys=0.15, strike=0.2, cloud=0.2, farmode=0.3),
      words=(F("Harbour Tide Squall Drizzle Estuary Buoy Kelp Undertow Pier Dockside Mist Rainline Channel "
               "Sounding Breakwater Slipway Ferry Downpour"),
             COMMON_SECOND + F("Pulse Current Dub Bay Rain Swell Hull Wake")),
      material=dict(kin=K("Biosphere .3 | Brian Eno .2"),
                    cats=W("electric-piano 1.2 mallet 1 tine .8 piano .8 string-ensemble .6 low-wind .6 rain-roof 1.2 rain-land .6 sea .5 underwater .5 digital-bell .6"),
                    worlds=W("luminous .6 cold .6 tape .3"), gestures=W("melodic .6 sustain .4 struck .4"), fr=0.4),
      types=T("Texture 1.3 Harmonic .8 FM .5 Wavetable .5 Spectral .5 Additive .5",
              "Stretch 1.1 Texture .8 Noise .5 Harmonic .5"),
      wavetables=W("harmonic:pluck 2 harmonic:pad 1 classic:akwf 1.2 ambient:consonant 1 harmonic:glass .8"),
      rooms=W("hall 2 chamber 1.5 plate 1 bloom 1"), noise=["Crackle", "Pink", "Band", "Brown"]),

    A("Hull Rumble", "Sleep Research Facility",
      P(reg(22, 32, 40, 56), pace((80, 300), (200, 600), (400, 1200)), env((20, 60), (30, 90)),
        master_gain=(-15.0, -9.0), brightness=(0.08, 0.3), tilt=(2.0, 3.0), cutoff=("log", 150, 700),
        brain_density=("int", 2, 4), sub_level=(0.4, 0.75), sub_octave=["-1", "-2"], far_decay=("log", 30, 80),
        air=(0.0, 0.08)),
      M(sub=1.0, noiseprimary=0.45, zplane=0.35, cloud=0.25, cosmos=0.2, farfreeze=0.2, memory=0.1),
      words=(F("Deck Hull Bulkhead Reactor Cryo Airlock Hangar Ballast Coolant Freighter Keel Hatch Turbine "
               "Vent Beacon Engineroom Stowage Gantry"),
             COMMON_SECOND + F("Rumble Hum Level Section Watch Hold Crew Bay")),
      material=dict(kin=K("Hazard .3 | Tho-So-Aa .3 | Ulf Söderberg .2"),
                    cats=W("engine-hull 1.5 sub-drone 1.2 electrical-hum 1 low-wind .8 factory 1 tunnel 1 ship-port 1 dark-drone-noise 1 ventilation 1 ice-snow .4"),
                    worlds=W("dark 1 industrial .8"), gestures=W("sustain 1"),
                    avoid=KEYS + ["flute", "guitar", "choir", "solo-voice", "zither-harp", "steel-guitar", "gamelan"], fr=0.6),
      types=T("Stretch 1.5 Spectral 1 Noise .8 Texture .5 Additive .4 Harmonic .3",
              "Stretch 1.4 Noise .8 Spectral .8"),
      wavetables=W("harmonic:sub 2 ambient:tube 1.5 harmonic:hollow .5"),
      rooms=W("far 2 vast 2 cavern 1.5 chamber 1"),
      zshapes=["Concrete Pipe", "Tunnel", "Closed Pipe", "Small Room"], noise=["Brown", "Brown", "Band", "Pink"]),

    A("Forest Rite", "Ulf Söderberg",
      P(reg(26, 36, 48, 62), pace((40, 150), (80, 240), (240, 600)), env((10, 40), (20, 60)),
        brightness=(0.15, 0.4), cutoff=("log", 250, 1200), sub_level=(0.3, 0.6), far_decay=("log", 25, 70),
        scale=["JI Minor", "Pythagorean", "Subharmonic 16-8", "12-TET"]),
      M(strike=0.35, cascade=0.3, sub=0.9, zplane=0.3, cosmos=0.3, memory=0.2, cloud=0.3, room=0.6),
      words=(F("Forest Pine Spruce Lichen Bog Taiga Birch Resin Cairn Rune Moor Fen Hearth Elk Marrow Root "
               "Deadfall Wolfden"),
             COMMON_SECOND + F("Tide Hollow Lodge Fire Winter Path Clearing Dusk")),
      material=dict(kin=K("Sleep Research Facility .2 | Voice of Eye .2 | Lustmord .2"),
                    cats=W("pipe-tank 1 sub-drone 1 cave-wind 1 engine-hull .6 tunnel .8 cave 1 abandoned .6 foliage .8 night-country .8 river .5 wind .5"),
                    worlds=W("dark .8 wild .5 ritual .5"), gestures=W("sustain .8 swell .4"), fr=0.55),
      types=T("Stretch 1.2 Spectral 1 Texture .8 Harmonic .6 Additive .5 Bow .3",
              "Stretch 1.2 Spectral .7 Noise .5 Texture .5"),
      wavetables=W("harmonic:sub 1.5 ambient:tube 1.5 ambient:sampled 1 harmonic:breath 1"),
      rooms=W("cavern 3 far 1.5 vast 1 drift 1"),
      strikes=["Wood", "Wood", "String"], zshapes=["Frame Drum", "Wood", "Cave", "Timpani"]),

    A("Weather Station", "Hazard",
      P(reg(24, 34, 44, 58), pace((60, 240), (120, 360), (300, 800)), env((10, 40), (20, 70)),
        master_gain=(-16.0, -10.0), brightness=(0.15, 0.45), brain_density=("int", 1, 3), sub_level=(0.25, 0.6),
        far_decay=("log", 20, 60), air=(0.05, 0.3)),
      M(noiseprimary=0.4, cloud=0.45, cloudplus=0.6, sub=0.8, zplane=0.3, memory=0.2),
      words=(F("Gale Squall Anemometer Barometer Storm Front Cyclone Blizzard Hail Static Pylon Transformer "
               "Mast Isotherm Downdraft Whirlwind Lightning Pressure"),
             COMMON_SECOND + F("Station Reading Record Log Weather Night Chart Report")),
      material=dict(kin=K("BJNilsen .6 | Chris Watson .3 | Thomas Köner .2"),
                    cats=W("electrical-hum 1.2 pipe-tank 1 sub-drone .8 engine-hull .8 pure-tone .6 electric 1.2 ventilation 1 factory .8 contact-structure 1 wind 1.2 ice-snow .8 polar-station .8"),
                    worlds=W("dark .6 industrial .6 cold .6"), gestures=W("sustain .8 swell .4"),
                    avoid=KEYS + ["flute", "choir", "solo-voice", "gamelan"], fr=0.65),
      types=T("Stretch 1.5 Spectral 1 Noise .6 Texture .5 Harmonic .3 Additive .3",
              "Stretch 1.4 Noise .7 Spectral .8"),
      wavetables=W("harmonic:sub 1 harmonic:hollow 1 ambient:tube 1"),
      rooms=W("far 2 vast 1.5 cavern 1.5 chamber .8"), noise=["Wind", "Wind", "Brown", "Band", "Crackle"]),

    A("Painted Field", "Andrew Chalk",
      P(reg(38, 48, 60, 76), pace((20, 80), (40, 140), (140, 360)), env((5, 25), (12, 40)),
        master_gain=(-15.0, -10.0), brightness=(0.35, 0.65), ens_mix=(0.3, 0.7), far_decay=("log", 18, 50),
        brain_density=("int", 3, 6), scale=["JI Major (Ptolemy)", "JI Pentatonic", "12-TET", "JI Minor"]),
      M(patina=0.6, blur=0.5, memory=0.45, cloud=0.2, keys=0.1, delay2=0.3, microshift=0.4, fardiffuse=0.4),
      words=(F("Painted Meadow Watercolour Ochre Pastel Linen Rushes Orchard Hedgerow Willow Parchment Umber "
               "Lantern Sepia Gouache Vellum Buttercup Loosestrife"),
             COMMON_SECOND + F("Field Study Garden Evening Window Shore Air Fold")),
      material=dict(kin=K("Mirror .5 | ora .4 | Darren Tate .2"),
                    cats=W("piano 1 tape-loop 1 tape-keyboard 1 pipe-organ 1 reed-organ .8 guitar .8 string-ensemble .6 zither-harp .5"),
                    worlds=W("tape 1 luminous .6"), gestures=W("chord .6 sustain .5 swell .3"), fr=0.2),
      types=T("Texture 1.3 Harmonic 1 Spectral .7 Additive .7 Stretch .5 Wavetable .3",
              "Stretch .9 Texture .8 Harmonic .6 Noise .3"),
      wavetables=W("harmonic:organ 2 harmonic:reed 1.5 ambient:sampled 1.2 harmonic:pad 1 ambient:otmorph 1"),
      rooms=W("hall 2 bloom 1.5 chamber 1 drift 1")),

    A("Millstone", "Jonathan Coleclough",
      P(reg(36, 46, 58, 74), pace((40, 150), (80, 240), (240, 600)), env((10, 40), (20, 60)),
        brightness=(0.3, 0.6), brain_density=("int", 2, 5), far_decay=("log", 20, 60), sub_level=(0.15, 0.45),
        scale=["JI 7-limit", "JI Major (Ptolemy)", "Harmonic 8-16", "Pythagorean"]),
      M(cloud=0.5, cloudplus=0.7, zplane=0.45, strike=0.3, room=0.5, sub=0.6, memory=0.25, bodychar=0.6),
      words=(F("Millstone Pin Glasspane Bellwether Kettle Sparkler Hillside Pewter Brass Wick Pail Tin Copper "
               "Pebble Thimble Hayloft Sheepbell Icecube"),
             COMMON_SECOND + F("Turn Weight Ring Boil Drop Dust Grain Melt")),
      material=dict(kin=K("ora .4 | Andrew Chalk .2 | Colin Potter .2"),
                    cats=W("bell 1.2 glass 1.2 bowed-metal 1 blown-vessel 1 pipe-organ .6 water-tone .8 grain-texture 1 empty-space .6 rain-roof .6 contact-structure .6"),
                    worlds=W("tape .6 ritual .3"), gestures=W("sustain .6 struck .5"), fr=0.35),
      types=T("Spectral 1.3 Texture 1 Stretch .8 Harmonic .6 Additive .4",
              "Stretch 1 Spectral .8 Texture .7"),
      wavetables=W("harmonic:glass 1.5 harmonic:metal 1.5 harmonic:resonator 1.5 ambient:sampled 1"),
      rooms=W("plate 1.5 chamber 1.5 hall 1 drift 1"),
      strikes=["Metal", "Metal", "Wood"], zshapes=["Glass", "Bottle", "Metal Bars", "Glockenspiel", "Steel Plate"]),

    A("Glass Vitrine", "Mirror",
      P(reg(38, 48, 62, 78), pace((40, 150), (80, 260), (260, 600)), env((10, 40), (20, 60)),
        brightness=(0.45, 0.8), inharmonic=(0.05, 0.25), far_decay=("log", 20, 60), brain_density=("int", 2, 4),
        ens_mix=(0.2, 0.5), scale=["JI Major (Ptolemy)", "JI 7-limit", "12-TET", "JI Minor"]),
      M(cosmos=0.35, zplane=0.35, memory=0.3, patina=0.35, blur=0.35, cloud=0.3, microshift=0.4),
      words=(F("Vitrine Mirrored Glasshouse Pane Silvered Lacquer Frost Harmonium Hush Pale Cabinet Lens Faded "
               "Opal Pearl Quartz Candlewax Glaze"),
             COMMON_SECOND + F("Room Hours Tone Veil Water Glass Breath Case")),
      material=dict(kin=K("Andrew Chalk .4 | ora .3"),
                    cats=W("reed-organ 1.2 glass 1.2 pipe-organ 1 tape-keyboard .8 mallet .5 solo-voice .5 zither-harp .5 rain-land .5 drone-ensemble .6"),
                    worlds=W("tape .8 luminous .6 dark .3"), gestures=W("sustain .8 chord .4"), fr=0.2),
      types=T("Harmonic 1.2 Texture 1.1 Spectral .8 Additive .6 Wavetable .4",
              "Stretch .8 Texture .7 Harmonic .7 Spectral .6"),
      wavetables=W("harmonic:organ 2 harmonic:reed 2 harmonic:glass 2 ambient:consonant 1"),
      rooms=W("hall 2 bloom 1.5 cathedral 1 drift .8")),

    A("Slow Carousel", "Mimir",
      P(reg(38, 48, 60, 76), pace((10, 50), (20, 80), (80, 240)), env((3, 15), (10, 35)),
        fb_tape=(0.3, 0.8), brain_density=("int", 3, 6), dly_mix=(0.15, 0.35),
        scale=["12-TET", "JI Minor", "JI Major (Ptolemy)", "Pythagorean"]),
      M(patina=0.65, memory=0.55, portamento=0.4, feedback=0.45, delay2=0.4, cloud=0.35, dejavu=0.5, strike=0.2, keys=0.1),
      words=(F("Carousel Warp Wobble Reel Spool Gramophone Flicker Tinsel Circus Lantern Gyre Zoetrope Parlour "
               "Arcade Merry Pinwheel Calliope Sideshow"),
             COMMON_SECOND + F("Loop Spin Dream Organ Waltz Float Circle Ride")),
      material=dict(kin=K("Colin Potter .3 | Andrew Chalk .2 | Bass Communion .2"),
                    cats=W("tape-keyboard 1.2 piano 1 tape-loop 1.2 string-ensemble .8 guitar .8 tine .6 grain-texture .8 city .6 crackle-media .8"),
                    worlds=W("tape 1"), gestures=W("chord .8 melodic .5"), fr=0.3),
      types=T("Texture 1.3 Harmonic .8 Wavetable .6 Additive .6 Spectral .5 FM .4",
              "Texture .9 Stretch .9 Noise .5 Harmonic .5"),
      wavetables=W("harmonic:morph 1.5 classic:wavedit 1.5 harmonic:pluck 1 ambient:otmorph 1"),
      rooms=W("plate 1.5 chamber 1.5 hall 1 swell 1"), noise=["Crackle", "Pink", "Brown"]),

    A("Chamber Grey", "In Camera",
      P(reg(36, 46, 56, 72), pace((30, 120), (60, 180), (180, 420)), env((4, 18), (10, 30)),
        master_gain=(-16.0, -11.0), near_mix=(0.2, 0.45), far_level=(0.3, 0.6), far_decay=("log", 8, 25),
        brightness=(0.25, 0.5), brain_density=("int", 2, 4), dly_mix=(0.08, 0.2),
        scale=["JI Minor", "12-TET", "Pythagorean"]),
      M(room=0.6, keys=0.3, patina=0.4, earlyroom=0.6, nearfield=0.5, presence=0.4, memory=0.2, microshift=0.3),
      words=(F("Grey Chamber Parlour Closet Curtain Ashen Dim Muted Cellar Attic Lamp Dust Wallpaper Keyhole "
               "Hushed Drawer Mantel Wainscot"),
             COMMON_SECOND + F("Room Corner Interior Tone Quiet Draft Afternoon Frame")),
      material=dict(kin=K("Andrew Chalk .2 | Colin Potter .2"),
                    cats=W("low-wind 1 tape-loop 1 piano 1 reed-organ .8 blown-vessel .8 bowed-low-string .8 radio .6 electric-piano .6 tunnel .6 empty-space .6"),
                    worlds=W("tape 1 cold .5"), gestures=W("sustain .5 chord .5"), fr=0.3),
      types=T("Texture 1.2 Harmonic .8 Additive .7 Spectral .6 Wavetable .4",
              "Stretch .9 Texture .8 Noise .4 Harmonic .5"),
      wavetables=W("harmonic:reed 1.5 harmonic:breath 1.5 harmonic:pad 1 classic:akwf 1"),
      rooms=W("chamber 3 plate 1 hall 1")),

    A("Loop Studio", "Colin Potter",
      P(reg(34, 44, 56, 72), pace((10, 60), (20, 90), (90, 260)), env((3, 15), (10, 35)),
        fb_tape=(0.3, 0.8), dly_time_l=("log", 0.8, 4.0), dly_time_r=("log", 1.0, 5.0), dly_feedback=(0.55, 0.85),
        dly_mix=(0.2, 0.4), brain_density=("int", 3, 6), brightness=(0.3, 0.6),
        scale=["12-TET", "JI Minor", "Pythagorean", "JI 7-limit"]),
      M(delay2=0.7, absorb=0.7, memory=0.6, feedback=0.45, cloud=0.35, patina=0.4, zplane=0.4, quantize=0.25, dejavu=0.4),
      words=(F("Reel Tapehead Splice Varispeed Steam Signal Static Relay Capstan Hiss Oscillator Patchbay Bias "
               "Echo Crossfade Headstock Leader Flutter"),
             COMMON_SECOND + F("Loop Studio Room Fog Session Machine Delay Take")),
      material=dict(kin=K("Monos .4 | Darren Tate .2 | Mimir .2"),
                    cats=W("bowed-metal 1 guitar .8 grain-texture 1 tunnel 1 tape-loop 1 radio 1 singing-wire .8 feedback-drone 1 pipe-tank .6"),
                    worlds=W("tape .8 industrial .8"), gestures=W("sustain .6"), fr=0.4),
      types=T("Texture 1.1 Stretch .9 FM .7 Wavetable .7 Spectral .6 Harmonic .5",
              "Stretch 1 Texture .8 Noise .5 FM .4"),
      wavetables=W("classic:wavedit 2 classic:akwf 1.5 harmonic:morph 1 harmonic:metal 1"),
      rooms=W("plate 2 chamber 1.5 far 1 drift 1")),

    A("Still Meadow", "Darren Tate",
      P(reg(36, 46, 56, 72), pace((60, 240), (120, 360), (300, 800)), env((10, 40), (20, 60)),
        master_gain=(-17.0, -11.0), brain_density=("int", 1, 3), brightness=(0.3, 0.6), purity=(0.85, 1.0),
        purity_drift=(0.1, 0.4), far_decay=("log", 15, 45), air=(0.1, 0.35),
        scale=["JI 7-limit", "JI Pentatonic", "Otonality 1-11", "12-TET"]),
      M(nearfield=0.4, patina=0.3, memory=0.2, cloud=0.25, vector=0.35),
      words=(F("Meadow Hedge Dew Nettle Barley Fallow Stile Dusk Heron Hazel Moss Brook Thistle Wold Loam "
               "Cowslip Rookery Bramble"),
             COMMON_SECOND + F("Stillness Air Pasture Hour Path Weather Bank Mist")),
      material=dict(kin=K("ora .5 | Andrew Chalk .2 | Paul Bradley .2"),
                    cats=W("bell .8 grain-texture 1 blown-vessel 1 night-country 1.2 guitar .6 rain-land 1 wind 1 foliage 1 bowed-folk .6"),
                    worlds=W("tape .8 wild .8"), gestures=W("sustain .8"), fr=0.6),
      types=T("Stretch 1.3 Texture 1 Spectral 1 Harmonic .5 Additive .5",
              "Stretch 1.3 Texture .8 Spectral .7"),
      wavetables=W("harmonic:breath 1.5 ambient:tube 1 ambient:sampled 1 harmonic:pure 1"),
      rooms=W("chamber 1 hall 1 far 1.5 bloom 1")),

    A("Water Hymn", "ora",
      P(reg(34, 44, 56, 72), pace((60, 200), (120, 360), (300, 800)), env((15, 45), (25, 70)),
        brightness=(0.3, 0.6), brain_density=("int", 2, 4), far_decay=("log", 25, 60), sub_level=(0.1, 0.35),
        scale=["JI Minor", "JI 7-limit", "Pythagorean", "JI Pentatonic"]),
      M(patina=0.35, memory=0.3, cloud=0.3, zplane=0.3, room=0.45, blur=0.3),
      words=(F("Hymn Waterside Weir Sluice Eddy Rill Culvert Tarn Mere Pool Spring Ebb Wharf Silt Lock "
               "Millrace Backwater Spillway"),
             COMMON_SECOND + F("Hymn Scrape Stone Bowing Dusk Drone Bank Water")),
      material=dict(kin=K("Andrew Chalk .4 | Darren Tate .4 | Jonathan Coleclough .3"),
                    cats=W("pipe-organ 1 guitar .6 glass .8 blown-vessel 1 reed-organ .8 drone-ensemble .8 long-string .8 bowed-metal 1 river 1 rain-land .6 wetland .6 flute .4"),
                    worlds=W("tape .8 dark .4"), gestures=W("sustain .8 swell .4"), fr=0.4),
      types=T("Texture 1 Spectral 1 Stretch .9 Harmonic .8 Additive .4 Bow .4",
              "Stretch 1.1 Spectral .8 Texture .7"),
      wavetables=W("harmonic:organ 1.5 harmonic:metal 1 ambient:sampled 1.2 harmonic:breath 1"),
      rooms=W("hall 1.5 cathedral 1 drift 1.2 bloom 1")),

    A("Vegetal Drone", "Monos",
      P(reg(32, 42, 54, 70), pace((40, 160), (80, 260), (260, 700)), env((10, 40), (20, 60)),
        purity=(0.8, 0.97), purity_drift=(0.2, 0.6), strands=("int", 3, 6), detune=("log", 4.0, 16.0),
        brightness=(0.3, 0.55), brain_density=("int", 3, 6), far_decay=("log", 20, 60),
        scale=["JI 7-limit", "Otonality 1-11", "Subharmonic 16-8", "JI Minor"]),
      M(coherence=0.4, sympathy=0.35, patina=0.35, feedback=0.3, memory=0.25, room=0.5),
      words=(F("Vegetal Root Tendril Sap Mycelium Humus Frond Spore Rhizome Bark Leafmould Pith Seedpod Lichen "
               "Tuber Sporangium Burdock Mulch"),
             COMMON_SECOND + F("Drone Growth Morning Chord Bed Hymn Ground Tangle")),
      material=dict(kin=K("Darren Tate .4 | Colin Potter .4"),
                    cats=W("pipe-organ 1 guitar .8 drone-ensemble 1 bowed-metal .8 long-string 1 feedback-drone .8 foliage .6 night-country .6"),
                    worlds=W("tape .8 dark .5 ritual .3"), gestures=W("sustain 1"), fr=0.3),
      types=T("Additive 1 Harmonic 1 Texture 1 Spectral .8 Stretch .6 Bow .4",
              "Stretch .9 Texture .7 Harmonic .7 Additive .6"),
      wavetables=W("harmonic:organ 1.5 harmonic:strings 1.5 ambient:bowed 1.5 ambient:sampled 1"),
      rooms=W("chamber 1.5 hall 1.5 drift 1 far 1")),

    A("Sustain", "Paul Bradley",
      P(reg(30, 40, 50, 64), pace((120, 400), (300, 900), (600, 1800)), env((20, 60), (40, 120)),
        brain_density=("int", 1, 2), strands=("int", 1, 3), detune=("log", 1.0, 5.0), drift=(0.3, 2.0),
        purity=(0.97, 1.0), brightness=(0.25, 0.5), far_decay=("log", 40, 100), far_size=(2.5, 3.0),
        sub_level=(0.15, 0.4), scale=["JI Minor", "Pythagorean", "JI 7-limit", "Subharmonic 16-8"]),
      M(farfreeze=0.2, sub=0.6, cosmos=0.15, memory=0.1, zplane=0.15, stack=0.1, src2=0.5, src3=0.2, src4=0.05),
      words=(F("Held Long Sustained Unbroken Level Single Even Constant Standing Pedal Steady Plain Lasting "
               "Solitary Patient Unmoving Endless Stilled"),
             COMMON_SECOND + F("Tone Note Line Hour Hum Pitch Horizon Glacier")),
      material=dict(kin=K("Darren Tate .3 | Moljebka Pvlse .2"),
                    cats=W("pipe-organ 1 modular-drone 1 singing-wire 1 bowed-low-string 1 pure-tone 1 long-string .8 empty-space 1 ice-snow .8"),
                    worlds=W("cold .8 tape .5 dark .5"), gestures=W("sustain 1.2"), fr=0.3),
      types=T("Additive 1 Harmonic 1 Spectral 1 Texture .8 Stretch .8 Bow .5",
              "Stretch 1 Spectral .8 Harmonic .5"),
      wavetables=W("harmonic:pure 2 harmonic:organ 1.5 ambient:bowed 1.5 ambient:tube 1"),
      rooms=W("vast 2 cathedral 1.5 far 1 cavern 1"), sparse=True),

    A("Street Resonance", "BJNilsen",
      P(reg(30, 40, 50, 64), pace((60, 240), (120, 360), (300, 800)), env((10, 40), (20, 60)),
        master_gain=(-16.0, -10.0), brain_density=("int", 1, 3), brightness=(0.25, 0.55), sub_level=(0.2, 0.5),
        far_decay=("log", 15, 45), air=(0.05, 0.25)),
      M(vector=0.5, noiseprimary=0.3, cloud=0.35, memory=0.2, earlyroom=0.4, sub=0.7, strike=0.15, src4=0.5),
      words=(F("Kerb Underpass Viaduct Siding Platform Girder Crane Pylon Scaffold Embankment Towpath Terminus "
               "Gantry Hoarding Bollard Concourse Turnstile Cobble"),
             COMMON_SECOND + F("Resonance Frequency Survey Walk Night Current Traffic Hour")),
      material=dict(kin=K("Hazard .5 | Chris Watson .4 | Francisco López .2"),
                    cats=W("ice .8 electrical-hum 1 radio .8 bell .8 pure-tone .6 singing-wire 1 contact-structure 1.2 city 1.2 transport 1 sea .8 wind .8 electric .8"),
                    worlds=W("cold .8 industrial .6 wild .4"), gestures=W("sustain .8 swell .3"), fr=0.7),
      types=T("Stretch 1.5 Spectral 1 Texture .6 Noise .3 Harmonic .3 Additive .3",
              "Stretch 1.5 Spectral .8 Noise .5"),
      wavetables=W("harmonic:hollow 1 harmonic:metal 1 harmonic:pure 1"),
      rooms=W("far 2 hall 1 vast 1 chamber 1")),

    A("Bowl Temple", "Klaus Wiese",
      P(reg(36, 46, 58, 74), pace((60, 200), (120, 360), (300, 900)), env((15, 45), (30, 90)),
        brain_density=("int", 3, 6), purity=(0.95, 1.0), brightness=(0.35, 0.7), far_decay=("log", 30, 80),
        sub_level=(0.15, 0.4), scale=["JI Major (Ptolemy)", "Harmonic 8-16", "JI 7-limit", "Otonality 1-11", "JI Pentatonic"]),
      M(cloud=0.55, cloudplus=0.8, strike=0.35, zplane=0.45, cosmos=0.3, memory=0.2, sub=0.6, stack=0.4),
      words=(F("Sufi Dervish Bowl Tanpura Santur Saffron Rose Minaret Caravan Lotus Amber Mandala Zenith Oud "
               "Rosewater Pomegranate Cypress Turquoise"),
             COMMON_SECOND + F("Temple Bowl Circle Dance Night Garden Breath Mystic")),
      material=dict(kin=K("Mathias Grassow .6 | Oöphoi .3"),
                    cats=W("singing-bowl 1.5 crystal-bowl 1 flute .8 overtone-voice .8 bowed-folk 1 drone-string 1 reed-organ .5 drone-ensemble .6 gong .5 ritual-objects .6"),
                    worlds=W("ritual 1 warm .6"), gestures=W("sustain .8 struck .4 swell .4"),
                    avoid=["piano", "electric-piano"] + INDUSTRIAL, fr=0.15),
      types=T("Spectral 1.2 Texture 1.2 Harmonic 1 Additive .8 Stretch .5",
              "Stretch .9 Spectral .8 Harmonic .6 Texture .6"),
      wavetables=W("ambient:overtone 2 harmonic:glass 1.5 harmonic:resonator 1.5 ambient:sampled 1.2 ambient:consonant 1"),
      rooms=W("cathedral 2 hall 1.5 vast 1.5 bloom 1"),
      strikes=["Metal", "Metal", "String"], zshapes=["Tubular Bell", "Church Bell", "Gong", "Glass", "Handpan"]),

    A("Temple of Air", "Oöphoi",
      P(reg(36, 46, 58, 74), pace((80, 300), (200, 600), (500, 1500)), env((30, 60), (40, 120)),
        partials=("int", 4, 10), tilt=(1.4, 2.4), brightness=(0.3, 0.6), brain_density=("int", 2, 5),
        purity=(0.95, 1.0), far_decay=("log", 40, 100), air=(0.15, 0.4),
        scale=["JI Major (Ptolemy)", "JI 7-limit", "Harmonic 8-16", "Slendro (JI)"]),
      M(cloud=0.4, cloudplus=0.7, sub=0.5, cosmos=0.3, memory=0.2, farfreeze=0.12, phase=0.6),
      words=(F("Hermetic Ancient Temple Oracle Chime Shell Stone Mist Delphi Aether Lagoon Sibyl Grotto "
               "Sanctuary Cedar Amphora Labyrinth Myrrh"),
             COMMON_SECOND + F("Air Memory Dream Circle Echo Gate Hours Dusk")),
      material=dict(kin=K("Klaus Wiese .3 | Voice of Eye .3 | Robert Rich .2"),
                    cats=W("rainforest 1.2 insects 1 ritual-objects 1 fire .6 river 1 singing-bowl .8 flute .8 crystal-bowl .6 blown-vessel .6"),
                    worlds=W("warm .8 ritual .6 cosmic .5 wild .4"), gestures=W("sustain .8"), fr=0.5),
      types=T("Additive 1.2 Harmonic 1 Spectral .9 Texture .8 Stretch .8",
              "Stretch 1.2 Spectral .8 Texture .6 Harmonic .5"),
      wavetables=W("harmonic:pure 2 ambient:consonant 1.5 harmonic:breath 1.5 harmonic:glass 1"),
      rooms=W("vast 2 cathedral 1.5 bloom 1.5 cavern 1")),

    A("Star Rite", "Inade",
      P(reg(26, 36, 50, 66), pace((20, 90), (40, 160), (160, 420)), env((8, 30), (20, 60)),
        brightness=(0.2, 0.5), far_decay=("log", 30, 80), far_size=(2.2, 3.0), sub_level=(0.35, 0.7),
        brain_density=("int", 3, 6), inharmonic=(0.1, 0.35), scale=["JI Minor", "Subharmonic 16-8", "12-TET", "Pythagorean"]),
      M(cosmos=0.75, zplane=0.5, sub=0.9, pulse=0.5, stack=0.4, cascade=0.3, strike=0.3, memory=0.3, cloud=0.3,
        cosmosswell=0.6, rotate=0.5),
      words=(F("Stellar Obelisk Pyramid Sigil Zenith Monolith Arcanum Celestial Hierophant Occult Pleiades "
               "Sirius Cygnus Umbra Ziggurat Nebulae Astrolabe Orrery"),
             COMMON_SECOND + F("Rite Dimension Gate Crown Throne Wall Signal Order")),
      material=dict(kin=K("Lustmord .3 | Raison d'être .2 | Phelios .2"),
                    cats=W("cave 1.2 ritual-objects 1 tunnel .8 dark-drone-noise 1 seismic 1 gong .8 chant .6 brass .6 bowed-metal .6"),
                    worlds=W("dark .8 ritual .8 cosmic .6"), gestures=W("sustain .6 swell .6"), fr=0.5),
      types=T("Spectral 1.2 Stretch 1.1 Texture .8 Harmonic .7 Additive .5 FM .3",
              "Stretch 1.2 Spectral .8 Noise .5 Texture .5"),
      wavetables=W("harmonic:metal 2 ambient:vowel_bass 1.5 harmonic:sub 1.5 ambient:tube 1"),
      rooms=W("cathedral 2 vast 2.5 cavern 1.5 swell 1"),
      strikes=["Metal", "Metal", "Wood"], zshapes=["Gong", "Tam Tam", "Church Bell", "Steel Plate"]),

    A("Dream Depth", "Troum",
      P(reg(30, 40, 52, 68), pace((60, 200), (120, 360), (300, 800)), env((20, 60), (30, 90)),
        fb_bus=(0.05, 0.18), fb_drive=(0.3, 0.8), fb_tape=(0.2, 0.6), brightness=(0.25, 0.55),
        far_decay=("log", 30, 80), sub_level=(0.25, 0.55), brain_density=("int", 4, 8), ens_mix=(0.3, 0.7),
        scale=["JI Minor", "Pythagorean", "12-TET", "JI 7-limit"]),
      M(feedback=0.6, memory=0.4, cloud=0.45, blur=0.35, sub=0.8, cosmos=0.25, farfreeze=0.12, fold=0.2),
      words=(F("Trance Slumber Hypnagogia Somnolent Lethe Undertow Mirage Reverie Delirium Nocturne Threnody "
               "Twilight Accordion Balalaika Maelstrom Chimera Phantasm Oneiric"),
             COMMON_SECOND + F("Depth Wave Sea Mass Current Tide Gathering Night")),
      material=dict(kin=K("Moljebka Pvlse .4 | Colin Potter .2 | Voice of Eye .2"),
                    cats=W("grain-texture 1 abandoned 1 empty-space 1 murmur 1 metal-creak .8 fire .8 ritual-objects .8 reed-organ .8 guitar .6 gong .6 feedback-drone .8"),
                    worlds=W("ritual .8 tape .6 industrial .4"), gestures=W("sustain .8 swell .6"), fr=0.6),
      types=T("Stretch 1.4 Spectral 1 Texture .8 Harmonic .6 Additive .4",
              "Stretch 1.4 Spectral .8 Texture .6 Noise .4"),
      wavetables=W("harmonic:reed 2 ambient:sampled 1.5 harmonic:breath 1 ambient:tube 1"),
      rooms=W("cavern 2.5 far 1.5 vast 1.5 swell 1")),

    A("Long Transit", "S.E.T.I.",
      P(reg(30, 40, 52, 68), pace((80, 300), (200, 600), (500, 1500)), env((20, 60), (40, 100)),
        master_gain=(-16.0, -10.0), brain_density=("int", 2, 4), brightness=(0.25, 0.55), far_decay=("log", 35, 90),
        sub_level=(0.2, 0.5), scale=["JI Minor", "12-TET", "JI 7-limit", "Harmonic 8-16"]),
      M(noiseprimary=0.25, cosmos=0.35, zplane=0.35, memory=0.2, farfreeze=0.15, arcclock=0.2, binaural=0.25, sub=0.7),
      words=(F("Transit Wormhole Probe Telemetry Exoplanet Heliosphere Lagrange Ecliptic Perihelion Capsule "
               "Tether Payload Relay Module Hibernation Starboard Sextant Voyager"),
             COMMON_SECOND + F("Transit Sleep Cycle Window Orbit Silence Channel Log")),
      material=dict(kin=K("Arecibo .3 | Bad Sector .3 | Sleep Research Facility .2"),
                    cats=W("geothermal 1 radio-space 1.2 electric 1 seismic .8 ventilation 1 underwater .8 engine-hull .6 radio .8 modular-drone .6"),
                    worlds=W("cosmic .8 industrial .6"), gestures=W("sustain 1"), fr=0.6),
      types=T("Stretch 1.4 Spectral 1 Harmonic .7 Additive .6 Noise .5 Texture .5",
              "Stretch 1.3 Noise .7 Spectral .8"),
      wavetables=W("harmonic:pure 1.5 harmonic:sub 1 classic:wavedit 1 ambient:otmorph 1"),
      rooms=W("vast 3 far 1.5 cavern 1")),

    A("Signal Algorithm", "Bad Sector",
      P(reg(34, 44, 56, 72), pace((4, 20), (8, 40), (40, 140)), env((1, 8), (6, 25)),
        master_gain=(-15.0, -9.0), brain_density=("int", 3, 6), brightness=(0.3, 0.65), cutoff=("log", 400, 4000),
        resonance=(0.2, 0.6), far_decay=("log", 15, 45), dly_mix=(0.12, 0.3),
        scale=["12-TET", "Pythagorean", "JI Minor", "Bohlen-Pierce (JI)"]),
      M(quantize=0.6, cascade=0.5, surprise=0.4, zplane=0.55, filtermodel=0.5, noiseprimary=0.3, cloud=0.35,
        cloudplus=0.6, strike=0.25, sync=0.35, lenia=0.3, chaos=0.35),
      words=(F("Algorithm Microbe Cipher Vector Neuron Protein Lattice Quantum Photon Enzyme Parity Axon Helix "
               "Sputnik Bacillus Genome Integer Mitosis"),
             COMMON_SECOND + F("Signal Code Sequence Pulse Cell Wave Matrix Grid")),
      material=dict(kin=K("S.E.T.I. .3 | Arecibo .3 | Tho-So-Aa .2"),
                    cats=W("factory 1 dark-drone-noise 1 radio-space 1.2 ventilation .8 electric 1.2 radio 1 electrical-hum .8 modular-drone .8"),
                    worlds=W("dark .6 industrial .6 cosmic .6"), gestures=W("sustain .6"), fr=0.5),
      types=T("Wavetable 1 FM .9 Stretch 1 Spectral .9 Harmonic .6 Noise .5",
              "Stretch 1.1 Noise .7 FM .5 Spectral .6"),
      wavetables=W("classic:wavedit 2.5 classic:akwf 1.5 harmonic:morph 1"),
      rooms=W("plate 1.5 chamber 1 far 1.5 vast 1"), noise=["Digital", "Band", "Crackle", "Brown"],
      filters=["BP 12", "Peak", "Comb", "Ladder", "Formant"]),

    A("Cosmic Procession", "Phelios",
      P(reg(28, 38, 52, 68), pace((20, 90), (40, 160), (160, 420)), env((10, 40), (20, 60)),
        brightness=(0.25, 0.55), far_decay=("log", 30, 80), far_size=(2.2, 3.0), sub_level=(0.3, 0.6),
        brain_density=("int", 3, 6), scale=["JI Minor", "12-TET", "Pythagorean", "JI 7-limit"]),
      M(strike=0.5, cascade=0.35, quantize=0.25, cosmos=0.5, stack=0.35, sub=0.85, memory=0.25, cloud=0.3,
        bodychar=0.5, rotate=0.4),
      words=(F("Nebula Titan Oceanid Leviathan Aeon Colossus Meteor Galaxy Magellan Ephemeris Nebulous Tempest "
               "Monument Cataclysm Primeval Halcyon Dominion Behemoth"),
             COMMON_SECOND + F("Procession March Drums Gate Tide Crossing Fire Storm")),
      material=dict(kin=K("Martin Stürtzer .6 | Inade .2 | Lustmord .2"),
                    cats=W("radio-space 1 desert 1 dark-drone-noise .8 wind .8 underwater .6 ice-snow .6 seismic .8 brass 1 string-ensemble .8 bowed-low-string .8 analog-pad .6"),
                    worlds=W("cosmic 1 dark .6"), gestures=W("swell .6 sustain .6"), fr=0.45),
      types=T("Texture 1 Stretch 1 Spectral .9 Harmonic .9 Additive .5 Bow .4",
              "Stretch 1.2 Texture .7 Spectral .7 Noise .4"),
      wavetables=W("ambient:bowed 2 harmonic:strings 1.5 ambient:sampled 1.5 harmonic:pad 1"),
      rooms=W("vast 3 cavern 1 cathedral 1.5 far 1"),
      strikes=["Wood", "Wood", "String"], zshapes=["Frame Drum", "Timpani", "Tabla", "Gong"]),

    A("Pulsar Field", "Arecibo",
      P(reg(22, 32, 42, 58), pace((60, 240), (120, 360), (300, 800)), env((20, 60), (30, 90)),
        master_gain=(-15.0, -9.0), brain_density=("int", 1, 3), brightness=(0.15, 0.45), sub_level=(0.35, 0.7),
        far_decay=("log", 30, 80), scale=["JI Minor", "Subharmonic 16-8", "12-TET"]),
      M(pulse=0.8, noiseprimary=0.4, sub=1.0, cosmos=0.35, zplane=0.3, farfreeze=0.1, lfomodes=0.4, sync=0.2),
      words=(F("Pulsar Quasar Magnetar Telescope Parabola Dish Hydrogen Redshift Wavelength Radiant Transient "
               "Sidereal Neutron Millisecond Blazar Cyclotron Synchrotron Occultation"),
             COMMON_SECOND + F("Field Message Emission Source Band Burst Count Noise")),
      material=dict(kin=K("S.E.T.I. .3 | Lustmord .3"),
                    cats=W("radio-space 1.5 seismic 1.2 underwater 1 geothermal 1 dark-drone-noise 1 radio 1 sub-drone .8"),
                    worlds=W("dark .8 cosmic .8"), gestures=W("sustain 1"),
                    avoid=KEYS + ["flute", "choir", "guitar"], fr=0.7),
      types=T("Stretch 1.5 Spectral 1 Noise .8 Additive .4 Harmonic .3",
              "Stretch 1.4 Noise .8 Spectral .7"),
      wavetables=W("harmonic:sub 2 harmonic:pure 1"),
      rooms=W("vast 3 far 2 cavern 1"), noise=["Band", "Brown", "Digital", "Crackle"]),

    A("Slow Unfolding", "Moljebka Pvlse",
      P(reg(28, 38, 48, 62), pace((120, 400), (300, 900), (600, 1800)), env((30, 60), (40, 120)),
        master_gain=(-16.0, -10.0), brain_density=("int", 1, 3), brightness=(0.2, 0.5), far_decay=("log", 35, 90),
        sub_level=(0.2, 0.5), scale=["JI Minor", "Pythagorean", "JI 7-limit", "Subharmonic 16-8"]),
      M(blur=0.4, memory=0.3, feedback=0.25, farfreeze=0.15, cloud=0.3, sub=0.7, fold=0.15),
      words=(F("Duration Patience Ash Iron Stillwater Silt Cinder Rust Quay Sediment Driftwood Slate Tarnish "
               "Oxide Brine Rigging Anchorage Coldstore"),
             COMMON_SECOND + F("Unfolding Passage Hour Surface Stillness Travel Depth Distance")),
      material=dict(kin=K("Troum .3 | Deutsch Nepal .2 | Paul Bradley .2"),
                    cats=W("metal-creak 1 cave 1 factory .8 abandoned 1 ship-port .8 electric .6 contact-structure 1 guitar .6 feedback-drone .8 bowed-metal .6"),
                    worlds=W("dark .8 industrial .5 ritual .4"), gestures=W("sustain 1"), fr=0.65),
      types=T("Stretch 1.5 Spectral 1 Texture .6 Harmonic .4 Additive .4",
              "Stretch 1.4 Spectral .8 Noise .4"),
      wavetables=W("ambient:sampled 1 harmonic:hollow 1 harmonic:metal 1"),
      rooms=W("cavern 2 far 2 vast 1.5 drift 1"), sparse=True),

    A("Field Absence", "Francisco López",
      P(reg(30, 40, 50, 66), pace((60, 240), (120, 360), (300, 800)), env((5, 30), (10, 50)),
        master_gain=(-18.0, -10.0), brain_density=("int", 1, 3), brightness=(0.25, 0.6), sub_level=(0.0, 0.3),
        air=(0.0, 0.15), far_level=(0.3, 0.7), far_decay=("log", 8, 30)),
      M(vector=0.6, cloud=0.35, sub=0.3, room=0.3, src4=0.6),
      words=(F("Canopy Understory Mangrove Cicada Humidity Leaf Echo Swamp Hydrophone Burrow Sediment Termite "
               "Epiphyte Rainwater Bromeliad Tapir Sloth Buttress"),
             COMMON_SECOND + F("Absence Listening Environment Matter Blind Depth Recording Place")),
      material=dict(kin=K("Chris Watson .4 | BJNilsen .2"),
                    cats=W("underwater 1 wind 1 grain-texture 1 contact-structure 1 wetland 1 desert 1 foliage 1 river 1 cave 1 rainforest 1.2 insects 1"),
                    worlds=W("wild 1 cold .4"), fr=0.95),
      types=T("Stretch 1.6 Spectral .9 Texture .6 Additive .15 Harmonic .1",
              "Stretch 1.5 Spectral .7 Texture .4"),
      wavetables=W("harmonic:pure 1"), rooms=W("far 1.5 chamber 1 hall 1"), field=True),

    A("Machine Depths", "Tho-So-Aa",
      P(reg(22, 32, 40, 56), pace((80, 300), (200, 600), (400, 1200)), env((15, 50), (30, 90)),
        master_gain=(-15.0, -9.0), brain_density=("int", 1, 3), brightness=(0.1, 0.4), cutoff=("log", 200, 1200),
        sub_level=(0.35, 0.7), far_decay=("log", 25, 70)),
      M(noiseprimary=0.5, sub=1.0, zplane=0.35, feedback=0.3, cloud=0.3, memory=0.2, patina=0.25),
      words=(F("Boiler Sump Conduit Sludge Pumphouse Silo Borehole Flue Catwalk Gasket Valve Drain Kiln "
               "Foundry Manifold Cistern Sluicegate Hopper"),
             COMMON_SECOND + F("Depth Level Chamber Pressure Hum Flow Pit Underside")),
      material=dict(kin=K("Sleep Research Facility .3 | Bad Sector .2 | Deutsch Nepal .2"),
                    cats=W("factory 1.2 tunnel 1.2 dark-drone-noise 1.2 ventilation 1 geothermal 1 mud-bubbles 1 electrical-hum .6 pipe-tank .6 engine-hull .6"),
                    worlds=W("dark .8 industrial .8"), gestures=W("sustain 1"),
                    avoid=KEYS + ["flute", "choir", "solo-voice", "guitar", "gamelan"], fr=0.8),
      types=T("Stretch 1.6 Noise .8 Spectral .9 Texture .4 Additive .3",
              "Stretch 1.5 Noise .8 Spectral .7"),
      wavetables=W("harmonic:sub 1 harmonic:hollow 1"),
      rooms=W("chamber 2 far 1.5 cavern 1.5 plate 1"), noise=["Brown", "Band", "Crackle", "Pink"]),

    A("Field Recordings", "Chris Watson",
      P(reg(34, 44, 56, 72), pace((60, 240), (120, 360), (300, 800)), env((5, 25), (10, 40)),
        master_gain=(-17.0, -10.0), brain_density=("int", 1, 3), osc_level=(0.1, 0.35), far_lowcut=("log", 30, 120),
        sub_level=(0.0, 0.25)),
      M(vector=0.8, src2=0.95, src3=0.7, src4=0.5, room=0.5, cloud=0.1, sub=0.3),
      words=(F("Tideline Kelp Tundra Birdsong Wetland Rookery Estuary Dawn Gannet Reedbed Sealion Glacier Moor "
               "Whalesong Saltmarsh Nightjar Bittern Plover"),
             COMMON_SECOND + F("Chorus Recording Morning Colony Night Shore Migration Season")),
      material=dict(kin=K("Francisco López .3 | BJNilsen .3 | Hazard .2"),
                    cats=W("underwater 1 wind 1 sea 1 river 1 polar-station 1 foliage 1 contact-structure 1 insects 1 ice-snow 1 transport 1 rainforest .8 night-country .8"),
                    worlds=W("wild 1 cold .6"), fr=0.95),
      types=T("Stretch 1.8 Additive .4 Spectral .5", "Stretch 1.8 Spectral .4"),
      wavetables=W("harmonic:pure 1"), rooms=W("hall 1 far 1 chamber 1 cathedral .5"), field=True),

    A("Shaman Objects", "Voice of Eye",
      P(reg(32, 42, 54, 70), pace((30, 120), (60, 200), (200, 500)), env((10, 40), (20, 60)),
        brain_density=("int", 3, 6), brightness=(0.3, 0.6), far_decay=("log", 25, 70), sub_level=(0.2, 0.5),
        scale=["JI Minor", "Pythagorean", "JI 7-limit", "Subharmonic 16-8"]),
      M(strike=0.5, cascade=0.4, cloud=0.5, cloudplus=0.7, zplane=0.45, memory=0.25, bodychar=0.6, sub=0.7),
      words=(F("Rattle Feather Obsidian Totem Cedar Adobe Coyote Ember Canyon Turquoise Sandstone Juniper "
               "Rawhide Gourd Arroyo Petroglyph Cairn Smudge"),
             COMMON_SECOND + F("Objects Ceremony Circle Fire Journey Drum Smoke Night")),
      material=dict(kin=K("Oöphoi .3 | Troum .2 | Ulf Söderberg .2"),
                    cats=W("ritual-objects 1.5 wetland 1 fire 1 rainforest .8 murmur 1 singing-bowl .8 bowed-metal .6 blown-vessel .8 gong .6 waterphone 1 bowed-folk .6"),
                    worlds=W("ritual 1 warm .6 wild .4"), gestures=W("sustain .6 struck .6"), fr=0.6),
      types=T("Stretch 1.1 Spectral 1 Texture 1.1 Harmonic .5 Bow .4",
              "Stretch 1.2 Texture .8 Spectral .7"),
      wavetables=W("harmonic:resonator 1.5 ambient:sampled 1.5 harmonic:metal 1"),
      rooms=W("cavern 1.5 hall 1 vast 1.5 bloom 1"),
      strikes=["Wood", "Metal", "Wood"], zshapes=["Frame Drum", "Tabla", "Handpan", "Kalimba", "Bottle"]),

    A("Ghost Signal", "Bass Communion",
      P(reg(32, 42, 52, 68), pace((40, 160), (80, 260), (260, 700)), env((10, 40), (20, 60)),
        master_gain=(-16.0, -10.0), brightness=(0.2, 0.5), brain_density=("int", 2, 4), far_decay=("log", 20, 60),
        sub_level=(0.2, 0.5), scale=["JI Minor", "12-TET", "Pythagorean"]),
      M(patina=0.8, noiseprimary=0.35, memory=0.45, cloud=0.4, blur=0.35, sub=0.7, cosmos=0.3, grainshimmer=0.5),
      words=(F("Ghost Shellac Phonograph Needle Groove Wax Seance Apparition Parlour Gramophone Spectre Relic "
               "Attic Hiss Lacquer Etching Ouija Wireless"),
             COMMON_SECOND + F("Signal Transmission Record Voice Room Memory Dust Haunt")),
      material=dict(kin=K("Mimir .2 | Andrew Chalk .2 | In Camera .2"),
                    cats=W("crackle-media 1.5 city .8 ice-snow .8 transport .8 sea .8 piano 1 tape-loop .8 string-ensemble .6"),
                    worlds=W("tape .8 cold .5 luminous .3"), gestures=W("sustain .6 chord .4"), fr=0.5),
      types=T("Texture 1 Stretch 1.1 Spectral .9 Harmonic .7 Additive .4",
              "Stretch 1.2 Noise .7 Texture .7"),
      wavetables=W("harmonic:strings 1.5 harmonic:pluck 1 ambient:otmorph 1 classic:akwf .8"),
      rooms=W("chamber 1.5 hall 1.5 far 1.5 swell 1"), noise=["Crackle", "Crackle", "Pink", "Brown"]),

    A("Atom Space", "Atomine Elektrine",
      P(reg(34, 44, 58, 76), pace((8, 40), (15, 60), (60, 180)), env((3, 15), (10, 35)),
        brightness=(0.4, 0.75), cutoff=("log", 800, 5000), resonance=(0.2, 0.6), filter_env=(0.1, 0.5),
        brain_density=("int", 3, 6), far_decay=("log", 25, 70), dly_mix=(0.15, 0.35),
        scale=["12-TET", "JI Minor", "Pythagorean", "JI Major (Ptolemy)"]),
      M(filtermodel=0.8, quantize=0.45, sync=0.4, cosmos=0.5, delay2=0.5, zplane=0.4, strike=0.2, rotate=0.4),
      words=(F("Atom Isotope Reactor Nucleus Electron Fission Plasma Orbital Positron Crater Quark Lepton Cathode "
               "Gamma Muon Deuterium Tritium Cyclotron"),
             COMMON_SECOND + F("Space Array Core Flux Sequence Glow Horizon Pulse")),
      material=dict(kin=K("Martin Stürtzer .4 | Raison d'être .2 | Phelios .2"),
                    cats=W("analog-pad 1.5 modular-drone 1.5 digital-bell .6 radio-space .8 brass .4"),
                    worlds=W("cosmic 1"), gestures=W("sustain .4 chord .4 melodic .4"), fr=0.2),
      types=T("Wavetable 1.3 Harmonic .9 Additive .7 FM .5 Texture .7 Spectral .4",
              "Wavetable .8 Stretch .8 Texture .6 FM .4"),
      wavetables=W("classic:akwf 2.5 classic:wavedit 1.5 harmonic:pad 1"),
      rooms=W("vast 2 hall 1.5 plate 1"), filters=["Ladder", "Ladder", "LP 24", "BP 12"]),

    A("Hybrid Melancholy", "Polygon",
      P(reg(34, 44, 56, 72), pace((30, 120), (60, 200), (200, 500)), env((8, 30), (15, 50)),
        brightness=(0.3, 0.6), brain_density=("int", 2, 5), far_decay=("log", 20, 60), sub_level=(0.15, 0.45),
        scale=["JI Minor", "12-TET", "Pythagorean", "JI 7-limit"]),
      M(zplane=0.5, memory=0.35, cloud=0.35, blur=0.3, cosmos=0.3, filtermodel=0.4, patina=0.25),
      words=(F("Hybrid Facet Vertex Shard Prism Angle Edge Tessera Mosaic Polyhedron Rhombus Wire Pane Contour "
               "Silhouette Octahedron Bevel Chamfer"),
             COMMON_SECOND + F("Melancholy Abstraction Figure Plane Shade Form Dusk Fold")),
      material=dict(kin=K("Raison d'être .3 | In Camera .3 | Bass Communion .2 | Mirror .2"),
                    cats=W("string-ensemble .8 piano .6 analog-pad .8 choir .6 radio .6 digital-bell .6 abandoned .6 empty-space .6"),
                    worlds=W("dark .6 tape .5 cold .5"), gestures=W("sustain .6 chord .5"), fr=0.3),
      types=T("Harmonic 1 Wavetable .8 Texture 1 Spectral .7 Additive .5 FM .3",
              "Stretch 1 Texture .7 Harmonic .6 Noise .4"),
      wavetables=W("harmonic:morph 1.5 classic:wavedit 1.5 ambient:otmorph 1.5 harmonic:strings 1"),
      rooms=W("hall 1.5 chamber 1.5 drift 1 far 1")),

    A("Corridor", "Kammarheit",
      P(reg(26, 36, 48, 62), pace((60, 240), (150, 450), (400, 1200)), env((15, 50), (30, 90)),
        brain_density=("int", 1, 3), brightness=(0.15, 0.45), far_decay=("log", 35, 90), far_size=(2.2, 3.0),
        sub_level=(0.3, 0.6), near_mix=(0.05, 0.15), scale=["JI Minor", "Pythagorean", "Subharmonic 16-8", "12-TET"]),
      M(room=0.7, strike=0.25, cosmos=0.35, zplane=0.4, memory=0.2, cloud=0.3, sub=0.8, farmode=0.4, envelop=0.5),
      words=(F("Corridor Stairwell Archway Lantern Undercroft Gallery Wellspring Tunnel Hallway Doorway Crevice "
               "Colonnade Recess Alcove Vestibule Cloakroom Landing Balustrade"),
             COMMON_SECOND + F("Stillness Hollow Sleep Depth Echo Refuge Hour Descent")),
      material=dict(kin=K("Raison d'être .4 | Lustmord .3 | Thomas Köner .2 | Inade .1"),
                    cats=W("cave 1.2 tunnel 1 cave-wind 1 empty-space .8 low-wind .8 bell .6 choir .4 bowed-low-string .6 drone-ensemble .6 water-tone .6"),
                    worlds=W("dark 1 ritual .5 cold .4"), gestures=W("sustain 1 swell .4"), fr=0.45),
      types=T("Stretch 1.2 Spectral 1.1 Texture .8 Harmonic .7 Additive .5",
              "Stretch 1.2 Spectral .8 Texture .5"),
      wavetables=W("harmonic:sub 1.5 ambient:vowel_bass 1 harmonic:breath 1 ambient:tube 1"),
      rooms=W("cavern 3 cathedral 1.5 vast 1.5"), strikes=["Metal", "String"]),

    A("Northern Dark", "Gustaf Hildebrand",
      P(reg(28, 38, 50, 66), pace((30, 120), (60, 200), (200, 500)), env((10, 40), (25, 70)),
        brightness=(0.2, 0.5), sub_level=(0.3, 0.6), width=(1.2, 1.6), spread=(0.7, 1.0), far_decay=("log", 30, 80),
        far_size=(2.2, 3.0), brain_density=("int", 3, 5), scale=["JI Minor", "12-TET", "Pythagorean", "Subharmonic 16-8"]),
      M(rotate=0.55, zplane=0.45, cosmos=0.45, presence=0.3, sub=0.95, stack=0.35, memory=0.25, cloud=0.3, elev=0.4),
      words=(F("Boreal Aurora Magnetosphere Meridian Solar Oort Kuiper Nova Halo Perigee Apogee Axial Umbra "
               "Terminator Syzygy Parallax Libration Albedo"),
             COMMON_SECOND + F("Dark Machinery Wind Silence Expanse Cloud Frontier Orbit")),
      material=dict(kin=K("Martin Stürtzer .3 | Phelios .3 | Lustmord .2 | Sleep Research Facility .2"),
                    cats=W("analog-pad .8 modular-drone .8 sub-drone .8 radio-space 1 factory .6 abandoned .8 seismic .6 brass .4"),
                    worlds=W("cosmic 1 dark .8"), gestures=W("sustain .8 swell .5"), fr=0.4),
      types=T("Harmonic 1 Texture .9 Stretch .9 Spectral .9 Wavetable .6 Additive .5",
              "Stretch 1.2 Texture .6 Spectral .6 Noise .4"),
      wavetables=W("harmonic:pad 1.5 harmonic:sub 1.5 ambient:otmorph 1 classic:wavedit 1"),
      rooms=W("vast 3 far 1.5 cavern 1")),

    A("Void Station", "Tholen",
      P(reg(32, 42, 54, 70), pace((20, 90), (40, 160), (160, 420)), env((8, 30), (20, 60)),
        brightness=(0.25, 0.6), far_decay=("log", 25, 70), sub_level=(0.2, 0.5), brain_density=("int", 3, 6),
        z_rate=("log", 0.005, 0.08), scale=["12-TET", "JI Minor", "Pythagorean"]),
      M(zplane=0.75, cosmos=0.55, feedback=0.3, noiseprimary=0.25, cloud=0.3, filtermodel=0.5, memory=0.25, quantize=0.2),
      words=(F("Pneumatic Fibreoptic Arc Arcology Conduit Skyline Filament Overpass Plasma Sodium Grid Neon "
               "Tower Relay Monorail Megastructure Substation Tramline"),
             COMMON_SECOND + F("Station City Void Level District Light Signal Stars")),
      material=dict(kin=K("Martin Stürtzer .3 | Hazard .3 | Bad Sector .2"),
                    cats=W("analog-pad 1 modular-drone 1 electric 1.2 factory 1 ventilation .8 radio .6 electrical-hum .8 radio-space .8"),
                    worlds=W("cosmic .8 industrial .8"), gestures=W("sustain .8"), fr=0.45),
      types=T("Wavetable 1 Harmonic .8 Stretch 1 Spectral .8 FM .5 Texture .6",
              "Stretch 1.1 Noise .6 Wavetable .6 FM .4"),
      wavetables=W("classic:akwf 1.5 classic:wavedit 1.5 harmonic:morph 1"),
      rooms=W("vast 2 plate 1.5 far 1"), noise=["Digital", "Band", "Brown"]),

    A("Strings at Rest", "Stars of the Lid",
      P(reg(36, 46, 58, 74), pace((40, 150), (80, 260), (260, 600)), env((20, 60), (30, 90)),
        brightness=(0.35, 0.6), brain_density=("int", 3, 6), brain_consonance=(0.75, 0.95), far_decay=("log", 25, 60),
        near_mix=(0.15, 0.35), ens_mix=(0.3, 0.6), sub_level=(0.1, 0.3),
        scale=["JI Major (Ptolemy)", "JI 7-limit", "12-TET", "JI Minor"]),
      M(ownenv=0.6, stack=0.4, cosmos=0.15, memory=0.2, blur=0.25, room=0.5),
      words=(F("Dimmer Evening Velvet Sepia Lull Lament Somnolent Cello Horn Adagio Largo Waning Drowsy Curtain "
               "Hush Viola Bassoon Rosin"),
             COMMON_SECOND + F("Strings Swell Rest Horns Movement Afternoon Decrescendo Hall")),
      material=dict(kin=K("Andrew Chalk .3 | Mirror .2 | Michael Stearns .2 | Robert Rich .1"),
                    cats=W("string-ensemble 1.5 long-string 1.5 bowed-low-string 1.2 brass 1.2 piano .8 reed-organ .4 drone-string .6"),
                    worlds=W("luminous .6 warm .5 tape .4"), gestures=W("swell 1.2 sustain .8 chord .4"),
                    avoid=INDUSTRIAL + ["radio"], fr=0.05),
      types=T("Texture 1.2 Bow .8 Harmonic 1 Spectral .8 Additive .5",
              "Texture 1 Stretch .8 Bow .5 Harmonic .7"),
      wavetables=W("harmonic:strings 2.5 ambient:bowed 2.5 ambient:sampled 1.5 harmonic:pad 1"),
      rooms=W("hall 3 cathedral 1.5 bloom 1")),

    A("Tape Saturation", "Tim Hecker",
      P(reg(34, 44, 58, 76), pace((15, 60), (30, 100), (100, 300)), env((5, 20), (15, 45)),
        master_gain=(-14.0, -9.0), fb_bus=(0.06, 0.2), fb_drive=(0.5, 0.95), fb_tape=(0.4, 0.9),
        fb_tone=("log", 800, 4000), brightness=(0.5, 0.85), far_decay=("log", 15, 50), brain_density=("int", 4, 7),
        scale=["12-TET", "JI Minor", "JI Major (Ptolemy)"]),
      M(feedback=1.0, fold=0.5, patina=0.5, memory=0.5, blur=0.45, cloud=0.45, cloudplus=0.6, noiseprimary=0.25,
        zplane=0.4, grainshimmer=0.5),
      words=(F("Bellows Clipped Saturate Degrade Overload Grit Rupture Static Burnt Smear Bleach Fracture Shatter "
               "Pixel Fuse Crush Artefact Bitrot"),
             COMMON_SECOND + F("Organ Chapel Loop Fog Hymn Decay Wash Nave")),
      material=dict(kin=K("Andrew Chalk .2 | Colin Potter .2 | Bass Communion .2 | Mirror .2"),
                    cats=W("pipe-organ 2 reed-organ .8 tape-loop 1 piano .6 feedback-drone 1 crackle-media .6 string-ensemble .4"),
                    worlds=W("tape 1 industrial .4"), gestures=W("sustain .6 chord .6 swell .4"), fr=0.2),
      types=T("Texture 1.2 Spectral 1 Harmonic .9 Wavetable .5 Stretch .6",
              "Stretch 1 Texture .8 Noise .5 Harmonic .5"),
      wavetables=W("harmonic:organ 2.5 classic:wavedit 1 harmonic:metal 1 ambient:sampled 1"),
      rooms=W("cathedral 2 plate 1 hall 1 swell 1"), noise=["Crackle", "Digital", "Pink"]),

    A("Drum Procession", "Apoptose",
      P(reg(28, 38, 52, 68), pace((20, 80), (40, 140), (140, 360)), env((8, 30), (20, 60)),
        brightness=(0.25, 0.55), far_decay=("log", 30, 80), sub_level=(0.25, 0.55), brain_density=("int", 3, 6),
        scale=["JI Minor", "Pythagorean", "12-TET"]),
      M(strike=0.7, cascade=0.5, quantize=0.3, zplane=0.5, bodychar=0.7, room=0.7, sub=0.8, cosmos=0.3, memory=0.2, brain2=0.3),
      words=(F("Penitent Drumroll Torch Hood Pilgrim Banner Ashes Lent Midnight Plaza Candlelight Mourner Bellrope "
               "Stonefloor Cortege Brotherhood Belfry Sackcloth"),
             COMMON_SECOND + F("Procession Drums March Night Square Vow Crowd Fire")),
      material=dict(kin=K("Raison d'être .4 | Phelios .3 | Voice of Eye .2"),
                    cats=W("ritual-objects 1.5 murmur 1 fire .6 chant 1 choir .8 bell 1 gong .8 brass .6 cave .6 city .4"),
                    worlds=W("ritual 1 dark .8"), gestures=W("struck .8 sustain .6"), fr=0.45),
      types=T("Texture 1 Spectral 1 Stretch .9 Harmonic .7 Additive .4 Bow .3",
              "Stretch 1.1 Texture .8 Spectral .6"),
      wavetables=W("harmonic:choir 1.5 ambient:vowel_bass 1.5 harmonic:metal 1"),
      rooms=W("cathedral 2.5 cavern 1.5 vast 1 chamber 1"),
      strikes=["Wood", "Wood", "Wood", "Metal"], zshapes=["Frame Drum", "Timpani", "Tabla", "Church Bell", "Tubular Bell"]),

    A("Shortwave Dark", "Land:Fire",
      P(reg(30, 40, 50, 66), pace((30, 120), (60, 200), (200, 500)), env((10, 40), (20, 60)),
        master_gain=(-15.0, -10.0), brightness=(0.2, 0.5), brain_density=("int", 2, 4), far_decay=("log", 25, 70),
        sub_level=(0.25, 0.55), scale=["12-TET", "JI Minor", "Pythagorean"]),
      M(noiseprimary=0.4, cosmos=0.5, zplane=0.5, cloud=0.35, feedback=0.3, memory=0.25, sub=0.8),
      words=(F("Shortwave Longwave Numbers Carrier Antenna Heterodyne Ionospheric Squelch Morse Dial Frequency "
               "Jammer Skip Static Callsign Crystal Sideband Wavetrap"),
             COMMON_SECOND + F("Dark Band Night Call Station Fade Burst Grid")),
      material=dict(kin=K("Bad Sector .3 | S.E.T.I. .3 | Arecibo .2 | Hazard .2"),
                    cats=W("radio 1.5 radio-space 1.5 electric 1 electrical-hum 1 dark-drone-noise 1 factory .6 crackle-media .6"),
                    worlds=W("dark .8 cosmic .6 industrial .6"), gestures=W("sustain .8"), fr=0.6),
      types=T("Stretch 1.4 Spectral 1 Noise .8 Wavetable .5 Harmonic .5 FM .4",
              "Stretch 1.3 Noise .9 Spectral .7"),
      wavetables=W("classic:wavedit 1.5 harmonic:hollow 1 harmonic:sub 1"),
      rooms=W("far 2 vast 2 cavern 1"), noise=["Band", "Crackle", "Digital", "Brown"],
      zshapes=["Comb", "Tunnel", "Concrete Pipe"]),

    A("Modular Nocturne", "Ian Boddy",
      P(reg(36, 46, 60, 78), pace((3, 20), (6, 30), (30, 120)), env((1, 8), (6, 25)),
        resonance=(0.25, 0.65), filter_env=(0.2, 0.6), filter_drift=(0.3, 0.8), cutoff=("log", 500, 4000),
        brain_density=("int", 3, 7), dly_mix=(0.15, 0.35), dly_cross=(0.3, 0.8),
        scale=["12-TET", "JI Minor", "Pythagorean", "JI Major (Ptolemy)"]),
      M(quantize=0.7, sync=0.5, filtermodel=0.8, zplane=0.6, delay2=0.6, cosmos=0.3, strike=0.25, cascade=0.3, coherence=0.3),
      words=(F("Patch Cable Voltage Gate Envelope Slew Sequencer Clock Oscillator Filter Ramp Trigger Knob Jack "
               "Offset Phase Multiple Attenuator"),
             COMMON_SECOND + F("Nocturne Pattern Circuit Pulse Loop Signal Night Scan")),
      material=dict(kin=K("Martin Stürtzer .2 | Steve Roach .2"),
                    cats=W("modular-drone 2 analog-pad 1.5 digital-bell .8 electric-piano .4 radio .4"),
                    worlds=W("cosmic .8 dark .4"), gestures=W("melodic .6 sustain .5"), fr=0.1),
      types=T("Wavetable 1.5 FM 1 Harmonic .8 Additive .7 Texture .5",
              "Wavetable .9 FM .6 Stretch .6 Noise .4"),
      wavetables=W("classic:akwf 3 classic:wavedit 2 harmonic:morph 1"),
      rooms=W("plate 2 hall 1.5 chamber 1"), filters=["Ladder", "Ladder", "BP 12", "Peak", "LP 24"]),

    A("Fog City", "Jeff Greinke",
      P(reg(34, 44, 54, 70), pace((20, 80), (40, 140), (140, 360)), env((5, 20), (15, 45)),
        master_gain=(-16.0, -10.0), brightness=(0.2, 0.5), cutoff=("log", 400, 2000), far_decay=("log", 15, 45),
        brain_density=("int", 3, 6), air=(0.1, 0.35), scale=["12-TET", "JI Minor", "JI 7-limit"]),
      M(blur=0.55, fardiffuse=0.6, patina=0.3, memory=0.3, cloud=0.35, keys=0.1, delay2=0.4, noiseprimary=0.2),
      words=(F("Fog Refinery Seawall Drainage Smokestack Quayside Tugboat Foghorn Rustbelt Streetlamp Warehouse "
               "Overcast Grey Pylon Gasworks Breakwater Container Wharfside"),
             COMMON_SECOND + F("City District Waterfront Dusk Haze Channel Ward Horn")),
      material=dict(kin=K("Loscil .3 | In Camera .2 | Bass Communion .2 | BJNilsen .2"),
                    cats=W("city 1.2 ship-port 1.2 sea .8 transport .8 tape-keyboard .8 electric-piano .6 low-wind .8 blown-vessel .6 radio .4"),
                    worlds=W("cold .6 tape .6 dark .4"), gestures=W("sustain .6 chord .4"), fr=0.5),
      types=T("Texture 1 Stretch 1 Harmonic .8 Spectral .8 Wavetable .4",
              "Stretch 1.2 Texture .7 Noise .6"),
      wavetables=W("harmonic:breath 1.5 harmonic:pad 1.5 ambient:otmorph 1 classic:akwf .8"),
      rooms=W("far 1.5 hall 1.5 drift 1.5 chamber 1")),

    A("River Loops", "Vidna Obmana",
      P(reg(38, 48, 60, 76), pace((20, 80), (40, 140), (140, 360)), env((5, 20), (15, 45)),
        brightness=(0.35, 0.65), brain_density=("int", 3, 5), far_decay=("log", 20, 55), sub_level=(0.1, 0.3),
        scale=["JI Major (Ptolemy)", "JI Pentatonic", "12-TET", "JI Minor"]),
      M(memory=0.5, dejavu=0.45, sync=0.3, strike=0.2, cloud=0.3, delay2=0.45, keys=0.1, pulse=0.3),
      words=(F("River Ocarina Rainstick Pebble Shore Delta Willow Heron Ripple Mist Meander Ford Cove Riverbed "
               "Rushwater Kingfisher Dragonfly Fernbank"),
             COMMON_SECOND + F("Loops Flow Breath Reflection Morning Pool Passage Calm")),
      material=dict(kin=K("Steve Roach .5 | Robert Rich .3 | Oöphoi .2"),
                    cats=W("flute 1.5 blown-vessel 1.2 piano .8 analog-pad .8 rain-land 1 river 1 rainforest .8 zither-harp .5"),
                    worlds=W("warm .8 luminous .5"), gestures=W("sustain .6 melodic .6"), fr=0.3),
      types=T("Texture 1.3 Harmonic .9 Additive .7 Spectral .6 Wavetable .4",
              "Stretch 1 Texture .8 Harmonic .5"),
      wavetables=W("harmonic:breath 2 harmonic:pad 1.5 ambient:consonant 1"),
      rooms=W("hall 2.5 bloom 1.5 cathedral 1"), strikes=["Wood", "String"]),

    A("Low Brass Swell", "Tom Heasley",
      P(reg(24, 34, 44, 58), pace((30, 120), (60, 200), (200, 500)), env((15, 45), (25, 70)),
        brightness=(0.2, 0.5), sub_level=(0.3, 0.6), far_decay=("log", 30, 70), brain_density=("int", 3, 6),
        scale=["JI Minor", "Harmonic 8-16", "JI 7-limit", "Subharmonic 16-8"]),
      M(memory=0.6, ownenv=0.4, sub=0.9, cosmos=0.3, stack=0.35, zplane=0.3, delay2=0.4),
      words=(F("Tuba Valve Mouthpiece Didgeridoo Sousaphone Lowland Embouchure Pedal Octave Furrow Cavernous "
               "Bellow Groundswell Fathom Undertone Tidewater Magma Bourdon"),
             COMMON_SECOND + F("Swell Tone Drone Plate Sigh Layer Loop Depth")),
      material=dict(kin=K("Lustmord .2 | Michael Stearns .2"),
                    cats=W("brass 2 overtone-horn 1.5 sub-drone .8 overtone-voice .6 blown-vessel .6 low-wind .4"),
                    worlds=W("dark .5 warm .5 ritual .4"), gestures=W("swell 1.2 sustain .8"), fr=0.1),
      types=T("Texture 1.3 Spectral 1 Harmonic 1 Additive .5 Stretch .5",
              "Texture .8 Stretch .8 Harmonic .6 Spectral .6"),
      wavetables=W("ambient:tube 2.5 ambient:sampled 2 ambient:vowel_bass 1.5 harmonic:sub 1"),
      rooms=W("cathedral 2 vast 1.5 cavern 1.5"), filters=["Formant", "LP 24", "Ladder"]),

    A("Guitar Twilight", "Jeff Pearce",
      P(reg(38, 48, 60, 76), pace((30, 100), (60, 180), (180, 420)), env((8, 30), (20, 60)),
        brightness=(0.35, 0.65), brain_density=("int", 3, 5), far_decay=("log", 25, 60), dly_mix=(0.15, 0.35),
        dly_feedback=(0.5, 0.8), scale=["JI Major (Ptolemy)", "12-TET", "JI Minor", "JI Pentatonic"]),
      M(ownenv=0.7, memory=0.55, delay2=0.5, cosmos=0.25, blur=0.3, room=0.5),
      words=(F("Twilight Dusk Afterglow Nightfall Gloaming Amber Violet Glow Solace Hymnal Moonrise Vesperal "
               "Starling Lantern Emberlight Dewfall Owlcall Hearthside"),
             COMMON_SECOND + F("Swell Loop Sustain Shore Arc Bloom Veil Tide")),
      material=dict(kin=K("Steve Roach .2 | Robert Rich .2 | Andrew Chalk .2"),
                    cats=W("guitar 2 steel-guitar 2 piano .6 long-string .4 drone-string .4"),
                    worlds=W("luminous .8 warm .5"), gestures=W("swell 1.2 sustain .8 melodic .4"), fr=0.05),
      types=T("Texture 1.5 Spectral .9 Harmonic .8 Stretch .5 Bow .4",
              "Texture 1 Stretch .7 Harmonic .5"),
      wavetables=W("harmonic:strings 1.5 harmonic:pluck 1.5 ambient:bowed 1 classic:akwf 1"),
      rooms=W("hall 2.5 bloom 1.5 cathedral 1")),

    A("Silk Road Space", "Amir Baghiri",
      P(reg(36, 46, 58, 74), pace((20, 80), (40, 140), (140, 360)), env((6, 25), (15, 50)),
        brightness=(0.35, 0.65), brain_density=("int", 3, 6), far_decay=("log", 25, 65), sub_level=(0.15, 0.45),
        scale=["JI Minor", "Pythagorean", "JI 7-limit", "JI Pentatonic"]),
      M(strike=0.35, cascade=0.3, cosmos=0.4, memory=0.25, cloud=0.3, zplane=0.4, delay2=0.4, sync=0.25),
      words=(F("Silk Oasis Bazaar Nomad Cardamom Tapestry Spice Indigo Sandalwood Caravanserai Lapis Dunescape "
               "Starlit Frankincense Qanat Pistachio Henna Mosaicwork"),
             COMMON_SECOND + F("Space Road Journey Night Tribe Garden Dream Wind")),
      material=dict(kin=K("Steve Roach .3 | Klaus Wiese .3 | Voice of Eye .2"),
                    cats=W("flute 1.2 zither-harp 1.2 bowed-folk 1 singing-bowl .8 drone-string 1 gamelan .6 desert 1 fire .4 analog-pad .6"),
                    worlds=W("warm .8 ritual .6 cosmic .5"), gestures=W("sustain .6 melodic .6"), fr=0.2),
      types=T("Texture 1.3 Harmonic .9 Spectral .8 Additive .6 Wavetable .5",
              "Stretch 1 Texture .8 Harmonic .5"),
      wavetables=W("harmonic:breath 1.5 harmonic:reed 1.5 ambient:sampled 1 harmonic:pad 1"),
      rooms=W("hall 2 cathedral 1.5 vast 1.5"),
      strikes=["Wood", "String", "Wood"], zshapes=["Tabla", "Frame Drum", "Handpan", "Kalimba"]),

    A("Water Tower Voices", "Jim Cole",
      P(reg(36, 46, 60, 74), pace((40, 150), (80, 260), (260, 600)), env((10, 35), (25, 70)),
        brain_density=("int", 3, 6), purity=(0.97, 1.0), far_decay=("log", 40, 100), far_size=(2.6, 3.0),
        brightness=(0.45, 0.75), sub_level=(0.1, 0.3),
        scale=["Harmonic 8-16", "Otonality 1-11", "JI 7-limit", "JI Major (Ptolemy)"]),
      M(cosmos=0.35, zplane=0.45, stack=0.4, room=0.6, adaptive=0.5, match=0.3, filtermodel=0.5),
      words=(F("Tower Cistern Overtone Harmonic Vowel Throat Resonant Echoing Monk Sutra Choral Reservoir "
               "Standpipe Silo Rotunda Dome Belltower Undertone"),
             COMMON_SECOND + F("Voices Tones Chant Chorus Echo Well Space Sound")),
      material=dict(kin=K("Mathias Grassow .4 | Klaus Wiese .2 | Michael Stearns .2"),
                    cats=W("overtone-voice 2 chant 1.2 solo-voice 1 choir 1 overtone-horn .6"),
                    worlds=W("ritual .6 warm .4"), gestures=W("sustain 1 swell .5"),
                    avoid=INDUSTRIAL + ["radio", "crackle-media"], fr=0.0),
      types=T("Harmonic 1.3 Texture 1.2 Spectral 1 Additive .6",
              "Texture .9 Harmonic .8 Spectral .7 Stretch .4"),
      wavetables=W("ambient:overtone 3 ambient:vowel_tenor 2 ambient:vowel_bass 2 ambient:vowel_alto 1 harmonic:choir 2"),
      rooms=W("vast 2 cathedral 2.5 bloom 1"), filters=["Formant", "Formant", "LP 24", "Peak"]),
]


# ---------------------------------------------------------------- the built-in presets

# No files: a built-in preset has to sound as it should on a machine where the sample library was
# never installed. So its sources are the ones the instrument makes itself -- the additive bank, the
# five built-in tables in the Harmonic and Wavetable types, FM, noise, the bowed string -- and its
# room is the synthetic reverbs, never the convolution (which keeps whatever impulse was loaded
# before). Sixteen families of sixteen, the first of them opening with Init.
NO_FILES = dict(texture=0.0, usertable=0.0, room=0.0)
BUILTIN_TABLES = W("builtin:Classic 1 builtin:Organ 1 builtin:Vocal 1 builtin:Glass 1 builtin:Metal 1")


def B(name, params, modules=None, words=None, types=None, wavetables=None, **kw):
    return A(name, name, params, dict(NO_FILES, **(modules or {})), words=words, types=types,
             wavetables=wavetables or BUILTIN_TABLES, rooms={}, builtin=True, **kw)


BUILTINS = [
    B("Just Drones",
      P(reg(34, 44, 62, 80), pace((25, 90), (45, 150), (150, 420)), env((8, 30), (15, 50)),
        scale=["JI 7-limit", "JI Major (Ptolemy)", "Harmonic 8-16", "Otonality 1-11", "JI Minor"],
        purity=(0.95, 1.0), sub_level=(0.15, 0.4), bloom=(0.2, 0.7)),
      M(sub=0.8, stack=0.5, adaptive=0.4, cosmos=0.2, zplane=0.25),
      words=(F("Just Otonal Ratio Seventh Lattice Harmonic Pure Consonant Sleeping Somnus Warm Night"),
             COMMON_SECOND + F("Drone Chord Stack Hours")),
      types=T("Additive 2 Harmonic 1", "Additive 1 Harmonic .8 Noise .2")),
    B("Glass and Bells",
      P(reg(44, 54, 70, 86), pace((20, 80), (40, 140), (140, 360)), env((4, 20), (12, 40)),
        brightness=(0.6, 0.95), inharmonic=(0.05, 0.3), scale=["Harmonic 8-16", "JI Major (Ptolemy)", "JI Pentatonic", "Slendro (JI)"]),
      M(strike=0.6, zplane=0.5, cosmos=0.3),
      words=(F("Glass Crystal Bell Chime Prism Quartz Frost Silver Wineglass Icicle Porcelain Celesta"),
             COMMON_SECOND + F("Bells Rim Needles Light")),
      types=T("Harmonic 1.2 FM 1.2 Wavetable .8 Additive .6", "FM .8 Harmonic .8 Additive .5 Noise .2"),
      wavetables=W("builtin:Glass 3 builtin:Metal 2 builtin:Classic .5"),
      strikes=["Metal", "Metal", "String"], zshapes=["Glockenspiel", "Tubular Bell", "Church Bell", "Glass", "Vibraphone"]),
    B("Choirs and Vowels",
      P(reg(38, 48, 62, 78), pace((30, 120), (60, 200), (200, 480)), env((8, 30), (20, 60))),
      M(zplane=0.45, cosmos=0.5, filtermodel=0.6),
      words=(F("Choir Vowel Chant Voice Hymn Throat Alto Choral Whispered Sung Formant Cantor"),
             COMMON_SECOND + F("Voices Morph Chorus Psalm")),
      types=T("Harmonic 1.5 Wavetable .8 Additive .5", "Harmonic 1 Additive .5 Noise .3"),
      wavetables=W("builtin:Vocal 4 builtin:Organ 1"), filters=["Formant", "Formant", "LP 24", "Peak"]),
    B("Deep and Sub",
      P(reg(22, 32, 40, 56), pace((40, 160), (90, 260), (260, 700)), env((10, 40), (20, 80)),
        brightness=(0.1, 0.35), tilt=(1.6, 2.8), cutoff=("log", 250, 1200), sub_level=(0.35, 0.7),
        sub_octave=["-1", "-2"], far_decay=("log", 35, 80), scale=["JI Minor", "Subharmonic 16-8", "Pythagorean"]),
      M(sub=1.0, cosmos=0.35, zplane=0.3, memory=0.2),
      words=(F("Abyss Tectonic Bedrock Undertow Magma Hollow Basalt Cavern Trench Seafloor Gravity Leviathan"),
             COMMON_SECOND + F("Sub Floor Pressure Weight")),
      types=T("Additive 1.5 Harmonic 1 Wavetable .5 Bow .3", "Additive .8 Harmonic .6 Noise .5")),
    B("Bowed Strings",
      P(reg(34, 44, 58, 76), pace((30, 120), (60, 200), (200, 480)), env((6, 25), (15, 50))),
      M(bodychar=0.6, earlyroom=0.5, stack=0.3, memory=0.2),
      words=(F("Rosin Bow Cello Viol Gut Bridge Soundpost Horsehair Sympathetic Fiddle Drone Tension"),
             COMMON_SECOND + F("Strings Wire Bowing Resonance")),
      types=T("Bow 2 Harmonic .6 Additive .5", "Bow 1 Additive .6 Harmonic .6")),
    B("Organs and Reeds",
      P(reg(34, 44, 60, 78), pace((20, 90), (40, 160), (160, 420)), env((4, 20), (12, 40))),
      M(zplane=0.3, memory=0.25, patina=0.2),
      words=(F("Organ Reed Harmonium Stop Pipe Bellows Diapason Mixture Chapel Pedal Manual Regal"),
             COMMON_SECOND + F("Organ Reeds Stops Psalter")),
      types=T("Harmonic 1.2 Wavetable 1.2 Additive .6", "Wavetable .8 Harmonic .8 Noise .4"),
      wavetables=W("builtin:Organ 4 builtin:Classic 1 builtin:Vocal .5"), noise=["Pink", "Band", "Wind"]),
    B("Played Keys",
      P(reg(40, 50, 64, 80), env((0.5, 6), (4, 20)), keys_depth=(0.0, 0.4)),
      M(keys=1.0, strike=0.5, presence=0.5, nearfield=0.5, memory=0.2),
      words=(F("Keys Hand Touch Played Near Dry Felt Hammer Wooden Parlour Spinet Lantern"),
             COMMON_SECOND + F("Keys Touch Hands Room")),
      types=T("Harmonic 1 Additive 1 FM .8 Wavetable .8", "Additive .6 Harmonic .6 Noise .2")),
    B("Generative Chords",
      P(reg(36, 46, 60, 78), pace((20, 80), (40, 140), (140, 360)), env((6, 25), (15, 45)),
        auto_mode=["Chords", "Chords", "Free"], auto_rate=("log", 20, 120), auto_tension=(0.1, 0.6)),
      M(brain2=0.6, keyfind=0.6, blend=0.6, adaptive=0.6, evensmooth=0.6, timbre=0.5, dejavu=0.4),
      words=(F("Turning Chord Progression Voicing Cadence Circle Changing Ladder Exchange Modal Pivot Relative"),
             COMMON_SECOND + F("Harmony Chords Turn Motion"))),
    B("Cosmos",
      P(reg(34, 44, 60, 78), pace((25, 90), (50, 160), (160, 420)), env((8, 30), (20, 60))),
      M(cosmos=1.0, cosmosswell=0.6, banks=0.6),
      words=(F("Nebula Pulsar Quasar Comet Orbit Stellar Ion Solar Galactic Plasma Corona Event"),
             COMMON_SECOND + F("Cosmos Horizon Wind Field"))),
    B("Clouds and Memory",
      P(reg(36, 46, 60, 78), pace((20, 80), (40, 140), (140, 360)), env((6, 25), (15, 45))),
      M(cloud=1.0, cloudplus=0.9, memory=0.85),
      words=(F("Cloud Grain Swarm Mist Memory Recall Echo Remembered Scatter Flock Haze Reverie"),
             COMMON_SECOND + F("Cloud Memory Grains Return"))),
    B("Weather and Noise",
      P(reg(34, 44, 58, 76), pace((25, 90), (50, 160), (160, 420)), env((8, 30), (20, 60)), air=(0.15, 0.45),
        breath=(0.1, 0.5)),
      M(air=0.5),
      words=(F("Wind Rain Storm Gale Drizzle Fog Weather Hail Squall Breath Gust Tempest"),
             COMMON_SECOND + F("Weather Storm Front Air")),
      types=T("Additive 1.2 Harmonic 1 Noise .4", "Noise 1.5 Additive .5 Harmonic .5"),
      noise=["Wind", "Band", "Pink", "Brown", "Crackle", "Blue"]),
    B("Metal and Feedback",
      P(reg(32, 42, 54, 72), pace((15, 60), (30, 100), (100, 300)), env((4, 20), (12, 40)),
        fb_bus=(0.05, 0.18), fb_drive=(0.4, 0.9), inharmonic=(0.15, 0.45)),
      M(feedback=1.0, fold=0.5, patina=0.5, zplane=0.45),
      words=(F("Rust Iron Anvil Chain Steel Foundry Rivet Girder Tin Copper Cymbal Bellmetal"),
             COMMON_SECOND + F("Metal Feedback Loop Drive")),
      types=T("Harmonic 1 FM 1.2 Wavetable 1 Additive .6", "FM .8 Wavetable .6 Noise .5"),
      wavetables=W("builtin:Metal 4 builtin:Classic 1"), strikes=["Metal"], zshapes=["Steel Plate", "Metal Bars", "Tam Tam", "Gong"]),
    B("Space and Motion",
      P(reg(36, 46, 60, 78), pace((20, 80), (40, 140), (140, 360)), env((6, 25), (15, 45))),
      M(binaural=0.6, vector=0.7, rotate=0.7, farmode=0.6, elev=0.7, doppler=0.5, depthlaw=0.6, src3=0.8, src4=0.6),
      words=(F("Rotating Orbit Around Circling Wheeling Turning Wide Near Far Above Passing Spiral"),
             COMMON_SECOND + F("Sky Head Motion Space"))),
    B("Slow Worlds",
      P(reg(34, 44, 58, 76), pace((60, 240), (120, 400), (400, 1200)), env((15, 50), (30, 90))),
      M(lenia=0.7, chaos=0.7, arcclock=0.3, tide=0.6, archarmony=0.5, sympathy=0.5),
      words=(F("Tidal Hour Aeon Glacial Evening Season Ninety Midnight Dawn Dusk Epoch Solstice"),
             COMMON_SECOND + F("Arc Hours Cycle Night"))),
    B("Microtonal",
      P(reg(36, 46, 60, 78), pace((25, 90), (50, 160), (160, 420)), env((8, 30), (20, 60)),
        scale=["Bohlen-Pierce (JI)", "Slendro (JI)", "Otonality 1-11", "Subharmonic 16-8", "Harmonic 8-16", "Timbre (Sethares)"],
        stretch=(0.0, 12.0)),
      M(match=0.5, adaptive=0.5, guard=0.4, timbre=0.5),
      words=(F("Bohlen Slendro Otonal Utonal Tritave Stretched Seventeenth Nineteenth Pelog Comma Diesis Schisma"),
             COMMON_SECOND + F("Scale Ratio Octave Tuning"))),
    B("Strike and Modal",
      P(reg(38, 48, 62, 80), pace((10, 40), (20, 80), (80, 240)), env((3, 15), (10, 35))),
      M(strike=1.0, cascade=0.6, zplane=0.7, bodychar=0.7, surprise=0.4),
      words=(F("Struck Plucked Marimba Kalimba Handpan Tabla Wooden Bar Plate Timpani Mallet Sympathetic"),
             COMMON_SECOND + F("Strike Ring Mode Body")),
      strikes=["String", "Wood", "Metal"], zshapes=["Marimba", "Vibraphone", "Kalimba", "Handpan", "Timpani", "Harp Body", "Piano Board"]),
]

STYLES = ARTISTS
