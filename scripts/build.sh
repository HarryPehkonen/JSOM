#!/usr/bin/env bash
#
# build — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.build] cmd = "scripts/build.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "build"
    cmake -S . -B "$CI_BUILD_DIR" > "$CI_LOG_DIR/configure.log" 2>&1 \
        || ci_fail build "cmake configure failed" "$CI_LOG_DIR/configure.log"
    if cmake --build "$CI_BUILD_DIR" -j "$CI_JOBS" > "$CI_LOG_DIR/build.log" 2>&1; then
        printf '    built with no warnings (-Werror)\n'
        ci_pass build
        return 0
    fi
    ci_fail build "build failed or warned" "$CI_LOG_DIR/build.log"
}

run_stage "$@"
