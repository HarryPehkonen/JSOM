#!/usr/bin/env bash
#
# JSOM's gate: the configuration every stage reads, and the helpers they share.
#
# The POLICY is gate.toml (which stages exist, which tier runs which of them, how a failure
# is recognised). The ENGINE is kit-ci, one binary installed once per machine
# (cmake --install build --prefix ~/.local). Everything a stage needs BEYOND that policy -
# a build dir, a job count, a fuzz budget - lives HERE, so the policy file stays a list of
# stages.
#
# SOURCED, never executed: scripts/gate.sh does not need it; every stage script does
# (`. "$(dirname "$0")/gate-env.sh"`). The knobs are the .ci.env knobs the old tools/ci.sh
# carried, each with the default it had there, and .ci.env is still sourced last, so a
# machine with no .ci.env behaves exactly as it did before the conversion
# (2026-10-06, card t_075c0a6f).

set -uo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT" || exit 1

# No colour from the tools: this script greps their output (warning:, error:, tests from)
# and ANSI escapes defeat the greps. The escapes this script prints itself are for the
# human reading the terminal.
export NO_COLOR=1

# git exports GIT_INDEX_FILE to a hook when the commit is made with a PATHSPEC
# (`git commit -- <path>`): it names git's TEMPORARY index for that one commit, not this
# repository's index, and every process a hook starts inherits it.
#
# This repo hits that in its OWN gate, on a cold `build` or on `pristine`: CMakeLists.txt
# FetchContents googletest, benchmark and nlohmann_json, so cmake's update step runs
# `git status` inside build/_deps/<dep>-src — with this repo's index handed to it, it reads
# entries whose blobs that clone's object store does not have and dies on the first one,
#     fatal: unable to read 691e2bdafaf312970644391de042d38c2c5972d8
#     CMake Error at .../<dep>-populate-gitupdate.cmake:186 (message): Failed to get the status
# so a pathspec commit fails its own gate at `build`, naming a dependency update, on a tree
# that builds fine — measured on Computo, whose FetchContent of JSOM is the same shape
# (cards t_9541aa62 -> t_0a9a0018). It leaves that checkout hollow too: no `.git/index`,
# empty worktree, `git status` reporting its whole tree as staged deletions.
#
# Unset it once, here, rather than `env -u` on the configure lines: the variable reaches
# every process the gate starts — all of them, the `pristine` archive build, and this repo's
# own kitprobes scripts, which run git in throwaway repositories by design — so a
# per-invocation fix covers the instance and leaves the class. Measured before choosing the
# place (a real pathspec commit in a throwaway clone, the gate's own stages run both ways):
# every input the `tree`/`format` stages read is identical and their output is byte-identical.
# Ported from the kit's templates/cpp/ci.sh at f9c3300; tools/kit-probes/git-index-file.sh
# holds this copy to it.
unset GIT_INDEX_FILE

# ---------------------------------------------------------------- defaults + config
CI_JOBS=${CI_JOBS:-$(nproc 2>/dev/null || echo 4)}
CI_BUILD_DIR=${CI_BUILD_DIR:-build}
CI_ASAN_BUILD_DIR=${CI_ASAN_BUILD_DIR:-build-asan}
CI_FUZZ_SECONDS=${CI_FUZZ_SECONDS:-20}          # smoke only; the real fuzzing is the nightly cron.
                                                # 20 not 10 since 2026-09-20: all three readings
                                                # now run per input (~870 exec/s versus ~8,200 for
                                                # the byte reading alone), so the same wall clock
                                                # does ~9x less byte work. The seconds claw that
                                                # back; CI_FUZZ_READINGS picks the readings.
CI_FUZZ_READINGS=${CI_FUZZ_READINGS:-all}       # all | byte | structured | mutator | comma list
CI_FUZZ_JSONFUZZ_DIR=${CI_FUZZ_JSONFUZZ_DIR:-}   # offline override for the JSONFuzz sibling repo
CI_LOG_DIR=${CI_LOG_DIR:-.ci-logs}
CI_STRICT_TOOLS=${CI_STRICT_TOOLS:-0}           # 1 = a missing tool fails the run instead of SKIPping
CI_KEEP_TMP=${CI_KEEP_TMP:-0}                   # 1 = keep the pristine-build temp dir for inspection
# The two hook tiers, ONE definition each. Both hooks name a tier instead of repeating a list, so a
# stage added below cannot be run by a hand run and skipped by a push (or the reverse) — which is
# what happened in FSMTable on 2026-10-06, where the gate had gained `fuzz` and the installed
# pre-push still named the kit's original eleven, so every push skipped the fuzzer. The comment
# below is the block probes/hook-tiers-agree.sh compares these two variables against.
#
#   fast  (pre-commit)  format build tests
#   full  (pre-push)    --require-clean tree format kitprobes build tests consumer asan fuzz tsan std cli conform docs tidy pristine
#
# `format` is in the fast tier deliberately: it is the one check that says "the file you are about
# to commit is not the file the formatter would write", it costs well under a second on a warm
# tree, and on 2026-10-06 its absence let an unformatted commit through in FSMTable that only a
# full run caught. `full` is also the default list below, so a hand run and a push run the same
# stages and only --require-clean differs.
CI_FAST_STAGES=${CI_FAST_STAGES:-"format build tests"}
CI_FULL_STAGES=${CI_FULL_STAGES:-"tree format kitprobes build tests consumer asan fuzz tsan std cli conform docs tidy pristine"}
CI_DEFAULT_STAGES=${CI_DEFAULT_STAGES:-$CI_FULL_STAGES}
CI_TIDY_BASELINE=${CI_TIDY_BASELINE:-.ci/tidy-baseline.txt}

