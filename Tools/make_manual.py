"""AmbientSynth -- turn the in-app help into an HTML manual and a PDF.

The help page inside the instrument is the manual, and its pictures are snapshots of the panel
itself: the real sections with their real values, taken as the page is opened. That is what keeps
them right -- a drawing of a section goes out of date the day the section changes, and nobody
notices for a year. It also means they only exist while an editor is running, so this is a two
step job:

    set AMBIENT_PRESET=Three Voices, One Key
    set AMBIENT_MANUAL=docs\\manual
    build\\...\\AmbientSynth.exe            waits five seconds, writes the folder, quits
    python Tools/make_manual.py           folder -> AmbientSynth-Manual.html -> .pdf

AMBIENT_PRESET matters as much as the rest. The pictures are of the panel as it stands, so a
preset with a source switched off gives a picture of a greyed-out section, and the manual then
illustrates its Sources chapter with a section that is doing nothing. Pick one that has all three
sources and the effects in use; Deploy/build_release.ps1 does.

Five seconds because the pictures are of the running instrument: its spectrum, its stage and its
note roll have nothing in them until it has been playing for a while, and a manual whose displays
are empty boxes is worse than one with no pictures.

The PDF is printed by Edge in headless mode. If that is not available the HTML is still written
and is perfectly readable; the manual is not held hostage by a browser.
"""
import argparse
import html
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
DIR = os.path.join(ROOT, "docs", "manual")

CSS = """
@page { size: A4; margin: 18mm 16mm 16mm 16mm; }
body { font: 10.5pt/1.55 "Segoe UI", "Helvetica Neue", Arial, sans-serif; color: #16181d;
       background: #fff; margin: 0; }
h1 { font-size: 30pt; margin: 0 0 2mm 0; letter-spacing: -0.5pt; }
h2 { font-size: 16pt; margin: 0 0 3mm 0; padding-bottom: 2mm; border-bottom: 1px solid #cfd4dc;
     color: #0c5c6b; }
h3 { font-size: 11pt; margin: 6mm 0 1.5mm 0; color: #0c5c6b; letter-spacing: 0.6pt;
     text-transform: uppercase; }
p  { margin: 0 0 3.2mm 0; }
.sub { color: #5b6470; font-size: 12pt; margin: 0 0 8mm 0; }
.cover { page-break-after: always; text-align: center; padding-top: 28mm; }
.cover img.panel { width: 100%; border: 1px solid #cfd4dc; margin-top: 10mm; }
.cover img.logo { width: 26mm; }
.facts { margin: 8mm auto 0 auto; color: #5b6470; font-size: 10pt; }
.toc { page-break-after: always; }
.toc ol { padding-left: 6mm; }
.toc li { margin: 1.2mm 0; }
.topic { page-break-before: always; }
.topic p { text-align: justify; hyphens: auto; }
figure { margin: 4mm 0 5mm 0; page-break-inside: avoid; }
figure img { max-width: 100%; border: 1px solid #d6dae1; border-radius: 3px; display: block; }
figcaption { font-size: 8.5pt; color: #6b7480; margin-top: 1.2mm; }
pre { font: 8.2pt/1.35 Consolas, "DejaVu Sans Mono", monospace; background: #f3f5f8;
      border: 1px solid #e2e6ec; border-radius: 3px; padding: 3mm;
      white-space: pre-wrap; page-break-inside: avoid; margin: 0 0 4mm 0; }
.params h3 { margin-top: 7mm; }
.params dl { margin: 0; }
.params dt { font-weight: 600; margin-top: 2.6mm; }
.params dt .key { font-weight: 400; color: #6b7480; font-family: Consolas, monospace;
                  font-size: 9pt; }
.params dd { margin: 0.4mm 0 0 0; color: #333a44; }
footer { margin-top: 10mm; padding-top: 3mm; border-top: 1px solid #cfd4dc; color: #6b7480;
         font-size: 8.5pt; }
"""


def paragraphs(text):
    """The help texts are plain prose with blank lines between paragraphs, and the occasional
    line that is a heading because it is short and ends without a full stop."""
    out = []
    for block in re.split(r"\n\s*\n", text.strip()):
        block = block.strip()
        if not block:
            continue
        lines = block.split("\n")
        if len(block) < 60 and len(lines) == 1 and not block.endswith((".", ":", "?")):
            out.append("<h3>%s</h3>" % html.escape(block))
        elif len(lines) > 3 and (block.count("->") > 2 or block.count("  ") > 4):
            # A block drawn in text: the signal flow, a table of shortcuts. Collapsing its line
            # breaks into spaces turns a diagram into one very long sentence, which is exactly
            # what happened to the first page of the first draft of this manual.
            out.append("<pre>%s</pre>" % html.escape(block))
        else:
            out.append("<p>%s</p>" % html.escape(block).replace("\n", " "))
    return "\n".join(out)


