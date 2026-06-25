#!/usr/bin/env python3
"""Coverage gate + exclusion ratchet.

Two checks over the library sources (src/, include/hegel/):

1. Coverage gate: every line that is NOT excluded from coverage must be
   covered. Purely structural uncovered lines (closing braces and
   punctuation) are tolerated, since gcov attributes them inconsistently.

2. Exclusion ratchet: the number of lines hidden from coverage via
   `// GCOVR_EXCL_LINE` and `// GCOVR_EXCL_START` .. `// GCOVR_EXCL_STOP`
   markers must exactly match `.github/coverage-ratchet.json`. Changing what
   is excluded — in either direction — requires a deliberate edit to that
   file, keeping exclusions under human review. The count comes from the
   source markers, so it is identical regardless of compiler/gcov.

Usage: check-coverage.py <gcovr-json>
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

RATCHET_FILE = Path(".github/coverage-ratchet.json")
SOURCE_DIRS = [Path("src"), Path("include/hegel")]
SOURCE_GLOBS = ("*.cpp", "*.h")

EXCL_LINE = re.compile(r"//\s*GCOVR_EXCL_LINE\b")
EXCL_START = re.compile(r"//\s*GCOVR_EXCL_START\b")
EXCL_STOP = re.compile(r"//\s*GCOVR_EXCL_STOP\b")
# A line that is only braces / brackets / parens / separators carries no
# testable behaviour; gcov reports such lines as uncovered inconsistently.
STRUCTURAL = re.compile(r"^[{}()\[\];,\s]*$")


def count_excluded() -> int:
    """Count non-blank source lines under coverage-exclusion markers."""
    total = 0
    for src_dir in SOURCE_DIRS:
        for glob in SOURCE_GLOBS:
            for path in sorted(src_dir.rglob(glob)):
                in_block = False
                for line in path.read_text().splitlines():
                    if EXCL_START.search(line):
                        in_block = True
                        continue
                    if EXCL_STOP.search(line):
                        in_block = False
                        continue
                    if in_block:
                        if line.strip():
                            total += 1
                    elif EXCL_LINE.search(line):
                        total += 1
    return total


def _source_lines(path: Path, cache: dict[Path, list[str]]) -> list[str]:
    if path not in cache:
        try:
            cache[path] = path.read_text().splitlines()
        except OSError:
            cache[path] = []
    return cache[path]


def find_gaps(gcovr_json: Path) -> list[tuple[str, int, str]]:
    """Return (file, line, content) for uncovered, non-excluded code lines."""
    data = json.loads(gcovr_json.read_text())
    cache: dict[Path, list[str]] = {}
    gaps: list[tuple[str, int, str]] = []
    for f in data.get("files", []):
        rel = f["file"]
        lines = _source_lines(Path(rel), cache)
        for record in f.get("lines", []):
            if record.get("gcovr/excluded"):
                continue
            if record.get("count", 0) != 0:
                continue
            n = record["line_number"]
            content = lines[n - 1] if 1 <= n <= len(lines) else ""
            if STRUCTURAL.match(content):
                continue
            gaps.append((rel, n, content.strip()))
    return gaps


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check-coverage.py <gcovr-json>", file=sys.stderr)
        return 2
    gcovr_json = Path(sys.argv[1])

    # 1. Coverage gate: 100% of non-excluded lines.
    gaps = find_gaps(gcovr_json)
    if gaps:
        print(f"\nUncovered, non-excluded lines ({len(gaps)}):", file=sys.stderr)
        for rel, n, content in gaps:
            print(f"  {rel}:{n}: {content}", file=sys.stderr)
        print(
            "\nEvery non-excluded line must be covered. Add tests, or — if a "
            "line is genuinely unreachable — mark it // GCOVR_EXCL_LINE (or "
            "wrap a region in // GCOVR_EXCL_START .. STOP) and raise the "
            "ratchet.",
            file=sys.stderr,
        )

    # 2. Exclusion ratchet.
    excluded = count_excluded()
    print(f"Coverage-excluded lines: {excluded}")
    try:
        ratchet = json.loads(RATCHET_FILE.read_text())["excluded"]
    except (OSError, KeyError, json.JSONDecodeError) as e:
        print(f"ERROR: cannot read ratchet from {RATCHET_FILE}: {e}",
              file=sys.stderr)
        return 2

    ratchet_ok = excluded == ratchet
    if ratchet_ok:
        print(f"Coverage exclusion ratchet OK (matches {ratchet}).")
    elif excluded > ratchet:
        print(
            f'\nExclusion ratchet EXCEEDED: {excluded} > {ratchet}. If the new '
            f'exclusions are justified, raise "excluded" in {RATCHET_FILE} to '
            f"{excluded} (human review required); otherwise remove the markers.",
            file=sys.stderr,
        )
    else:
        print(
            f'\nExclusion ratchet is LOOSE: {excluded} < {ratchet}. Tighten it: '
            f'set "excluded" in {RATCHET_FILE} to {excluded}.',
            file=sys.stderr,
        )

    return 0 if (not gaps and ratchet_ok) else 1


if __name__ == "__main__":
    sys.exit(main())
