"""HarmonicGen: Tabellen fuer den Harmonic-Oszillator.

Der Oszillator, der in AmbientSynth "Harmonic" heisst (bis 09/2026 "Wavetable"), ist additiv: eine Tabelle
ist fuer ihn keine Folge von Wellenformen, sondern eine Folge von SPEKTREN. Die Engine liest eine
Tabellen-WAV (Frames zu 2048 Samples hintereinander), behaelt bis zu 64 Frames, nimmt aus jedem die
FFT-Bins 1..32 und normiert den Frame auf Energie 1 (Wavetable::analyse). Die Phase verwirft sie.

Darum schreibt dieser Generator keine Wellenformen, die er irgendwo gefunden hat, sondern Spektren:
jeder Frame ist eine Summe von Sinustoenen genau auf den Bins 1..32. Beim Einlesen kommt so bitgenau
heraus, was hier entworfen wurde -- und nichts wird mitgeschleppt, was die Engine ohnehin wegwirft.

Warum der Vorgaenger nicht trug. Tools/WavetableGen rechnete mit 48 Teiltoenen, von denen die Engine
32 liest, und mit Phasenstreuung, die im Spektralweg keine Wirkung hat; der groessere Teil der
Bibliothek war aus Aufnahmen geschnitten -- und ein Zyklus aus einer Aufnahme ist, wie
make_wavetables.py selbst sagt, "noise with a period". Nach Renes Einschaetzung klangen die Tabellen
duenn und nur hoch.

Wie dieser Generator stabil wird: nicht durch Hoffen, sondern durch Leitplanken, die jede Tabelle
passieren muss.

  Grundton     der erste Teilton traegt in JEDEM Frame wenigstens einen Mindestanteil der Energie;
               wo ein Rezept darunter bleibt, wird er genau so weit angehoben (dieselbe Rechnung wie
               Root in der Engine) -- das richtet sich direkt gegen "duenn und nur hoch"
  Schwerpunkt  der Leistungs-Schwerpunkt (in Teiltonnummern) bleibt im Bereich seiner Klangfamilie
  Glaette      kein Frame ist weiter als eine Schrittweite von seinem Nachbarn entfernt; ein Rezept,
               das schneller will, wird ueber die Frames geglaettet, bis es das nicht mehr tut
  Fuelle       genug Teiltoene ueber -40 dB (ausser in der Familie "pure", die das nicht will)
  Vielfalt     jede Tabelle muss sich hoerbar von allen bisherigen unterscheiden (gewichteter
               dB-Abstand, siehe DIST_MIN_DB); das alte Skript beschreibt sein Problem als
               "thirty-two neighbours of one theme"

Deterministisch: derselbe Seed schreibt dieselben Bytes. Neben jeder Tabelle liegt ein JSON mit
Familie, Seed, den gezogenen Rezeptwerten und den Messwerten.

    python harmonicgen.py generate --out DIR [--per-family 40] [--seed 1] [--families organ,choir]
    python harmonicgen.py selftest
    python harmonicgen.py measure <tabelle.wav | ordner> [...]
"""
import argparse
import collections
import json
import math
import os
import sys

import numpy as np

FRAMES = 64
PARTIALS = 32
FRAME_LEN = 2048
SR = 44100
VERSION = 2
H = np.arange(1, PARTIALS + 1, dtype=np.float64)

# Schroeder-Phasen: jede Tabelle wird mit denselben festen Phasen je Teilton gerendert. Die Betraege
# -- das Einzige, was der Harmonic-Oszillator liest -- bleiben unberuehrt, aber die Wellenform hat
# einen niedrigen Scheitelfaktor statt einer Nadel bei Phase 0, und weil die Phasen in allen Frames
# gleich sind, klickt die Tabelle auch in einem Zeitbereichs-Spieler nicht zwischen den Frames.
PHASES = np.pi * H * H / PARTIALS

