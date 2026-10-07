#!/usr/bin/env bash
#
# std — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.std] cmd = "scripts/std.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "std (C++17, C++20, C++23)"
    local lang
    for lang in 17 20 23; do
        if ! g++ -std="c++$lang" -Wall -Wextra -Wpedantic -Werror -Iinclude \
            tools/std_probe.cpp build/libjsom_lib.a -o "$CI_LOG_DIR/std_probe_cxx$lang" \
            > "$CI_LOG_DIR/std-c++$lang.log" 2>&1; then
            ci_fail std "C++$lang build failed" "$CI_LOG_DIR/std-c++$lang.log"
        fi
        if ! "$CI_LOG_DIR/std_probe_cxx$lang" >> "$CI_LOG_DIR/std-c++$lang.log" 2>&1; then
            ci_fail std "the C++$lang probe returned non-zero" "$CI_LOG_DIR/std-c++$lang.log"
        fi
        printf '      C++%s: compiles and runs\n' "$lang"
    done
    ci_pass std
}

run_stage "$@"
