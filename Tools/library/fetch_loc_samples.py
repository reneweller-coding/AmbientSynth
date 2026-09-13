"""AmbientSynth -- the Library of Congress's Citizen DJ sample packs, into the archive.

Citizen DJ (citizen-dj.labs.loc.gov) is the Library of Congress's own sampling project: audio
from its collections that the Library has identified as free to use, already cut into
sampler-ready clips of a few seconds, each pack with a statement of why it is free. This fetches
the packs (16-bit WAV, from the Library's S3 bucket), converts every clip to FLAC without loss,
writes them under Library/Archive/LoC/<collection>/, and records the source, the Library's own
rights statement and its suggested credit line in Archive/SOURCES.md.

    python Tools/library/fetch_loc_samples.py --list                 # the packs and their sizes
    python Tools/library/fetch_loc_samples.py --only variety-stage   # one pack
    python Tools/library/fetch_loc_samples.py                        # every pack in COLLECTIONS

Which packs, and why these (13.09.2026, Rene's wish for a thousand or two): the ones whose
statement is a matter of law or a gift to the Library -- Edison's companies (the assets went to
the National Park Service), the Variety Stage (the same), the National Screening Room's
government films, Tony Schwartz's recordings (acquired by the Library), Joe Smith's interviews
(donated, attribution asked for), the MusicBox Project (rights relinquished), and the National
Jukebox's opera, classical and folk songs (published before 1923: public domain since 2022 under
the Music Modernization Act, and their composers long dead, which is what an EU listener has to
ask as well). Not taken: the Jukebox's popular, jazz, blues and musical theatre (the recordings
are free in the United States, but many of their songs are by composers who died after 1955 and
are still protected in Europe), the dialect interviews (private people, the Library asks for
care, and a voice at the ear is not the place), and the Free Music Archive subset (music, not
the near layer's material).
"""
import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ARCHIVE = os.path.join(ROOT, "Library", "Archive")
WORK = os.path.join(ROOT, "build", "loc-work")
FFMPEG = os.environ.get("AMBIENT_FFMPEG") or "ffmpeg"
for cand in (r"C:\Anw\Tools\ffmpeg\bin\ffmpeg.exe",):
    if FFMPEG == "ffmpeg" and os.path.exists(cand):
        FFMPEG = cand
AGENT = "AmbientSynth/2.0 (library tool; +https://github.com/reneweller-coding/AmbientSynth)"
S3 = "https://s3.amazonaws.com/citizen-dj-assets.labs.loc.gov/samplepacks/"
SITE = "https://citizen-dj.labs.loc.gov/"

# slug on the site -> (folder under Archive/LoC, pack name on S3, the Library's statement)
COLLECTIONS = {
    "edison": ("Edison", "loc.gov_edison-company-motion-pictures-and-sound-recordings",
               "All recordings made by the companies of Thomas A. Edison between 1890 and 1929 are in the public "
               "domain because the assets of Edison Records were transferred to the National Park Service, a "
               "federal agency, in the 1950s."),
    "variety-stage": ("Variety-Stage", "loc.gov_variety-stage-sound-recordings-and-motion-pictures",
                      "Edison recordings of the variety stage, 1890s-1920s: the same transfer to the National Park "
                      "Service puts them in the public domain."),
    "national-screening-room": ("Screening-Room", "loc.gov_national-screening-room",
                                "A subset of films from the National Screening Room that were identified to have been "
                                "created by the U.S. government, thus in the public domain."),
    "tony-schwartz": ("Tony-Schwartz", "loc.gov_tony-schwartz",
                      "In 2007 Tony Schwartz's entire body of work was acquired by the Library of Congress, which "
                      "makes his recordings available for reuse; Citizen DJ excludes the ones with embedded material "
                      "he did not own."),
    "joe-smith": ("Joe-Smith", "loc.gov_joe-smith",
                  "Joe Smith, the copyright holder, donated the recordings to the Library of Congress and agreed to "
                  "make the material free to use and reuse with proper attribution; interviews with performances of "
                  "songs still in copyright were excluded."),
    "musicbox": ("MusicBox", "loc.gov_musicbox",
                 "Dyann and Rick Arthur, the original copyright holders of the MusicBox Project, relinquished all "
                 "ownership and copyright of the collection to the American Folklife Center in 2010, with the "
                 "performers' release forms."),
    "jukebox-opera": ("Jukebox-Opera", "loc.gov_national-jukebox-opera",
                      "Under the Music Modernization Act, items published prior to 1923 entered the public domain on "
                      "January 1, 2022."),
    "jukebox-classical": ("Jukebox-Classical", "loc.gov_national-jukebox-classical",
                          "Under the Music Modernization Act, items published prior to 1923 entered the public domain "
                          "on January 1, 2022."),
    "jukebox-folk-songs": ("Jukebox-Folk-Songs", "loc.gov_national-jukebox-folk-songs",
                           "Under the Music Modernization Act, items published prior to 1923 entered the public domain "
                           "on January 1, 2022."),
}
CREDIT = "Citizen DJ Project, Library of Congress"