# Doppel-Mass. Version 1 nahm den Kosinus der Betraege, und der wird vom Grundton beherrscht: ihm galten
# weiche Spektren mit Neigung 1,3 und 2,2 als dieselbe Tabelle, von 1000 Pads kamen zwei durch. Jetzt
# zaehlt der dB-Abstand ueber fuenf Frames, jeder Teilton gewichtet mit seinem Pegel ueber dem Boden --
# ein lauter Teilton, der sich wenig aendert, wiegt mehr als ein leiser, der sich viel aendert.
# Zum Einordnen, gegen einen Saegezahn gemessen: +-1,5 dB Zufall je Teilton 0,9 dB; Neigung 1,1 statt
# 1,0 2,2 dB; gerade Teiltoene -6 dB 4,2 dB; Tiefpass bei Teilton 16 5,5 dB; Neigung 1,3 6,7 dB.
SIG_FRAMES = (0, 16, 32, 48, 63)
SIG_FLOOR_DB = -60.0
DIST_MIN_DB = 4.0          # naeher an einer schon angenommenen Tabelle: Doppel

# Familien, deren Raum von Natur aus klein ist, bekommen nur einen Teil der Stueckzahl.
SHARE = {"pure": 0.5}


# ---------------------------------------------------------------------------- Verlaeufe

def through(points):
    """64 Werte, die durch die Stuetzstellen gehen, mit Kosinus-Uebergaengen dazwischen. Jeder
    Uebergang hat an seinen Enden die Steigung 0, also ist der ganze Verlauf glatt."""
    pts = np.asarray(points, dtype=np.float64)
    if pts.size == 1:
        return np.full(FRAMES, pts[0])
    pos = np.linspace(0.0, 1.0, FRAMES) * (pts.size - 1)
    i = np.minimum(pos.astype(int), pts.size - 2)
    w = 0.5 - 0.5 * np.cos(np.pi * (pos - i))
    return pts[i] * (1.0 - w) + pts[i + 1] * w


def curve(rng, lo, hi, keys=3):
    return through(rng.uniform(lo, hi, size=keys))


# ---------------------------------------------------------------------------- Bausteine
# Jeder Baustein liefert Faktoren der Form (Frames, Teiltoene); ein Rezept ist ihr Produkt.

def tilt(alpha):
    """Neigung: Betrag ~ h^-alpha. alpha 1 ist ein Saegezahn, 2 ein Dreieck-artig weiches Spektrum."""
    return H[None, :] ** (-np.asarray(alpha)[:, None])


def lowpass(hc, order=2.0):
    """Weiche Helligkeitsgrenze bei Teilton hc (Butterworth-artig)."""
    return 1.0 / np.sqrt(1.0 + (H[None, :] / np.asarray(hc)[:, None]) ** (2.0 * order))


def odd_even(even_gain):
    g = np.ones((FRAMES, PARTIALS))
    g[:, 1::2] = np.asarray(even_gain)[:, None]          # Index 1 ist Teilton 2
    return g


# Formanten der Vokale (Maennerstimme, Peterson & Barney 1952), F1..F3 in Hz.
VOWELS = {"a": (730.0, 1090.0, 2440.0), "e": (530.0, 1840.0, 2480.0), "i": (270.0, 2290.0, 3010.0),
          "o": (570.0, 840.0, 2410.0), "u": (300.0, 870.0, 2240.0)}


def formants(f_ref, centres, bw_oct, gains_db):
    """Glocken im Log-Frequenzraum auf dem Spektrum eines Tons der Hoehe f_ref. Eine Tabelle hat
    keine eigene Tonhoehe: die Formanten wandern mit der gespielten Note, wie bei jeder Wavetable,
    und f_ref sagt nur, bei welcher Note sie dort sitzen, wo die Vokaltabelle sie hinlegt."""
    freq = H[None, :] * f_ref
    out = np.zeros((FRAMES, PARTIALS))
    for c, g in zip(centres, gains_db):
        d = np.log2(freq / np.asarray(c)[:, None]) / bw_oct
        out += np.asarray(g).reshape(-1, 1) * np.exp(-0.5 * d * d)
    return 10.0 ** (out / 20.0)


def comb(period, depth, phase):
    """Kerben im Abstand `period` Teiltoene; mit wanderndem `phase` ziehen sie durchs Spektrum."""
    return 1.0 - np.asarray(depth)[:, None] * (0.5 + 0.5 * np.cos(2.0 * np.pi * H[None, :] / np.asarray(period)[:, None]
                                                                  + np.asarray(phase)[:, None]))


