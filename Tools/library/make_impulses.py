"""Two hundred impulse responses for the convolution Room, aimed at drones rather than at
reproducing a concert hall.

`Tools/ImpulseGen` designs one room at a time; this builds a library out of eight families, of
which only the first is an ordinary room. The rest exist because a convolution reverb fed a
drone is not really a room simulator -- it is a resonator you can shape:

    room        designed halls, chambers, caverns, plates: per-band decay, size, colour
    tuned       a bank of decaying partials on a just-intoned chord: the reverb rings in key
    modal       inharmonic modes (bells, plates, springs): metal instead of air
    reverse     the decay run backwards, so every note swells into its own reflection
    comb        regularly spaced taps: corridors, pipes, wells, flutter
    scatter     sparse random taps with a thinning density: rain rooms, shattering
    shimmer     the tail mixed with an octave-up copy of itself
    spectral    narrow bands that decay at different rates, so the room changes colour as it dies

    python Tools/library/make_impulses.py --out-dir Library/Impulses

Deterministic: the same seed writes the same 200 files with the same names, which is what lets a
preset name one before it has been rendered. Roughly a minute, CPU only, about 400 MB.

Everything is checked before it is written: an impulse must decay, must not be silent, must not
carry a DC offset, and must not start with a click that the Room would stamp on every note.
"""
import argparse
import math
import os
import random
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "ImpulseGen"))
from impulsegen_core import band_split, decay_env, normalise, procedural, save  # noqa: E402

SR = 48000
# The desktop Room holds eight seconds, the Quest four (Engine::setRoomMaxSeconds).
MAX_SECONDS = 8.0


# ---------------------------------------------------------------- helpers

def stereo_noise(rng, n):
    return np.stack([rng.standard_normal(n), rng.standard_normal(n)])


def fade_in(ir, sr, ms=1.5):
    """Every impulse starts from silence: a hard first sample is a click on every single note."""
    k = max(2, int(sr * ms / 1000.0))
    ir[:, :k] *= np.linspace(0.0, 1.0, k) ** 2
    return ir


def fade_out(ir, sr, ms=40.0):
    k = max(2, int(sr * ms / 1000.0))
    ir[:, -k:] *= np.linspace(1.0, 0.0, k) ** 2
    return ir


def dc_block(ir, sr, hz=25.0):
    """y[n] = x[n] - x[n-1] + a*y[n-1]. Through scipy, not a Python loop: two hundred impulses of
    eight seconds is 150 million samples."""
    from scipy.signal import lfilter
    a = math.exp(-2.0 * math.pi * hz / sr)
    return lfilter([1.0, -1.0], [1.0, -a], ir, axis=-1)


def finish(ir, sr):
    ir = np.asarray(ir, dtype=np.float64)
    if ir.ndim == 1:
        ir = np.stack([ir, ir])
    ir = np.nan_to_num(ir)
    ir = fade_out(fade_in(ir, sr), sr)
    return normalise(ir)


# ---------------------------------------------------------------- families

JI = [1.0, 9 / 8, 6 / 5, 5 / 4, 4 / 3, 3 / 2, 8 / 5, 5 / 3, 7 / 4, 2.0, 9 / 4, 5 / 2, 3.0, 4.0]


