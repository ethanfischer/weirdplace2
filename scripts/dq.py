"""
dq — lint and preview the game's dialogue files (Content/Dialogue/).

    python scripts/dq.py lint               # lint all dialogue files; exit 1 on error
    python scripts/dq.py lint --file Content/Dialogue/Seneca.txt
    python scripts/dq.py preview            # write + open Saved/DialoguePreview/preview.html
    python scripts/dq.py preview --no-open  # write only

Format (parsed identically by Source/weirdplace2/DialogueScript.cpp):

    == SectionName ==
    # comment
    You found it?
    Rick: And of course that took me a week to research
    [Give key]

Blank lines ignored. Optional single-word "Speaker:" prefix (default = the NPC
owning the file). "[Tag]" lines attach an action to the preceding line.
MovieComments.txt is a separate keyed table: "KEY: line one|line two".
"""

import argparse
import json
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SCRIPTS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPTS_DIR.parent
DIALOGUE_DIR = PROJECT_ROOT / "Content/Dialogue"
SOURCE_DIR = PROJECT_ROOT / "Source"
PREVIEW_PATH = PROJECT_ROOT / "Saved/DialoguePreview/preview.html"

NPC_FILES = ["Seneca.txt", "Rick.txt", "Hudson.txt"]
MOVIE_COMMENTS = "MovieComments.txt"
KNOWN_SPEAKERS = {"Seneca", "Rick", "Hudson"}

# Exact-match table of every valid [Tag] and the section it belongs to.
# A tag anywhere else (typo'd or misplaced) is a lint error — tag typos
# previously only failed at runtime.
TAG_SECTIONS = {
    "Bladder": "CarRide",
    "Give key": "ReadyToGiveKey",
    "Give combined tape": "ReadyToGiveCombinedTape",
    "Gives Cash": "RickGivesMoney",
}

# Runtime wrap width is 700 slate units (UI_Dialogue.h BackingTextWrapWidth).
# ~55 chars/row keeps the current corpus clean (longest line: 101 chars = 2
# rows); warn when a line would wrap to 3+ rows.
CHARS_PER_ROW = 55
WARN_ROWS = 3

# Runtime typewriter timing (UI_Dialogue.cpp UpdateWithText).
TYPE_FIRST_DELAY = 0.04
TYPE_CHAR_DELAY = 0.03

SPEAKER_COLORS = {"Seneca": "#8ecae6", "Rick": "#f4a261", "Hudson": "#95d5b2"}


class Line:
    def __init__(self, speaker, text, lineno):
        self.speaker = speaker  # None => file owner
        self.text = text
        self.tag = None
        self.tag_lineno = None
        self.pause = 0.0  # [Pause N] seconds of silence after this line
        self.lineno = lineno


def parse_dialogue_file(path):
    """Parse a sectioned dialogue file. Returns (sections, errors) where
    sections is {name: [Line]} and errors is [(lineno, message)]."""
    sections = {}
    errors = []
    current = None
    for lineno, raw in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue

        if line.startswith("==") and line.endswith("==") and len(line) > 4:
            name = line[2:-2].strip()
            if name in sections:
                errors.append((lineno, f"duplicate section '{name}'"))
            current = sections.setdefault(name, [])
            continue

        if current is None:
            errors.append((lineno, f"line before any section header: {line!r}"))
            continue

        if line.startswith("[") and line.endswith("]"):
            if not current:
                errors.append((lineno, f"tag {line} has no preceding dialogue line"))
                continue
            body = line[1:-1].strip()
            # [Pause N] is timing, not an action cue (DialogueScript.cpp mirrors this)
            m = re.fullmatch(r"[Pp]ause\s+(\d+(?:\.\d+)?)", body)
            if m:
                if float(m.group(1)) <= 0:
                    errors.append((lineno, f"bad pause {line}"))
                else:
                    current[-1].pause = float(m.group(1))
                continue
            current[-1].tag = body
            current[-1].tag_lineno = lineno
            continue

        speaker, text = None, line
        colon = line.find(":")
        if colon > 0:
            prefix = line[:colon].strip()
            if " " not in prefix:
                speaker = prefix
                text = line[colon + 1:].strip() or line
        current.append(Line(speaker, text, lineno))
    return sections, errors


def referenced_sections():
    """Section names referenced from C++: FString ...Section = TEXT("Name")."""
    pat = re.compile(r'Section\w*\s*=\s*TEXT\("([^"]+)"\)')
    refs = set()
    for src in list(SOURCE_DIR.rglob("*.h")) + list(SOURCE_DIR.rglob("*.cpp")):
        refs.update(pat.findall(src.read_text(encoding="utf-8", errors="replace")))
    return refs


