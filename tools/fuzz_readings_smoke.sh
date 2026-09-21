#!/usr/bin/env bash
# Contract test for JSOM_FUZZ_READINGS — which of the harness's three readings run.
#
# The knob exists because the readings cost very different amounts: the plain byte reading ran
# ~8,200 exec/s on the committed harness, while all three together run ~870 (three readings per
# input). A long byte-focused campaign wants the first alone; the gate uses this file to prove
# every reading is still live and that a deselected one really does nothing.
#
# Usage: tools/fuzz_readings_smoke.sh [path-to-fuzz_jsom]      (default build-fuzz/fuzz_jsom)
#
# Contract:
#   1. unset                      -> all three readings, and the reach guard requires all three
#   2. JSOM_FUZZ_READINGS=byte    -> only the byte reading runs; -runs=0 with the reach guard
#                                    still exits 0, because an ENABLED reading is what is required
#   3. JSOM_FUZZ_READINGS=structured,mutator -> those two only, guard satisfied, byte does nothing
#   4. JSOM_FUZZ_READINGS=bogus   -> a typo FAILS LOUDLY rather than silently disabling a reading
#
# Two things this file is careful about, because both produced a false pass while it was written:
# the per-input `readings` line reports whether a reading REACHED its laws on that input, not
# whether it is enabled (a rejected input legitimately reports byte=0), so the enabled set is
# asserted on the harness's own `readings enabled:` startup line instead; and "did a deselected
# reading do any work" is checked against the aggregate REACH COUNTS.
# The startup line is part of the contract this file exists to hold, not a convenience.
set -uo pipefail

BIN=${1:-build-fuzz/fuzz_jsom}
SEEDS=${SEEDS:-fuzz/seeds}
fails=0
checks=0

if [ ! -x "$BIN" ]; then
    echo "FAIL: no fuzz binary at $BIN (build it first: cmake -S . -B build-fuzz -DJSOM_BUILD_FUZZING=ON)"
    exit 1
fi

ok()  { checks=$((checks + 1)); printf '  ok   %s\n' "$1"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); printf '  FAIL %s\n' "$1"; }

OUT=""
RC=0
run() { # run <env-assignment-or-empty>
    local assign="$1" log
    log=$(mktemp)
    # The subshell with its stderr sent to /dev/null is for the abort case: bash otherwise
    # prints "Aborted" for the killed child, which is noise on top of the message under test.
    RC=$({
        if [ -n "$assign" ]; then
            env "$assign" JSOM_FUZZ_REQUIRE_REACH=1 "$BIN" "$SEEDS" -runs=0 > "$log" 2>&1
        else
            env -u JSOM_FUZZ_READINGS JSOM_FUZZ_REQUIRE_REACH=1 "$BIN" "$SEEDS" -runs=0 > "$log" 2>&1
        fi
        echo $?
    } 2>/dev/null)
    OUT=$(cat "$log")
    rm -f "$log"
}

enabled_line() { grep -m1 -o 'readings enabled:.*' <<< "$OUT" | sed 's/[[:space:]]*$//'; }
count() { # count <name> -> the number from the aggregate REACH COUNTS line
    grep -o "$1=[0-9]*" <<< "$OUT" | tail -1 | cut -d= -f2
}
expect_enabled() { # expect_enabled <expected names> <label>
    local got
    got=$(enabled_line)
    if [ "$got" = "readings enabled: $1" ]; then
        ok "$2: $got"
    else
        bad "$2: expected 'readings enabled: $1', got '${got:-<no startup line>}'"
    fi
}

echo "=== JSOM_FUZZ_READINGS contract ($BIN) ==="

echo "1. unset means all three readings"
run ""
[ "$RC" -eq 0 ] && ok "exit 0 with the reach guard on" || bad "exit $RC (guard should be satisfied)"
expect_enabled "byte structured mutator" "all three enabled"
[ "$(count byte_accept)" -gt 0 ] && ok "the byte reading reached its laws ($(count byte_accept) inputs)" \
    || bad "byte_accept=$(count byte_accept)"
[ "$(count structured_accept)" -gt 0 ] && ok "the structured reading reached ($(count structured_accept) inputs)" \
    || bad "structured_accept=$(count structured_accept)"
[ "$(count mutator_accept)" -gt 0 ] && ok "the mutator reading reached ($(count mutator_accept) inputs)" \
    || bad "mutator_accept=$(count mutator_accept)"

echo "2. byte only"
run "JSOM_FUZZ_READINGS=byte"
[ "$RC" -eq 0 ] && ok "exit 0 (only an ENABLED reading is required to reach)" \
    || bad "exit $RC — the guard still demanded a deselected reading"
expect_enabled "byte" "only the byte reading is enabled"
[ "$(count structured_accept)" -eq 0 ] && [ "$(count mutator_accept)" -eq 0 ] \
    && ok "the deselected readings did no work (structured_accept=0 mutator_accept=0)" \
    || bad "a deselected reading still ran: structured_accept=$(count structured_accept) mutator_accept=$(count mutator_accept)"
[ "$(count byte_accept)" -gt 0 ] && ok "the selected reading still did its work ($(count byte_accept) inputs)" \
    || bad "byte_accept=$(count byte_accept) — the selected reading stopped working"

echo "3. structured,mutator"
run "JSOM_FUZZ_READINGS=structured,mutator"
[ "$RC" -eq 0 ] && ok "exit 0 with the guard on" || bad "exit $RC"
expect_enabled "structured mutator" "exactly the named readings are enabled"
[ "$(count byte_accept)" -eq 0 ] && ok "the byte reading did no work (byte_accept=0)" \
    || bad "byte_accept=$(count byte_accept) — the deselected byte reading still ran"

echo "4. a typo must fail loudly"
run "JSOM_FUZZ_READINGS=bogus"
[ "$RC" -ne 0 ] && ok "exit $RC (non-zero) on an unknown reading" || bad "exit 0 — a typo silently disabled readings"
grep -qi "bogus" <<< "$OUT" && ok "the message names the offending token" || bad "the failure does not name 'bogus'"

echo
if [ "$fails" -eq 0 ]; then
    echo "fuzz_readings contract: $checks checks passed, 0 failed"
    exit 0
fi
echo "fuzz_readings contract: $fails of $checks checks FAILED"
exit 1