DRAWBARS = [1, 2, 3, 4, 5, 6, 8]           # 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1'
MIXTURES = [10, 12, 16]


def organ(bars):
    """Zugriegel 0..8, je Stufe 3 dB; 0 ist stumm."""
    g = np.full((FRAMES, PARTIALS), 1.0e-5)
    for j, p in enumerate(DRAWBARS + MIXTURES):
        v = bars[:, j]
        g[:, p - 1] += np.where(v > 0.05, 10.0 ** ((v - 8.0) * 3.0 / 20.0), 0.0)
    return g


# Just-intonation-Akkorde als Teiltonnummern (4:5:6 ist Dur usw.); ihre Oktaven kommen dazu.
CHORDS = {"major": (4, 5, 6), "minor": (10, 12, 15), "sus2": (8, 9, 12), "sus4": (6, 8, 9),
          "open5": (2, 3), "maj7": (8, 10, 12, 15), "add9": (8, 10, 12, 18), "minor7": (10, 12, 15, 18)}


def chord_mask(ratios, body):
    m = np.full(PARTIALS, 1.0e-4)
    for r in ratios:
        p = r
        while p <= PARTIALS:
            m[p - 1] = 1.0 / (1.0 + 0.15 * math.log2(p / r)) if p != r else 1.0
            p *= 2
    m[0] = max(m[0], body)                                 # der eigene Grundton der Tabelle als Koerper
    return m


def walk(rng, depth_db, keys=5):
    """Langsame, glatte Bewegung je Teilton: fuer jeden Teilton eigene Stuetzstellen in +-depth_db."""
    pts = rng.uniform(-1.0, 1.0, size=(keys, PARTIALS)) * depth_db
    return 10.0 ** (np.stack([through(pts[:, h]) for h in range(PARTIALS)], axis=1) / 20.0)


# ---------------------------------------------------------------------------- Familien

def fam_organ(rng):
    n = len(DRAWBARS) + len(MIXTURES)
    reg = rng.uniform(0.0, 8.0, size=(int(rng.integers(2, 4)), n))
    reg[:, 0] = rng.uniform(5.0, 8.0, size=reg.shape[0])   # der 8' traegt immer
    reg[:, len(DRAWBARS):] *= rng.uniform(0.0, 0.6)         # Mixturen zurueckhaltend
    bars = np.stack([through(reg[:, j]) for j in range(n)], axis=1)
    spec = organ(bars) * tilt(np.full(FRAMES, rng.uniform(0.0, 0.3)))
    return spec, dict(registrations=np.round(reg, 2).tolist())


def fam_choir(rng):
    seq = [str(v) for v in rng.choice(list(VOWELS), size=int(rng.integers(2, 5)))]
    f_ref = float(rng.uniform(98.0, 175.0))
    cent = [through([VOWELS[v][j] for v in seq]) for j in range(3)]
    gains = [float(rng.uniform(9, 15)), float(rng.uniform(6, 12)), float(rng.uniform(3, 9))]
    bw = float(rng.uniform(0.25, 0.45))
    spec = (tilt(curve(rng, 1.1, 1.6)) * lowpass(curve(rng, 14.0, 30.0))
            * formants(f_ref, cent, bw, gains) * walk(rng, 1.5))
    return spec, dict(vowels=seq, f_ref=round(f_ref, 1), gains_db=np.round(gains, 1).tolist(), bw_oct=round(bw, 2))


def fam_strings(rng):
    f_ref = float(rng.uniform(110.0, 220.0))
    body = [np.full(FRAMES, rng.uniform(250, 450)), np.full(FRAMES, rng.uniform(1800, 3200))]
    spec = (tilt(curve(rng, 0.85, 1.25)) * lowpass(curve(rng, 5.0, 28.0, keys=4))
            * odd_even(curve(rng, 0.7, 1.0)) * formants(f_ref, body, 0.6, [4.0, 3.0]) * walk(rng, 2.5))
    return spec, dict(f_ref=round(f_ref, 1))


def fam_reed(rng):
    f_ref = float(rng.uniform(110.0, 260.0))
    nasal = [curve(rng, 1000.0, 1800.0, keys=2)]
    spec = (tilt(curve(rng, 0.8, 1.2)) * odd_even(curve(rng, 0.05, 0.4)) * lowpass(curve(rng, 8.0, 30.0))
            * formants(f_ref, nasal, 0.5, [6.0]) * walk(rng, 1.5))
    return spec, dict(f_ref=round(f_ref, 1))


