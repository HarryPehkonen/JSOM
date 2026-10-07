#!/usr/bin/env bash
#
# consumer — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.consumer] cmd = "scripts/consumer.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "consumer (JSOM's defaults configure JSOM only)"
    if ! bash "$REPO_ROOT/tools/consumer_probe.sh" > "$CI_LOG_DIR/consumer.log" 2>&1; then
        grep -E '^(ok|FAIL) ' "$CI_LOG_DIR/consumer.log" | sed 's/^/      /'
        ci_fail consumer "a consumer of this repo is configured by JSOM's own defaults — the FAIL lines above name which" "$CI_LOG_DIR/consumer.log"
    fi
    tail -n 1 "$CI_LOG_DIR/consumer.log" | sed 's/^/      /'
    ci_pass consumer
}

run_stage "$@"
