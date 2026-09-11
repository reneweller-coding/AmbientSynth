"""Measure impulse responses: is it a room, a resonator, or a slice of a recording?

For each WAV:
  seconds, channels, rate
  t30 / r2      Schroeder energy decay, linear fit from -5 to -35 dB (or -25 dB when the IR is short),
                and how straight the decay is (1 = a clean exponential)
  edt           early decay time (0 to -10 dB)
  env_resid_db  std of the 50 ms level envelope around a straight line, after the peak: a room is
                smooth, a slice of rain or a crowd is not
  rerises       how often the 50 ms envelope climbs back up by more than 6 dB after the peak
  ned_tail      normalised echo density (Abel & Huang 2006) over the tail: ~1 = diffuse, low = sparse
  ned_t80       seconds until the echo density first reaches 0.8
  peaky_db      95th percentile of the magnitude spectrum above its third-octave smoothing: ringing
                (diffuse noise sits near 7 dB)
  flatness      spectral flatness 100 Hz .. 8 kHz
  lf_db         energy below 150 Hz against the whole, in dB
  centroid_hz   power centroid
  corr          L/R correlation (1 = mono)
  peak_s        where the largest sample is
  jump          largest sample-to-sample step against the RMS (clicks)

  python Tools/library/ir_metrics.py <folder> <out.json>
"""
import json
import math
import os
import sys

import numpy as np
import soundfile as sf
from scipy.special import erfc


def edc_db(x):
    e = np.cumsum((x ** 2)[::-1])[::-1]
    e = e / (e[0] + 1e-30)
    return 10.0 * np.log10(e + 1e-30)


def fit_decay(edc, sr, lo, hi):
    idx = np.where((edc <= lo) & (edc >= hi))[0]
    if idx.size < max(8, int(sr * 0.02)):
        return None, None
    t = idx / sr
    y = edc[idx]
    A = np.vstack([t, np.ones_like(t)]).T
    slope, icpt = np.linalg.lstsq(A, y, rcond=None)[0]
    pred = A @ np.array([slope, icpt])
    ss = float(np.sum((y - y.mean()) ** 2))
    r2 = 1.0 - float(np.sum((y - pred) ** 2)) / max(ss, 1e-12)
    return (-60.0 / slope if slope < 0 else None), r2


def ned(x, sr, win_ms=20.0):
    w = max(16, int(sr * win_ms / 1000.0))
    hop = w // 2
    k = erfc(1.0 / math.sqrt(2.0))
    times, vals = [], []
    for s in range(0, max(0, x.size - w), hop):
        seg = x[s:s + w]
        sd = seg.std()
        vals.append(0.0 if sd < 1e-12 else float(np.mean(np.abs(seg - seg.mean()) > sd)) / k)
        times.append((s + w / 2) / sr)
    return np.array(times), np.array(vals)


def third_octave_smooth(freqs, mag_db):
    out = np.empty_like(mag_db)
    lf = np.log2(np.maximum(freqs, 1.0))
    c = np.concatenate([[0.0], np.cumsum(mag_db)])
    lo = np.searchsorted(lf, lf - 1.0 / 6.0)
    hi = np.searchsorted(lf, lf + 1.0 / 6.0)
    hi = np.maximum(hi, lo + 1)
    out[:] = (c[hi] - c[lo]) / (hi - lo)
    return out


def analyse_array(x, sr, name=""):
    """x: (channels, samples) float64."""
    ch = x.shape[0]
    L = x[0]
    R = x[1] if ch > 1 else x[0]
    mono = 0.5 * (L + R) if ch > 1 else L
    n = mono.size
    res = {"file": name, "seconds": n / sr, "channels": ch, "rate": sr}
    if n < sr * 0.02 or float(np.sqrt(np.mean(mono ** 2))) < 1e-7:
        res["silent"] = True
        return res
    energy = L ** 2 + R ** 2
    edc = edc_db(np.sqrt(energy))
    t30, r2 = fit_decay(edc, sr, -5.0, -35.0)
    if t30 is None:
        t30, r2 = fit_decay(edc, sr, -5.0, -25.0)
    edt, _ = fit_decay(edc, sr, 0.0, -10.0)
    res.update(t30=t30, r2=r2, edt=edt)
    hop = int(sr * 0.05)
    frames = np.sqrt(energy[: (n // hop) * hop].reshape(-1, hop).mean(axis=1)) + 1e-12
    db = 20.0 * np.log10(frames / frames.max())
    p = int(np.argmax(db))
    tail = db[p:]
    tail = tail[tail > -50.0]
    if tail.size >= 4:
        tt = np.arange(tail.size)
        coef = np.polyfit(tt, tail, 1)
        res["env_resid_db"] = float(np.std(tail - np.polyval(coef, tt)))
        run_min = np.minimum.accumulate(tail)
        res["rerises"] = int(np.sum(np.diff((tail - run_min) > 6.0).astype(int) == 1))
    else:
        res["env_resid_db"] = None
        res["rerises"] = 0
    times, vals = ned(mono, sr)
    peak_t = float(np.argmax(np.abs(mono)) / sr)
    res["peak_s"] = peak_t
    if vals.size:
        after = vals[times > peak_t + 0.05]
        tail_end = times[-1] * 0.7
        mid = vals[(times > peak_t + 0.1) & (times < max(peak_t + 0.2, tail_end))]
        res["ned_tail"] = float(np.mean(mid)) if mid.size else (float(np.mean(after)) if after.size else None)
        hit = np.where(vals >= 0.8)[0]
        res["ned_t80"] = float(times[hit[0]] - peak_t) if hit.size else None
    nfft = 1 << int(min(20, max(12, math.ceil(math.log2(n)))))
    spec = np.abs(np.fft.rfft(mono, n=nfft)) + 1e-12
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)
    band = (freqs >= 100.0) & (freqs <= 8000.0)
    mag_db = 20.0 * np.log10(spec)
    sm = third_octave_smooth(freqs[band], mag_db[band])
    res["peaky_db"] = float(np.percentile(mag_db[band] - sm, 95))
    pw = spec[band] ** 2
    res["flatness"] = float(np.exp(np.mean(np.log(pw))) / np.mean(pw))
    allp = spec ** 2
    res["lf_db"] = float(10.0 * np.log10(allp[freqs < 150.0].sum() / allp.sum() + 1e-30))
    res["centroid_hz"] = float((freqs * allp).sum() / allp.sum())
    res["corr"] = float(np.corrcoef(L, R)[0, 1]) if ch > 1 and np.std(L) > 0 and np.std(R) > 0 else 1.0
    rms = float(np.sqrt(np.mean(mono ** 2)))
    res["jump"] = float(np.max(np.abs(np.diff(mono))) / max(rms, 1e-12))
    return res


def analyse(path):
    x, sr = sf.read(path, always_2d=True, dtype="float64")
    return analyse_array(x.T, sr, os.path.basename(path))


def main():
    folder = sys.argv[1]
    out = sys.argv[2]
    files = sorted(f for f in os.listdir(folder) if f.lower().endswith(".wav"))
    rows = []
    for i, f in enumerate(files):
        try:
            rows.append(analyse(os.path.join(folder, f)))
        except Exception as exc:   # a broken file is a finding, not a crash
            rows.append({"file": f, "error": str(exc)})
        if (i + 1) % 50 == 0:
            print(f"{i + 1}/{len(files)}", flush=True)
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(rows, fh, indent=1)
    print(f"wrote {len(rows)} rows to {out}")


if __name__ == "__main__":
    main()
