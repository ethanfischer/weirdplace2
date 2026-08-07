"""
logq — triage UE logs without hand-rolled grep incantations.

Scopes to the latest PIE session (editor log) or a specific E2E test, strips
timestamp prefixes, dedupes repeated messages, and prints errors/warnings
sorted by count.

Usage:
    python scripts/logq.py                      # editor log, latest PIE session, errors + warnings
    python scripts/logq.py --session -2        # second-to-last PIE session
    python scripts/logq.py --all               # whole editor log (since editor launch)
    python scripts/logq.py --e2e               # E2ETest.log, all tests, grouped summary
    python scripts/logq.py --e2e --test CarRideScenery   # one test's scope
    python scripts/logq.py --grep "Seneca|Bladder"       # raw matching lines in scope
    python scripts/logq.py --errors-only --full          # no truncation
"""

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

# Windows consoles default to cp1252; log lines can contain arbitrary unicode.
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

PROJECT_ROOT = Path(__file__).resolve().parent.parent
EDITOR_LOG = PROJECT_ROOT / "Saved/Logs/weirdplace2.log"
E2E_LOG = PROJECT_ROOT / "Saved/Logs/E2ETest.log"

TIMESTAMP_RE = re.compile(r"^\[[\d.:-]+\]\[\s*\d+\]")
# LogAutomationController re-echoes test-context lines as
# "LogAutomationController: Warning: LogTemp: <original> [log]" — unwrap so the
# same message isn't counted twice at two severities.
AUTOMATION_ECHO_RE = re.compile(
    r"^LogAutomationController: (?:Warning|Error|Display): (.*?) \[log\]$"
)
PIE_SESSION_RE = re.compile(r"LogPlayLevel: Creating play world package")
E2E_TEST_RE = re.compile(r"=== E2E TEST START === (\S+)")
FATAL_RE = re.compile(r"Fatal error|Assertion failed|=== Critical error|StaticShutdownAfterError")


def strip_prefix(line: str) -> str:
    line = TIMESTAMP_RE.sub("", line).strip()
    m = AUTOMATION_ECHO_RE.match(line)
    if m:
        line = m.group(1).strip()
    return line


def classify(stripped: str):
    """Return 'error', 'warning', or None for a prefix-stripped line."""
    if FATAL_RE.search(stripped):
        return "error"
    if ": Error:" in stripped:
        return "error"
    if ": Warning:" in stripped:
        return "warning"
    return None


def find_scopes(lines, marker_re, label_group=None):
    """Return list of (label, start_idx) for each marker occurrence."""
    scopes = []
    for i, line in enumerate(lines):
        # LogAutomationController re-echoes the marker line; only the original counts.
        if "LogAutomationController" in line:
            continue
        m = marker_re.search(line)
        if m:
            label = m.group(label_group) if label_group else f"session {len(scopes) + 1}"
            scopes.append((label, i))
    return scopes


def slice_scope(lines, scopes, index):
    """index is 1-based from start or negative from end; returns (label, sublines)."""
    if not scopes:
        return ("whole file (no session markers found)", lines)
    idx = index - 1 if index > 0 else len(scopes) + index
    if idx < 0 or idx >= len(scopes):
        sys.exit(f"ERROR: session index {index} out of range (found {len(scopes)} sessions)")
    label, start = scopes[idx]
    end = scopes[idx + 1][1] if idx + 1 < len(scopes) else len(lines)
    return (label, lines[start:end])


