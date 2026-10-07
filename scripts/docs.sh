#!/usr/bin/env bash
#
# docs — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.docs] cmd = "scripts/docs.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "docs (no retired identifiers; generated tables match the code)"
    python3 "$REPO_ROOT/tools/check_docs.py" > "$CI_LOG_DIR/docs.log" 2>&1 \
        || ci_fail docs "the documentation has drifted from the code" "$CI_LOG_DIR/docs.log"
    tail -n 1 "$CI_LOG_DIR/docs.log" | sed 's/^/      /'
    ci_pass docs
}

run_stage "$@"