def _sparse_mask(rng):
    pool = np.arange(2, PARTIALS + 1)
    p = pool ** 0.6
    k = int(rng.integers(5, 10))
    chosen = rng.choice(pool, size=k, replace=False, p=p / p.sum())
    m = np.full(PARTIALS, 1.0e-4)
    m[0] = 1.0
    m[chosen - 1] = rng.uniform(0.4, 1.0, size=k)
    return m, sorted(int(c) for c in chosen)


def fam_glass(rng):
    (a, ca), (b, cb) = _sparse_mask(rng), _sparse_mask(rng)
    x = curve(rng, 0.0, 1.0, keys=3)
    mask = a[None, :] * (1.0 - x[:, None]) + b[None, :] * x[:, None]
    spec = mask * tilt(curve(rng, 0.2, 0.6)) * lowpass(curve(rng, 18.0, 32.0))
    return spec, dict(set_a=ca, set_b=cb)


def fam_pad(rng):
    spec = (tilt(curve(rng, 1.3, 2.2)) * lowpass(curve(rng, 2.5, 12.0, keys=4))
            * comb(curve(rng, 3.0, 7.0), curve(rng, 0.0, 0.4), curve(rng, 0.0, 2.0 * np.pi)))
    return spec, {}


def fam_shimmer(rng):
    turns = float(rng.uniform(0.5, 3.0))
    phase = through([0.0, turns * np.pi, turns * 2.0 * np.pi])
    spec = (tilt(curve(rng, 0.8, 1.3)) * comb(curve(rng, 2.5, 6.0), curve(rng, 0.4, 0.85), phase)
            * lowpass(curve(rng, 12.0, 32.0)))
    return spec, dict(turns=round(turns, 2))


def fam_chord(rng):
    names = [str(c) for c in rng.choice(list(CHORDS), size=int(rng.integers(2, 4)), replace=True)]
    body = float(rng.uniform(0.3, 0.7))
    masks = np.stack([chord_mask(CHORDS[c], body) for c in names])
    x = np.linspace(0.0, 1.0, FRAMES) * (len(names) - 1)
    i = np.minimum(x.astype(int), len(names) - 2)
    w = 0.5 - 0.5 * np.cos(np.pi * (x - i))
    mask = masks[i] * (1.0 - w)[:, None] + masks[i + 1] * w[:, None]
    spec = mask * tilt(curve(rng, 0.3, 0.9)) * lowpass(curve(rng, 14.0, 32.0))
    return spec, dict(chords=names, body=round(body, 2))


def fam_pure(rng):
    n = int(rng.integers(2, 9))
    band = (H <= n).astype(float) + 1.0e-4
    spec = tilt(curve(rng, 1.5, 3.0)) * band[None, :] * walk(rng, 3.0)
    return spec, dict(partials=n)


FAMILIES = {"organ": fam_organ, "choir": fam_choir, "strings": fam_strings, "reed": fam_reed,
            "glass": fam_glass, "pad": fam_pad, "shimmer": fam_shimmer, "chord": fam_chord, "pure": fam_pure}


def fam_breath(rng):
    """Zwei andere Familien, langsam ineinander uebergeblendet."""
    a, b = rng.choice([k for k in FAMILIES], size=2, replace=False)
    sa, pa = FAMILIES[str(a)](rng)
    sb, pb = FAMILIES[str(b)](rng)
    sa, sb = normalise_frames(sa), normalise_frames(sb)
    x = curve(rng, 0.0, 1.0, keys=4)
    return sa * (1.0 - x[:, None]) + sb * x[:, None], dict(blend=[str(a), str(b)], a=pa, b=pb)


FAMILIES["breath"] = fam_breath

