"""WavetableGen core: wavetables (single-cycle frames, 2048 samples) from audio, from a
text prompt (through TextureGen's models) or from a procedural spectral walk.

A wavetable here is an array of shape (frames, 2048), each row one cycle, peak-normalised,
phase-aligned to its neighbour so morphing between frames does not click. Export writes the
Serum/Vital layout (frames back to back) as a 32-bit float or 16-bit WAV; AmbientSynth's
User table analyses that back into spectra (Tools/render --wavetable, the plugin's
"Wavetable..." button, wavetable.wav on the Quest).
"""
import json
import math
import os
import re

import numpy as np

FRAME = 2048
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


# ---------------------------------------------------------------- helpers

def note_name(hz):
    midi = 69 + 12 * math.log2(max(hz, 1e-3) / 440.0)
    n = int(round(midi))
    return f"{NOTE_NAMES[n % 12]}{n // 12 - 1}"


def slugify(text, limit=40):
    s = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_")
    return (s[:limit] or "table").rstrip("_")


def read_wav_mono(path):
    import soundfile as sf
    a, sr = sf.read(path, dtype="float32", always_2d=True)
    return a.mean(axis=1), sr


def write_table_wav(path, table, sr=44100, float32=True):
    """table: (frames, FRAME). Frames back to back, the common wavetable layout."""
    import soundfile as sf
    data = np.asarray(table, dtype=np.float32).reshape(-1)
    sf.write(path, data, sr, subtype="FLOAT" if float32 else "PCM_16")


def spectrum(frame, partials=64):
    """Magnitudes of partials 1..partials of one cycle (normalised to the strongest)."""
    f = np.abs(np.fft.rfft(frame))[1:partials + 1]
    m = f.max()
    return f / m if m > 0 else f