def parameter_reference(text):
    """The generated topic is
           SECTION
             Name  (key, range)
                 help text
       which is a definition list wearing plain-text clothes."""
    out, in_dl = [], False
    dt = None
    for line in text.split("\n"):
        if not line.strip():
            continue
        if not line.startswith(" "):                       # a section heading
            if in_dl:
                out.append("</dl>")
                in_dl = False
            out.append("<h3>%s</h3>" % html.escape(line.strip().title()))
            continue
        if line.startswith("      "):                       # the help text of the entry before it
            if dt is not None:
                out.append("<dd>%s</dd>" % html.escape(line.strip()))
            continue
        if not in_dl:
            out.append("<dl>")
            in_dl = True
        m = re.match(r"\s*(.*?)\s\s+\((.*)\)\s*$", line)
        if m:
            dt = m.group(1)
            out.append('<dt>%s <span class="key">(%s)</span></dt>'
                       % (html.escape(dt), html.escape(m.group(2))))
        else:
            dt = line.strip()
            out.append("<dt>%s</dt>" % html.escape(dt))
    if in_dl:
        out.append("</dl>")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=DIR, help="the folder AMBIENT_MANUAL wrote")
    ap.add_argument("--no-pdf", action="store_true")
    a = ap.parse_args()

    src = os.path.join(a.dir, "manual.json")
    if not os.path.isfile(src):
        sys.exit("no manual.json in %s -- run the standalone with AMBIENT_MANUAL set first" % a.dir)
    with open(src, encoding="utf-8") as f:
        man = json.load(f)
    topics = man["topics"]

    body = []
    logo = os.path.relpath(os.path.join(ROOT, "docs", "logo-256.png"), a.dir).replace("\\", "/")
    body.append('<div class="cover">')
    body.append('<img class="logo" src="%s" alt="">' % logo)
    body.append("<h1>AmbientSynth</h1>")
    body.append('<p class="sub">Manual &middot; version %s</p>' % html.escape(man.get("version", "")))
    if os.path.isfile(os.path.join(a.dir, "panel.png")):
        body.append('<img class="panel" src="panel.png" alt="The instrument">')
    body.append('<p class="facts">%s built-in presets and 5000 in the library &middot; '
                '%s filter shapes &middot; %s Cosmos, %s filter and %s Strike presets</p>'
                % (man.get("presets", "?"), man.get("shapes", "?"), man.get("cosmos", "?"),
                   man.get("zpresets", "?"), man.get("strike", "?")))
    body.append("</div>")

    body.append('<div class="toc"><h2>Contents</h2><ol>')
    for t in topics:
        body.append("<li>%s</li>" % html.escape(t["title"]))
    body.append("</ol></div>")

    for i, t in enumerate(topics):
        params = t["title"] == "All parameters"
        body.append('<div class="topic%s">' % (" params" if params else ""))
        body.append("<h2>%d. %s</h2>" % (i + 1, html.escape(t["title"])))
        body.append(parameter_reference(t["text"]) if params else paragraphs(t["text"]))
        for img in t["images"]:
            cap = "The panel, as it stands" if img.endswith(tuple("0123456789.png")) else ""
            if img.endswith("flow.png"):
                cap = "Signal flow"
            elif img.endswith("live.png"):
                cap = "The live display of this section"
            body.append('<figure><img src="%s" alt="">%s</figure>'
                        % (html.escape(img), ("<figcaption>%s</figcaption>" % cap) if cap else ""))
        body.append("</div>")

    body.append('<footer>AmbientSynth %s &middot; the pictures in this manual are snapshots of the '
                'instrument itself, taken while it was running. AGPL-3.0.</footer>'
                % html.escape(man.get("version", "")))

    out_html = os.path.join(a.dir, "AmbientSynth-Manual.html")
    with open(out_html, "w", encoding="utf-8") as f:
        f.write("<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
                "<title>AmbientSynth Manual</title>\n<style>%s</style></head><body>\n%s\n"
                "</body></html>\n" % (CSS, "\n".join(body)))
    print("wrote %s (%.0f KB)" % (out_html, os.path.getsize(out_html) / 1024))
    if a.no_pdf:
        return 0

    edge = next((p for p in (r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
                             r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
                             r"C:\Program Files\Google\Chrome\Application\chrome.exe")
                 if os.path.isfile(p)), None)
    if edge is None:
        print("no Edge or Chrome found -- the HTML is written, print it yourself")
        return 0
    pdf = os.path.join(a.dir, "AmbientSynth-Manual.pdf")
    if os.path.exists(pdf):
        os.remove(pdf)
    # --headless=new, not --headless. On Edge 152 the old flag exits without a word and without
    # a file; the new one prints in a second. Tried in that order so an older browser still works.
    url = "file:///" + out_html.replace("\\", "/")
    for flag in ("--headless=new", "--headless"):
        cmd = [edge, flag, "--disable-gpu", "--no-pdf-header-footer", "--print-to-pdf=" + pdf, url]
        try:
            subprocess.run(cmd, timeout=180, capture_output=True)
        except subprocess.TimeoutExpired:
            print("%s did not finish in three minutes" % flag)
            continue
        if os.path.isfile(pdf):
            break
    if os.path.isfile(pdf):
        print("wrote %s (%.1f MB)" % (pdf, os.path.getsize(pdf) / 1e6))
        return 0
    print("the browser produced no PDF; the HTML is there and prints fine by hand")
    return 1


if __name__ == "__main__":
    sys.exit(main())
