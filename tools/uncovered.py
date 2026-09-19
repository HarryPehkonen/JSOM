#!/usr/bin/env python3
"""Which lines of the formatting engine have never executed?"""
import re
from pathlib import Path

BUILD = Path("/home/harri/hermes-workspace/JSOM/build-cov/gcov-merged")
TARGET = "json_formatter.hpp"

best: dict[int, tuple[str, int]] = {}
for gcov_file in BUILD.rglob("*.gcov"):
    text = gcov_file.read_text(errors="replace")
    if f"Source:" not in text or TARGET not in text.splitlines()[0]:
        continue
    for line in text.splitlines():
        m = re.match(r"\s*([0-9]+|#####|=====):\s*(\d+):(.*)$", line)
        if not m:
            continue
        count = 0 if m.group(1) in {"#####", "====="} else int(m.group(1))
        line_no = int(m.group(2))
        source = m.group(3)
        previous = best.get(line_no)
        if previous is None or count > previous[1]:
            best[line_no] = (source, count)

if not best:
    print(f"no gcov data for {TARGET}")
    raise SystemExit(1)

uncovered = sorted(n for n, (_, c) in best.items() if c == 0)
covered = sorted(n for n, (_, c) in best.items() if c > 0)
print(f"{TARGET}: {len(covered)} covered lines, {len(uncovered)} uncovered "
      f"({100 * len(covered) / (len(covered) + len(uncovered)):.1f}%)")

# group uncovered lines into runs and show the source
runs, start, prev = [], None, None
for n in uncovered:
    if start is None:
        start = prev = n
    elif n == prev + 1:
        prev = n
    else:
        runs.append((start, prev))
        start = prev = n
if start is not None:
    runs.append((start, prev))

print(f"\nuncovered regions ({len(runs)}):")
for a, b in runs:
    body = " ".join(best[n][0].strip() for n in range(a, min(b + 1, a + 3)) if best[n][0].strip())
    print(f"  {a:>4}-{b:<4} {body[:96]}")