def frame_from_spectrum(mags, phases=None):
    """One cycle from partial magnitudes (index 0 = fundamental); zero phase unless given."""
    n = len(mags)
    spec = np.zeros(FRAME // 2 + 1, dtype=np.complex128)
    ph = np.zeros(n) if phases is None else phases
    spec[1:n + 1] = mags * np.exp(1j * ph)
    x = np.fft.irfft(spec, FRAME)
    m = np.max(np.abs(x))
    return (x / m).astype(np.float32) if m > 0 else x.astype(np.float32)


def normalise(table):
    out = np.asarray(table, dtype=np.float32).copy()
    for i in range(out.shape[0]):
        out[i] -= out[i].mean()
        m = np.max(np.abs(out[i]))
        if m > 0:
            out[i] /= m
    return out


def phase_align(table):
    """Rotate every frame so it correlates best with the previous one (circular shift), and
    start the first frame at its rising zero crossing. Keeps morphs click-free."""
    out = np.asarray(table, dtype=np.float32).copy()
    if out.shape[0] == 0:
        return out
    f0 = out[0]
    zc = np.flatnonzero((f0[:-1] <= 0) & (f0[1:] > 0))
    if len(zc):
        out[0] = np.roll(f0, -int(zc[0]))
    for i in range(1, out.shape[0]):
        a, b = out[i - 1], out[i]
        # circular cross-correlation via FFT
        corr = np.fft.irfft(np.fft.rfft(a) * np.conj(np.fft.rfft(b)), FRAME)
        shift = int(np.argmax(corr))
        out[i] = np.roll(b, shift)
    return out


# ---------------------------------------------------------------- from audio

def track_pitch(mono, sr, lo_hz=40.0, hi_hz=2000.0, frame_s=0.05):
    """Per-frame fundamental by autocorrelation (same rules as TextureGen: skip the lag-0
    lobe, shortest peak >= 0.97 of the best, interior maxima only). Returns arrays
    (times, hz) with hz = nan where nothing periodic was found."""
    frame = int(sr * frame_s)
    hop = frame // 2
    lo, hi = max(2, int(sr / hi_hz)), min(int(sr / lo_hz), frame - 3)
    times, hzs = [], []
    for start in range(0, max(1, len(mono) - frame), hop):
        x = mono[start:start + frame].astype(np.float64)
        x = x - x.mean()
        times.append((start + frame / 2) / sr)
        if float(np.dot(x, x)) < 1e-7:
            hzs.append(np.nan); continue
        ac = np.correlate(x, x, mode="full")[frame - 1:]
        ac = ac / (ac[0] + 1e-12)
        neg = np.flatnonzero(ac[:hi] <= 0.0)
        first = max(lo, int(neg[0])) if len(neg) else lo
        if first >= hi - 1:
            hzs.append(np.nan); continue
        seg = ac[first:hi]
        best = float(seg.max())
        if best <= 0.5:
            hzs.append(np.nan); continue
        k = int(np.flatnonzero(seg >= 0.97 * best)[0]) + first
        while k + 1 < hi and ac[k + 1] > ac[k]:
            k += 1
        if k <= first or k >= hi - 2 or ac[k] < ac[k - 1] or ac[k] < ac[k + 1]:
            hzs.append(np.nan); continue
        # parabolic interpolation of the peak for a sub-sample period
        y0, y1, y2 = ac[k - 1], ac[k], ac[k + 1]
        d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0.0
        hzs.append(sr / (k + d))
    return np.array(times), np.array(hzs)


def table_from_audio(mono, sr, frames=32, pitch_hz=None, start=0.0, end=None, cycles_per_frame=4):
    """Slice `frames` single cycles evenly between start and end (seconds). The period comes
    from the pitch track (median, or `pitch_hz` to force it); each frame averages
    `cycles_per_frame` consecutive cycles to tame noise. Returns (table, info)."""
    n = len(mono)
    end = n / sr if end is None else min(end, n / sr)
    if pitch_hz is None:
        t, hz = track_pitch(mono, sr)
        sel = (t >= start) & (t <= end) & np.isfinite(hz)
        if sel.sum() < 3:
            raise ValueError("no stable pitch found in the selection; set the pitch by hand")
        pitch_hz = float(np.median(hz[sel]))
        voiced = float(sel.sum()) / max(1, ((t >= start) & (t <= end)).sum())
    else:
        voiced = 1.0
    period = sr / pitch_hz
    s0, s1 = int(start * sr), int(end * sr) - int(period * (cycles_per_frame + 1)) - 2
    if s1 <= s0:
        raise ValueError("selection too short for the period")
    table = np.zeros((frames, FRAME), dtype=np.float32)
    grid = np.arange(FRAME) / FRAME
    for i in range(frames):
        pos = s0 + (s1 - s0) * (i / max(frames - 1, 1))
        # start each cycle at the nearest rising zero crossing after pos (steadier phase)
        p = int(pos)
        for q in range(p, min(p + int(period), n - 2)):
            if mono[q] <= 0 < mono[q + 1]:
                p = q; break
        acc = np.zeros(FRAME)
        for c in range(cycles_per_frame):
            idx = p + (c + grid) * period
            i0 = np.floor(idx).astype(int)
            fr = idx - i0
            i0 = np.clip(i0, 0, n - 2)
            acc += mono[i0] * (1 - fr) + mono[i0 + 1] * fr
        table[i] = acc / cycles_per_frame
    table = phase_align(normalise(table))
    return table, {"pitch_hz": pitch_hz, "note": note_name(pitch_hz), "voiced": voiced, "frames": frames}


# ---------------------------------------------------------------- procedural

RECIPES = {
    # name: function(partial index array 1..N, t in 0..1, rng) -> magnitudes
    "Saw to square": lambda h, t, r: (1.0 / h) * np.where(h % 2 == 1, 1.0, 1.0 - t),
    "Tilt walk":     lambda h, t, r: h ** -(0.5 + 1.5 * t),
    "Formant sweep": lambda h, t, r: (1.0 / np.sqrt(h)) * (0.05 + np.exp(-((h - (2 + 14 * t)) / 2.5) ** 2) + 0.4 * np.exp(-((h - (8 + 20 * t)) / 4.0) ** 2)),
    "Comb":          lambda h, t, r: (1.0 / np.sqrt(h)) * np.abs(np.cos(h * (0.3 + 1.2 * t))),
    "Glass thinning": lambda h, t, r: np.where(np.isin(h, [1, 3, 7, 12, 19, 27, 36, 47]), 1.0 / h ** (0.3 + 0.7 * t), 0.02 / h),
    "Odd breathing": lambda h, t, r: (1.0 / h) * np.where(h % 2 == 1, 1.0, 0.2 + 0.8 * (0.5 + 0.5 * np.sin(2 * np.pi * t))),
    "Random walk":   None,   # handled below: smooth random spectra
}


def table_procedural(recipe, frames=32, partials=48, seed=0, noise=0.0, phase_scatter=0.0):
    """A wavetable from a spectral recipe over t = 0..1, optionally with a random component
    (noise: random per-partial gain walk) and random phases (phase_scatter 0..1: 0 = all
    partials in phase = classic waveform look; 1 = fully random phases = smoother, wider)."""
    rng = np.random.default_rng(seed)
    h = np.arange(1, partials + 1, dtype=np.float64)
    phases = rng.uniform(-np.pi, np.pi, partials) * phase_scatter
    walk = np.ones(partials)
    table = np.zeros((frames, FRAME), dtype=np.float32)
    # smooth random targets for the walk: new target every 8 frames, smoothstep between
    targets = [np.exp(rng.normal(0, 1.0, partials)) for _ in range(frames // 8 + 2)]
    for i in range(frames):
        t = i / max(frames - 1, 1)
        if recipe == "Random walk":
            base = h ** -(0.7 + 0.6 * t)
        else:
            base = np.maximum(RECIPES[recipe](h, t, rng), 0.0)
        if noise > 0 or recipe == "Random walk":
            seg = i / 8.0
            a, b = targets[int(seg)], targets[int(seg) + 1]
            u = seg - int(seg); u = u * u * (3 - 2 * u)
            walk = a + (b - a) * u
            amount = max(noise, 1.0 if recipe == "Random walk" else 0.0)
            base = base * (walk ** amount)
        table[i] = frame_from_spectrum(base, phases)
    return phase_align(normalise(table))


def morph_tables(a, b, frames=32):
    """Cross-fade between the first frames of a and b (spectral domain, so it stays clean)."""
    out = np.zeros((frames, FRAME), dtype=np.float32)
    sa, sb = np.fft.rfft(a[0]), np.fft.rfft(b[0])
    for i in range(frames):
        t = i / max(frames - 1, 1)
        x = np.fft.irfft(sa * (1 - t) + sb * t, FRAME)
        out[i] = x
    return phase_align(normalise(out))


# ---------------------------------------------------------------- preview

def render_preview(table, sr=48000, seconds=6.0, hz=110.0, sweep=True):
    """Plays the table at `hz`, sweeping the frame position 0 -> 1 over the clip (or holding
    frame 0), with linear interpolation between frames and a short fade."""
    n = int(seconds * sr)
    frames = table.shape[0]
    phase = (np.arange(n) * hz / sr) % 1.0
    pos = np.linspace(0, 1, n) if sweep and frames > 1 else np.zeros(n)
    fpos = pos * (frames - 1)
    i0 = np.minimum(fpos.astype(int), max(frames - 2, 0))
    fr = fpos - i0
    idx = phase * FRAME
    j0 = idx.astype(int)
    jf = idx - j0
    j1 = (j0 + 1) % FRAME
    def sample(fi):   # per-sample gather: frame index and phase index vary together
        return table[fi, j0] * (1 - jf) + table[fi, j1] * jf
    y = sample(i0) * (1 - fr) + sample(np.minimum(i0 + 1, frames - 1)) * fr
    fade = np.minimum(1.0, np.minimum(np.arange(n), n - np.arange(n)) / (0.02 * sr))
    return (0.4 * y * fade).astype(np.float32)


def save_table(path, table, info=None, float32=True):
    write_table_wav(path, table, float32=float32)
    meta = {"frames": int(table.shape[0]), "frame_len": FRAME}
    if info:
        meta.update({k: (float(v) if isinstance(v, (np.floating, float)) else v) for k, v in info.items()})
    with open(os.path.splitext(path)[0] + ".txt", "w", encoding="utf-8") as f:
        f.write(json.dumps(meta, indent=2))
    return path
