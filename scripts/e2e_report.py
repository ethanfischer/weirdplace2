"""
e2e_report — compare E2E screenshots against golden baselines and build an
HTML gallery so a whole run can be reviewed on one page.

    python scripts/e2e_report.py                 # compare all E2E_* shots vs goldens
    python scripts/e2e_report.py --bless         # accept ALL current shots as goldens
    python scripts/e2e_report.py --bless E2E_Poster_01_Pole.png   # bless one
    python scripts/e2e_report.py --pattern "Diag_*"              # diagnostics too
    python scripts/e2e_report.py --threshold 5   # allow 5% changed pixels

Verdicts: PASS (within threshold), DIFF (over threshold; heatmap generated),
NEW (no golden yet), BAD (unreadable/zero-byte — e.g. NullRHI run).
Report: Saved/E2EReport/report.html. Exit 1 on any DIFF or BAD.
Goldens live in Tests/E2EGoldens/ (committed; PNGs are LFS-tracked).
"""

import argparse
import fnmatch
import html
import shutil
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SCREENSHOT_DIRS = [
    PROJECT_ROOT / "Saved/Screenshots/WindowsEditor",
    PROJECT_ROOT / "Saved/Screenshots/Windows",
]
GOLDEN_DIR = PROJECT_ROOT / "Tests/E2EGoldens"
REPORT_DIR = PROJECT_ROOT / "Saved/E2EReport"

PIXEL_DELTA = 25        # per-channel 0-255 delta above which a pixel counts as changed
DEFAULT_THRESHOLD = 2.0  # % changed pixels above which the verdict is DIFF


def find_screenshots(pattern):
    seen = {}
    for d in SCREENSHOT_DIRS:
        if not d.is_dir():
            continue
        for f in sorted(d.glob("*.png")):
            if fnmatch.fnmatch(f.name, pattern) and f.name not in seen:
                seen[f.name] = f
    return list(seen.values())


def load_rgb(path):
    with Image.open(path) as im:
        return np.asarray(im.convert("RGB"), dtype=np.int16)


def compare(current_path, golden_path, heatmap_path):
    """Returns (pct_changed, heatmap_written)."""
    cur = load_rgb(current_path)
    gold = load_rgb(golden_path)
    if cur.shape != gold.shape:
        with Image.open(golden_path) as g:
            with Image.open(current_path) as c:
                cur = np.asarray(c.convert("RGB").resize(g.size), dtype=np.int16)
    delta = np.abs(cur - gold).max(axis=2)
    changed = delta > PIXEL_DELTA
    pct = 100.0 * changed.mean()
    if pct > 0:
        # Heatmap: golden dimmed to grayscale, changed pixels in red.
        gray = (gold.mean(axis=2) * 0.4).astype(np.uint8)
        heat = np.stack([gray, gray, gray], axis=2)
        heat[changed] = [255, 40, 40]
        heatmap_path.parent.mkdir(parents=True, exist_ok=True)
        Image.fromarray(heat).save(heatmap_path)
        return pct, True
    return pct, False


def rel(p):
    import os
    return os.path.relpath(p, REPORT_DIR).replace("\\", "/")


