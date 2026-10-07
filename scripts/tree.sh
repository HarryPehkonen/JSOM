#!/usr/bin/env bash
#
# tree — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.tree] cmd = "scripts/tree.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "tree"
    local dirty tracked_ignored ignored_count
    dirty=$(git status --porcelain)
    if [ -n "$dirty" ]; then
        printf '    worktree is dirty:\n'
        printf '%s\n' "$dirty" | sed 's/^/      /'
        if [ "$REQUIRE_CLEAN" = "1" ]; then
            ci_fail tree "uncommitted or untracked files (nothing uncommitted is the point of this check)"
        fi
        printf '    (not failing: --require-clean was not passed)\n'
    else
        printf '    worktree clean\n'
    fi

    tracked_ignored=$(git ls-files -i -c --exclude-standard)
    if [ -n "$tracked_ignored" ]; then
        printf '%s\n' "$tracked_ignored" | sed 's/^/      /' > "$CI_LOG_DIR/tree.log"
        ci_fail tree "tracked files matched by .gitignore (stale rules) — see $CI_LOG_DIR/tree.log"
    fi

    ignored_count=$(git status --porcelain --ignored=matching | grep -c '^!!')
    printf '    .gitignore audit: no tracked file is ignored; %s ignored paths present (build dirs, corpus, logs)\n' "$ignored_count"
    ci_pass tree
}

run_stage "$@"
