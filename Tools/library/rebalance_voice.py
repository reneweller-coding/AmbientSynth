"""Put the instrument's voice above its own noise bed. Measured, one preset at a time.

Every preset has two kinds of material in it. One is pitched to what is played -- the source slots
and everything downstream of them, their filters, their reverb. The other is not: Air and Breath,
a noise slot, the Foundation, the strike bed. Only the first can be heard as harmony, and if the
second is louder the preset is a wash that ripples when notes come and go.

That is what the library was. `make_presets.fill_slot` gives EVERY slot a level drawn from
0.15 .. 0.55 -- a figure meant for a supporting layer -- and the first slot is the voice. A preset
becomes Additive only when no type is drawn at all, and then keeps the parameter's own default of
1.0. So the additive presets stood at full level over the bed and every other preset stood at a
quarter to a third of it, over the same bed. Measured on two of them, source muted against the
whole preset:

    Bardo Stillness   voice 8.3 dB BELOW the rest of the preset
    Hymn Span         voice 11.5 dB below

Hence "only the additive presets are played tonally": the others were there, in tune, following
the note exactly -- and inaudible under their own Air.

Numbers, not guesses. Each preset is rendered twice at the same clock hour: once whole, once with
every source slot at zero. The difference in energy is the voice, what is left is the bed, and
their ratio says how much has to move. The voice is raised first, as far as 1.0, because that
leaves the preset its character and changes only the balance; Air and Breath are lowered only for
what the voice alone cannot cover, and never below a floor, so the air in the room stays.

Presets whose first slot is Noise are left alone: there the bed IS the piece.

    python Tools/library/rebalance_voice.py --limit 40 --dry-run
    python Tools/library/rebalance_voice.py --jobs 12

Afterwards the library has to be measured again: this changes loudness and every descriptor.
"""
import argparse, concurrent.futures as cf, glob, math, os, re, statistics, subprocess, sys

SLOT_LEVEL = {"1": "osc_level", "2": "src2_level", "3": "src3_level", "4": "src4_level"}
BED = ("air", "breath")
BED_DEFAULT = {"air": 0.15, "breath": 0.0}
BED_FLOOR = 0.03
RMS_RE = re.compile(r"measure: rms=(-?[\d.]+)")


# ---------------------------------------------------------------- the pack line
def parse_settings(text):
    out = []
    for tok in text.split(";"):
        if "=" in tok:
            k, _, v = tok.partition("=")
            out.append([k, v])
        elif tok:
            out.append([tok, None])
    return out


def write_settings(pairs):
    return ";".join(k if v is None else f"{k}={v}" for k, v in pairs)


def get(pairs, key, default):
    for k, v in pairs:
        if k == key:
            try:
                return float(v)
            except (TypeError, ValueError):
                return default
    return default


def get_str(pairs, key, default=""):
    for k, v in pairs:
        if k == key:
            return v if v is not None else default
    return default


def put(pairs, key, value):
    text = f"{value:.4g}"
    for pair in pairs:
        if pair[0] == key:
            pair[1] = text
            return
    pairs.append([key, text])


def active_slots(pairs):
    """Which slots make a sound, and at what level."""
    out = {}
    for s in "1234":
        t = get_str(pairs, f"src{s}_type", "Additive" if s == "1" else "Off")
        if t in ("Off", "", "Noise"):
            continue
        out[s] = get(pairs, SLOT_LEVEL[s], 1.0 if s == "1" else 0.5)
    return out


# ---------------------------------------------------------------- the measurement
def render_rms(exe, name, seconds, hour, sets, env):
    cmd = [exe, "--preset", name, "--seconds", str(seconds), "--hour", str(hour), "--measure"]
    for k, v in sets:
        cmd += ["--set", f"{k}={v}"]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=300, env=env)
    except subprocess.TimeoutExpired:
        return None
    m = RMS_RE.search(r.stdout)
    return float(m.group(1)) if m else None


