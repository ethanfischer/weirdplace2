"""
sfx — search / download / import SFX from the FreeToUseSounds "All In One Bundle".

The bundle's ~18.4k-row metadata CSV lives locally; the actual .wav files are
zipped "sets" (e.g. AIR_01, AMBIENCE_59) on the Gumroad library page and are
downloaded on demand. `fetch` downloads directly: the /d/<hash> URL in
.sfxcli.json is itself the access token, so no browser or login is needed.
Downloads resume (Range) if interrupted — just rerun.

    python scripts/sfx.py search telephone ring        # any-term match (default)
    python scripts/sfx.py search --all rain metal      # every term must match
    python scripts/sfx.py sets rain                    # matches grouped by set
    python scripts/sfx.py catalog                      # downloadable sets + sizes
    python scripts/sfx.py fetch AMBIENCE_59            # direct download + extract
    python scripts/sfx.py locate 1234                  # local wav path for a RecID
    python scripts/sfx.py import 1234 --name S_Rain    # import into UE + credit

`import` needs the editor running (scripts/launch_editor.ps1) — it goes through
scripts/ue_remote_exec.py. It also appends a credit line to CREDITS.md.

Search tip: bare "phone" matches microphone/headphone/smartphone — terms are
matched on word boundaries, so use "telephone" or "phone" (boundary-matched)
rather than substrings.
"""

import argparse
import csv
import html as html_mod
import json
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SCRIPTS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPTS_DIR.parent
CREDITS_PATH = PROJECT_ROOT / "CREDITS.md"

BUNDLE_DIR = Path(r"C:\Users\ethan\Music\ALL IN ONE SOUND LIBRARY BUNDLE")
CSV_PATH = (
    BUNDLE_DIR
    / "FTUS All In One Bundle Metadata"
    / "FTUS All In One Bundle Metadata"
    / "1. All In One Bundle Until 2025 Metadata"
    / "All In One Bundle Metadata Until 2026.csv"
)
LIBRARY_DIR = BUNDLE_DIR / "Library"   # extracted wavs, mirrors CSV FilePath
ZIPS_DIR = BUNDLE_DIR / "Zips"         # downloaded zips are archived here
CONFIG_PATH = BUNDLE_DIR / ".sfxcli.json"

CREDITS_SECTION_HEADER = '### "All In One Bundle" — Free To Use Sounds (Gumroad)'
CREDITS_SECTION = f"""
{CREDITS_SECTION_HEADER}
- **Author:** Free To Use Sounds LLC (https://www.freetousesounds.com)
- **Source:** All In One Bundle, purchased on Gumroad
- **License:** All sound effects are copyright and owned by Free To Use Sounds LLC - all rights reserved (licensed for use via bundle purchase)
- **Used in:**
"""


def load_rows():
    if not CSV_PATH.exists():
        sys.exit(f"ERROR: metadata CSV not found: {CSV_PATH}")
    with open(CSV_PATH, newline="", encoding="utf-8-sig", errors="replace") as f:
        return list(csv.DictReader(f))


def set_name(row):
    """Second segment of FilePath (e.g. AIR/AIR_01/HISS/x.wav -> AIR_01) = Gumroad zip name."""
    parts = row["FilePath"].split("/")
    return parts[1] if len(parts) > 1 else "?"


def local_path(row):
    return LIBRARY_DIR / row["FilePath"].replace("/", "\\")


def match_rows(rows, terms, require_all):
    pats = [re.compile(r"\b" + re.escape(t) + r"\b", re.IGNORECASE) for t in terms]
    out = []
    for row in rows:
        blob = " ".join(
            row.get(k, "") for k in ("Filename", "Description", "Keywords", "Category", "SubCategory")
        )
        hits = [p.search(blob) is not None for p in pats]
        if (all(hits) if require_all else any(hits)):
            out.append(row)
    return out


def cmd_search(args):
    rows = match_rows(load_rows(), args.terms, args.all)
    for row in rows[: args.limit]:
        loc = " [LOCAL]" if local_path(row).exists() else ""
        print(f"{row['RecID']:>6}  {row['Duration']:>10}  {set_name(row):<14}{loc}  {row['Filename']}")
    print(f"-- {len(rows)} match(es)" + (f", showing {args.limit}" if len(rows) > args.limit else ""))


def cmd_sets(args):
    rows = match_rows(load_rows(), args.terms, args.all)
    groups = {}
    for row in rows:
        groups.setdefault(set_name(row), []).append(row)
    for name in sorted(groups):
        have = LIBRARY_DIR.joinpath(*groups[name][0]["FilePath"].split("/")[:2]).exists()
        print(f"{name:<16} {len(groups[name]):>4} match(es)" + ("  [DOWNLOADED]" if have else ""))
    print(f"-- {len(rows)} match(es) across {len(groups)} set(s)")


