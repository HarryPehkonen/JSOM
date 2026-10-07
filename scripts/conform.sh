#!/usr/bin/env bash
#
# conform — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.conform] cmd = "scripts/conform.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "conform"
    cmake --build "$CI_BUILD_DIR" --target jsom_conformance > "$CI_LOG_DIR/conform-build.log" 2>&1 \
        || ci_fail conform "conformance runner build failed" "$CI_LOG_DIR/conform-build.log"
    # The suite is a gate on the DEFAULT configuration: the number grammar is enforced
    # while scanning, so every judged class must pass with no flags.
    if ! "$CI_BUILD_DIR/jsom_conformance" > "$CI_LOG_DIR/conform.log" 2>&1; then
        ci_fail conform "must-accept or must-reject disagreements" "$CI_LOG_DIR/conform.log"
    fi
    grep -E "y_ must accept|n_ must reject" "$CI_LOG_DIR/conform.log" | sed 's/^/      /'
    # Loose mode is reported, not asserted: it is the documented extension mode, and the
    # number cases are EXPECTED to disagree there, so a non-zero exit is not a failure.
    "$CI_BUILD_DIR/jsom_conformance" --validation=loose 2>&1 | grep -E "n_ must reject" \
        | sed 's/^/      loose mode:   /' || true
    ci_pass conform
}

run_stage "$@"
