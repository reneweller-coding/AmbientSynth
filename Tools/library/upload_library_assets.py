"""Upload the sample library (Textures, FieldRecordings) to a GitHub release as archives the installer
can take: AmbientSynth-library-<version>-partN.zip, each at most --max-part-mb (GitHub refuses a release
asset over 2 GiB). Files are stored as they are -- FLAC does not shrink any further -- under the paths
Textures/<name> and FieldRecordings/<name>, the layout the installer unpacks into the AmbientSynth folder.
A manifest in the shape make_content_pack.py writes (version, parts with name, bytes, sha256, files)
goes up last, so the installer's include can be generated from it.

Resumable and frugal with disk: the parts are planned once (sorted names, filled in order), then built,
uploaded, checked against the size GitHub reports and deleted, one at a time. The state file in the
staging folder records what is done; a rerun carries on and refuses to continue if the library changed.
The release is created as a draft: nothing is published until someone publishes it.

  python Tools/library/upload_library_assets.py --plan-only
  python Tools/library/upload_library_assets.py [--tag library-v5] [--version v5]
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
import zipfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LIB = os.path.join(ROOT, "Library")
FOLDERS = ("Textures", "FieldRecordings")
REPO = "reneweller-coding/AmbientSynth"
DEFAULT_STAGE = os.path.normpath(os.path.join(ROOT, "..", "AmbientSynth-Upload"))


def plan(version, max_bytes):
    files = []
    for kind in FOLDERS:
        folder = os.path.join(LIB, kind)
        for name in sorted(os.listdir(folder)):
            path = os.path.join(folder, name)
            if os.path.isfile(path) and name.lower().endswith((".flac", ".wav")):
                files.append([kind, name, os.path.getsize(path)])
    parts, current, size = [], [], 0
    for entry in files:
        # zip headers cost about 100 bytes plus twice the name per file; keep a margin for them
        cost = entry[2] + 2 * len(entry[1]) + 256
        if current and size + cost > max_bytes:
            parts.append(current)
            current, size = [], 0
        current.append(entry)
        size += cost
    if current:
        parts.append(current)
    return [{"name": f"AmbientSynth-library-{version}-part{i + 1}.zip", "files": p} for i, p in enumerate(parts)]


def gh(*args, check=True):
    r = subprocess.run(["gh", *args, "-R", REPO], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and r.returncode != 0:
        raise RuntimeError(f"gh {' '.join(args)} failed: {r.stderr.strip()}")
    return r


def remote_sizes(tag):
    out = gh("release", "view", tag, "--json", "assets").stdout
    return {a["name"]: a["size"] for a in json.loads(out)["assets"]}


def ensure_release(tag, version, summary):
    if gh("release", "view", tag, "--json", "name", check=False).returncode == 0:
        return
    notes = (f"Sample library {version}: {summary}. Draft, not published: the preset packs that use it are "
             "being rebuilt. Each archive unpacks into the AmbientSynth folder (Textures/, FieldRecordings/); "
             f"AmbientSynth-library-{version}-manifest.json lists every part with its SHA-256 and files.")
    gh("release", "create", tag, "--draft", "--title", f"AmbientSynth sample library {version}", "--notes", notes)


def build(part, path):
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED, allowZip64=True) as z:
        for kind, name, _ in part["files"]:
            z.write(os.path.join(LIB, kind, name), arcname=f"{kind}/{name}")
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8 << 20), b""):
            h.update(chunk)
    return os.path.getsize(path), h.hexdigest()


def upload(tag, path):
    for attempt in range(1, 5):
        r = gh("release", "upload", tag, path, "--clobber", check=False)
        if r.returncode == 0:
            return
        print(f"  upload attempt {attempt} failed: {r.stderr.strip()[:300]}", flush=True)
        time.sleep(60 * attempt)
    raise RuntimeError(f"upload of {os.path.basename(path)} failed four times")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--tag", default="library-v5")
    ap.add_argument("--version", default="v5")
    ap.add_argument("--max-part-mb", type=int, default=1800)
    ap.add_argument("--stage", default=DEFAULT_STAGE)
    ap.add_argument("--plan-only", action="store_true")
    a = ap.parse_args()

    parts = plan(a.version, a.max_part_mb * 1024 * 1024)
    counts = {k: sum(1 for p in parts for f in p["files"] if f[0] == k) for k in FOLDERS}
    total = sum(f[2] for p in parts for f in p["files"])
    summary = ", ".join(f"{n} {k}" for k, n in counts.items()) + f", {total / 1e9:.1f} GB in {len(parts)} archives"
    print(summary)
    if a.plan_only:
        for p in parts[:3] + parts[-2:]:
            print(f"  {p['name']}: {len(p['files'])} files, {sum(f[2] for f in p['files']) / 2**20:.0f} MiB, "
                  f"{p['files'][0][0]}/{p['files'][0][1]} .. {p['files'][-1][0]}/{p['files'][-1][1]}")
        return

    os.makedirs(a.stage, exist_ok=True)
    state_path = os.path.join(a.stage, f"upload-{a.version}.json")
    if os.path.exists(state_path):
        with open(state_path, encoding="utf-8") as f:
            state = json.load(f)
        if [p["files"] for p in state["parts"]] != [p["files"] for p in parts]:
            sys.exit(f"the library changed since the upload began ({state_path}); not mixing two states")
    else:
        state = {"tag": a.tag, "version": a.version, "parts": parts, "done": {}}

    def save():
        with open(state_path, "w", encoding="utf-8") as f:
            json.dump(state, f, indent=1)

    save()
    ensure_release(a.tag, a.version, summary)
    for i, part in enumerate(state["parts"], 1):
        name = part["name"]
        if name in state["done"]:
            continue
        path = os.path.join(a.stage, name)
        t0 = time.time()
        size, digest = build(part, path)
        t1 = time.time()
        upload(a.tag, path)
        t2 = time.time()
        remote = remote_sizes(a.tag).get(name)
        if remote != size:
            sys.exit(f"{name}: GitHub reports {remote} bytes, the archive has {size}; stopping with it kept at {path}")
        os.remove(path)
        state["done"][name] = {"bytes": size, "sha256": digest, "uploaded": time.strftime("%Y-%m-%d %H:%M:%S")}
        save()
        print(f"[{i}/{len(state['parts'])}] {name}: {size / 2**30:.2f} GiB, built {t1 - t0:.0f} s, "
              f"uploaded {t2 - t1:.0f} s ({size * 8 / 1e6 / max(t2 - t1, 1e-3):.0f} Mbit/s)", flush=True)

    manifest = {"version": a.version, "kind": "library", "folders": list(FOLDERS),
                "parts": [{"name": p["name"], "bytes": state["done"][p["name"]]["bytes"],
                           "sha256": state["done"][p["name"]]["sha256"],
                           "files": [f"{k}/{n}" for k, n, _ in p["files"]]} for p in state["parts"]]}
    mpath = os.path.join(a.stage, f"AmbientSynth-library-{a.version}-manifest.json")
    with open(mpath, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1)
    upload(a.tag, mpath)
    print(f"done: {len(state['parts'])} archives and the manifest on release {a.tag} (draft)")


if __name__ == "__main__":
    main()