def summarize(lines, include_warnings, full, limit):
    errors, warnings = Counter(), Counter()
    for line in lines:
        stripped = strip_prefix(line)
        kind = classify(stripped)
        if kind == "error":
            errors[stripped] += 1
        elif kind == "warning":
            warnings[stripped] += 1

    def emit(title, counter):
        print(f"\n{title} ({sum(counter.values())} lines, {len(counter)} unique):")
        if not counter:
            print("  (none)")
            return
        for msg, count in counter.most_common(limit):
            if not full and len(msg) > 220:
                msg = msg[:220] + " ..."
            print(f"  {count:5d} x {msg}")
        if len(counter) > limit:
            print(f"  ... {len(counter) - limit} more unique messages (use --limit/--full)")

    emit("ERRORS", errors)
    if include_warnings:
        emit("WARNINGS", warnings)
    return sum(errors.values())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--e2e", action="store_true", help="use Saved/Logs/E2ETest.log")
    ap.add_argument("--log", type=str, help="explicit log file path")
    ap.add_argument("--session", type=int, default=-1,
                    help="PIE session to scope to (1-based, negative from end; default -1 = latest)")
    ap.add_argument("--test", type=str, help="E2E test name to scope to (implies --e2e)")
    ap.add_argument("--all", action="store_true", help="whole file, ignore session scoping")
    ap.add_argument("--sessions", action="store_true", help="just list the sessions/tests found and exit")
    ap.add_argument("--grep", type=str, help="print raw lines matching this regex within the scope")
    ap.add_argument("--errors-only", action="store_true", help="skip the warnings section")
    ap.add_argument("--full", action="store_true", help="don't truncate long messages")
    ap.add_argument("--limit", type=int, default=25, help="max unique messages per section (default 25)")
    ap.add_argument("--tail", type=int, default=120, help="max lines printed by --grep (default 120)")
    args = ap.parse_args()

    if args.test:
        args.e2e = True
    log_path = Path(args.log) if args.log else (E2E_LOG if args.e2e else EDITOR_LOG)
    if not log_path.exists():
        sys.exit(f"ERROR: log not found: {log_path}")

    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()

    if args.e2e:
        scopes = find_scopes(lines, E2E_TEST_RE, label_group=1)
    else:
        scopes = find_scopes(lines, PIE_SESSION_RE)

    if args.sessions:
        print(f"{log_path.name}: {len(scopes)} scope(s)")
        for label, start in scopes:
            ts = TIMESTAMP_RE.match(lines[start])
            print(f"  {label}  (line {start + 1}{', ' + ts.group(0) if ts else ''})")
        return

    if args.all or (args.e2e and not args.test):
        label, scoped = f"whole file ({log_path.name})", lines
    elif args.test:
        matching = [i for i, (lbl, _) in enumerate(scopes) if lbl == args.test]
        if not matching:
            names = ", ".join(sorted({lbl for lbl, _ in scopes})) or "(none)"
            sys.exit(f"ERROR: test '{args.test}' not found. Tests in log: {names}")
        # Last occurrence wins (reruns append).
        label, scoped = slice_scope(lines, scopes, matching[-1] + 1)
        label = f"test {label}"
    else:
        label, scoped = slice_scope(lines, scopes, args.session)
        label = f"PIE {label} of {len(scopes)}"

    first_ts = next((TIMESTAMP_RE.match(l).group(0) for l in scoped if TIMESTAMP_RE.match(l)), "?")
    last_ts = next((TIMESTAMP_RE.match(l).group(0) for l in reversed(scoped) if TIMESTAMP_RE.match(l)), "?")
    print(f"Scope: {label} -- {len(scoped)} lines, {first_ts} -> {last_ts}")

    if args.grep:
        pat = re.compile(args.grep)
        matches = [strip_prefix(l) for l in scoped if pat.search(l)]
        for line in matches[-args.tail:]:
            print(f"  {line}")
        print(f"\n{len(matches)} matching line(s)" +
              (f", showing last {args.tail}" if len(matches) > args.tail else ""))
        return

    if args.e2e and not args.test:
        # Grouped per-test error counts first, then the overall summary.
        print("\nPer-test error counts:")
        if scopes:
            for i, (lbl, _) in enumerate(scopes):
                _, sub = slice_scope(lines, scopes, i + 1)
                n = sum(1 for l in sub if classify(strip_prefix(l)) == "error")
                flag = "  <-- " if n else "      "
                print(f"{flag}{n:5d}  {lbl}")
        else:
            print("  (no test markers found)")

    error_count = summarize(scoped, include_warnings=not args.errors_only, full=args.full, limit=args.limit)
    sys.exit(1 if error_count else 0)


if __name__ == "__main__":
    main()