# Leitplanken je Familie: kleinster Energieanteil des ersten Teiltons je Frame, erlaubter Bereich des
# Leistungs-Schwerpunkts (Median ueber die Frames, in Teiltonnummern), kleinste Zahl hoerbarer Teiltoene.
LIMITS = {
    "organ":   dict(f1_min=0.10, centroid=(1.0, 8.0),  active_min=3),
    "choir":   dict(f1_min=0.08, centroid=(1.5, 12.0), active_min=8),
    "strings": dict(f1_min=0.08, centroid=(1.5, 10.0), active_min=8),
    "reed":    dict(f1_min=0.08, centroid=(1.5, 10.0), active_min=6),
    "glass":   dict(f1_min=0.08, centroid=(1.5, 16.0), active_min=4),
    "pad":     dict(f1_min=0.20, centroid=(1.0, 5.0),  active_min=3),
    "shimmer": dict(f1_min=0.08, centroid=(1.5, 12.0), active_min=8),
    "chord":   dict(f1_min=0.08, centroid=(2.0, 14.0), active_min=4),
    "pure":    dict(f1_min=0.30, centroid=(1.0, 3.5),  active_min=1),
    "breath":  dict(f1_min=0.10, centroid=(1.0, 12.0), active_min=3),
}
STEP_MAX = 0.12            # groesster Abstand zweier benachbarter Frames (Einheitsvektoren, 0..sqrt 2)


# ---------------------------------------------------------------------------- Leitplanken

def normalise_frames(s):
    s = np.maximum(np.asarray(s, dtype=np.float64), 0.0)
    n = np.sqrt((s * s).sum(axis=1, keepdims=True))
    return s / np.maximum(n, 1.0e-12)


def lift_fundamental(s, f1_min):
    """Den ersten Teilton genau so weit anheben, dass er f1_min der Energie traegt. Dieselbe Rechnung
    wie Root in der Engine: nach der Normierung ist die Summe 1, der Rest ist 1 - a1^2, und gesucht ist
    a1'^2 / (a1'^2 + Rest) = f1_min."""
    s = normalise_frames(s)
    share = s[:, 0] ** 2
    need = share < f1_min
    if need.any():
        rest = 1.0 - share[need]
        s[need, 0] = np.sqrt(f1_min * rest / (1.0 - f1_min))
        s = normalise_frames(s)
    return s, int(need.sum())


def steps(s):
    return np.sqrt(((s[1:] - s[:-1]) ** 2).sum(axis=1))


def smooth_until(s, step_max, f1_min, passes=40):
    """Ueber die Frames glaetten (1-2-1), bis kein Schritt mehr zu gross ist. Die Glaettung kann den
    Grundton-Anteil senken, also wird er nach jedem Durchgang wieder gesichert."""
    n = 0
    while n < passes and steps(s).max() > step_max:
        padded = np.vstack([s[:1], s, s[-1:]])
        s = normalise_frames(0.25 * padded[:-2] + 0.5 * padded[1:-1] + 0.25 * padded[2:])
        s, _ = lift_fundamental(s, f1_min)
        n += 1
    return s, n


def metrics(s):
    p = s * s
    tot = p.sum(axis=1)
    f1 = p[:, 0] / tot
    cen = (p * H[None, :]).sum(axis=1) / tot
    st = steps(s)
    active = (s > s.max(axis=1, keepdims=True) * 10.0 ** (-40.0 / 20.0)).sum(axis=1)
    return dict(f1_min=float(f1.min()), f1_median=float(np.median(f1)),
                centroid_median=float(np.median(cen)), centroid_min=float(cen.min()), centroid_max=float(cen.max()),
                step_max=float(st.max()) if st.size else 0.0, step_median=float(np.median(st)) if st.size else 0.0,
                active_min=int(active.min()), active_median=float(np.median(active)))


def check(m, lim):
    if m["f1_min"] < lim["f1_min"] - 1.0e-6:
        return "Grundton"
    lo, hi = lim["centroid"]
    if not lo <= m["centroid_median"] <= hi:
        return "Schwerpunkt"
    if m["step_max"] > STEP_MAX + 1.0e-6:
        return "Sprung"
    if m["active_min"] < lim["active_min"]:
        return "zu wenig Teiltoene"
    return None


def signature(s):
    """dB-Spektren von fuenf Frames, je Frame auf seinen lautesten Teilton bezogen, unten am Boden gekappt."""
    rows = s[list(SIG_FRAMES)]
    db = 20.0 * np.log10(np.maximum(rows, 1.0e-12))
    db -= db.max(axis=1, keepdims=True)
    return np.maximum(db, SIG_FLOOR_DB)


