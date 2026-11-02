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

expect_absent() { # expect_absent <description> <pattern> <command...>
    local desc=$1 pattern=$2
    shift 2
    local out
    out=$("$@" 2>&1)
    if printf '%s' "$out" | grep -qE -e "$pattern"; then
        fail=$((fail + 1))
        printf '      FAIL %s: output still matched /%s/\n' "$desc" "$pattern"
    else
        pass=$((pass + 1))
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

# The number grammar is the default now; --validation=loose is the extension switch.
check        "default rejects 01"              1 "$JSOM" validate "$TMP/leading_zero.json"
check        "loose mode accepts 01"           0 "$JSOM" validate --validation=loose "$TMP/leading_zero.json"
check        "numbers is spelled out"          1 "$JSOM" validate --validation=numbers "$TMP/leading_zero.json"
check        "unknown validation value fails"  1 "$JSOM" validate --validation=strict "$TMP/good.json"

check        "format exits 0"                  0 "$JSOM" format "$TMP/good.json"
check        "format --validation=loose"       0 "$JSOM" format --validation=loose "$TMP/good.json"
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

# ---------------------------------------------------------------- regressions
# pointer --help used to advertise --max-depth/--include-values/--format, leftovers of
# the deleted path cache that pointer_command REJECTS ("Unknown option", exit 1). A help
# screen must not name an option the CLI refuses, so the flags are gone from both.
expect_absent "pointer --help drops the deleted path-cache flags" \
    '--max-depth|--include-values|--format' "$JSOM" pointer --help

# `format` starts from the pretty preset, so its help must quote the preset's values, not
# the bare JsonFormatOptions defaults (they differ for inline arrays: 8 vs 10, and width:
# 100 vs 120). The numbers are printed from the same constants the preset uses.
expect_output "format --help quotes the pretty inline-arrays default" \
    'inline-arrays=<n> Max array size for inlining \(default: 8\)' "$JSOM" format --help
expect_output "format --help quotes the pretty max-width default" \
    'max-width=<n>.*\(default: 100' "$JSOM" format --help

# A pointer read with no file argument reads standard input, exactly as format does and as
# man/jsom.1.scd's EXAMPLES show; the path must not be mistaken for a filename. The old
# `args.size() > expected_args` test treated the pointer operand itself as the file.
check        "pointer get reads stdin"         0 bash -c "printf '{\"users\":[{\"name\":\"Alice\"}]}' | '$JSOM' pointer get /users/0/name"
expect_output "pointer get from stdin returns the value" '"Alice"' \
    bash -c "printf '{\"users\":[{\"name\":\"Alice\"}]}' | '$JSOM' pointer get /users/0/name"
check        "pointer list reads stdin"        0 bash -c "printf '{\"a\":1}' | '$JSOM' pointer list"
check        "pointer get still reads a file"  0 "$JSOM" pointer get /name "$TMP/good.json"
expect_output "pointer get from a file returns the value" '"jsom"' \
    "$JSOM" pointer get /name "$TMP/good.json"

# RFC 6901 escapes: the key "a/b" is /a~1b and "m~n" is /m~0n. Both file and stdin forms.
printf '{"a/b":1,"m~n":2}\n' > "$TMP/escaped.json"
expect_output "pointer get resolves ~1 (a slash in a key)" '^1$' \
    "$JSOM" pointer get '/a~1b' "$TMP/escaped.json"
expect_output "pointer get resolves ~0 (a tilde in a key)" '^2$' \
    "$JSOM" pointer get '/m~0n' "$TMP/escaped.json"
expect_output "pointer get resolves ~1 from stdin"  '^1$' \
    bash -c "printf '{\"a/b\":1}' | '$JSOM' pointer get /a~1b"

printf '      %s checks passed, %s failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
