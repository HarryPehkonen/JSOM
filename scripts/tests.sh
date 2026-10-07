#!/usr/bin/env bash
#
# tests — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.tests] cmd = "scripts/tests.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "tests"
    "$CI_BUILD_DIR/jsom_tests" > "$CI_LOG_DIR/tests.log" 2>&1 \
        || ci_fail tests "test failures" "$CI_LOG_DIR/tests.log"
    tail -n 2 "$CI_LOG_DIR/tests.log" | sed 's/^/      /'
    ci_pass tests
}

run_stage "$@"