def probe(job):
    exe, name, seconds, hour, slots, env = job
    whole = render_rms(exe, name, seconds, hour, [], env)
    bed = render_rms(exe, name, seconds, hour, [(SLOT_LEVEL[s], 0) for s in slots], env)
    return name, whole, bed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", default="Library/Packs")
    ap.add_argument("--render", default="build/Tools/render/Release/ambient_render.exe")
    ap.add_argument("--target-db", type=float, default=0.0,
                    help="how far the voice stands above the bed. Not a taste: the additive presets, the ones "
                         "that already sound played, measure at a median of -0.1 dB over a sample of 90.")
    ap.add_argument("--max-bed-cut", type=float, default=4.0,
                    help="how far Air and Breath may be taken down. They are the room, and a preset that loses "
                         "them stops sounding large; better to leave one short of the target than hollow.")
    ap.add_argument("--seconds", type=float, default=40.0, help="long enough for the slowest attacks in the library")
    ap.add_argument("--hour", type=float, default=12.0, help="the clock is an input; hold it still or nothing repeats")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 8) - 2))
    ap.add_argument("--limit", type=int, default=0, help="measure only the first N, to try the thing out")
    ap.add_argument("--only-type", default="", help="only presets whose first slot is this type -- for asking "
                                                    "the additive presets, which sound right, what the target should be")
    ap.add_argument("--every", type=int, default=1, help="take every Nth preset, for a spread sample")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    if not os.path.exists(a.render):
        sys.exit(f"no renderer at {a.render}")
    # CreateProcess wants a real path, not the shell's idea of one.
    a.render = os.path.normpath(os.path.abspath(a.render))
    files = sorted(glob.glob(os.path.join(a.packs, "*.ambientpack")))
    if not files:
        sys.exit(f"no packs in {a.packs}")

    # Read everything first: one pass to measure, one to write.
    book = []           # (path, line index, name, pairs)
    raw = {}            # path -> list of lines
    for path in files:
        raw[path] = open(path, encoding="utf-8").read().split("\n")
        for i, line in enumerate(raw[path]):
            if line.startswith("#") or "|" not in line:
                continue
            fields = line.split("|")
            book.append((path, i, fields[0].strip(), parse_settings(fields[1])))
    if a.only_type:
        book = [b for b in book if get_str(b[3], "src1_type", "Additive") == a.only_type]
    if a.every > 1:
        book = book[::a.every]
    if a.limit:
        book = book[:a.limit]

    env = dict(os.environ)
    env.setdefault("AMBIENT_PACKS", os.path.abspath(a.packs))
    jobs, skipped_noise = [], 0
    for path, i, name, pairs in book:
        slots = active_slots(pairs)
        if not slots:                      # a preset whose voice is a noise slot: the bed is the piece
            skipped_noise += 1
            continue
        jobs.append((a.render, name, a.seconds, a.hour, slots, env))

    print(f"{len(jobs)} Presets messen ({skipped_noise} uebersprungen), {a.jobs} parallel, "
          f"{a.seconds:.0f} s je Render, zwei Renders je Preset")
    result = {}
    done = 0
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        for name, whole, bed in ex.map(probe, jobs):
            result[name] = (whole, bed)
            done += 1
            if done % 100 == 0:
                print(f"  {done}/{len(jobs)}", flush=True)

    # ------------------------------------------------------------ decide and write
    before, after = [], []
    raised = cut = failed = ok_already = 0
    slot_db, bed_db = [], []
    touched_files = set()
    for path, i, name, pairs in book:
        whole, bed_rms = result.get(name, (None, None))
        if whole is None or bed_rms is None:
            failed += 1
            continue
        e_whole, e_bed = 10.0 ** (whole / 10.0), 10.0 ** (bed_rms / 10.0)
        e_voice = e_whole - e_bed
        if e_voice <= 0.0 or e_bed <= 0.0:
            # Either the bed is silent (nothing to fix) or the voice measured as nothing at all.
            ok_already += 1
            continue
        have = 10.0 * math.log10(e_voice / e_bed)
        before.append(have)
        need = a.target_db - have
        if need <= 0.25:
            ok_already += 1
            after.append(have)
            continue
        slots = active_slots(pairs)
        # Raise every slot by the same factor, so the mix between them is kept. The factor is
        # capped by whichever slot reaches 1.0 first.
        headroom = min(1.0 / max(l, 1e-6) for l in slots.values())
        want = 10.0 ** (need / 20.0)
        factor = min(want, headroom)
        if factor > 1.0001:
            for s, level in slots.items():
                put(pairs, SLOT_LEVEL[s], min(1.0, level * factor))
            slot_db.append(20.0 * math.log10(factor))
            raised += 1
        # Whatever the voice could not reach comes off the bed.
        rest = min(need - 20.0 * math.log10(factor), a.max_bed_cut)
        if rest > 0.25:
            scale = 10.0 ** (-rest / 20.0)
            moved = False
            for key in BED:
                v = get(pairs, key, BED_DEFAULT[key])
                if v <= BED_FLOOR:
                    continue
                put(pairs, key, max(BED_FLOOR, v * scale))
                moved = True
            if moved:
                bed_db.append(-rest)
                cut += 1
        after.append(have + 20.0 * math.log10(factor) + max(0.0, rest))
        fields = raw[path][i].split("|")
        fields[1] = write_settings(pairs)
        raw[path][i] = "|".join(fields)
        touched_files.add(path)

    if not a.dry_run:
        for path in touched_files:
            open(path, "w", encoding="utf-8", newline="\n").write("\n".join(raw[path]))

    def dist(v, label):
        if not v:
            return
        v = sorted(v)
        q = lambda p: v[int(p * (len(v) - 1))]
        print(f"  {label:32s} Median {statistics.median(v):+6.1f} dB   10% {q(.1):+6.1f}   90% {q(.9):+6.1f}"
              f"   unter 0 dB: {100 * sum(x < 0 for x in v) / len(v):3.0f} %")

    print(f"\nStimme gegen Rauschteppich (Ziel {a.target_db:+.0f} dB)")
    dist(before, "vorher")
    dist(after, "nachher (gerechnet)")
    print(f"\n  schon in Balance   {ok_already}")
    print(f"  Stimme angehoben   {raised}" + (f", Median +{statistics.median(slot_db):.1f} dB" if slot_db else ""))
    print(f"  Teppich gesenkt    {cut}" + (f", Median {statistics.median(bed_db):.1f} dB" if bed_db else ""))
    print(f"  nicht messbar      {failed}")
    print("\n(Probelauf -- nichts geschrieben)" if a.dry_run
          else "\nDie Bibliothek muss neu vermessen werden: das aendert Lautheit und alle Deskriptoren.")


main()