def distance(sig, bank):
    """Gewichteter dB-Abstand von `sig` zu jeder Signatur im Stapel `bank` (Anzahl, Frames, Teiltoene)."""
    bank = np.asarray(bank)
    if bank.ndim == 2:
        bank = bank[None]
    w = np.maximum(bank, sig[None]) - SIG_FLOOR_DB
    d = bank - sig[None]
    return np.sqrt((w * d * d).sum(axis=(1, 2)) / np.maximum(w.sum(axis=(1, 2)), 1.0e-12))


def build(family, seed):
    """Ein Rezept ziehen und durch die Leitplanken fuehren. Rueckgabe (Spektren, Rezeptwerte, Korrekturen)."""
    rng = np.random.default_rng(seed)
    raw, params = FAMILIES[family](rng)
    lim = LIMITS[family]
    s, lifted = lift_fundamental(raw, lim["f1_min"])
    s, passes = smooth_until(s, STEP_MAX, lim["f1_min"])
    return s, params, dict(lifted_frames=lifted, smoothing_passes=passes)


# ---------------------------------------------------------------------------- Rendern und Einlesen

def render(s):
    """Spektren -> Frames zu 2048 Samples: Summe der Sinustoene genau auf den Bins 1..32 mit festen
    Schroeder-Phasen, gemeinsam auf -1 dBFS normiert (die Engine normiert je Frame ohnehin selbst)."""
    n = np.arange(FRAME_LEN)
    basis = np.sin(2.0 * np.pi * np.outer(H, n) / FRAME_LEN + PHASES[:, None])
    frames = s @ basis
    peak = float(np.abs(frames).max())
    return (frames * (10.0 ** (-1.0 / 20.0) / max(peak, 1.0e-12))).astype(np.float32)


def analyse(frames):
    """Nachbau von Wavetable::analyse: FFT je Frame, Betraege der Bins 1..32, Frame auf Energie 1."""
    X = np.fft.rfft(np.asarray(frames, dtype=np.float64), axis=1)
    return normalise_frames(np.abs(X[:, 1:PARTIALS + 1]))


