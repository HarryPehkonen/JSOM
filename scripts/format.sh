#!/usr/bin/env bash
#
# format — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.format] cmd = "scripts/format.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "format"
    require_tool clang-format format || return 0
    local -a sources=()
    mapfile -t sources < <(ci_sources)
    if clang-format --dry-run -Werror "${sources[@]}" > "$CI_LOG_DIR/format.log" 2>&1; then
        # The check above read the WORKING TREE, and a commit records the INDEX. Stage an unformatted
        # file, then format it on disk -- what anyone does after this stage rejects a commit -- and the
        # check above passes while the commit still records the unformatted text. HEAD then differs from
        # the working tree, and the next `--require-clean` push fails in `tree` with "uncommitted changes
        # to tracked files": a message that never mentions formatting. Kit fix `format-checks-staged`
        # (kit commit 96719c4, docs/KIT-FIXES.md); the probe is in tools/kit-probes/.
        local -a staged=() index_drift=()
        local staged_f
        while IFS= read -r staged_f; do
            [ -n "$staged_f" ] && [ -f "$staged_f" ] && [[ "$staged_f" =~ \.(cpp|cc|cxx|hpp|hh|h)$ ]] && staged+=("$staged_f")
        done < <(git diff --cached --name-only --diff-filter=ACMR)
        if [ "${#staged[@]}" -gt 0 ]; then
            for staged_f in "${staged[@]}"; do
                git show ":$staged_f" 2>/dev/null |
                    clang-format --dry-run -Werror --assume-filename="$staged_f" - > /dev/null 2>> "$CI_LOG_DIR/format.log" ||
                    index_drift+=("$staged_f")
            done
            if [ "${#index_drift[@]}" -gt 0 ]; then
                printf '    the STAGED copy is not formatted (that is what the commit would record):\n'
                printf '%s\n' "${index_drift[@]}" | sed 's/^/      /'
                ci_fail format "the staged copy of the file(s) above fails clang-format -- this stage checks the working tree, and a commit records the index (fix: clang-format -i <files> && git add <files>)" "$CI_LOG_DIR/format.log"
                return 1
            fi
        fi
        printf '    %s files conform to .clang-format\n' "${#sources[@]}"
        ci_pass format
        return 0
    fi
    grep -oE '^[^:]+\.(cpp|hpp)' "$CI_LOG_DIR/format.log" | sort -u | sed 's/^/      /'
    ci_fail format "clang-format drift (fix with: clang-format -i \$(git ls-files 'include/jsom/*.hpp' 'src/*.cpp' 'tests/*.cpp' 'tools/*.cpp' 'benchmarks/*.cpp'))" "$CI_LOG_DIR/format.log"
}

run_stage "$@"