def safe_name(s):
    s = re.sub(r"[^A-Za-z0-9 _\-\.]+", " ", s)
    s = re.sub(r"\s+", " ", s).strip(" .")
    return s[:90]


def remote_size(url):
    req = urllib.request.Request(url, method="HEAD", headers={"User-Agent": AGENT})
    with urllib.request.urlopen(req, timeout=60) as r:
        return int(r.headers.get("Content-Length") or 0)


def download(url, dest):
    size = remote_size(url)
    if os.path.exists(dest) and size > 0 and os.path.getsize(dest) == size:
        return dest, size
    req = urllib.request.Request(url, headers={"User-Agent": AGENT})
    tmp = dest + ".part"
    done = 0
    with urllib.request.urlopen(req, timeout=300) as r, open(tmp, "wb") as f:
        for chunk in iter(lambda: r.read(4 << 20), b""):
            f.write(chunk)
            done += len(chunk)
            if done % (256 << 20) < (4 << 20):
                print("    %d / %d MB" % (done >> 20, size >> 20), flush=True)
    os.replace(tmp, dest)
    return dest, size


def to_flac(src, dst):
    """16-bit WAV to 16-bit FLAC, the samples untouched (a 16-bit file gains nothing from 24)."""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    r = subprocess.run([FFMPEG, "-v", "error", "-y", "-i", src, "-c:a", "flac", "-compression_level", "8", dst],
                       capture_output=True)
    return r.returncode == 0


def note_sources(slug, folder, statement, count, names):
    path = os.path.join(ARCHIVE, "SOURCES.md")
    lines = ["", "## LoC/%s" % folder, "",
             "Library of Congress, Citizen DJ sample pack [%s](%sloc-%s/use/): %d clips, cut by the Library, "
             "taken as 16-bit WAV and stored as FLAC without loss." % (slug, SITE, slug, count),
             "Rights, in the Library's words: %s" % statement,
             "Suggested credit: %s." % CREDIT, ""]
    for n in names:
        lines.append("- `%s`" % n)
    with open(path, "a", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--only", action="append", default=[], help="a slug of COLLECTIONS (repeatable)")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--mp3", action="store_true", help="the 192 kbps packs instead of the WAV ones (a fifth of the download)")
    a = ap.parse_args()
    slugs = a.only or list(COLLECTIONS)
    for slug in slugs:
        if slug not in COLLECTIONS:
            sys.exit("unknown collection %s; one of %s" % (slug, ", ".join(COLLECTIONS)))
    os.makedirs(WORK, exist_ok=True)
    total = 0
    for slug in slugs:
        folder, pack, statement = COLLECTIONS[slug]
        url = S3 + pack + ("_mp3.zip" if a.mp3 else "_wav.zip")
        if a.list:
            try:
                print("%-24s %6.0f MB  %s" % (slug, remote_size(url) / 1e6, url))
            except Exception as e:
                print("%-24s ??? %s" % (slug, e))
            continue
        print("%s: fetching %s" % (slug, url), flush=True)
        dest, size = download(url, os.path.join(WORK, os.path.basename(url)))
        print("  %d MB" % (size >> 20), flush=True)
        out_dir = os.path.join(ARCHIVE, "LoC", folder)
        extracted = []
        with zipfile.ZipFile(dest) as z:
            members = [m for m in z.namelist() if m.lower().endswith((".wav", ".mp3")) and not m.startswith("__MACOSX")]
            print("  %d clips in the pack" % len(members), flush=True)
            src_dir = os.path.join(WORK, pack)
            os.makedirs(src_dir, exist_ok=True)
            for m in members:
                target = os.path.join(src_dir, os.path.basename(m))
                if not os.path.exists(target):
                    with z.open(m) as s, open(target, "wb") as f:
                        f.write(s.read())
                extracted.append(target)
        names, jobs = [], []
        for src in sorted(extracted):
            stem = safe_name(os.path.splitext(os.path.basename(src))[0])
            dst = os.path.join(out_dir, stem + ".flac")
            names.append(stem + ".flac")
            if not os.path.exists(dst):
                jobs.append((src, dst))
        failed = 0
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, a.jobs)) as ex:
            for ok in ex.map(lambda j: to_flac(*j), jobs):
                failed += 0 if ok else 1
        print("  %d converted (%d already there, %d failed) -> Library/Archive/LoC/%s" % (len(jobs) - failed, len(names) - len(jobs), failed, folder), flush=True)
        note_sources(slug, folder, statement, len(names), names)
        total += len(names)
    if not a.list:
        print("done: %d clips" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