def load_config():
    if not CONFIG_PATH.exists():
        sys.exit(
            f"ERROR: config not found: {CONFIG_PATH}\n"
            'Create it with: {"gumroad_url": "<your All In One Bundle content page URL>"}\n'
            "(Find it at https://app.gumroad.com/library -> All In One Bundle -> copy the page URL.)"
        )
    cfg = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    if "gumroad_url" not in cfg:
        sys.exit(f'ERROR: {CONFIG_PATH} is missing "gumroad_url"')
    return cfg


def gumroad_catalog(cfg):
    """Scrape the /d/<hash> content page: {file_name: (abs_download_url, file_size)}.

    The /d/ link itself is the access token — the page and its download URLs
    work without login. Each file entry is embedded as HTML-escaped JSON.
    """
    req = urllib.request.Request(cfg["gumroad_url"], headers={"User-Agent": "Mozilla/5.0"})
    page = html_mod.unescape(urllib.request.urlopen(req).read().decode("utf-8", "replace"))
    out = {}
    for m in re.finditer(
        r'"file_name":"([^"]+)"[^{}]*?"file_size":(\d+)[^{}]*?"download_url":"([^"]+)"', page
    ):
        name, size, url = m.group(1), int(m.group(2)), m.group(3).replace("\\u0026", "&")
        # Sets appear twice: full-res 192kHz section first, then the 48kHz/24bit
        # section. Last-write-wins keeps the 48/24 variant — the right one for
        # game audio (4x smaller, UE-native rate).
        out[name] = ("https://gumroad.com" + url, size)
    if not out:
        sys.exit("ERROR: no file entries found on the Gumroad page — layout may have changed")
    return out


def download(url, dest, expected_size):
    """Stream url to dest with Range-based resume and progress output."""
    part = dest.with_suffix(dest.suffix + ".part")
    done = part.stat().st_size if part.exists() else 0
    if done >= expected_size > 0:
        part.rename(dest)
        return
    headers = {"User-Agent": "Mozilla/5.0"}
    if done:
        headers["Range"] = f"bytes={done}-"
        print(f"  resuming at {done // (1 << 20)} MB")
    resp = urllib.request.urlopen(urllib.request.Request(url, headers=headers))
    if done and resp.status != 206:
        sys.exit(f"ERROR: server ignored Range resume (HTTP {resp.status}) — delete {part} and retry")
    mode = "ab" if done else "wb"
    next_report = done
    with open(part, mode) as f:
        while True:
            chunk = resp.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
            done += len(chunk)
            if done >= next_report:
                print(f"  {done // (1 << 20)} / {expected_size // (1 << 20)} MB", flush=True)
                next_report += 200 << 20
    if expected_size and done < expected_size:
        sys.exit(f"ERROR: download incomplete ({done}/{expected_size} bytes) — rerun to resume from {part}")
    part.rename(dest)


def extract_zip(zp, set_id):
    """Extract wavs into Library/<Category>/<SET>/... to mirror the CSV FilePath layout."""
    category = set_id.rsplit("_", 1)[0]
    dest_root = LIBRARY_DIR / category / set_id
    dest_root.mkdir(parents=True, exist_ok=True)
    count = 0
    with zipfile.ZipFile(zp) as z:
        for info in z.infolist():
            if info.is_dir() or "__MACOSX" in info.filename or not info.filename.lower().endswith(".wav"):
                continue
            # Zip internal layout varies; anchor on the path after the SET dir if
            # present, else after the subcategory dir, else flat.
            parts = Path(info.filename).parts
            rel = Path(*parts[parts.index(set_id) + 1:]) if set_id in parts else Path(parts[-1])
            # \\?\ prefix: many FTUS filenames push the full path past MAX_PATH (260).
            target = Path("\\\\?\\" + str(dest_root / rel))
            target.parent.mkdir(parents=True, exist_ok=True)
            with z.open(info) as src, open(target, "wb") as dst:
                shutil.copyfileobj(src, dst)
            count += 1
    return dest_root, count


def cmd_fetch(args):
    cfg = load_config()
    ZIPS_DIR.mkdir(parents=True, exist_ok=True)
    catalog = gumroad_catalog(cfg)
    for set_id in args.sets:
        category = set_id.rsplit("_", 1)[0]
        if (LIBRARY_DIR / category / set_id).exists():
            print(f"{set_id}: already extracted, skipping")
            continue
        if set_id not in catalog:
            sys.exit(f"ERROR: '{set_id}' not on the Gumroad page. Known sets: {', '.join(sorted(catalog))}")
        url, size = catalog[set_id]
        zp = ZIPS_DIR / f"{set_id}.zip"
        if not zp.exists():
            print(f"{set_id}: downloading {size // (1 << 20)} MB...")
            download(url, zp, size)
        print(f"{set_id}: extracting {zp.name}...")
        dest, count = extract_zip(zp, set_id)
        print(f"{set_id}: {count} wav(s) -> {dest}")


