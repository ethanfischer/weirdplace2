"""
Morning-after analysis for an overnight `-LLMCSV` run.

Finds the most recent LLM CSV under Saved/Profiling/, then compares the first
and last sample to show which memory tags grew the most. Big positive deltas
are the leak candidates.

Usage:
    python scripts/analyze_llm.py                   # default: top 20 growers
    python scripts/analyze_llm.py --top 40
    python scripts/analyze_llm.py --csv path/to/specific.csv
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
PROFILING_DIR = PROJECT_ROOT / "Saved" / "Profiling"


def find_latest_llm_csv() -> Path:
    candidates = list(PROFILING_DIR.rglob("LLM*.csv"))
    if not candidates:
        raise SystemExit(
            "No LLM CSVs found under {0}. Was the editor launched with -LLMCSV?".format(
                PROFILING_DIR
            )
        )
    candidates.sort(key=lambda p: p.stat().st_mtime)
    return candidates[-1]


def parse_value(raw: str) -> float | None:
    if raw is None:
        return None
    s = raw.strip()
    if not s:
        return None
    try:
        return float(s)
    except ValueError:
        return None


def load_first_and_last(csv_path: Path):
    with csv_path.open("r", newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header:
            raise SystemExit("Empty CSV: {0}".format(csv_path))
        first = None
        last = None
        row_count = 0
        for row in reader:
            if not row or all((c or "").strip() == "" for c in row):
                continue
            if first is None:
                first = row
            last = row
            row_count += 1
    if first is None or last is None or row_count < 2:
        raise SystemExit(
            "Need at least 2 data rows to diff; got {0} in {1}".format(row_count, csv_path)
        )
    return header, first, last, row_count


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=20, help="how many growing tags to show")
    ap.add_argument("--csv", type=str, default=None, help="explicit CSV path")
    args = ap.parse_args()

    csv_path = Path(args.csv) if args.csv else find_latest_llm_csv()
    print("File:    {0}".format(csv_path))
    print("Size:    {0:,} bytes".format(csv_path.stat().st_size))

    header, first, last, row_count = load_first_and_last(csv_path)
    print("Samples: {0}".format(row_count))
    print()

    deltas = []
    for idx, name in enumerate(header):
        v0 = parse_value(first[idx]) if idx < len(first) else None
        v1 = parse_value(last[idx]) if idx < len(last) else None
        if v0 is None or v1 is None:
            continue
        deltas.append((name, v0, v1, v1 - v0))

    if not deltas:
        raise SystemExit("No numeric columns found in CSV.")

    # LLM CSV values are typically MB. Print as-is and let column name carry units.
    deltas.sort(key=lambda r: r[3], reverse=True)

    name_w = max(len(d[0]) for d in deltas[: args.top])
    name_w = max(name_w, len("Tag"))
    print(
        "{0:<{w}}  {1:>14}  {2:>14}  {3:>14}".format(
            "Tag", "Start", "End", "Delta", w=name_w
        )
    )
    print("-" * (name_w + 2 + 14 + 2 + 14 + 2 + 14))
    for name, v0, v1, delta in deltas[: args.top]:
        print(
            "{0:<{w}}  {1:>14,.2f}  {2:>14,.2f}  {3:>+14,.2f}".format(
                name, v0, v1, delta, w=name_w
            )
        )

    print()
    print("Negative deltas (shrunk):")
    shrinkers = [d for d in deltas if d[3] < 0]
    shrinkers.sort(key=lambda r: r[3])
    for name, v0, v1, delta in shrinkers[:5]:
        print(
            "  {0:<{w}}  {1:>+14,.2f}".format(name, delta, w=name_w)
        )


if __name__ == "__main__":
    main()
