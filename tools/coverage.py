#!/usr/bin/env python3
"""Line coverage for the JSOM library, measured with gcov.

Counts only the objects from the test binary (a gcda left behind by a binary that was
built but never RUN reports 0% for every line it instantiated, which silently wrecks the
total), and only files under include/jsom/ and src/.

    python3 tools/coverage.py            # configure + build build-cov, run, report
    python3 tools/coverage.py --report   # report from an existing build-cov
"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build-cov"
BINARY = "jsom_tests"


def run(cmd: list[str], cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ! {' '.join(cmd)} -> rc={result.returncode}")
        print((result.stdout + result.stderr)[-600:])
    return result


def gcov_objects() -> list[Path]:
    """Objects belonging to the test binary only."""
    return [p for p in BUILD.rglob("*.gcda") if "jsom_tests.dir" in str(p)]


def collect() -> dict[str, tuple[int, int]]:
    # gcov writes its report into the current directory, and a HEADER appears in the
    # report of every translation unit that includes it. Running gcov once per .gcda from
    # one directory therefore OVERWRITES the header report: whichever TU was processed
    # last won, which is how a parser that the whole suite exercises showed 0% coverage.
    # So: one output directory per object, then merge.
    out_root = BUILD / "gcov-merged"
    if out_root.exists():
        shutil.rmtree(out_root)

    objects = list(BUILD.rglob("*.gcda"))
    for index, gcda in enumerate(objects):
        out_dir = out_root / f"{index:03d}"
        out_dir.mkdir(parents=True)
        run(["gcov", "-p", "-o", str(gcda.parent), str(gcda)], cwd=out_dir)

    # Merge per source PATH and per LINE, keeping the max: a line counts as covered if any
    # translation unit executed it.
    lines: dict[str, dict[int, int]] = {}
    for gcov_file in out_root.rglob("*.gcov"):
        text = gcov_file.read_text(errors="replace")
        source = ""
        for line in text.splitlines():
            if "Source:" in line and line.startswith("        -:"):
                source = line.split("Source:", 1)[1].strip()
                break
        if not source or not re.search(r"(include/jsom/|/src/)[^/]+\.(hpp|cpp)$", source):
            continue
        per_line = lines.setdefault(source, {})
        for line in text.splitlines():
            m = re.match(r"\s*([0-9]+|#####|=====):\s*(\d+):", line)
            if not m:
                continue
            count = 0 if m.group(1) in {"#####", "====="} else int(m.group(1))
            line_no = int(m.group(2))
            per_line[line_no] = max(per_line.get(line_no, 0), count)

    return {Path(src).name: (sum(1 for c in per_line.values() if c > 0), len(per_line))
            for src, per_line in lines.items() if per_line}


def main() -> int:
    if "--report" not in sys.argv:
        print("configuring + building build-cov with --coverage ...")
        run(["cmake", "-S", ".", "-B", "build-cov", "-DCMAKE_BUILD_TYPE=Debug",
             "-DCMAKE_CXX_FLAGS=--coverage -O0 -g", "-DCMAKE_EXE_LINKER_FLAGS=--coverage"])
        run(["cmake", "--build", "build-cov", "-j4", "--target", BINARY])
        result = run([f"./{BINARY}"], cwd=BUILD)
        for line in result.stdout.splitlines():
            if "PASSED" in line or "FAILED" in line:
                print("  " + line.strip())

    per_file = collect()
    if not per_file:
        print("no gcov data found (build with --coverage first)")
        return 1

    executed = sum(ex for ex, _ in per_file.values())
    total = sum(tot for _, tot in per_file.values())
    print("\n=== library line coverage (test suite only) ===")
    print(f"  {executed}/{total} lines = {100.0 * executed / total:.1f}%\n")
    print("  per file, lowest coverage first:")
    for name, (ex, tot) in sorted(per_file.items(), key=lambda kv: kv[1][0] / kv[1][1]):
        print(f"    {name:<32} {100.0 * ex / tot:>5.1f}%  ({ex}/{tot})")
    print("\n  note: src/jsom_cli.cpp is exercised by the `jsom` binary, not by this test")
    print("        binary, so it is absent here — the CLI has no automated tests yet.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
