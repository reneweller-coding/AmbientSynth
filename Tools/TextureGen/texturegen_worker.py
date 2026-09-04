"""TextureGen worker: keeps one text-to-audio model loaded and renders jobs to WAV.

Runs as a child process of the GUI (torch never inside a Qt thread). Protocol on stdio,
one JSON object per line:
  in : {"cmd": "generate", "model": "sao", "prompt": "...", "negative": "...", "seconds": 30,
        "steps": 100, "guidance": 7.0, "seed": 1234, "out_dir": "...", "name": "slug"}
  in : {"cmd": "quit"}
  out: {"event": "status", "text": "..."}          progress / info
       {"event": "progress", "step": i, "total": n}
       {"event": "done", "path": "...", "seconds": s, "note": "A3" | null, "hz": 220.0 | null}
       {"event": "error", "text": "..."}
Can also be used from the command line:  python texturegen_worker.py --model musicgen-large
--prompt "..." --seconds 20 --out-dir Textures
"""
import argparse
import json
import math
import os
import re
import sys
import time

import numpy as np

MODELS = {
    # key: (label, hub id, sample rate, max seconds, notes)
    "sao":            ("Stable Audio Open 1.0 (44.1 kHz stereo, textures)", "stabilityai/stable-audio-open-1.0", 44100, 47.0,
                       "gated: accept the licence on Hugging Face and run huggingface-cli login"),
    "musicgen-large": ("MusicGen Large (32 kHz, tonal drones)", "facebook/musicgen-large", 32000, 30.0, ""),
    "musicgen-medium":("MusicGen Medium (32 kHz, faster)", "facebook/musicgen-medium", 32000, 30.0, ""),
    "musicgen-small": ("MusicGen Small (32 kHz, quick tests)", "facebook/musicgen-small", 32000, 30.0, ""),
    "audioldm2":      ("AudioLDM 2 Large (16 kHz, dark, effects)", "cvssp/audioldm2-large", 16000, 30.0, "16 kHz output: no air above 8 kHz"),
}

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def emit(**kw):
    sys.stdout.write(json.dumps(kw) + "\n")
    sys.stdout.flush()


def slugify(text, limit=40):
    s = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_")
    return (s[:limit] or "texture").rstrip("_")