if [ -f .ci.env ]; then
    # shellcheck disable=SC1091
    . ./.ci.env
fi


# The old tools/ci.sh took these as FLAGS (--require-clean, --changed, --extra-checks,
# --write-tidy-baseline, --write-coverage-baseline). kit-ci's vocabulary has no per-run flags
# for a stage: a caller sets the knob in the ENVIRONMENT it launches the gate with, and the
# default below is what the script had. .githooks/pre-push does exactly that with
# CI_REQUIRE_CLEAN=1. (Until this line the assignment overwrote whatever the caller exported,
# so the push hook's flag-as-env-var had no effect at all - measured 2026-10-06.)

REQUIRE_CLEAN=${CI_REQUIRE_CLEAN:-0}
WRITE_TIDY_BASELINE=0
STAGES_REQUESTED=()

# ---------------------------------------------------------------- plumbing
RESULT_LINES=()
RAN_STAGES=()
FAILED_STAGE=""
RUN_TMP_DIRS=()


log() { printf '%s\n' "$*"; }






require_tool() {
    local tool="$1" stage="$2"
    if command -v "$tool" >/dev/null 2>&1; then
        return 0
    fi
    if [ "$CI_STRICT_TOOLS" = "1" ]; then
        ci_fail "$stage" "$tool not installed (CI_STRICT_TOOLS=1)"
    fi
    ci_skip "$stage" "$tool not installed — gate not exercised on this machine"
    return 1
}

# The file set the format gate owns: same list in the CMake `format` target.
# Includes untracked (but not ignored) files: while working, a new source file is not in
# `git ls-files` yet, and a gate that cannot see it would let it through unformatted —
# it only surfaced the first time because the push hook saw it after it was committed.
ci_sources() {
    git ls-files --cached --others --exclude-standard -- \
        'include/jsom/*.hpp' 'src/*.cpp' 'tests/*.cpp' 'tools/*.cpp' 'benchmarks/*.cpp' \
        | sort -u
}

# The comparable form of a clang-tidy finding — applied to BOTH sides of the baseline
# comparison, so the file a repo captures and the log this gate just wrote are the same
# shape:
#
#   <repo>/src/foo.cpp:42:7: warning: ...   ->   src/foo.cpp: warning: ...
#
#   * the repo root is stripped: clang-tidy reports the path it was handed by the compile
#     database, which CMake writes as an absolute path, so a baseline captured in one clone
#     names no finding in a checkout at another path (the nightly clean checkout, a
#     colleague's machine) and every inherited finding reads as new;
#   * :line:column is stripped, so the same finding after an unrelated edit above it is
#     still the same finding. This is line-blind on purpose, and the flip side is worth
#     knowing: a SECOND identical finding in a file that already has one collapses into the
#     first. Fix the baselined finding instead of growing the baseline.
#
# `tools/ci.sh --write-tidy-baseline` captures the baseline through this same function, so
# the documented way to accept findings cannot drift from the way they are compared.
tidy_key() {
    awk -v root="$REPO_ROOT/" '
        { i = index($0, root); if (i) $0 = substr($0, i + length(root)); print }' \
        | sed 's/:[0-9]*:[0-9]*:/:/'
}

mkdir -p "$CI_LOG_DIR"


# ---------------------------------------------------------------- stage helpers
cleanup() {
    local dir
    if [ "$CI_KEEP_TMP" != "1" ]; then
        for dir in "${RUN_TMP_DIRS[@]:-}"; do
            [ -n "$dir" ] && [ -d "$dir" ] && rm -rf "$dir"
        done
    fi
}

trap cleanup EXIT

# ---------------------------------------------------------------- verdict shims
# kit-ci calls a stage and reads its EXIT STATUS: 0 is a pass, non-zero is a failure, and
# the engine names the stage and prints the first lines of a failed stage's output itself
# (SPEC.md §4). The stage bodies in this directory were split verbatim out of the old
# tools/ci.sh and still speak that script's vocabulary, so it is defined here, once:
#
#   ci_pass <stage>                 END the stage, exit 0
#   ci_fail <stage> <reason> [log]  print why, show the log's tail, exit 1
#   ci_skip <stage> <reason>        say so; the caller then exits 0 (a SKIP is not a failure,
#                                   which is also what kit-ci's `when = "tool:<name>"` means)
#   ci_begin <title>                the old banner line
ci_begin() { printf '\n==> %s\n' "$1"; }
ci_pass() { exit 0; }
ci_skip() { printf '    SKIP: %s\n' "$2"; }
ci_fail() {
    printf 'FAILED: %s — %s\n' "$1" "$2" >&2
    [ -n "${3:-}" ] && show_log "$3"
    exit 1
}
show_log() {  # show_log <file> — the tail, for out-of-order output like a build log
    local file="${1:-}"
    if [ -n "$file" ] && [ -f "$file" ]; then
        printf -- '--- %s (last 25 lines) ---\n' "$file" >&2
        tail -n 25 "$file" | sed 's/^/      /' >&2
    fi
    return 0
}