def rel_label(path):
    try:
        return path.relative_to(PROJECT_ROOT)
    except ValueError:
        return path.name


def lint_npc_file(path, findings):
    rel = rel_label(path)
    sections, parse_errors = parse_dialogue_file(path)
    for lineno, msg in parse_errors:
        findings.append(("error", f"{rel}:{lineno}: {msg}"))

    for name, lines in sections.items():
        for line in lines:
            if line.speaker is not None and line.speaker not in KNOWN_SPEAKERS:
                findings.append(("error",
                    f"{rel}:{line.lineno}: unknown speaker '{line.speaker}' "
                    f"(known: {', '.join(sorted(KNOWN_SPEAKERS))})"))
            if line.tag is not None:
                expected = TAG_SECTIONS.get(line.tag)
                if expected is None:
                    findings.append(("error",
                        f"{rel}:{line.tag_lineno}: unknown tag '[{line.tag}]' "
                        f"(known: {', '.join(sorted(TAG_SECTIONS))})"))
                elif expected != name:
                    findings.append(("error",
                        f"{rel}:{line.tag_lineno}: tag '[{line.tag}]' belongs in "
                        f"section '{expected}', found in '{name}'"))
            rows = -(-len(line.text) // CHARS_PER_ROW)
            if rows >= WARN_ROWS:
                findings.append(("warn",
                    f"{rel}:{line.lineno}: line wraps to ~{rows} rows "
                    f"({len(line.text)} chars; 700-unit plate fits ~{CHARS_PER_ROW}/row)"))
    return sections


def lint_movie_comments(path, findings):
    rel = rel_label(path)
    for lineno, raw in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not raw.strip():
            continue
        if ": " not in raw:
            findings.append(("error", f"{rel}:{lineno}: malformed row (expected 'KEY: a|b'): {raw[:60]!r}"))
            continue
        key, comment = raw.split(": ", 1)
        if not key.strip() or not comment.strip():
            findings.append(("error", f"{rel}:{lineno}: empty key or comment"))
        if comment.count("|") > 1:
            findings.append(("error", f"{rel}:{lineno}: more than one '|' (runtime shows at most 2 lines)"))


def run_lint(only_file=None):
    findings = []

    if only_file:
        # Single-file mode: per-file rules only (no Source/ cross-ref).
        path = Path(only_file)
        if not path.is_file():
            print(f"ERROR no such file: {path}")
            return 1
        if path.name == MOVIE_COMMENTS:
            lint_movie_comments(path, findings)
        else:
            lint_npc_file(path, findings)
        return report(findings)

    all_sections = {}  # name -> file
    for fname in NPC_FILES:
        path = DIALOGUE_DIR / fname
        if not path.is_file():
            findings.append(("error", f"missing dialogue file: {path.relative_to(PROJECT_ROOT)}"))
            continue
        sections = lint_npc_file(path, findings)
        for name in sections:
            if name in all_sections:
                findings.append(("error",
                    f"{fname}: section '{name}' also defined in {all_sections[name]}"))
            else:
                all_sections[name] = fname

    refs = referenced_sections()
    for name, fname in sorted(all_sections.items()):
        if name not in refs:
            findings.append(("error", f"{fname}: section '{name}' not referenced anywhere in Source/"))
    for name in sorted(refs - set(all_sections)):
        findings.append(("error", f"Source/ references section '{name}' but no dialogue file defines it"))

    mc = DIALOGUE_DIR / MOVIE_COMMENTS
    if mc.is_file():
        lint_movie_comments(mc, findings)
    else:
        findings.append(("error", f"missing {MOVIE_COMMENTS}"))

    return report(findings)


def report(findings):
    errors = [m for lvl, m in findings if lvl == "error"]
    warns = [m for lvl, m in findings if lvl == "warn"]
    for m in errors:
        print(f"ERROR {m}")
    for m in warns:
        print(f"WARN  {m}")
    print(f"dq lint: {len(errors)} error(s), {len(warns)} warning(s)")
    return 1 if errors else 0


# --- preview ---

def type_seconds(text):
    return TYPE_FIRST_DELAY + TYPE_CHAR_DELAY * len(text)


def build_preview():
    npc_data = []
    findings = []
    for fname in NPC_FILES:
        path = DIALOGUE_DIR / fname
        if not path.is_file():
            continue
        owner = path.stem
        sections = lint_npc_file(path, findings)
        npc_data.append({
            "npc": owner,
            "sections": [
                {
                    "name": name,
                    "lines": [
                        {
                            "speaker": ln.speaker or owner,
                            "text": ln.text,
                            "tag": ln.tag,
                            "pause": ln.pause,
                            "chars": len(ln.text),
                            "secs": round(type_seconds(ln.text), 2),
                            "rows": -(-len(ln.text) // CHARS_PER_ROW),
                        }
                        for ln in lines
                    ],
                }
                for name, lines in sections.items()
            ],
        })

    comments = []
    mc = DIALOGUE_DIR / MOVIE_COMMENTS
    if mc.is_file():
        for raw in mc.read_text(encoding="utf-8-sig").splitlines():
            if ": " in raw:
                key, comment = raw.split(": ", 1)
                comments.append([key.strip(), comment.strip()])

    lint_notes = [f"{lvl.upper()} {msg}" for lvl, msg in findings]

    data_json = json.dumps({"npcs": npc_data, "comments": comments, "lint": lint_notes},
                           ensure_ascii=False)

    # Script-element content is raw text — only `</` needs defusing.
    page = PREVIEW_TEMPLATE.replace("__DATA__", data_json.replace("</", "<\\/"))
    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    PREVIEW_PATH.write_text(page, encoding="utf-8")
    print(f"wrote {PREVIEW_PATH}")
    return PREVIEW_PATH


PREVIEW_TEMPLATE = r"""<!doctype html>
<html><head><meta charset="utf-8"><title>weirdplace2 dialogue preview</title>
<style>
  body { background: #1b1b22; color: #ddd; font-family: Segoe UI, sans-serif; margin: 0; }
  .wrap { max-width: 1100px; margin: 0 auto; padding: 24px; }
  h1 { font-size: 18px; color: #aaa; }
  h2 { font-size: 15px; color: #8ecae6; margin: 28px 0 6px; }
  .section { border: 1px solid #333; border-radius: 8px; margin: 10px 0; padding: 10px 14px; }
  .sec-head { display: flex; align-items: center; gap: 12px; cursor: pointer; }
  .sec-name { font-weight: 600; }
  .meta { color: #777; font-size: 12px; }
  .lineRow { display: flex; align-items: center; gap: 10px; margin: 6px 0; }
  .lineRow button { background: #2c2c38; color: #bbb; border: 1px solid #444; border-radius: 4px; cursor: pointer; }
  .speaker { font-weight: 600; min-width: 64px; }
  .chip { background: #6d3b96; color: #fff; border-radius: 10px; padding: 1px 8px; font-size: 11px; }
  .stats { color: #666; font-size: 11px; margin-left: auto; white-space: nowrap; }
  /* In-game plate look: black backing 0.7 opacity, 60x40 padding, 700px wrap, white text */
  #plateHolder { position: sticky; top: 0; background: #1b1b22; padding: 12px 0; z-index: 5; }
  #plate { background: rgba(0,0,0,0.7); padding: 40px 60px; max-width: 700px; border-radius: 6px;
           min-height: 60px; box-sizing: border-box; }
  #plateSpeaker { font-weight: 700; margin-bottom: 8px; }
  #plateText { color: #fff; font-size: 17px; line-height: 1.45; white-space: pre-wrap; }
  .lint { background: #3a1d1d; border: 1px solid #7a3030; border-radius: 6px; padding: 8px 12px;
          font-family: Consolas, monospace; font-size: 12px; white-space: pre-wrap; }
  input[type=search] { width: 320px; background: #2c2c38; border: 1px solid #444; color: #ddd;
                       padding: 6px 10px; border-radius: 4px; }
  table { border-collapse: collapse; width: 100%; font-size: 13px; }
  td, th { border-bottom: 1px solid #2e2e38; padding: 4px 8px; text-align: left; }
  th { color: #888; }
</style></head><body><div class="wrap">
<h1>weirdplace2 dialogue preview</h1>
<div id="plateHolder"><div id="plate"><div id="plateSpeaker"></div><div id="plateText">&nbsp;</div></div></div>
<div id="lint"></div>
<div id="npcs"></div>
<h2>MovieComments</h2>
<input type="search" id="mcSearch" placeholder="filter movies...">
<table id="mcTable"><thead><tr><th>Key</th><th>Comment</th></tr></thead><tbody></tbody></table>
</div>
<script type="application/json" id="data">__DATA__</script>
<script>
const DATA = JSON.parse(document.getElementById('data').textContent);
const SPEAKER_COLORS = {Seneca:'#8ecae6', Rick:'#f4a261', Hudson:'#95d5b2'};
let typeTimer = null, playQueue = [], playing = false;

function typeLine(speaker, text, done) {
  clearTimeout(typeTimer);
  const sp = document.getElementById('plateSpeaker');
  const tx = document.getElementById('plateText');
  sp.textContent = speaker; sp.style.color = SPEAKER_COLORS[speaker] || '#fff';
  tx.textContent = '';
  let i = 0;
  function step() {
    if (i < text.length) {
      tx.textContent += text[i++];
      typeTimer = setTimeout(step, 30); // 0.03s/char, matches UI_Dialogue.cpp
    } else if (done) { typeTimer = setTimeout(done, 700); }
  }
  typeTimer = setTimeout(step, 40); // 0.04s initial delay
}

function playSection(lines) {
  playQueue = lines.slice(); playing = true;
  (function next() {
    if (!playQueue.length) { playing = false; return; }
    const l = playQueue.shift();
    typeLine(l.speaker, l.text, next);
  })();
}

const npcsDiv = document.getElementById('npcs');
for (const npc of DATA.npcs) {
  const h = document.createElement('h2'); h.textContent = npc.npc + '.txt'; npcsDiv.appendChild(h);
  for (const sec of npc.sections) {
    const d = document.createElement('div'); d.className = 'section';
    const total = sec.lines.reduce((a, l) => a + l.secs + 0.7 + (l.pause || 0), 0).toFixed(1);
    d.innerHTML = '<div class="sec-head"><span class="sec-name">== ' + sec.name +
      ' ==</span><button class="playAll">&#9654; play section</button>' +
      '<span class="meta">' + sec.lines.length + ' lines, ~' + total + 's</span></div>';
    d.querySelector('.playAll').onclick = () => playSection(sec.lines);
    for (const l of sec.lines) {
      const r = document.createElement('div'); r.className = 'lineRow';
      const b = document.createElement('button'); b.innerHTML = '&#9654;';
      b.onclick = () => typeLine(l.speaker, l.text);
      r.appendChild(b);
      const s = document.createElement('span'); s.className = 'speaker';
      s.textContent = l.speaker; s.style.color = SPEAKER_COLORS[l.speaker] || '#fff';
      r.appendChild(s);
      const t = document.createElement('span'); t.textContent = l.text; r.appendChild(t);
      if (l.tag) { const c = document.createElement('span'); c.className = 'chip';
                   c.textContent = '[' + l.tag + ']'; r.appendChild(c); }
      if (l.pause) { const c = document.createElement('span'); c.className = 'chip';
                     c.textContent = '[Pause ' + l.pause + ']'; r.appendChild(c); }
      const st = document.createElement('span'); st.className = 'stats';
      st.textContent = l.chars + ' ch · ' + l.secs + 's · ' + l.rows + ' row' + (l.rows > 1 ? 's' : '');
      r.appendChild(st);
      d.appendChild(r);
    }
    npcsDiv.appendChild(d);
  }
}

if (DATA.lint.length) {
  const l = document.getElementById('lint');
  l.className = 'lint'; l.textContent = DATA.lint.join('\n');
}

const tbody = document.querySelector('#mcTable tbody');
function renderComments(filter) {
  tbody.innerHTML = '';
  const f = (filter || '').toLowerCase();
  for (const [k, c] of DATA.comments) {
    if (f && !(k.toLowerCase().includes(f) || c.toLowerCase().includes(f))) continue;
    const tr = document.createElement('tr');
    const td1 = document.createElement('td'); td1.textContent = k;
    const td2 = document.createElement('td'); td2.textContent = c.replace('|', ' ⏎ ');
    tr.appendChild(td1); tr.appendChild(td2); tbody.appendChild(tr);
  }
}
document.getElementById('mcSearch').oninput = e => renderComments(e.target.value);
renderComments('');
</script>
</body></html>
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="verb", required=True)
    lint_p = sub.add_parser("lint", help="lint all dialogue files")
    lint_p.add_argument("--file", help="lint a single file (skips cross-file rules)")
    prev_p = sub.add_parser("preview", help="write + open the HTML previewer")
    prev_p.add_argument("--no-open", action="store_true")
    args = ap.parse_args()

    if args.verb == "lint":
        sys.exit(run_lint(args.file))
    else:
        path = build_preview()
        if not args.no_open:
            import os
            os.startfile(path)  # noqa


if __name__ == "__main__":
    main()
