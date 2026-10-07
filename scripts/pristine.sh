#!/usr/bin/env bash
#
# pristine — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.pristine] cmd = "scripts/pristine.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "pristine (does HEAD build on its own?)"
    # A STABLE scratch path, so the BUILD directory can be reused between runs. With a fresh
    # mktemp directory every time, this stage recompiled googletest and gmock from scratch on
    # every push — minutes of work that pushed the whole hook past the remote's patience: the
    # SSH connection to GitHub went idle and was dropped mid-push (git died with SIGPIPE, 141)
    # after the gates had already passed.
    local scratch="$REPO_ROOT/.ci/pristine"
    local build="$REPO_ROOT/.ci/pristine-build"
    case "$scratch" in
    "$REPO_ROOT/.ci/pristine") ;;
    *) ci_fail pristine "refusing to clean an unexpected path: $scratch" ;;
    esac

    rm -rf "$scratch"
    mkdir -p "$scratch"
    git archive HEAD | tar -x -C "$scratch" \
        || ci_fail pristine "git archive failed"
    local file_count
    file_count=$(find "$scratch" -type f | wc -l)
    printf '    HEAD checked out: %s files\n' "$file_count"

    cmake -S "$scratch" -B "$build" > "$CI_LOG_DIR/pristine-configure.log" 2>&1 \
        || ci_fail pristine "a fresh clone of HEAD does not even configure (a needed file is not committed)" "$CI_LOG_DIR/pristine-configure.log"
    cmake --build "$build" -j "$CI_JOBS" > "$CI_LOG_DIR/pristine-build.log" 2>&1 \
        || ci_fail pristine "a fresh clone of HEAD does not build" "$CI_LOG_DIR/pristine-build.log"
    "$build/jsom_tests" > "$CI_LOG_DIR/pristine-tests.log" 2>&1 \
        || ci_fail pristine "tests fail on a fresh clone of HEAD" "$CI_LOG_DIR/pristine-tests.log"
    "$build/jsom" version | sed 's/^/      /'
    tail -n 2 "$CI_LOG_DIR/pristine-tests.log" | sed 's/^/      /'
    if [ "$CI_KEEP_TMP" = "1" ]; then
        printf '    tree kept at %s (CI_KEEP_TMP=1)\n' "$scratch"
    fi
    ci_pass pristine
}

run_stage "$@"
