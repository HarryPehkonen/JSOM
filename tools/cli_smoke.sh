#!/usr/bin/env bash
# CLI smoke tests. The `jsom` binary is 800+ lines of argument handling and it had no
# automated tests at all — every check below is a claim the CLI makes in its own help text.
#
# Usage: tools/cli_smoke.sh [path-to-jsom]     (default: build/jsom)
set -u

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
JSOM=${1:-$REPO_ROOT/build/jsom}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

check() { # check <description> <expected-exit> <command...>
    local desc=$1 expected=$2
    shift 2
    local out rc
    out=$("$@" 2>&1)
    rc=$?
    if [ "$rc" -eq "$expected" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf '      FAIL %s: exit %s (expected %s)\n        %s\n' \
            "$desc" "$rc" "$expected" "$(printf '%s' "$out" | head -2 | tr '\n' ' ')"
    fi
}

expect_output() { # expect_output <description> <pattern> <command...>
    local desc=$1 pattern=$2
    shift 2
    local out
    out=$("$@" 2>&1)
    if printf '%s' "$out" | grep -qE "$pattern"; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf '      FAIL %s: output did not match /%s/\n        %s\n' \
            "$desc" "$pattern" "$(printf '%s' "$out" | head -2 | tr '\n' ' ')"
    fi
}

printf '{"name":"jsom","n":[1,2,3],"ok":true}\n' > "$TMP/good.json"
printf '{"broken":\n' > "$TMP/bad.json"
printf '[01]\n' > "$TMP/leading_zero.json"
printf '["a\tb"]\n' > "$TMP/raw_tab.json"   # raw control char in a string: always invalid

if [ ! -x "$JSOM" ]; then
    printf '      FAIL: no executable at %s\n' "$JSOM"
    exit 1
fi

check        "version exits 0"                 0 "$JSOM" version
expect_output "version prints a version"      'JSOM version [0-9]+\.[0-9]+\.[0-9]+' "$JSOM" version
check        "--help exits 0"                  0 "$JSOM" --help
check        "no arguments is an error"        1 "$JSOM"

check        "validate accepts good JSON"      0 "$JSOM" validate "$TMP/good.json"
check        "validate rejects broken JSON"    1 "$JSOM" validate "$TMP/bad.json"
check        "null is valid JSON"              0 "$JSOM" validate <(printf 'null\n')
check        "raw control char is rejected"    1 "$JSOM" validate "$TMP/raw_tab.json"

# The number grammar is opt-in: lazy accepts 01, --validation=numbers rejects it.
check        "lazy mode accepts 01"            0 "$JSOM" validate "$TMP/leading_zero.json"
check        "numbers mode rejects 01"         1 "$JSOM" validate --validation=numbers "$TMP/leading_zero.json"
check        "unknown validation value fails"  1 "$JSOM" validate --validation=strict "$TMP/good.json"

check        "format exits 0"                  0 "$JSOM" format "$TMP/good.json"
check        "format --validation=numbers"     0 "$JSOM" format --validation=numbers "$TMP/good.json"
check        "format rejects broken JSON"      1 "$JSOM" format "$TMP/bad.json"
expect_output "format output is JSON"         '^\{' "$JSOM" format "$TMP/good.json"

check        "pointer get resolves a path"     0 "$JSOM" pointer get /name "$TMP/good.json"
expect_output "pointer get prints the value"  '"jsom"' "$JSOM" pointer get /name "$TMP/good.json"
check        "pointer get fails on a miss"     1 "$JSOM" pointer get /nope "$TMP/good.json"
check        "pointer set writes a value"      0 "$JSOM" pointer set /name other "$TMP/good.json"
check        "pointer exists answers"          0 "$JSOM" pointer exists /name "$TMP/good.json"
check        "pointer list enumerates"         0 "$JSOM" pointer list "$TMP/good.json"

# Flags deleted with the path cache must fail cleanly, not crash or be silently ignored.
check        "removed --cache-warm fails"      1 "$JSOM" pointer benchmark --cache-warm "$TMP/good.json"

printf '      %s checks passed, %s failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
