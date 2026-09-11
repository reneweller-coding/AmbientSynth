"""Every place the generator picks something by MATCHING A NAME, checked against reality.

Three bugs of exactly one kind turned up today: a style asking for a wavetable recipe called
"Harmonic drawbars" (it is "Organ drawbars"), two modulation targets that do not exist
(far_spread, time_width), and an impulse family searching for the prefix "field_recordings_"
which no clip has carried since the library moved to prompt lists. None of the three ever raised
an error. They just stopped matching, and the generator went on drawing from whatever was left.

This lists every such rule and says how much it still catches.
"""
import glob
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "library"))
sys.path.insert(0, os.path.join(ROOT, "Tools", "WavetableGen"))
from styles import STYLES                      # noqa: E402
from wavetablegen_core import RECIPES          # noqa: E402

bad = 0


def report(title, misses, total, note=""):
    global bad
    if misses:
        bad += len(misses)
        print(f"MISS  {title}: {len(misses)} of {total} match nothing -> {sorted(misses)[:6]}")
    else:
        print(f"ok    {title}: all {total} match something {note}")


# 1 -- the wavetable recipes a style names
miss = {t for st in STYLES for t in st.get("tables", []) if t not in RECIPES}
report("style tables -> RECIPES", miss, sum(len(st.get("tables", [])) for st in STYLES))

# 2 -- the impulse families a style names, against the files on the shelf
irs = [os.path.basename(f) for f in glob.glob(os.path.join(ROOT, "Library", "Impulses", "*.wav"))]
prefixes = {p for st in STYLES for p in st.get("impulses", [])}
miss = {p for p in prefixes if not any(f.startswith(p) for f in irs)}
report("style impulses -> Library/Impulses", miss, len(prefixes), f"({len(irs)} files)")

# 3 -- the noise colours a style names, against the list the core actually has
src = open(os.path.join(ROOT, "Core", "src", "Sources.cpp"), encoding="utf-8").read()
i = src.index('"White", "Pink"')
colours = set(re.findall(r'"([A-Za-z ]+)"', src[i:src.index("}", i)]))
names = {n for st in STYLES for n in st.get("noise", [])}
report("style noise -> Sources.cpp", names - colours, len(names))

# 4 -- the per-style clip prefix in texture_pool: does any style still own clips by name?
tex = [os.path.basename(f) for f in glob.glob(os.path.join(ROOT, "Library", "Textures", "*.wav"))]
tex += [os.path.basename(f) for f in glob.glob(os.path.join(ROOT, "Library", "FieldRecordings", "*.wav"))]
owned = 0
for st in STYLES:
    slug = re.sub(r"[^a-z0-9]+", "_", st["name"].lower()).strip("_")[:20]
    if any(f.startswith(slug + "_") for f in tex):
        owned += 1
print(f"note  texture_pool style prefix: {owned} of {len(STYLES)} styles still own clips by name; "
      f"the rest fall back to the shelf and are steered by Library/affinity.json")

# 5 -- affinity: does every style have entries, and do they point at files that exist?
import json                                    # noqa: E402
aff = os.path.join(ROOT, "Library", "affinity.json")
if os.path.exists(aff):
    blob = json.load(open(aff, encoding="utf-8"))
    have = set(tex)
    empty, dangling = [], 0
    for nm, folders in blob.get("byStyle", {}).items():
        refs = [c for lst in folders.values() for c in lst]
        if not refs:
            empty.append(nm)
        dangling += sum(1 for c in refs if os.path.basename(c) not in have)
    print(f"{'MISS ' if empty or dangling else 'ok   '} affinity.json: {len(blob.get('byStyle', {}))} styles, "
          f"{len(empty)} empty, {dangling} references to clips that are not there")
    bad += len(empty) + (1 if dangling else 0)
else:
    print("note  affinity.json not written yet")

# 6 -- the wavetable recipe slugs, as wavetable_pool matches them against file names
wts = [os.path.basename(f) for f in glob.glob(os.path.join(ROOT, "Library", "Wavetables", "**", "*.wav"), recursive=True)]
miss = set()
for r in RECIPES:
    slug = re.sub(r"[^a-z0-9]+", "_", r.lower()).strip("_")
    if not any(f.startswith(slug + "_") for f in wts):
        miss.add(r)
report("RECIPES -> Library/Wavetables file names", miss, len(RECIPES), f"({len(wts)} files)")

print("\n" + ("all name rules still catch something" if not bad else f"{bad} rule(s) match nothing"))
