"""Make new texture from the statistics of an old one, after McDermott and Simoncelli (2011).

Their result is that sound texture -- rain, wind, fire, a crowd, a stream -- is recognised from a
small set of TIME-AVERAGED statistics of the auditory periphery, not from the waveform. Measure
those statistics on one recording, impose them on noise, and a listener hears the same texture
without hearing the same sound. That is exactly what an ambient instrument wants from a field
recording: an endless bed with the character of the original and none of its repetition.

What is measured, per the paper's model of the periphery:

  * a cochlear filter bank -- 32 bands one equal step of the ERB rate apart (Glasberg and Moore
    1990), the same scale the instrument's own Spectral source uses;
  * the envelope of each band, compressed by the power 0.3, which is the paper's stand-in for the
    compressive nonlinearity of the cochlea;
  * per band, the first four moments of that envelope: mean, normalised variance, skew, kurtosis;
  * the correlation between the envelopes of every pair of bands, which is what tells fire (bands
    that crackle together) from wind (bands that do not);
  * a modulation spectrum per band -- the power of the envelope in nine octave-spaced modulation
    bands from 0.5 to 128 Hz -- which is the rate at which each band's loudness fluctuates.

Synthesis is alternating projection, in the manner of Heeger and Bergen (1995) and Portilla and
Simoncelli (2000): start from noise, then repeatedly impose the cross-band correlations, impose
the modulation spectrum on each envelope, and impose the marginal distribution by matching
histograms. Each step undoes a little of the last, and the loop converges. The paper itself uses
gradient descent on the statistics; the projection loop is cheaper, needs no derivatives, and this
tool reports how far it got, so the difference is visible rather than assumed.

What it reports, on a wind recording at fourteen iterations, as a relative error against the
source's own statistics: mean 0.008, variance 0.042, skew 0.074, kurtosis 0.066, correlation 0.025
absolute, modulation power 0.34. The modulation figure is the loose one, and the reason is stated:
below half a hertz nothing is imposed, and a recording with a slow swell keeps a good deal of its
envelope down there. The largest cross-correlation between source and result is around 0.04, so
nothing has been copied.

    python Tools/library/texture_statistics.py --in Textures/rain_sao_003.wav --out new_rain.wav
    python Tools/library/texture_statistics.py --in-dir Textures --out-dir Textures/synth \
        --count 40 --seconds 30 --report stats.csv

Only the standard library and numpy are needed; WAV reading and writing are here so the tool runs
in any environment the rest of the library runs in.
"""
import argparse
import csv
import glob
import os
import random
import struct
import sys
import wave

import numpy as np

BANDS = 32
COMPRESS = 0.3
MOD_BANDS = [0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0]


# ---------------------------------------------------------------------------- files