def detect_pitch(mono, sr):
    """Median autocorrelation pitch over 100 ms frames; returns (hz, note) or (None, None)
    when the clip is not periodic enough (noise, rain, wind)."""
    frame = int(sr * 0.1)
    hop = frame
    lo, hi = max(2, int(sr / 2000.0)), int(sr / 40.0)   # 40 Hz .. 2 kHz
    if len(mono) < 3 * frame:
        return None, None
    votes = []
    for start in range(0, len(mono) - frame, hop):
        x = mono[start:start + frame].astype(np.float64)
        x = x - x.mean()
        e = float(np.dot(x, x))
        if e < 1e-6:
            continue
        ac = np.correlate(x, x, mode="full")[frame - 1:]
        ac = ac / (ac[0] + 1e-12)
        # Skip the lobe around lag 0 (up to the first zero crossing), otherwise a low tone
        # "peaks" at the shortest allowed lag.
        neg = np.flatnonzero(ac[:hi] <= 0.0)
        first = max(lo, int(neg[0])) if len(neg) else lo
        if first >= hi - 1:
            continue
        seg = ac[first:hi]
        best = float(seg.max())
        if best <= 0.6:
            continue
        # The fundamental is the SHORTEST lag whose peak is (almost) as high as the best one:
        # a pure 1.5 kHz tone also peaks at 2, 3, 4 periods and argmax alone would pick a
        # subharmonic; 0.97 keeps a dominant 4th partial from being called the fundamental.
        cands = np.flatnonzero(seg >= 0.97 * best)
        k = int(cands[0]) + first
        while k + 1 < hi and ac[k + 1] > ac[k]:   # walk up to the actual local maximum
            k += 1
        votes.append(sr / k)
    if len(votes) < 3 or len(votes) < 0.4 * max(1, (len(mono) - frame) // hop):
        return None, None
    hz = float(np.median(votes))
    midi = 69 + 12 * math.log2(hz / 440.0)
    n = int(round(midi))
    if abs(midi - n) > 0.35:
        return None, None
    return hz, f"{NOTE_NAMES[n % 12]}{n // 12 - 1}"


def write_wav(path, audio, sr):
    """audio: (channels, samples) float32."""
    try:
        import soundfile as sf
        sf.write(path, audio.T, sr, subtype="FLOAT")
    except ImportError:
        import wave
        w = wave.open(path, "wb")
        w.setnchannels(audio.shape[0]); w.setsampwidth(2); w.setframerate(sr)
        w.writeframes((np.clip(audio.T, -1, 1) * 32767).astype(np.int16).tobytes()); w.close()


class Generator:
    def __init__(self):
        self.key = None
        self.pipe = None
        self.proc = None
        self.device = None

    def load(self, key):
        if key == self.key and self.pipe is not None:
            return
        import torch
        self.pipe = None
        self.proc = None
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        label, hub, sr, _, _ = MODELS[key]
        emit(event="status", text=f"loading {label} ...")
        t0 = time.time()
        if key == "sao":
            from diffusers import StableAudioPipeline
            self.pipe = StableAudioPipeline.from_pretrained(hub, torch_dtype=torch.float16).to(self.device)
        elif key.startswith("musicgen"):
            from transformers import AutoProcessor, MusicgenForConditionalGeneration
            self.proc = AutoProcessor.from_pretrained(hub)
            self.pipe = MusicgenForConditionalGeneration.from_pretrained(hub, torch_dtype=torch.float16).to(self.device)
        elif key == "audioldm2":
            from diffusers import AudioLDM2Pipeline
            self.pipe = AudioLDM2Pipeline.from_pretrained(hub, torch_dtype=torch.float16).to(self.device)
        else:
            raise ValueError(f"unknown model {key}")
        self.key = key
        emit(event="status", text=f"{label} ready ({time.time() - t0:.0f} s)")

    def generate(self, job):
        import torch
        key = job["model"]
        self.load(key)
        label, hub, sr, max_secs, _ = MODELS[key]
        seconds = float(min(max(job.get("seconds", 20.0), 1.0), max_secs))
        seed = int(job.get("seed", 0))
        steps = int(job.get("steps", 100))
        guidance = float(job.get("guidance", 7.0))
        prompt = job["prompt"]
        negative = job.get("negative", "") or None
        gen = torch.Generator(self.device).manual_seed(seed)
        emit(event="status", text=f"{label}: {seconds:.0f} s, seed {seed}")
        t0 = time.time()
        if key == "sao":
            def cb(pipe, i, t, kw):
                emit(event="progress", step=i + 1, total=steps); return kw
            out = self.pipe(prompt, negative_prompt=negative, num_inference_steps=steps, guidance_scale=guidance,
                            audio_end_in_s=seconds, num_waveforms_per_prompt=1, generator=gen,
                            callback_on_step_end=cb)
            audio = out.audios[0].float().cpu().numpy()          # (channels, samples)
        elif key.startswith("musicgen"):
            inputs = self.proc(text=[prompt], padding=True, return_tensors="pt").to(self.device)
            tokens = int(seconds * 50)                              # 50 Hz frame rate
            torch.manual_seed(seed)
            with torch.inference_mode():
                wav = self.pipe.generate(**inputs, do_sample=True, guidance_scale=max(guidance, 1.0), max_new_tokens=tokens)
            audio = wav[0].float().cpu().numpy()                    # (channels, samples)
        else:
            def cb2(pipe, i, t, kw):
                emit(event="progress", step=i + 1, total=steps); return kw
            out = self.pipe(prompt, negative_prompt=negative, num_inference_steps=steps, guidance_scale=guidance,
                            audio_length_in_s=seconds, num_waveforms_per_prompt=1, generator=gen,
                            callback_on_step_end=cb2)
            audio = np.asarray(out.audios[0], dtype=np.float32)[None, :]
        if audio.ndim == 1:
            audio = audio[None, :]
        audio = np.nan_to_num(audio.astype(np.float32))
        peak = float(np.max(np.abs(audio))) if audio.size else 0.0
        if peak > 0:
            audio *= min(1.0, 0.5 / peak)                            # -6 dBFS peak: leave headroom for the synth
        mono = audio.mean(axis=0)
        hz, note = detect_pitch(mono, sr)
        out_dir = job.get("out_dir", "Textures")
        os.makedirs(out_dir, exist_ok=True)
        name = job.get("name") or slugify(prompt)
        fname = f"{name}_{key}_{seed}" + (f"_{note}" if note else "") + ".wav"
        path = os.path.join(out_dir, fname)
        write_wav(path, audio, sr)
        with open(os.path.splitext(path)[0] + ".txt", "w", encoding="utf-8") as f:
            f.write(json.dumps({"model": hub, "prompt": prompt, "negative": negative, "seconds": seconds, "steps": steps,
                                "guidance": guidance, "seed": seed, "sample_rate": sr, "pitch_hz": hz, "note": note}, indent=2))
        emit(event="done", path=path, seconds=audio.shape[1] / sr, note=note, hz=hz, elapsed=time.time() - t0)


def serve():
    g = Generator()
    emit(event="status", text="worker ready")
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            job = json.loads(line)
        except json.JSONDecodeError as e:
            emit(event="error", text=f"bad job: {e}"); continue
        if job.get("cmd") == "quit":
            break
        try:
            g.generate(job)
        except Exception as e:   # report, keep serving
            emit(event="error", text=f"{type(e).__name__}: {e}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--serve", action="store_true", help="read jobs from stdin (used by the GUI)")
    ap.add_argument("--model", default="sao", choices=sorted(MODELS))
    ap.add_argument("--prompt")
    ap.add_argument("--negative", default="")
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--steps", type=int, default=100)
    ap.add_argument("--guidance", type=float, default=7.0)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--count", type=int, default=1)
    ap.add_argument("--out-dir", default="Textures")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    if a.list:
        for k, (label, hub, sr, mx, note) in MODELS.items():
            print(f"{k:16s} {label}  [{hub}, {sr} Hz, <= {mx:.0f} s] {note}")
        return
    if a.serve:
        serve(); return
    if not a.prompt:
        ap.error("--prompt is required")
    g = Generator()
    for i in range(a.count):
        g.generate({"model": a.model, "prompt": a.prompt, "negative": a.negative, "seconds": a.seconds, "steps": a.steps,
                    "guidance": a.guidance, "seed": a.seed + i, "out_dir": a.out_dir})


if __name__ == "__main__":
    main()
