"""Measure a rendered WAV instead of listening to it.

Prints per-window RMS, spectral centroid, stereo correlation, the largest
sample-to-sample jump (click detector) and the strongest spectral peaks.

    python Tools/analyze.py file.wav [--window 5]
"""
import sys
import struct
import numpy as np


def read_wav(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE", "not a WAV"
    pos = 12
    fmt = None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, sr, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            fmt = (tag, ch, sr, bits)
        elif cid == b"data":
            tag, ch, sr, bits = fmt
            if tag == 3 and bits == 32:
                x = np.frombuffer(body, dtype="<f4")
            elif tag == 1 and bits == 16:
                x = np.frombuffer(body, dtype="<i2").astype(np.float32) / 32768.0
            else:
                raise SystemExit(f"unsupported format tag={tag} bits={bits}")
            return x.reshape(-1, ch), sr
        pos += 8 + size + (size & 1)
    raise SystemExit("no data chunk")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = sys.argv[1]
    win = 5.0
    if "--window" in sys.argv:
        win = float(sys.argv[sys.argv.index("--window") + 1])
    x, sr = read_wav(path)
    n, ch = x.shape
    mono = x.mean(axis=1)
    print(f"{path}: {n / sr:.1f} s, {ch} ch, {sr} Hz")
    print(f"peak {np.abs(x).max():.3f}, rms {20 * np.log10(np.sqrt((x ** 2).mean()) + 1e-12):.1f} dBFS")
    jump = np.abs(np.diff(mono))
    j = int(jump.argmax())
    print(f"largest sample jump {jump.max():.4f} at {j / sr:.2f} s (click if > ~0.3)")
    if ch == 2:
        L, R = x[:, 0], x[:, 1]
        corr = np.corrcoef(L, R)[0, 1] if L.std() > 0 and R.std() > 0 else 1.0
        print(f"stereo correlation {corr:.3f} (1 = mono, 0 = fully wide)")

    w = int(win * sr)
    print(f"\n{'t':>6} {'rms dB':>8} {'centroid':>9} {'lo<200':>7} {'hi>4k':>6}")
    freqs = np.fft.rfftfreq(w, 1.0 / sr)
    for start in range(0, n - w + 1, w):
        seg = mono[start:start + w] * np.hanning(w)
        spec = np.abs(np.fft.rfft(seg))
        p = spec ** 2
        tot = p.sum() + 1e-20
        cent = (freqs * p).sum() / tot
        lo = p[freqs < 200].sum() / tot
        hi = p[freqs > 4000].sum() / tot
        rms = 20 * np.log10(np.sqrt((mono[start:start + w] ** 2).mean()) + 1e-12)
        print(f"{start / sr:6.0f} {rms:8.1f} {cent:9.0f} {lo:7.2f} {hi:6.2f}")

    # Strongest peaks over the whole file
    seg = mono[: min(n, sr * 30)]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    idx = np.argsort(spec)[::-1]
    picked = []
    for i in idx:
        f = freqs[i]
        if f < 20 or any(abs(f - q) < 3 for q in picked):
            continue
        picked.append(f)
        if len(picked) >= 12:
            break
    print("\nstrongest peaks (Hz, first 30 s):", ", ".join(f"{f:.1f}" for f in sorted(picked)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