def read_wav(path):
    """Mono float64 and the sample rate.

    Written out rather than handed to the standard library's `wave`, which refuses format tag 3 --
    IEEE float -- and that is what half of this library's own texture renders are.
    """
    with open(path, "rb") as fh:
        head = fh.read(12)
        if head[:4] != b"RIFF" or head[8:12] != b"WAVE":
            raise ValueError("%s: not a RIFF/WAVE file" % path)
        fmt = None
        data = None
        while True:
            hdr = fh.read(8)
            if len(hdr) < 8:
                break
            cid, size = hdr[:4], struct.unpack("<I", hdr[4:])[0]
            body = fh.read(size)
            if size & 1:
                fh.read(1)              # chunks are word aligned
            if cid == b"fmt ":
                fmt = body
            elif cid == b"data":
                data = body
                if fmt is not None:
                    break
    if fmt is None or data is None:
        raise ValueError("%s: no fmt or data chunk" % path)
    tag, ch, sr, _, _, bits = struct.unpack("<HHIIHH", fmt[:16])
    if tag == 0xFFFE and len(fmt) >= 40:
        tag = struct.unpack("<H", fmt[24:26])[0]     # extensible: the real tag is in the GUID
    if tag == 3 and bits == 32:
        x = np.frombuffer(data, dtype="<f4").astype(np.float64)
    elif tag == 3 and bits == 64:
        x = np.frombuffer(data, dtype="<f8").astype(np.float64)
    elif tag == 1 and bits == 16:
        x = np.frombuffer(data, dtype="<i2").astype(np.float64) / 32768.0
    elif tag == 1 and bits == 24:
        n = len(data) // 3
        b = np.frombuffer(data[: n * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = np.where(v & 0x800000, v - 0x1000000, v).astype(np.float64) / 8388608.0
    elif tag == 1 and bits == 32:
        x = np.frombuffer(data, dtype="<i4").astype(np.float64) / 2147483648.0
    elif tag == 1 and bits == 8:
        x = (np.frombuffer(data, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
    else:
        raise ValueError("%s: format tag %d at %d bits is not read here" % (path, tag, bits))
    if ch > 1:
        x = x[: (x.size // ch) * ch].reshape(-1, ch).mean(axis=1)
    return x, sr


def write_wav(path, x, sr):
    """24-bit PCM, which is what the texture library is in."""
    peak = float(np.max(np.abs(x))) if x.size else 0.0
    if peak > 0.0:
        x = x * (0.89 / peak)               # -1 dBFS, leaving room for the reader's own gain
    v = np.clip(np.round(x * 8388607.0), -8388608, 8388607).astype(np.int32)
    b = np.empty((v.size, 3), dtype=np.uint8)
    b[:, 0] = v & 0xFF
    b[:, 1] = (v >> 8) & 0xFF
    b[:, 2] = (v >> 16) & 0xFF
    d = os.path.dirname(os.path.abspath(path))
    if d:
        os.makedirs(d, exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(3)
        w.setframerate(int(sr))
        w.writeframes(b.tobytes())


# ---------------------------------------------------------------------------- the periphery


def erb_rate(f):
    return 21.4 * np.log10(0.00437 * np.asarray(f, dtype=np.float64) + 1.0)


def erb_inv(e):
    return (np.power(10.0, np.asarray(e, dtype=np.float64) / 21.4) - 1.0) / 0.00437


def band_edges(sr, bands=BANDS, low=50.0, high=None):
    high = high if high is not None else min(16000.0, 0.45 * sr)
    e = np.linspace(erb_rate(low), erb_rate(high), bands + 1)
    return erb_inv(e)


def band_windows(nfft, sr, bands=BANDS):
    """One half-cosine window per band on the ERB axis. Neighbours overlap by exactly one band, so
    the SQUARED responses sum to one: filtering on the way in and again on the way out puts the
    signal back together unchanged, which is what lets the loop below replace envelopes without
    also colouring what it did not touch."""
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)
    ee = erb_rate(band_edges(sr, bands))
    er = erb_rate(np.maximum(freqs, 1e-6))
    w = np.zeros((bands, freqs.size))
    for b in range(bands):
        c, span = 0.5 * (ee[b] + ee[b + 1]), ee[b + 1] - ee[b]
        m = np.abs(er - c) < span
        w[b, m] = np.cos(np.pi * 0.5 * (er[m] - c) / span)
    return w


def cochlea(x, sr, bands=BANDS, win=None):
    """The filter bank. Returns each band and its envelope. One complex transform per band gives
    both: the analytic signal's real part is the band, its magnitude is the envelope, and no
    separate Hilbert transform is needed."""
    # Single precision throughout the transforms. A texture's envelope statistics are quantities
    # of a few significant figures, and the selftest reports the same numbers to three places
    # either way -- while the whole tool runs in half the time.
    n = x.size
    nfft = 1 << int(np.ceil(np.log2(n)))
    X = np.fft.rfft(np.asarray(x, dtype=np.float32), nfft)
    w = (band_windows(nfft, sr, bands) if win is None else win).astype(np.float32)
    sub = np.zeros((bands, n), dtype=np.float32)
    env = np.zeros((bands, n), dtype=np.float32)
    buf = np.zeros(nfft, dtype=np.complex64)
    for b in range(bands):
        buf[:] = 0.0
        buf[: X.size] = 2.0 * X * w[b]
        za = np.fft.ifft(buf)[:n]
        sub[b] = za.real
        env[b] = np.abs(za)
    return sub, env


def resynthesise(sub, sr, nfft, win):
    """Back to one signal: each band filtered a second time, then summed. The squares of the
    windows sum to one, so a band that was not touched comes back exactly as it went in."""
    w = win.astype(np.float32)
    Y = np.zeros(win.shape[1], dtype=np.complex64)
    for b in range(sub.shape[0]):
        Y += np.fft.rfft(np.asarray(sub[b], dtype=np.float32), nfft) * w[b]
    y = np.fft.irfft(Y, nfft)
    return y[: sub.shape[1]]


def mod_bins(nfft, sr):
    """How many spectrum bins the envelope work has to touch at all.

    The envelope has been band-limited to twice ENV_CUT, so everything above that is exactly zero
    and multiplying it by nine window shapes three times over is nine-and-twenty times nothing. At
    48 kHz this is one bin in sixty, and it is the difference between a tool that takes a quarter
    of an hour for a clip and one that takes half a minute.
    """
    return min(nfft // 2 + 1, int(2.2 * ENV_CUT * nfft / sr) + 4)


def modulation_filters(nfft, sr_env, kmax=None):
    """Octave-spaced modulation band shapes, half-cosine on a log rate axis."""
    f = np.fft.rfftfreq(nfft, 1.0 / sr_env)
    if kmax is not None:
        f = f[:kmax]
    out = []
    lf = np.log2(np.maximum(f, 1e-6))
    for c in MOD_BANDS:
        w = np.zeros_like(f)
        m = np.abs(lf - np.log2(c)) < 1.0
        w[m] = np.cos(np.pi * 0.5 * (lf[m] - np.log2(c)))
        out.append(w)
    return out


# ---------------------------------------------------------------------------- statistics


def moments(e):
    """Mean, variance over the squared mean, skew and kurtosis -- the four the paper keeps.
    Dividing the variance by the squared mean is what makes it a shape rather than a level."""
    e = np.asarray(e, dtype=np.float64)
    m = e.mean(axis=1, keepdims=True)
    d = e - m
    v = (d ** 2).mean(axis=1)
    s = (d ** 3).mean(axis=1) / np.maximum(v ** 1.5, 1e-20)
    k = (d ** 4).mean(axis=1) / np.maximum(v ** 2, 1e-20)
    return m[:, 0], v / np.maximum(m[:, 0] ** 2, 1e-20), s, k


ENV_CUT = 200.0    # Hz: the top of the modulation range, as in the paper's own envelope rate


def split_envelope(e, sr, cut=ENV_CUT):
    """The envelope's slow part and what is left of it.

    The paper downsamples envelopes to 400 Hz before measuring anything, and that step is not
    housekeeping. An envelope at the full sample rate carries the carrier's own jitter -- a band
    two kilohertz wide fluctuates at up to two kilohertz -- and for this material that jitter is
    SEVENTY PER CENT of the envelope's variance. Statistics taken over all of it then describe the
    noise in the band rather than the texture: measured, only 0.305 of the variance sat inside the
    modulation bands at all, so setting the marginal distribution and setting the modulation power
    were pulling against each other, and the loop settled at an error it would not leave.

    Splitting the envelope makes the three constraints agree. Everything is imposed on the slow
    part; the fast part is carried through untouched, which is right, because in this model it
    carries nothing.
    """
    n = e.shape[1]
    nfft = 1 << int(np.ceil(np.log2(n)))
    E = np.fft.rfft(np.asarray(e, dtype=np.float32), nfft, axis=1)
    f = np.fft.rfftfreq(nfft, 1.0 / sr)
    w = np.ones(f.size)
    band = (f > cut) & (f < 2.0 * cut)
    w[band] = 0.5 + 0.5 * np.cos(np.pi * (f[band] - cut) / cut)    # a soft edge, never a step
    w[f >= 2.0 * cut] = 0.0
    slow = np.fft.irfft(E * w.astype(np.float32), nfft, axis=1)[:, :n]
    return slow, np.asarray(e, dtype=np.float32) - slow


def mod_power(e, sr):
    """The power of each envelope in each modulation band, as a DENSITY.

    Dividing by the number of samples is not cosmetic. An unnormalised transform's summed power
    grows with the length of what went into it, and this tool measures a twenty-second source
    against an eight-second result: without the division the result read as having two and a half
    times too little modulation, and -- worse -- the synthesis was being driven towards that wrong
    figure, because the same expression set the gain. The error stood at 0.86 and would not fall
    however long the loop ran, which is what gave it away: a projection that is converging does not
    stop at a constant.
    """
    d = e - e.mean(axis=1, keepdims=True)
    n = e.shape[1]
    nfft = 1 << int(np.ceil(np.log2(n)))
    kmax = mod_bins(nfft, sr)
    E = np.fft.rfft(d, nfft, axis=1)[:, :kmax]
    filt = modulation_filters(nfft, sr, kmax)
    # The mean is over the whole half spectrum, not over the slice, so the figure means the same
    # thing whatever the slice happens to be.
    denom = float(nfft // 2 + 1) * n
    out = np.zeros((e.shape[0], len(filt)))
    for i, w in enumerate(filt):
        out[:, i] = np.sum(np.abs(E * w) ** 2, axis=1) / denom
    return out


def measure(x, sr, bands=BANDS):
    _, env = cochlea(x, sr, bands)
    e, _ = split_envelope(np.power(np.maximum(env, 0.0), COMPRESS), sr)
    mean, var, skew, kurt = moments(e)
    e = np.asarray(e, dtype=np.float64)
    d = e - e.mean(axis=1, keepdims=True)
    sd = np.sqrt((d ** 2).mean(axis=1)) + 1e-20
    corr = (d @ d.T) / (e.shape[1] * np.outer(sd, sd))
    modpow = mod_power(e, sr)
    return {
        "mean": mean, "var": var, "skew": skew, "kurt": kurt,
        "corr": corr, "modpow": modpow,
        "env": e,                       # kept for histogram matching, not part of the statistics
    }


def distance(a, b):
    """How far apart two sets of statistics are, each term as a relative error.

    Aggregate rather than per element: the modulation power of a slow drone is nearly nothing at
    16 Hz, and an elementwise relative error there divides by nearly nothing and reports a
    catastrophe over a difference the ear could not hear.
    """
    def rel(u, v):
        return float(np.mean(np.abs(u - v)) / (np.mean(np.abs(v)) + 1e-12))
    return {
        "mean": rel(a["mean"], b["mean"]),
        "var": rel(a["var"], b["var"]),
        "skew": rel(a["skew"], b["skew"]),
        "kurt": rel(a["kurt"], b["kurt"]),
        "corr": float(np.mean(np.abs(a["corr"] - b["corr"]))),
        "mod": rel(a["modpow"], b["modpow"]),
    }


# ---------------------------------------------------------------------------- synthesis


def max_cross_correlation(a, b):
    """How much of a is literally in b: the largest normalised cross-correlation at any lag.

    By transform, not by np.correlate, which computes the sum directly -- a pair of five-second
    excerpts is then more than eighty thousand million multiply-adds, and it took longer than the
    synthesis it was checking.
    """
    n = min(a.size, b.size)
    a = np.asarray(a[:n], dtype=np.float64)
    b = np.asarray(b[:n], dtype=np.float64)
    a = (a - a.mean()) / (np.std(a) + 1e-12)
    b = (b - b.mean()) / (np.std(b) + 1e-12)
    nfft = 1 << int(np.ceil(np.log2(2 * n)))
    c = np.fft.irfft(np.fft.rfft(a, nfft) * np.conj(np.fft.rfft(b, nfft)), nfft)
    return float(np.max(np.abs(c))) / n


def match_histogram(x, target_sorted):
    """Give x the value distribution of the target, keeping its order. The one operation that sets
    a marginal distribution exactly, and the reason the moments do not have to be chased one by
    one."""
    order = np.argsort(x, kind="stable")
    out = np.empty_like(x)
    if target_sorted.size != x.size:
        idx = np.linspace(0, target_sorted.size - 1, x.size)
        target_sorted = np.interp(idx, np.arange(target_sorted.size), target_sorted)
    out[order] = target_sorted
    return out


def impose_correlation(e, target_corr):
    """Give a set of envelopes the target correlation matrix, by whitening them and colouring them
    again with the target's own square root. Levels are put back afterwards."""
    m = e.mean(axis=1, keepdims=True)
    d = e - m
    sd = np.sqrt((d ** 2).mean(axis=1, keepdims=True)) + 1e-20
    z = d / sd
    c = (z @ z.T) / z.shape[1]
    # A symmetric square root either way, so the transform is the closest one that does the job.
    def inv_sqrt(mat):
        w, v = np.linalg.eigh(mat + 1e-6 * np.eye(mat.shape[0]))
        w = np.maximum(w, 1e-9)
        return v @ np.diag(w ** -0.5) @ v.T

    def sqrtm(mat):
        w, v = np.linalg.eigh(mat + 1e-6 * np.eye(mat.shape[0]))
        w = np.maximum(w, 1e-9)
        return v @ np.diag(np.sqrt(w)) @ v.T

    z = sqrtm(target_corr) @ (inv_sqrt(c) @ z)
    return z * sd + m


def impose_modulation(e, modpow, sr):
    """Give each envelope the target power in each modulation band, leaving the phases alone --
    which is what keeps the result from being a copy: same rates, different moments in time."""
    n = e.shape[1]
    nfft = 1 << int(np.ceil(np.log2(n)))
    kmax = mod_bins(nfft, sr)
    m = e.mean(axis=1, keepdims=True)
    d = e - m
    E = np.fft.rfft(d, nfft, axis=1)
    lo = E[:, :kmax]
    filt = modulation_filters(nfft, sr, kmax)
    denom = float(nfft // 2 + 1) * n
    # The modulation bands overlap, so scaling one changes what its neighbours measure: a single
    # pass of gains lands near the target but not on it (measured, it halved the error instead of
    # clearing it). Three passes, each correcting what the last one disturbed, do clear it.
    rest = np.ones(kmax)
    for w in filt:
        rest = np.maximum(rest - w * w, 0.0)
    rest = np.sqrt(rest)                 # outside the bands, left exactly as it was
    acc = lo.copy()
    for _ in range(3):
        nxt = lo * rest
        for i, w in enumerate(filt):
            have = np.sum(np.abs(acc * w) ** 2, axis=1) / denom    # the density the target is in
            g = np.sqrt(modpow[:, i] / np.maximum(have, 1e-30))[:, None]
            nxt = nxt + acc * w * g * w
        acc = nxt
    E[:, :kmax] = acc
    return np.fft.irfft(E, nfft, axis=1)[:, :n] + m


def synthesise(stats, sr, seconds, seed=0, iterations=12, bands=BANDS, progress=None):
    """Noise, projected onto the statistics until it stops moving.

    The loop runs on the SIGNAL, not on a set of envelopes. That distinction is the whole
    difference between a tool that works and one that only looks as if it does: envelopes imposed
    on noise carriers and then summed do not measure as the envelopes that were imposed, because
    re-analysing the sum finds the carrier's own fluctuation multiplied into every band. Measured
    that way the first version of this reported a mean off by 114 per cent and a modulation power
    off by 332. Analysing and resynthesising inside the loop closes it.
    """
    rng = np.random.default_rng(seed)
    n = int(seconds * sr)
    nfft = 1 << int(np.ceil(np.log2(n)))
    win = band_windows(nfft, sr, bands)
    target_sorted = [np.sort(row) for row in stats["env"]]

    y = rng.standard_normal(n)
    y /= np.max(np.abs(y)) + 1e-12
    for it in range(iterations):
        sub, env = cochlea(y, sr, bands, win)
        slow, fast = split_envelope(np.power(np.maximum(env, 0.0), COMPRESS), sr)
        # The histogram goes last, which is worth saying because with a narrower set of modulation
        # bands it was the wrong way round. Matching a histogram is a hard, pointwise remapping,
        # and while the rates only reached 0.25 to 32 Hz it undid the modulation step every time:
        # the modulation error sat near one however long the loop ran. The bands now cover the
        # whole range the envelope is band-limited to, the two constraints no longer contradict
        # each other, and the four orderings were measured against one another before this one was
        # kept: it is the best on every statistic at once.
        e = impose_correlation(slow, stats["corr"])
        e = impose_modulation(e, stats["modpow"], sr)
        for b in range(bands):
            e[b] = match_histogram(e[b], target_sorted[b])
        want = np.power(np.maximum(e + fast, 0.0), 1.0 / COMPRESS)
        # Keep the fine structure, replace the envelope. The ratio is bounded: where a band has
        # gone quiet the quotient would otherwise be an amplification of nothing.
        ratio = np.clip(want / (env + 1e-9), 0.0, 40.0)
        y = resynthesise(sub * ratio, sr, nfft, win)
        # No normalising in here. The histogram match has just set every band's envelope to the
        # distribution it is meant to have, level included; rescaling afterwards would undo
        # exactly that, and the mean of every band would then be measured wrong by one constant.
        if not np.all(np.isfinite(y)):
            y = np.nan_to_num(y)
        if progress is not None:
            progress(it)
    return y


# ---------------------------------------------------------------------------- driver


def one(path, out, seconds, seed, iterations, verbose=True):
    x, sr = read_wav(path)
    if x.size < sr * 2:
        raise ValueError("%s: shorter than two seconds" % path)
    x = x[: int(sr * min(20.0, x.size / sr))]      # twenty seconds is plenty to measure
    stats = measure(x, sr)
    y = synthesise(stats, sr, seconds, seed=seed, iterations=iterations)
    got = measure(y, sr)
    d = distance(got, stats)
    # How much of the source is literally in the result, which for a texture built from statistics
    # should be near nothing.
    xc = max_cross_correlation(x, y)
    write_wav(out, y, sr)
    if verbose:
        print("%-44s mean %.3f var %.3f skew %.3f kurt %.3f corr %.3f mod %.3f | copied %.3f"
              % (os.path.basename(out), d["mean"], d["var"], d["skew"], d["kurt"], d["corr"], d["mod"], xc))
    d["file"] = os.path.basename(out)
    d["source"] = os.path.basename(path)
    d["copied"] = xc
    return d


def selftest(seconds=6.0, iterations=14, sr=48000):
    """Three textures built here, so the tool can be checked without the library.

    Rain is sparse impulses through a bright filter, fire is the same with bursts and a low
    rumble, wind is noise whose bands swell slowly and independently. Each has a different answer
    for the statistics that matter -- rain is sparse (high kurtosis), fire's bands crackle together
    (high cross-band correlation), wind's do not -- so a tool that only matched the average
    spectrum would pass on none of them.
    """
    rng = np.random.default_rng(11)
    n = int(seconds * 2 * sr)
    t = np.arange(n) / sr

    def rain():
        y = np.zeros(n)
        hits = rng.integers(0, n, size=int(seconds * 2 * 900))
        y[hits] = rng.standard_normal(hits.size)
        z = np.zeros(n)
        s = 0.0
        for k in range(1, 12):                      # a short bright ring on each drop
            z += np.roll(y, k) * (0.8 ** k) * np.cos(k * 2.0)
            s += (0.8 ** k) ** 2
        return z / np.sqrt(s)

    def fire():
        base = rng.standard_normal(n)
        low = np.cumsum(base) / 400.0
        low -= low.mean()
        burst = np.exp(1.6 * np.sin(2 * np.pi * 0.7 * t) - 1.0)
        crack = np.zeros(n)
        hits = rng.integers(0, n, size=int(seconds * 2 * 400))
        crack[hits] = rng.standard_normal(hits.size) * 3.0
        return 0.4 * low + burst * (0.3 * base + crack)

    def wind():
        # Each band swells at its own slow rate, and no two rates are the same: the bands are then
        # loud at different moments, which is exactly the low cross-band correlation that tells
        # wind from fire.
        sub, _ = cochlea(rng.standard_normal(n), sr, BANDS)
        y = np.zeros(n)
        for b in range(BANDS):
            y += sub[b] * (0.5 + 0.5 * np.sin(2 * np.pi * (0.11 + 0.09 * b) * t + 1.7 * b))
        return y

    ok = True
    for name, gen in (("rain", rain), ("fire", fire), ("wind", wind)):
        x = gen()
        x /= np.max(np.abs(x)) + 1e-12
        stats = measure(x, sr)
        y = synthesise(stats, sr, seconds, seed=7, iterations=iterations)
        d = distance(measure(y, sr), stats)
        copied = max_cross_correlation(x[: int(seconds * sr)], y)
        good = (d["mean"] < 0.15 and d["var"] < 0.35 and d["skew"] < 0.6
                and d["kurt"] < 0.8 and d["corr"] < 0.12 and copied < 0.2)
        ok = ok and good
        print("%-6s mean %.3f var %.3f skew %.3f kurt %.3f corr %.3f mod %.3f | copied %.3f  %s"
              % (name, d["mean"], d["var"], d["skew"], d["kurt"], d["corr"], d["mod"], copied,
                 "ok" if good else "FAIL"))
    print("texture_statistics selftest: %s" % ("all checks passed" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="src", help="one source recording")
    ap.add_argument("--in-dir", help="a directory of source recordings")
    ap.add_argument("--out", help="the file to write (with --in)")
    ap.add_argument("--out-dir", help="the directory to write into (with --in-dir)")
    ap.add_argument("--count", type=int, default=0, help="how many sources to take from --in-dir (0 = all)")
    ap.add_argument("--seconds", type=float, default=30.0)
    ap.add_argument("--iterations", type=int, default=12)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--report", help="write the measured distances here as CSV")
    ap.add_argument("--selftest", action="store_true", help="check the tool on textures built here")
    a = ap.parse_args(argv)

    if a.selftest:
        return selftest(iterations=a.iterations)

    rows = []
    if a.src:
        out = a.out or os.path.splitext(a.src)[0] + "_stat.wav"
        rows.append(one(a.src, out, a.seconds, a.seed, a.iterations))
    elif a.in_dir:
        if not a.out_dir:
            ap.error("--in-dir needs --out-dir")
        files = sorted(glob.glob(os.path.join(a.in_dir, "*.wav")))
        if not files:
            ap.error("no .wav under %s" % a.in_dir)
        rng = random.Random(a.seed)
        if a.count and a.count < len(files):
            files = rng.sample(files, a.count)
        for i, f in enumerate(files):
            base = os.path.splitext(os.path.basename(f))[0]
            out = os.path.join(a.out_dir, base + "_stat_loop.wav")
            try:
                rows.append(one(f, out, a.seconds, a.seed + i, a.iterations))
            except Exception as exc:            # a broken source must not stop the batch
                print("  skipped %s: %s" % (os.path.basename(f), exc))
    else:
        ap.error("--in or --in-dir")

    if a.report and rows:
        with open(a.report, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=["file", "source", "mean", "var", "skew", "kurt", "corr", "mod", "copied"])
            w.writeheader()
            for r in rows:
                w.writerow(r)
        print("report: %s" % a.report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
