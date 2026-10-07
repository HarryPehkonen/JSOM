#!/usr/bin/env bash
#
# tidy — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.tidy] cmd = "scripts/tidy.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "tidy"
    require_tool clang-tidy tidy || return 0
    # find_program() results are CACHED: a build dir configured before clang-tidy was
    # installed keeps the "not found" branch, so re-configure before trusting this.
    cmake -S . -B "$CI_BUILD_DIR" > /dev/null 2>&1
    cmake --build "$CI_BUILD_DIR" --target tidy > "$CI_LOG_DIR/tidy.log" 2>&1
    local findings
    findings=$(grep -cE "warning:|error:" "$CI_LOG_DIR/tidy.log" || true)
    if [ -f "$CI_TIDY_BASELINE" ]; then
        local new_findings
        # Both sides go through tidy_key, and both sides are de-duplicated: one key per
        # distinct finding. A baseline captured by `--write-tidy-baseline` is already in
        # that form; anything else in the file is normalised here rather than trusted.
        new_findings=$(comm -13 \
            <(tidy_key < "$CI_TIDY_BASELINE" | sort -u) \
            <(grep -E "warning:|error:" "$CI_LOG_DIR/tidy.log" | tidy_key | sort -u) | wc -l)
        if [ "$new_findings" -gt 0 ]; then
            ci_fail tidy "$new_findings new finding(s) vs $CI_TIDY_BASELINE" "$CI_LOG_DIR/tidy.log"
        fi
        printf '    no new findings vs %s (%s total)\n' "$CI_TIDY_BASELINE" "$findings"
    else
        printf '    %s finding(s), no baseline file (%s)\n' "$findings" "$CI_TIDY_BASELINE"
        if [ "$findings" -gt 0 ]; then
            ci_fail tidy "$findings finding(s) and no baseline file — accept them in one step with 'scripts/write-tidy-baseline.sh', or fix them; see .ci.env.example" "$CI_LOG_DIR/tidy.log"
        fi
    fi
    ci_pass tidy
}

run_stage "$@"