def write_table(path, frames, meta):
    import soundfile as sf
    tmp = path + ".tmp.wav"
    sf.write(tmp, frames.reshape(-1), SR, subtype="FLOAT")
    os.replace(tmp, path)
    with open(os.path.splitext(path)[0] + ".json", "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)


def read_table(path, frame_len=FRAME_LEN):
    import soundfile as sf
    x, _ = sf.read(path, dtype="float32", always_2d=True)
    mono = x.mean(axis=1)
    k = len(mono) // frame_len
    if k == 0:
        return None
    frames = mono[:k * frame_len].reshape(k, frame_len)
    if k > FRAMES:                                        # wie die Engine: gleichmaessig ausduennen
        idx = [int(i * (k - 1) / (FRAMES - 1)) for i in range(FRAMES)]
        frames = frames[idx]
    return frames


# ---------------------------------------------------------------------------- Befehle

def generate(out, per_family, seed, families):
    os.makedirs(out, exist_ok=True)
    master = np.random.default_rng(seed)
    bank = np.empty((0, len(SIG_FRAMES), PARTIALS))
    report = collections.defaultdict(collections.Counter)
    for fam in families:
        want = max(1, int(round(per_family * SHARE.get(fam, 1.0))))
        made = tries = 0
        while made < want and tries < want * 25:
            tries += 1
            tseed = int(master.integers(0, 2 ** 31 - 1))
            s, params, fixes = build(fam, tseed)
            m = metrics(s)
            why = check(m, LIMITS[fam])
            if why:
                report[fam][why] += 1
                continue
            sig = signature(s)
            if len(bank) and float(distance(sig, bank).min()) < DIST_MIN_DB:
                report[fam]["zu aehnlich"] += 1
                continue
            frames = render(s)
            err = float(np.abs(analyse(frames) - s).max())
            if err > 1.0e-4:                              # darf nie passieren: dann rechnet render() falsch
                raise RuntimeError(f"Rundlauf verletzt: {fam} Seed {tseed}, Abweichung {err:.2e}")
            name = f"harmonic_{fam}_{made:03d}.wav"
            write_table(os.path.join(out, name), frames, dict(
                generator="harmonicgen", version=VERSION, family=fam, seed=tseed, frames=FRAMES,
                frame_len=FRAME_LEN, partials=PARTIALS, recipe=params, fixes=fixes,
                metrics={k: (round(v, 4) if isinstance(v, float) else v) for k, v in m.items()},
                roundtrip_max_error=err))
            bank = np.concatenate([bank, sig[None]])
            made += 1
            report[fam]["angenommen"] += 1
        print(f"  {fam:8s} {made:3d} Tabellen aus {tries:4d} Versuchen  " +
              ", ".join(f"{k} {v}" for k, v in report[fam].most_common() if k != "angenommen"), flush=True)
    return report


def at_positions(s, count=FRAMES):
    """Spektren an `count` gleichmaessig verteilten Positionen, linear zwischen den Frames wie spectrumAt.
    Eine Tabelle mit 32 Frames springt je Frame doppelt so weit wie eine mit 64 bei gleicher Bewegung;
    verglichen wird, was die Engine beim Durchfahren der Position hoert."""
    k = len(s)
    if k == count:
        return s
    if k == 1:
        return np.repeat(s, count, axis=0)
    x = np.linspace(0.0, k - 1.0, count)
    i = np.minimum(x.astype(int), k - 2)
    f = (x - i)[:, None]
    return s[i] * (1.0 - f) + s[i + 1] * f


def measure(paths):
    files = []
    for p in paths:
        if os.path.isdir(p):
            files += sorted(os.path.join(p, f) for f in os.listdir(p) if f.lower().endswith((".wav", ".flac")))
        else:
            files.append(p)
    rows, sigs = [], []
    for f in files:
        try:
            fr = read_table(f)
        except Exception:
            continue
        if fr is None:
            continue
        s = at_positions(analyse(fr))
        rows.append(metrics(s))
        sigs.append(signature(s))
    if not rows:
        print("keine lesbaren Tabellen")
        return
    def pct(key, qs=(10, 50, 90)):
        v = np.array([r[key] for r in rows], dtype=float)
        return " / ".join(f"{np.percentile(v, q):.2f}" for q in qs)
    print(f"{len(rows)} Tabellen (Werte: 10. / 50. / 90. Perzentil; Frames auf {FRAMES} Positionen gebracht)")
    print(f"  Grundton-Anteil, schwaechster Frame: {pct('f1_min')}")
    print(f"  Grundton-Anteil, Median der Frames:  {pct('f1_median')}")
    print(f"  Schwerpunkt (Teiltonnummer), Median: {pct('centroid_median')}")
    print(f"  groesster Schritt zwischen Frames:   {pct('step_max')}")
    print(f"  hoerbare Teiltoene, schwaechster Frame: {pct('active_min')}")
    weak = sum(1 for r in rows if r["f1_min"] < 0.08)
    print(f"  Tabellen mit einem Frame unter 8 % Grundton: {weak} ({100 * weak / len(rows):.0f} %)")
    jumpy = sum(1 for r in rows if r["step_max"] > STEP_MAX + 1.0e-6)
    print(f"  Tabellen mit einem Schritt ueber {STEP_MAX}: {jumpy} ({100 * jumpy / len(rows):.0f} %)")
    if len(sigs) > 1:
        bank = np.stack(sigs)
        nn = np.array([np.partition(distance(bank[i], bank), 1)[1] for i in range(len(bank))])
        dup = int((nn < DIST_MIN_DB).sum())
        print(f"  naechster Nachbar, dB-Abstand:       {' / '.join(f'{v:.2f}' for v in np.percentile(nn, (10, 50, 90)))}")
        print(f"  Tabellen mit einem Doppel unter {DIST_MIN_DB:.0f} dB: {dup} ({100 * dup / len(rows):.0f} %)")


def selftest():
    fails = 0

    def ok(label, cond):
        nonlocal fails
        fails += not cond
        print(f"  {'ok  ' if cond else 'FEHL'} {label}")

    rng = np.random.default_rng(3)
    s = normalise_frames(rng.uniform(0.0, 1.0, size=(FRAMES, PARTIALS)))
    err = float(np.abs(analyse(render(s)) - s).max())
    ok(f"Rundlauf Zufallsspektren: Abweichung {err:.1e}", err < 1.0e-5)

    saw = normalise_frames(np.tile(1.0 / H, (FRAMES, 1)))
    ok("Saegezahn 1/h kommt als 1/h zurueck", float(np.abs(analyse(render(saw)) - saw).max()) < 1.0e-5)

    only6 = np.zeros((FRAMES, PARTIALS))
    only6[:, 5] = 1.0
    lifted, n = lift_fundamental(only6, 0.2)
    ok(f"Grundton-Anhebung: nur Teilton 6 -> Anteil {lifted[0, 0] ** 2:.3f} (Soll 0,200), {n} Frames",
       abs(lifted[0, 0] ** 2 - 0.2) < 1.0e-9 and n == FRAMES)

    a = np.zeros(PARTIALS); a[0] = 1.0; a[1] = 0.5
    b = np.zeros(PARTIALS); b[0] = 0.5; b[9] = 1.0
    jump = normalise_frames(np.vstack([np.tile(a, (32, 1)), np.tile(b, (32, 1))]))
    before = float(steps(jump).max())
    smoothed, passes = smooth_until(jump, STEP_MAX, 0.1)
    ok(f"Sprung {before:.2f} geglaettet auf {steps(smoothed).max():.3f} in {passes} Durchgaengen", steps(smoothed).max() <= STEP_MAX)

    s1, _, _ = build("choir", 42)
    s2, _, _ = build("choir", 42)
    ok("gleicher Seed, gleiche Tabelle", np.array_equal(render(s1), render(s2)))
    ok("gleicher Seed wird als Doppel erkannt", float(distance(signature(s1), signature(s2))[0]) < DIST_MIN_DB)
    jitter = normalise_frames(saw * 10.0 ** (rng.uniform(-1.5, 1.5, size=(1, PARTIALS)) / 20.0))
    darker = normalise_frames(np.tile(H ** -1.3, (FRAMES, 1)))
    square = normalise_frames(np.tile(np.where(H % 2 == 1, 1.0 / H, 1.0e-4), (FRAMES, 1)))
    d_jit = float(distance(signature(saw), signature(jitter))[0])
    d_dark = float(distance(signature(saw), signature(darker))[0])
    d_sq = float(distance(signature(saw), signature(square))[0])
    ok(f"Doppel-Mass: Saege +-1,5 dB Zufall {d_jit:.1f} dB ist Doppel, Neigung 1,3 {d_dark:.1f} dB und "
       f"Rechteck {d_sq:.1f} dB nicht", d_jit < DIST_MIN_DB <= d_dark < d_sq)
    ramp = normalise_frames(np.linspace(0.1, 1.0, 32)[:, None] * np.eye(PARTIALS)[0] + np.eye(PARTIALS)[1])
    ok("32 Frames auf 64 Positionen: Enden bleiben, Mitte liegt dazwischen",
       np.allclose(at_positions(ramp)[0], ramp[0]) and np.allclose(at_positions(ramp)[-1], ramp[-1]))

    for fam in FAMILIES:
        good = 0
        whys = collections.Counter()
        for sd in range(12):
            s, _, _ = build(fam, 1000 + sd)
            why = check(metrics(s), LIMITS[fam])
            good += why is None
            whys[why] += why is not None
        ok(f"Familie {fam:8s}: {good}/12 Rezepte bestehen" + (f"  ({dict(whys)})" if whys else ""), good >= 6)

    print(f"\n  {'alle bestanden' if not fails else f'{fails} fehlgeschlagen'}")
    return fails


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate")
    g.add_argument("--out", required=True)
    g.add_argument("--per-family", type=int, default=40)
    g.add_argument("--seed", type=int, default=1)
    g.add_argument("--families", default=",".join(FAMILIES))
    sub.add_parser("selftest")
    m = sub.add_parser("measure")
    m.add_argument("paths", nargs="+")
    a = ap.parse_args()
    if a.cmd == "generate":
        generate(a.out, a.per_family, a.seed, [f.strip() for f in a.families.split(",") if f.strip()])
    elif a.cmd == "selftest":
        sys.exit(1 if selftest() else 0)
    else:
        measure(a.paths)


if __name__ == "__main__":
    main()