def make_tuned(rng, seconds, root_hz, ratios, rt60, spread_cents, bright):
    """A bank of decaying partials: the Room becomes a resonator that rings in key. Fed a drone
    this is the difference between a space and an instrument."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    out = np.zeros((2, n))
    for c in range(2):
        for k, r in enumerate(ratios):
            f = root_hz * r * (2.0 ** (rng.uniform(-spread_cents, spread_cents) / 1200.0))
            if f > 0.45 * SR:
                continue
            # higher partials die sooner unless the room is bright
            rt = rt60 * (1.0 / (1.0 + (1.0 - bright) * 0.7 * k))
            amp = (1.0 / (1.0 + k)) ** (1.2 - 0.7 * bright)
            out[c] += amp * np.sin(2 * math.pi * f * t + rng.uniform(0, 6.283)) * np.exp(-6.9078 * t / max(rt, 0.05))
    # a breath of noise so it is a room and not a bank of oscillators
    noise = stereo_noise(rng, n) * decay_env(n, SR, rt60 * 0.35)
    return out + 0.18 * noise


BELL = [1.0, 2.76, 5.40, 8.93, 13.34, 18.64]
PLATE = [1.0, 2.00, 3.01, 3.44, 4.03, 5.65, 6.98, 8.71]
SPRING = [1.0, 1.59, 2.14, 2.30, 2.65, 3.16, 3.50, 4.06, 4.60]


def make_modal(rng, seconds, root_hz, modes, rt60, damp):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    out = np.zeros((2, n))
    for c in range(2):
        for k, r in enumerate(modes):
            f = root_hz * r * (1.0 + rng.uniform(-0.004, 0.004))
            if f > 0.45 * SR:
                continue
            rt = rt60 / (1.0 + damp * k * 0.6)
            out[c] += (0.9 ** k) * np.sin(2 * math.pi * f * t + rng.uniform(0, 6.283)) * np.exp(-6.9078 * t / max(rt, 0.03))
    return out


def make_reverse(rng, seconds, rt60, tone):
    """A decay run backwards: every note swells into its own reflection before it arrives."""
    ir = procedural(sr=SR, seconds=seconds, rt60_low=rt60 * 1.3, rt60_mid=rt60, rt60_high=rt60 * 0.6,
                    size=rng.uniform(0.5, 2.0), tone=tone, early_level=0.0,
                    diffusion_ms=rng.uniform(10.0, 60.0), seed=rng.integers(1, 10 ** 6))
    return np.ascontiguousarray(np.asarray(ir, dtype=np.float64)[:, ::-1])


def make_comb(rng, seconds, period_ms, feedback, tone, stereo_offset_ms):
    """Regularly spaced taps: a corridor, a pipe, a well. The period is what you hear as a pitch
    when it is short and as a flutter when it is long."""
    n = int(seconds * SR)
    out = np.zeros((2, n))
    for c in range(2):
        period = (period_ms + (stereo_offset_ms if c else 0.0)) * SR / 1000.0
        amp, at, k = 1.0, period, 0
        while at < n - 4 and amp > 0.0008 and k < 4000:
            i = int(at)
            frac = at - i
            burst = max(2, int(SR * 0.0008 * (1.0 + 2.0 * tone)))
            env = np.hanning(burst * 2)[burst:]
            seg = out[c, i:i + burst]
            m = min(burst, seg.size)
            sign = 1.0 if k % 2 == 0 else -1.0
            out[c, i:i + m] += sign * amp * (1.0 - frac * 0.2) * env[:m] * rng.uniform(0.85, 1.0)
            amp *= feedback
            at += period * (1.0 + rng.uniform(-0.01, 0.01))
            k += 1
    n2 = out.shape[1]
    return out * decay_env(n2, SR, seconds * 0.9)


def make_scatter(rng, seconds, density, thinning, tone):
    """Sparse taps whose rate falls off: rain in a room, glass shattering, a slow collapse."""
    n = int(seconds * SR)
    out = np.zeros((2, n))
    t = 0.0
    while t < seconds:
        rate = density * math.exp(-thinning * t)
        if rate < 0.5:
            break
        t += rng.exponential(1.0 / rate)
        i = int(t * SR)
        if i >= n - 8:
            break
        burst = max(3, int(SR * rng.uniform(0.0004, 0.004) * (1.0 + tone)))
        env = np.hanning(burst * 2)[burst:]
        grain = rng.standard_normal(burst) * env
        pan = rng.uniform(0.0, 1.0)
        m = min(burst, n - i)
        out[0, i:i + m] += grain[:m] * math.cos(pan * math.pi / 2) * math.exp(-1.5 * t / seconds)
        out[1, i:i + m] += grain[:m] * math.sin(pan * math.pi / 2) * math.exp(-1.5 * t / seconds)
    return out


def make_shimmer(rng, seconds, rt60, tone, amount):
    """The tail plus a copy of itself an octave up: the reverb rises as it decays."""
    base = np.asarray(procedural(sr=SR, seconds=seconds, rt60_low=rt60, rt60_mid=rt60 * 0.8,
                                 rt60_high=rt60 * 0.5, size=rng.uniform(0.8, 2.5), tone=tone,
                                 seed=rng.integers(1, 10 ** 6)), dtype=np.float64)
    n = base.shape[1]
    up = np.zeros_like(base)
    idx = np.minimum(np.arange(n) * 2, n - 1)          # read twice as fast = an octave up
    for c in range(2):
        up[c, : n // 2] = base[c][idx][: n // 2]
    # the octave enters late, so the swell is heard rather than the transient
    ramp = np.clip(np.linspace(-0.15, 1.0, n), 0.0, 1.0) ** 1.5
    return base + amount * up * ramp


def make_spectral(rng, seconds, bands, spread, tone):
    """Narrow bands with different decay times: the room changes colour while it dies."""
    n = int(seconds * SR)
    out = np.zeros((2, n))
    freqs = np.exp(np.linspace(math.log(70.0), math.log(9000.0), bands))
    from scipy.signal import lfilter
    for c in range(2):
        noise = rng.standard_normal(n)
        for k, f in enumerate(freqs):
            q = rng.uniform(6.0, 22.0)
            w = 2 * math.pi * f / SR
            r = math.exp(-w / (2 * q))
            b0 = 1.0 - r
            y = lfilter([b0, 0.0, -b0], [1.0, -2.0 * r * math.cos(w), r * r], noise)
            rt = seconds * (0.25 + 0.75 * (1.0 - k / max(1, bands - 1)) ** (1.0 + spread)) * (1.0 + 0.3 * tone)
            out[c] += y * decay_env(n, SR, max(0.15, rt))
    return out


def make_diffusion(rng, seconds, density, tone, decay):
    """Velvet noise: taps at random times, all the same size, sign chosen at random. It is what a
    diffusion network converges to, and it is the smoothest tail there is -- no comb colour, no
    grain, just air. The designed rooms cannot do this: their early pattern always leaves a
    signature."""
    n = int(seconds * SR)
    out = np.zeros((2, n))
    for c in range(2):
        step = SR / max(density, 20.0)
        at = rng.uniform(0.0, step)
        while at < n - 2:
            i = int(at + rng.uniform(0.0, step))
            if i >= n:
                break
            out[c, i] += 1.0 if rng.random() < 0.5 else -1.0
            at += step
    from scipy.signal import lfilter
    # One pole of colour, so the tail is not white: a room absorbs the top.
    a = 1.0 - math.exp(-2 * math.pi * (1200.0 * (1.0 + 2.0 * tone)) / SR)
    out = lfilter([a], [1.0, -(1.0 - a)], out, axis=-1)
    return out * decay_env(n, SR, decay)


def make_echoes(rng, seconds, taps, spread, tone):
    """A handful of separate reflections and almost nothing between them: a stone circle, a cliff
    face, a canyon. What makes it a place rather than a reverb is that you can count them."""
    n = int(seconds * SR)
    out = np.zeros((2, n))
    out[:, :3] += np.array([[1.0, 0.4, 0.15], [1.0, 0.4, 0.15]]) * 0.6
    for k in range(taps):
        t = (k + 1) * spread * rng.uniform(0.75, 1.3)
        i = int(t * SR)
        if i >= n - 64:
            break
        width = max(8, int(SR * 0.002 * (1.0 + 3.0 * tone) * (1.0 + k * 0.4)))
        env = np.hanning(width * 2)[width:]
        grain = rng.standard_normal(width) * env
        amp = 0.8 ** (k + 1)
        pan = rng.uniform(0.1, 0.9)
        m = min(width, n - i)
        out[0, i:i + m] += grain[:m] * amp * math.cos(pan * math.pi / 2)
        out[1, i:i + m] += grain[:m] * amp * math.sin(pan * math.pi / 2)
    return out * decay_env(n, SR, seconds * 0.8)


def make_tube(rng, seconds, hz, q, air):
    """One resonance and the air around it: a pipe, a ventilation shaft, a chimney, a bottle. The
    Room becomes a body with a pitch rather than a space with a size."""
    from scipy.signal import lfilter
    n = int(seconds * SR)
    x = stereo_noise(rng, n) * decay_env(n, SR, seconds * rng.uniform(0.3, 0.7))
    out = np.zeros((2, n))
    for k, ratio in enumerate((1.0, 3.0, 5.0, 7.0)):          # a stopped pipe: odd partials
        f = hz * ratio
        if f > SR * 0.45:
            break
        r = 1.0 - math.pi * (f / q) / SR
        theta = 2 * math.pi * f / SR
        b = [1.0 - r]
        a = [1.0, -2 * r * math.cos(theta), r * r]
        out += lfilter(b, a, x, axis=-1) * (0.7 ** k)
    return out + x * air


def make_underwater(rng, seconds, cutoff, decay):
    """Everything above a few hundred hertz gone, and a very long tail: under ice, inside a tank,
    a room heard through a wall. The one direction the designed halls never go, because a hall
    that dark would be a broken hall."""
    from scipy.signal import lfilter
    n = int(seconds * SR)
    x = stereo_noise(rng, n) * decay_env(n, SR, decay)
    a = 1.0 - math.exp(-2 * math.pi * cutoff / SR)
    for _ in range(4):                                        # four poles: really gone, not tilted
        x = lfilter([a], [1.0, -(1.0 - a)], x, axis=-1)
    # A slow swell in the tail, so it breathes rather than just fades.
    t = np.arange(n) / SR
    return x * (1.0 + 0.35 * np.sin(2 * math.pi * rng.uniform(0.05, 0.4) * t))


def make_chord_comb(rng, seconds, root_hz, ratios, feedback):
    """Several combs at once, tuned to a chord in just intonation. `tuned` rings on one note; this
    is a room that answers with a chord, whatever you play into it."""
    n = int(seconds * SR)
    out = np.zeros((2, n))
    for j, ratio in enumerate(ratios):
        period = SR / (root_hz * ratio)
        for c in range(2):
            amp, at, k = 1.0 / (1 + j), period * (1.0 + 0.002 * c), 0
            while at < n - 4 and amp > 0.001 and k < 3000:
                i = int(at)
                out[c, i] += amp * (1.0 if k % 2 == 0 else -1.0)
                amp *= feedback
                at += period
                k += 1
    return out * decay_env(n, SR, seconds * 0.85)


def make_sheet(rng, seconds, modes, rt60, spread):
    """A steel sheet, a waterphone, a saw blade: dozens of inharmonic modes, close together and
    decaying at different rates, so the metal changes colour while it rings."""
    from scipy.signal import lfilter
    n = int(seconds * SR)
    x = stereo_noise(rng, n) * decay_env(n, SR, 0.02)
    out = np.zeros((2, n))
    base = rng.uniform(60.0, 320.0)
    for k in range(modes):
        # Square-law spacing (a stiff plate) scattered a little: nothing lands on a harmonic.
        f = base * (1.0 + spread * (k ** 1.7) / modes) * rng.uniform(0.94, 1.06)
        if f > SR * 0.45:
            break
        rt = rt60 * rng.uniform(0.3, 1.4)
        r = 10.0 ** (-3.0 / (rt * SR))
        theta = 2 * math.pi * f / SR
        a = [1.0, -2 * r * math.cos(theta), r * r]
        gain = (1.0 / (1.0 + k * 0.25)) * rng.uniform(0.5, 1.0)
        out += lfilter([1.0 - r], a, x, axis=-1) * gain
    return out


# ---------------------------------------------------------------- the catalogue

def catalogue(rng):
    """(family, name, builder) for every impulse. Names are stable, so a preset can name one."""
    jobs = []

    # -- rooms: the ordinary end, but tuned dark and long the way this instrument wants
    room_kinds = [
        ("chamber",   (0.6, 1.6), (0.3, 0.8), (-0.4, 0.3), 1.2),
        ("hall",      (2.0, 5.0), (0.8, 1.8), (-0.5, 0.2), 3.0),
        ("cathedral", (5.0, 9.0), (1.5, 2.6), (-0.6, 0.1), 6.0),
        ("cavern",    (6.0, 12.0), (2.0, 3.0), (-0.9, -0.3), 7.0),
        ("plate",     (1.5, 4.0), (0.2, 0.6), (0.0, 0.6), 3.0),
        ("bunker",    (1.0, 3.0), (0.4, 1.0), (-0.9, -0.5), 2.5),
    ]
    for kind, rt, size, tone, secs in room_kinds:
        for i in range(9):
            def build(rng=rng, rt=rt, size=size, tone=tone, secs=secs):
                base = rng.uniform(*rt)
                return procedural(sr=SR, seconds=min(MAX_SECONDS, secs * rng.uniform(0.8, 1.2)),
                                  rt60_low=base * rng.uniform(1.0, 1.6),
                                  rt60_mid=base,
                                  rt60_high=base * rng.uniform(0.25, 0.7),
                                  size=rng.uniform(*size), tone=rng.uniform(*tone),
                                  diffusion_ms=rng.uniform(8.0, 70.0),
                                  predelay_ms=rng.uniform(0.0, 90.0),
                                  width=rng.uniform(0.7, 1.4),
                                  early_level=rng.uniform(0.1, 0.7),
                                  modulation=rng.uniform(0.0, 0.7),
                                  seed=int(rng.integers(1, 10 ** 6)))
            jobs.append(("room", f"room_{kind}_{i:02d}", build))

    # -- tuned resonators
    for i in range(30):
        def build(rng=rng):
            root = rng.choice([55.0, 65.4, 73.4, 82.4, 98.0, 110.0, 130.8, 146.8, 164.8, 196.0])
            take = int(rng.integers(3, 9))
            ratios = sorted(rng.choice(JI, size=take, replace=False).tolist())
            return make_tuned(rng, min(MAX_SECONDS, rng.uniform(2.5, 7.0)), float(root), ratios,
                              rt60=rng.uniform(1.5, 6.0), spread_cents=rng.uniform(0.0, 14.0),
                              bright=rng.uniform(0.0, 1.0))
        jobs.append(("tuned", f"tuned_{i:02d}", build))

    # -- modal metal
    for i in range(25):
        def build(rng=rng):
            modes = [BELL, PLATE, SPRING][i % 3]
            return make_modal(rng, min(MAX_SECONDS, rng.uniform(1.5, 6.0)),
                              float(rng.uniform(70.0, 320.0)), modes,
                              rt60=rng.uniform(1.0, 5.0), damp=rng.uniform(0.2, 1.4))
        name = ["bell", "plate", "spring"][i % 3]
        jobs.append(("modal", f"modal_{name}_{i:02d}", build))

    # -- reverse swells
    for i in range(20):
        def build(rng=rng):
            return make_reverse(rng, min(MAX_SECONDS, rng.uniform(1.2, 5.0)),
                                rt60=rng.uniform(0.8, 4.0), tone=rng.uniform(-0.7, 0.5))
        jobs.append(("reverse", f"reverse_{i:02d}", build))

    # -- combs, pipes, corridors
    for i in range(20):
        def build(rng=rng):
            return make_comb(rng, min(MAX_SECONDS, rng.uniform(1.5, 6.0)),
                             period_ms=float(math.exp(rng.uniform(math.log(2.0), math.log(180.0)))),
                             feedback=rng.uniform(0.80, 0.985), tone=rng.uniform(0.0, 1.0),
                             stereo_offset_ms=rng.uniform(0.0, 3.0))
        jobs.append(("comb", f"comb_{i:02d}", build))

    # -- scattered
    for i in range(20):
        def build(rng=rng):
            return make_scatter(rng, min(MAX_SECONDS, rng.uniform(2.0, 7.0)),
                                density=rng.uniform(30.0, 900.0), thinning=rng.uniform(0.05, 1.2),
                                tone=rng.uniform(0.0, 1.0))
        jobs.append(("scatter", f"scatter_{i:02d}", build))

    # -- shimmer
    for i in range(15):
        def build(rng=rng):
            return make_shimmer(rng, min(MAX_SECONDS, rng.uniform(3.0, 7.0)),
                                rt60=rng.uniform(2.0, 6.0), tone=rng.uniform(-0.5, 0.4),
                                amount=rng.uniform(0.3, 0.9))
        jobs.append(("shimmer", f"shimmer_{i:02d}", build))

    # -- spectral
    for i in range(16):
        def build(rng=rng):
            return make_spectral(rng, min(MAX_SECONDS, rng.uniform(2.0, 6.0)),
                                 bands=int(rng.integers(5, 13)), spread=rng.uniform(0.3, 2.5),
                                 tone=rng.uniform(-0.5, 0.8))
        jobs.append(("spectral", f"spectral_{i:02d}", build))

    # -- struck objects, cut from the field recordings (see above). Skipped silently when the
    # recordings have not been generated: everything else in this library must still build.
    # -- diffusion, echoes, tube, underwater, chord combs, sheets: six shapes the designed rooms
    # cannot make, because a room that dark or that empty would be a broken room.
    for i in range(12):
        def build(rng=rng):
            return make_diffusion(rng, min(MAX_SECONDS, rng.uniform(2.0, 7.0)),
                                  density=rng.uniform(400.0, 4000.0), tone=rng.uniform(-0.4, 0.9),
                                  decay=rng.uniform(1.5, 6.0))
        jobs.append(("diffusion", f"diffusion_{i:02d}", build))
    for i in range(12):
        def build(rng=rng):
            return make_echoes(rng, min(MAX_SECONDS, rng.uniform(2.0, 7.0)),
                               taps=int(rng.integers(3, 9)), spread=rng.uniform(0.06, 0.5),
                               tone=rng.uniform(0.0, 1.0))
        jobs.append(("echoes", f"echoes_{i:02d}", build))
    for i in range(12):
        def build(rng=rng):
            return make_tube(rng, min(MAX_SECONDS, rng.uniform(1.5, 5.0)),
                             hz=rng.uniform(45.0, 260.0), q=rng.uniform(12.0, 90.0),
                             air=rng.uniform(0.02, 0.25))
        jobs.append(("tube", f"tube_{i:02d}", build))
    for i in range(12):
        def build(rng=rng):
            return make_underwater(rng, min(MAX_SECONDS, rng.uniform(4.0, 8.0)),
                                   cutoff=rng.uniform(120.0, 700.0), decay=rng.uniform(3.0, 9.0))
        jobs.append(("underwater", f"underwater_{i:02d}", build))
    for i in range(12):
        def build(rng=rng):
            root = rng.uniform(45.0, 150.0)
            ratios = [[1, 1.5, 2], [1, 1.25, 1.5], [1, 1.2, 1.5, 1.8],
                      [1, 1.5, 2.25], [1, 1.75, 2.5], [1, 1.5, 1.75, 2]][int(rng.integers(0, 6))]
            return make_chord_comb(rng, min(MAX_SECONDS, rng.uniform(2.0, 6.0)),
                                   root, list(ratios), feedback=rng.uniform(0.93, 0.995))
        jobs.append(("chord", f"chord_{i:02d}", build))
    for i in range(12):
        def build(rng=rng):
            return make_sheet(rng, min(MAX_SECONDS, rng.uniform(2.0, 6.0)),
                              modes=int(rng.integers(14, 40)), rt60=rng.uniform(0.8, 4.0),
                              spread=rng.uniform(0.6, 3.0))
        jobs.append(("sheet", f"sheet_{i:02d}", build))

    # -- struck objects and places, cut from the clip library. Skipped silently when the clips
    # have not been generated: everything else here must still build.
    clips = [os.path.join(ROOT, "Library", "Textures"), os.path.join(ROOT, "Library", "FieldRecordings")]
    for i, f in enumerate(source_files(STRUCK_WORDS, clips, 160)):
        def build(rng=rng, f=f):
            return make_struck(rng, f, rng.uniform(0.12, 0.45))
        jobs.append(("struck", f"struck_{i:03d}", build))
    for i, f in enumerate(source_files(SPACE_WORDS, [clips[1], clips[0]], 120)):
        def build(rng=rng, f=f):
            return make_space(rng, f, rng.uniform(1.2, 4.5))
        jobs.append(("space", f"space_{i:03d}", build))

    return jobs


# ---------------------------------------------------------------- struck objects
#
# Convolutional cross-synthesis: the impulse is not a room at all but a fragment of a real
# recording -- the moment a chain lands on concrete, a pipe is knocked, a door closes. Convolved
# with a pad, the mathematics transfers the resonance of that object onto the synthetic waveform,
# and the drone is suddenly being played on something that exists. This is the one trick in the
# dark ambient literature that a synthesiser cannot fake with filters, because what it imprints
# is a whole measured resonance, poles, zeros and all.
#
# The material is the field recordings the library already ships. Only the categories that have
# events in them are used -- rain and wind are continuous, and a slice of them is a burst of
# noise, not an object.

STRUCK_CATEGORIES = ("industrial", "machines", "interior", "city", "cave", "forest", "water")


# What a struck object sounds like, and what a place sounds like: the two vocabularies the clip
# folders are searched with. The old rule wanted a "field_recordings_" prefix, which only the
# clips generated per style ever had -- everything made from the prompt lists was invisible to it,
# and the shelf quietly stopped growing.
STRUCK_WORDS = ("bell|gong|bowl|chime|anvil|cymbal|tam_tam|struck|strike|hammer|mallet|plate|"
                "metal|steel|copper|brass|iron|aluminium|rail|pipe|tube|rod|spring|glass|crystal|"
                "porcelain|ceramic|terracotta|stone|slate|marble|flint|wood|timber|log|block|"
                "drum|tabla|udu|ghatam|piano|harp|kalimba|mbira|tine|clang|knock|tap")
SPACE_WORDS = ("hall|cathedral|church|chapel|corridor|stairwell|tunnel|cave|cavern|cistern|"
               "warehouse|hangar|silo|bunker|basement|cellar|attic|room|shaft|pool|reservoir|"
               "quarry|mine|bridge|underpass|car_park|station|concourse|factory|boiler|"
               "rain|wind|storm|forest|river|stream|waves|surf|ocean|snow|ice|fog|"
               "machinery|ventilation|fan|engine|turbine|generator|compressor|transformer|"
               "traffic|motorway|railway|harbour|ferry|market|crowd|library|museum")


def source_files(words, folders, want):
    """Clips whose name says they are the right kind of material, in a stable order, spread over
    the whole shelf rather than taken from the front of the alphabet."""
    import re as _re
    pat = _re.compile(words, _re.I)
    out = []
    for d in folders:
        if not os.path.isdir(d):
            continue
        out += [os.path.join(d, f) for f in sorted(os.listdir(d))
                if f.endswith(".wav") and pat.search(f)]
    if not out or want <= 0:
        return out[:max(want, 0)]
    if len(out) <= want:
        return out
    step = len(out) / float(want)
    return [out[int(i * step)] for i in range(want)]


def read_wav_mono(path):
    """Enough of a WAV reader for our own files: 16/24/32-bit PCM or 32-bit float, any channels."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return None, 0
    pos, fmt, sr, bits, ch = 12, None, 0, 0, 1
    samples = None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = int.from_bytes(data[pos + 4:pos + 8], "little")
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = int.from_bytes(body[0:2], "little")
            ch = int.from_bytes(body[2:4], "little")
            sr = int.from_bytes(body[4:8], "little")
            bits = int.from_bytes(body[14:16], "little")
        elif cid == b"data":
            if fmt == 3 and bits == 32:
                samples = np.frombuffer(body[: (len(body) // 4) * 4], dtype="<f4").astype(np.float64)
            elif bits == 16:
                samples = np.frombuffer(body[: (len(body) // 2) * 2], dtype="<i2").astype(np.float64) / 32768.0
            elif bits == 24:
                raw = np.frombuffer(body[: (len(body) // 3) * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
                v = (raw[:, 0] | (raw[:, 1] << 8) | (raw[:, 2] << 16))
                v = np.where(v & 0x800000, v - 0x1000000, v)
                samples = v.astype(np.float64) / 8388608.0
            elif bits == 32:
                samples = np.frombuffer(body[: (len(body) // 4) * 4], dtype="<i4").astype(np.float64) / 2147483648.0
            break
        pos += 8 + size + (size & 1)
    if samples is None or samples.size == 0:
        return None, 0
    if ch > 1:
        samples = samples[: (samples.size // ch) * ch].reshape(-1, ch).mean(axis=1)
    return samples, sr


def make_struck(rng, path, seconds):
    """Cut the sharpest event out of a recording and shape it into an impulse."""
    x, sr = read_wav_mono(path)
    if x is None or sr <= 0 or x.size < sr // 2:
        return None
    # Where the recording most suddenly gets louder. An envelope in 5 ms hops, and the biggest
    # rise in it over four hops: sharper than looking for the peak, which in a field recording is
    # usually the middle of the loudest continuous passage rather than the start of anything.
    hop = max(1, int(sr * 0.005))
    frames = x[: (x.size // hop) * hop].reshape(-1, hop)
    env = np.sqrt((frames ** 2).mean(axis=1)) + 1e-9
    if env.size < 20:
        return None
    rise = np.log(env[4:]) - np.log(env[:-4])
    k = int(np.argmax(rise))
    onset = max(0, (k) * hop - int(sr * 0.004))
    n = int(seconds * sr)
    if onset + n > x.size:
        onset = max(0, x.size - n)
    seg = x[onset:onset + n].copy()
    if seg.size < n // 2 or float(np.sqrt((seg ** 2).mean())) < 1e-5:
        return None
    # An impulse must decay, and a slice of the world does not: the tail is shaped by an
    # exponential that reaches -60 dB at the end, which also removes the discontinuity there.
    t = np.arange(seg.size) / sr
    seg *= np.exp(-6.9078 * t / (seconds * 0.9))
    seg -= seg.mean()
    # Two channels from one: the right is the same event a couple of milliseconds later and
    # slightly differently coloured, which is what a second microphone would have heard.
    lag = int(sr * rng.uniform(0.0008, 0.0035))
    right = np.concatenate([np.zeros(lag), seg[: seg.size - lag]])
    ir = np.stack([seg, right * rng.uniform(0.8, 1.0)])
    if sr != SR:   # resample by linear interpolation: these are short and broadband
        m = int(round(ir.shape[1] * SR / sr))
        src = np.linspace(0.0, ir.shape[1] - 1.0, m)
        ir = np.stack([np.interp(src, np.arange(ir.shape[1]), ir[c]) for c in range(2)])
    return normalise(fade_out(fade_in(ir, SR, 1.0), SR, 8.0))


def make_space(rng, path, seconds):
    """The other way to use a recording: not its sharpest event but a quiet stretch of it, shaped
    into a tail. Convolved with a drone, a minute of rain or of a ventilation shaft becomes a room
    with that texture in its walls -- irregular in a way no designed hall is, because no designed
    hall was ever outdoors."""
    x, sr = read_wav_mono(path)
    if x is None or sr <= 0 or x.size < sr:
        return None
    n = int(seconds * sr)
    if x.size < n + sr // 4:
        return None
    # The steadiest window rather than the loudest: an event in the tail would read as an echo of
    # something that never happened.
    hop = max(1, int(sr * 0.05))
    frames = x[: (x.size // hop) * hop].reshape(-1, hop)
    env = np.log(np.sqrt((frames ** 2).mean(axis=1)) + 1e-9)
    w = max(2, n // hop)
    if env.size <= w:
        return None
    # Rolling mean and variance, cheaply: the window with the least movement in it.
    c1 = np.concatenate([[0.0], np.cumsum(env)])
    c2 = np.concatenate([[0.0], np.cumsum(env ** 2)])
    mean = (c1[w:] - c1[:-w]) / w
    var = np.maximum((c2[w:] - c2[:-w]) / w - mean ** 2, 0.0)
    # Loud enough to be something, steady enough to be a texture.
    score = var - 0.15 * mean
    start = int(np.argmin(score)) * hop
    seg = x[start:start + n].copy()
    if seg.size < n or float(np.sqrt((seg ** 2).mean())) < 1e-5:
        return None
    # A room decays; a recording does not. Two envelopes: the tail falls to -60 dB, and the head
    # rises over a few milliseconds so it is a room and not a sample being triggered.
    t = np.arange(seg.size) / sr
    seg = seg * np.exp(-6.9078 * t / (seconds * 0.85)) * np.minimum(1.0, t / 0.004)
    seg -= seg.mean()
    # Two channels: the same texture, decorrelated by a few milliseconds, which is what makes a
    # convolution reverb wide instead of centred.
    lag = int(sr * rng.uniform(0.003, 0.012))
    right = np.concatenate([np.zeros(lag), seg[: seg.size - lag]]) * rng.uniform(0.85, 1.0)
    ir = np.stack([seg, right])
    if sr != SR:
        m = int(round(ir.shape[1] * SR / sr))
        src = np.linspace(0.0, ir.shape[1] - 1.0, m)
        i0 = np.floor(src).astype(int)
        fr = src - i0
        i0 = np.clip(i0, 0, ir.shape[1] - 2)
        ir = ir[:, i0] * (1 - fr) + ir[:, i0 + 1] * fr
    return ir


# ---------------------------------------------------------------- checks

def check(ir, sr, family):
    """An impulse has to be there, must carry no offset and must not click, and must decay --
    except in the one family whose whole point is that it does not: a reversed decay swells, and
    for that one the check is that it actually rises."""
    problems = []
    mono = ir.mean(axis=0)
    n = mono.size
    if not np.all(np.isfinite(ir)):
        problems.append("non-finite")
        return problems
    rms = float(np.sqrt((mono ** 2).mean()))
    if rms < 1e-5:
        problems.append("silent")
    head = float(np.sqrt((mono[: n // 8] ** 2).mean()) + 1e-12)
    tail = float(np.sqrt((mono[-n // 8:] ** 2).mean()) + 1e-12)
    if family == "reverse":
        if head > tail * 0.5:
            problems.append(f"does not swell ({20 * math.log10(tail / head):+.0f} dB)")
    elif tail > head * 0.5:
        problems.append(f"does not decay ({20 * math.log10(tail / head):+.0f} dB)")
    if abs(float(mono.mean())) > 0.001 + 0.01 * rms:
        problems.append("dc")
    if abs(float(mono[0])) > 0.02:
        problems.append("starts with a step")
    return problems


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out-dir", default=os.path.join(ROOT, "Library", "Impulses"))
    ap.add_argument("--seed", type=int, default=31)
    ap.add_argument("--float32", action="store_true", help="32-bit float instead of 24-bit PCM")
    ap.add_argument("--only", default=None, help="only families whose name contains this")
    a = ap.parse_args()
    os.makedirs(a.out_dir, exist_ok=True)
    rng = np.random.default_rng(a.seed)
    jobs = catalogue(rng)
    if a.only:
        jobs = [j for j in jobs if a.only in j[0] or a.only in j[1]]
    print(f"{len(jobs)} impulses -> {a.out_dir}")

    bad, families, skipped = 0, {}, 0
    for family, name, build in jobs:
        raw = build()
        if raw is None:          # a builder that found nothing usable in its source material
            skipped += 1
            print(f"  SKIP {name}: nothing to cut")
            continue
        ir = finish(raw, SR)
        ir = dc_block(ir, SR)
        ir = normalise(ir)
        problems = check(ir, SR, family)
        if problems:
            bad += 1
            print(f"  PROBLEM {name}: {', '.join(problems)}")
            continue
        save(os.path.join(a.out_dir, name + ".wav"), ir, SR,
             {"family": family, "seconds": ir.shape[1] / SR}, float32=a.float32)
        families[family] = families.get(family, 0) + 1
    total = sum(families.values())
    print(f"{total} written" + (f", {bad} rejected" if bad else "") + (f", {skipped} skipped" if skipped else ""))
    for f in sorted(families):
        print(f"  {f:9s} {families[f]}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