def cmd_catalog(args):
    catalog = gumroad_catalog(load_config())
    for name, (_, size) in sorted(catalog.items()):
        print(f"{name:<40} {size / (1 << 30):6.2f} GB")
    print(f"-- {len(catalog)} downloadable file(s)")


def find_row(rows, key):
    by_id = [r for r in rows if r["RecID"] == key]
    if by_id:
        return by_id[0]
    by_name = [r for r in rows if key.lower() in r["Filename"].lower()]
    if len(by_name) == 1:
        return by_name[0]
    if not by_name:
        sys.exit(f"ERROR: no row matches RecID or filename substring '{key}'")
    sys.exit(f"ERROR: '{key}' is ambiguous ({len(by_name)} filename matches) — use a RecID")


def cmd_locate(args):
    row = find_row(load_rows(), args.key)
    p = local_path(row)
    if not p.exists():
        sys.exit(f"ERROR: not downloaded. Set '{set_name(row)}' — run: python scripts/sfx.py fetch {set_name(row)}")
    print(p)


def default_asset_name(row):
    """First descriptive phrase of the filename -> S_LikeThis."""
    desc = row["Filename"].split("-", 2)[-1]
    words = re.findall(r"[A-Za-z]+", desc.split(",")[0])[:4]
    return "S_" + "".join(w.capitalize() for w in words)


def cmd_import(args):
    rows = load_rows()
    row = find_row(rows, args.key)
    wav = local_path(row)
    if not wav.exists():
        sys.exit(f"ERROR: wav not downloaded. Run: python scripts/sfx.py fetch {set_name(row)}")
    asset_name = args.name or default_asset_name(row)
    code = (
        "import unreal\n"
        f"task = unreal.AssetImportTask()\n"
        f"task.filename = r'{wav}'\n"
        f"task.destination_path = '{args.dest}'\n"
        f"task.destination_name = '{asset_name}'\n"
        "task.automated = True\n"
        "task.save = True\n"
        "unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])\n"
        f"print('IMPORTED:' + '{args.dest}/{asset_name}')\n"
    )
    result = subprocess.run(
        [sys.executable, str(SCRIPTS_DIR / "ue_remote_exec.py"), "--code", code],
        capture_output=True, text=True,
    )
    out = (result.stdout or "") + (result.stderr or "")
    if "IMPORTED:" not in out:
        sys.exit(
            f"ERROR: UE import failed (is the editor running? scripts/launch_editor.ps1).\nOutput:\n{out}"
        )
    print(f"Imported {wav.name}\n  -> {args.dest}/{asset_name}")
    update_credits(row, f"{args.dest}/{asset_name}")


def update_credits(row, asset_path):
    text = CREDITS_PATH.read_text(encoding="utf-8")
    if CREDITS_SECTION_HEADER not in text:
        text = text.rstrip() + "\n" + CREDITS_SECTION
    line = f"  - `{asset_path}` — \"{row['Filename']}\" (RecID {row['RecID']})\n"
    if line in text:
        return
    idx = text.index(CREDITS_SECTION_HEADER)
    used_in = text.index("- **Used in:**", idx) + len("- **Used in:**")
    eol = text.index("\n", used_in) + 1
    text = text[:eol] + line + text[eol:]
    CREDITS_PATH.write_text(text, encoding="utf-8")
    print(f"CREDITS.md updated ({asset_path})")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    for name in ("search", "sets"):
        p = sub.add_parser(name)
        p.add_argument("terms", nargs="+")
        p.add_argument("--all", action="store_true", help="require every term (default: any)")
        if name == "search":
            p.add_argument("-n", "--limit", type=int, default=30)

    p = sub.add_parser("fetch")
    p.add_argument("sets", nargs="+", metavar="SET", help="e.g. AMBIENCE_59")

    sub.add_parser("catalog", help="list downloadable sets + sizes from Gumroad")

    p = sub.add_parser("locate")
    p.add_argument("key", help="RecID or filename substring")

    p = sub.add_parser("import")
    p.add_argument("key", help="RecID or filename substring")
    p.add_argument("--name", help="asset name (default derived from filename)")
    p.add_argument("--dest", default="/Game/Sounds")

    args = ap.parse_args()
    {"search": cmd_search, "sets": cmd_sets, "fetch": cmd_fetch, "catalog": cmd_catalog,
     "locate": cmd_locate, "import": cmd_import}[args.cmd](args)


if __name__ == "__main__":
    main()
