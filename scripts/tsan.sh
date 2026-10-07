#!/usr/bin/env bash
#
# tsan — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.tsan] cmd = "scripts/tsan.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "tsan (const reads from several threads)"
    if ! command -v g++ >/dev/null 2>&1; then
        ci_skip tsan "no g++ (ThreadSanitizer needs a compiler with libtsan)"
        return 0
    fi
    # One small probe binary, not the gtest suite: a TSan build of the whole test suite
    # costs minutes and this repo is header-heavy, so it would rebuild on every commit.
    cmake -S . -B "$CI_ASAN_BUILD_DIR-tsan" -DJSOM_SANITIZE=thread -DJSOM_BUILD_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Debug > "$CI_LOG_DIR/tsan-configure.log" 2>&1 \
        || ci_fail tsan "cmake configure failed" "$CI_LOG_DIR/tsan-configure.log"
    cmake --build "$CI_ASAN_BUILD_DIR-tsan" --target thread_safety_probe -j "$CI_JOBS" \
        > "$CI_LOG_DIR/tsan-build.log" 2>&1 \
        || ci_fail tsan "probe build failed" "$CI_LOG_DIR/tsan-build.log"
    if "$CI_ASAN_BUILD_DIR-tsan/thread_safety_probe" > "$CI_LOG_DIR/tsan.log" 2>&1; then
        tail -n 1 "$CI_LOG_DIR/tsan.log" | sed 's/^/      /'
        ci_pass tsan
        return 0
    fi
    ci_fail tsan "ThreadSanitizer reported a data race (or a wrong answer)" "$CI_LOG_DIR/tsan.log"
}

run_stage "$@"
