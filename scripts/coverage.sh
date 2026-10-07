#!/usr/bin/env bash
#
# coverage — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.coverage] cmd = "scripts/coverage.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    # Informational, never a gate: a coverage threshold mostly teaches people to write
    # tests that raise the number. It is here so the number is one command away.
    ci_begin "coverage (gcov, library only)"
    if ! command -v gcov >/dev/null 2>&1; then
        ci_skip coverage "gcov not installed (it ships with gcc)"
        return 0
    fi
    if ! python3 "$REPO_ROOT/tools/coverage.py" > "$CI_LOG_DIR/coverage.log" 2>&1; then
        ci_fail coverage "coverage run failed" "$CI_LOG_DIR/coverage.log"
    fi
    grep -E "= [0-9.]+%$" "$CI_LOG_DIR/coverage.log" | sed 's/^/      /'
    grep -E "^    [a-z_]+\.[ch]pp" "$CI_LOG_DIR/coverage.log" | head -6 | sed 's/^/    /'
    ci_pass coverage
}

run_stage "$@"