def build_report(rows, threshold):
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    cards = []
    for r in rows:
        badge_color = {"PASS": "#2e7d32", "DIFF": "#c62828", "NEW": "#f9a825", "BAD": "#6a1b9a"}[r["verdict"]]
        imgs = f'<figure><figcaption>current</figcaption><img src="{rel(r["current"])}" loading="lazy"></figure>'
        if r.get("golden"):
            imgs += f'<figure><figcaption>golden</figcaption><img src="{rel(r["golden"])}" loading="lazy"></figure>'
        if r.get("heatmap"):
            imgs += f'<figure><figcaption>diff heatmap</figcaption><img src="{rel(r["heatmap"])}" loading="lazy"></figure>'
        detail = f'{r["pct"]:.2f}% pixels changed' if r.get("pct") is not None else r.get("note", "")
        cards.append(f'''
<section class="card {r["verdict"].lower()}">
  <h2><span class="badge" style="background:{badge_color}">{r["verdict"]}</span> {html.escape(r["name"])}
      <small>{html.escape(detail)}</small></h2>
  <div class="imgs">{imgs}</div>
</section>''')

    counts = {}
    for r in rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    summary = "  ".join(f"{v}: {n}" for v, n in sorted(counts.items()))
    page = f'''<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>E2E Screenshot Report</title>
<style>
  body {{ font-family: system-ui, sans-serif; margin: 1rem; background: #111; color: #eee; }}
  .card {{ border: 1px solid #333; border-radius: 8px; padding: .5rem 1rem; margin-bottom: 1rem; }}
  .card.diff, .card.bad {{ border-color: #c62828; }}
  .badge {{ color: #fff; padding: 2px 8px; border-radius: 4px; font-size: .8em; }}
  h2 {{ font-size: 1rem; }} h2 small {{ color: #999; font-weight: normal; margin-left: .5rem; }}
  .imgs {{ display: flex; gap: .5rem; overflow-x: auto; }}
  figure {{ margin: 0; }} figcaption {{ color: #888; font-size: .75rem; }}
  img {{ max-height: 260px; display: block; }}
</style></head>
<body>
<h1>E2E Screenshot Report</h1>
<p>{time.strftime("%Y-%m-%d %H:%M:%S")} &mdash; threshold {threshold}% &mdash; {summary}</p>
{"".join(cards)}
</body></html>'''
    out = REPORT_DIR / "report.html"
    out.write_text(page, encoding="utf-8")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pattern", default="E2E_*", help="screenshot filename glob (default E2E_*)")
    ap.add_argument("--threshold", type=float, default=DEFAULT_THRESHOLD,
                    help=f"%% changed pixels above which verdict is DIFF (default {DEFAULT_THRESHOLD})")
    ap.add_argument("--bless", nargs="*", default=None,
                    help="copy current screenshots to goldens (no names = all matching pattern)")
    args = ap.parse_args()

    shots = find_screenshots(args.pattern)
    if not shots:
        sys.exit(f"ERROR: no screenshots matching '{args.pattern}' under Saved/Screenshots/")

    if args.bless is not None:
        targets = shots if not args.bless else [s for s in shots if s.name in args.bless]
        missing = set(args.bless or []) - {t.name for t in targets}
        if missing:
            sys.exit(f"ERROR: --bless names not found among screenshots: {sorted(missing)}")
        GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
        for t in targets:
            shutil.copy2(t, GOLDEN_DIR / t.name)
            print(f"blessed {t.name}")
        print(f"({len(targets)} golden(s) written to {GOLDEN_DIR})")
        return

    rows = []
    for shot in shots:
        row = {"name": shot.name, "current": shot, "pct": None}
        golden = GOLDEN_DIR / shot.name
        try:
            if shot.stat().st_size == 0:
                raise OSError("zero-byte file")
            if not golden.exists():
                load_rgb(shot)  # still validate readability
                row.update(verdict="NEW", note="no golden — bless when it looks right")
            else:
                heatmap = REPORT_DIR / f"diff_{shot.name}"
                pct, wrote = compare(shot, golden, heatmap)
                row.update(golden=golden, pct=pct,
                           verdict="DIFF" if pct > args.threshold else "PASS",
                           heatmap=heatmap if wrote and pct > args.threshold else None)
        except OSError as e:
            row.update(verdict="BAD", note=f"unreadable: {e} (NullRHI run? use -Headed)")
        rows.append(row)

    order = {"DIFF": 0, "BAD": 1, "NEW": 2, "PASS": 3}
    rows.sort(key=lambda r: (order[r["verdict"]], r["name"]))
    report = build_report(rows, args.threshold)

    width = max(len(r["name"]) for r in rows)
    for r in rows:
        detail = f'{r["pct"]:.2f}%' if r["pct"] is not None else r.get("note", "")
        print(f'{r["verdict"]:<5} {r["name"]:<{width}} {detail}')
    bad = sum(1 for r in rows if r["verdict"] in ("DIFF", "BAD"))
    print(f"\nreport: {report}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
