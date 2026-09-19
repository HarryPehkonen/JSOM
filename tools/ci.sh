#!/usr/bin/env bash
#
# JSOM local CI — the gates from CODING_STANDARDS.md, in one script.
#
# No GitHub, no network, no framework: this is what the git hooks in .githooks/ run,
# and you can run it by hand at any time (it is non-destructive — nothing is committed,
# staged, reverted or reformatted for you).
#
#   tools/ci.sh                      # all stages
#   tools/ci.sh build tests          # just these stages, in the order given
#   tools/ci.sh --list               # what the stages are
#   tools/ci.sh --help
#
#   git config core.hooksPath .githooks     # one-time, per clone, enables the hooks
#
# Configuration lives in .ci.env (gitignored, optional); every knob has a default
# here, so the repo works with no config at all. See .ci.env.example.
#
# Exit status: 0 only if every stage that ran passed. A failing stage stops the run,
# prints why, and leaves its full output in .ci-logs/<stage>.log.

set -uo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT" || exit 1

# ---------------------------------------------------------------- defaults + config
CI_JOBS=${CI_JOBS:-$(nproc 2>/dev/null || echo 4)}
CI_BUILD_DIR=${CI_BUILD_DIR:-build}
CI_ASAN_BUILD_DIR=${CI_ASAN_BUILD_DIR:-build-asan}
CI_FUZZ_SECONDS=${CI_FUZZ_SECONDS:-10}          # smoke only; the real fuzzing is the nightly cron
CI_LOG_DIR=${CI_LOG_DIR:-.ci-logs}
CI_STRICT_TOOLS=${CI_STRICT_TOOLS:-0}           # 1 = a missing tool fails the run instead of SKIPping
CI_KEEP_TMP=${CI_KEEP_TMP:-0}                   # 1 = keep the pristine-build temp dir for inspection
CI_DEFAULT_STAGES=${CI_DEFAULT_STAGES:-"tree format build tests asan fuzz conform tidy pristine"}
CI_TIDY_BASELINE=${CI_TIDY_BASELINE:-.ci/tidy-baseline.txt}

if [ -f .ci.env ]; then
    # shellcheck disable=SC1091
    . ./.ci.env
fi

REQUIRE_CLEAN=0
STAGES_REQUESTED=()

# ---------------------------------------------------------------- plumbing
RESULT_LINES=()
FAILED_STAGE=""
RUN_TMP_DIRS=()

usage() {
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    cat <<'EOF'

Stages:
  tree        worktree clean? (only enforced with --require-clean) + .gitignore audit
  format      clang-format drift — dry run, reports files that need reformatting
  build       cmake configure + build, zero warnings (-Werror)
  tests       ./<build>/jsom_tests
  asan        build-asan (-DJSOM_SANITIZE=ON) + the same tests under ASan+UBSan
  fuzz        libFuzzer smoke run, CI_FUZZ_SECONDS seconds, both configurations
  conform     RFC 8259 suite: asserts --validation=numbers (n_ 188/188), reports default
  tidy        clang-tidy against CI_TIDY_BASELINE (no NEW findings)
  pristine    git archive HEAD -> temp dir -> configure, build, test: proves the
              COMMITTED tree is complete (catches files that are uncommitted or ignored)

Options:
  --require-clean   make the tree stage fail when the worktree is dirty (used by pre-push)
  --strict-tools    a missing tool (clang-format/tidy) fails instead of skipping
  --list            list the default stages and exit
  --help            this text
EOF
}

log() { printf '%s\n' "$*"; }

stage_begin() {
    local name="$1"
    printf '\n\033[1m==> %s\033[0m\n' "$name"
}

stage_pass() {
    RESULT_LINES+=("pass  $1")
}

stage_skip() {
    RESULT_LINES+=("SKIP  $1 ($2)")
    printf '    SKIP: %s\n' "$2"
}

stage_fail() {
    local name="$1" reason="$2" logfile="${3:-}"
    RESULT_LINES+=("FAIL  $name")
    printf '\n\033[1;31mFAILED: %s — %s\033[0m\n' "$name" "$reason"
    if [ -n "$logfile" ] && [ -f "$logfile" ]; then
        printf '    last output from %s:\n' "$logfile"
        tail -n 25 "$logfile" | sed 's/^/      /'
        printf '    full log: %s\n' "$logfile"
    fi
    FAILED_STAGE="$name"
    summary
    exit 1
}

summary() {
    printf '\n--- ci summary ---\n'
    local line
    for line in "${RESULT_LINES[@]}"; do printf '  %s\n' "$line"; done
    if [ -n "$FAILED_STAGE" ]; then
        printf '  stopped at: %s\n' "$FAILED_STAGE"
    fi
}

require_tool() {
    local tool="$1" stage="$2"
    if command -v "$tool" >/dev/null 2>&1; then
        return 0
    fi
    if [ "$CI_STRICT_TOOLS" = "1" ]; then
        stage_fail "$stage" "$tool not installed (CI_STRICT_TOOLS=1)"
    fi
    stage_skip "$stage" "$tool not installed — gate not exercised on this machine"
    return 1
}

# The file set the format gate owns: same list in the CMake `format` target.
ci_sources() {
    git ls-files 'include/jsom/*.hpp' 'src/*.cpp' 'tests/*.cpp' 'tools/*.cpp' 'benchmarks/*.cpp'
}

mkdir -p "$CI_LOG_DIR"

# ---------------------------------------------------------------- stages
stage_tree() {
    stage_begin "tree"
    local dirty tracked_ignored ignored_count
    dirty=$(git status --porcelain)
    if [ -n "$dirty" ]; then
        printf '    worktree is dirty:\n'
        printf '%s\n' "$dirty" | sed 's/^/      /'
        if [ "$REQUIRE_CLEAN" = "1" ]; then
            stage_fail tree "uncommitted or untracked files (nothing uncommitted is the point of this check)"
        fi
        printf '    (not failing: --require-clean was not passed)\n'
    else
        printf '    worktree clean\n'
    fi

    tracked_ignored=$(git ls-files -i -c --exclude-standard)
    if [ -n "$tracked_ignored" ]; then
        printf '%s\n' "$tracked_ignored" | sed 's/^/      /' > "$CI_LOG_DIR/tree.log"
        stage_fail tree "tracked files matched by .gitignore (stale rules) — see $CI_LOG_DIR/tree.log"
    fi

    ignored_count=$(git status --porcelain --ignored=matching | grep -c '^!!')
    printf '    .gitignore audit: no tracked file is ignored; %s ignored paths present (build dirs, corpus, logs)\n' "$ignored_count"
    stage_pass tree
}

stage_format() {
    stage_begin "format"
    require_tool clang-format format || return 0
    local -a sources
    mapfile -t sources < <(ci_sources)
    if clang-format --dry-run -Werror "${sources[@]}" > "$CI_LOG_DIR/format.log" 2>&1; then
        printf '    %s files conform to .clang-format\n' "${#sources[@]}"
        stage_pass format
        return 0
    fi
    grep -oE '^[^:]+\.(cpp|hpp)' "$CI_LOG_DIR/format.log" | sort -u | sed 's/^/      /'
    stage_fail format "clang-format drift (fix with: clang-format -i \$(git ls-files 'include/jsom/*.hpp' 'src/*.cpp' 'tests/*.cpp' 'tools/*.cpp' 'benchmarks/*.cpp'))" "$CI_LOG_DIR/format.log"
}

stage_build() {
    stage_begin "build"
    cmake -S . -B "$CI_BUILD_DIR" > "$CI_LOG_DIR/configure.log" 2>&1 \
        || stage_fail build "cmake configure failed" "$CI_LOG_DIR/configure.log"
    if cmake --build "$CI_BUILD_DIR" -j "$CI_JOBS" > "$CI_LOG_DIR/build.log" 2>&1; then
        printf '    built with no warnings (-Werror)\n'
        stage_pass build
        return 0
    fi
    stage_fail build "build failed or warned" "$CI_LOG_DIR/build.log"
}

stage_tests() {
    stage_begin "tests"
    "$CI_BUILD_DIR/jsom_tests" > "$CI_LOG_DIR/tests.log" 2>&1 \
        || stage_fail tests "test failures" "$CI_LOG_DIR/tests.log"
    tail -n 2 "$CI_LOG_DIR/tests.log" | sed 's/^/      /'
    stage_pass tests
}

stage_asan() {
    stage_begin "asan"
    cmake -S . -B "$CI_ASAN_BUILD_DIR" -DJSOM_SANITIZE=ON > "$CI_LOG_DIR/asan-configure.log" 2>&1 \
        || stage_fail asan "cmake configure failed" "$CI_LOG_DIR/asan-configure.log"
    cmake --build "$CI_ASAN_BUILD_DIR" -j "$CI_JOBS" > "$CI_LOG_DIR/asan-build.log" 2>&1 \
        || stage_fail asan "sanitizer build failed" "$CI_LOG_DIR/asan-build.log"
    "$CI_ASAN_BUILD_DIR/jsom_tests" > "$CI_LOG_DIR/asan-tests.log" 2>&1 \
        || stage_fail asan "ASan/UBSan reported something" "$CI_LOG_DIR/asan-tests.log"
    tail -n 2 "$CI_LOG_DIR/asan-tests.log" | sed 's/^/      /'
    stage_pass asan
}

stage_fuzz() {
    stage_begin "fuzz (${CI_FUZZ_SECONDS}s smoke)"
    if ! command -v clang++ >/dev/null 2>&1; then
        stage_skip fuzz "clang++ not installed (libFuzzer needs clang)"
        return 0
    fi
    cmake --build "$CI_BUILD_DIR" --target build_fuzzer > "$CI_LOG_DIR/fuzz-build.log" 2>&1 \
        || stage_fail fuzz "fuzz target build failed" "$CI_LOG_DIR/fuzz-build.log"
    mkdir -p corpus
    if ./fuzz_jsom corpus fuzz/seeds -dict=fuzz/jsom.dict -artifact_prefix=corpus/ \
            -max_total_time="$CI_FUZZ_SECONDS" > "$CI_LOG_DIR/fuzz.log" 2>&1; then
        grep -E "^Done |^#[0-9]+.*cov:" "$CI_LOG_DIR/fuzz.log" | tail -n 1 | sed 's/^/      /'
        stage_pass fuzz
        return 0
    fi
    stage_fail fuzz "the fuzzer found something (artifact in corpus/, add a regression test)" "$CI_LOG_DIR/fuzz.log"
}

stage_conform() {
    stage_begin "conform"
    cmake --build "$CI_BUILD_DIR" --target jsom_conformance > "$CI_LOG_DIR/conform-build.log" 2>&1 \
        || stage_fail conform "conformance runner build failed" "$CI_LOG_DIR/conform-build.log"
    # The suite is a gate in strict mode: every judged class must pass.
    if ! "$CI_BUILD_DIR/jsom_conformance" --validation=numbers > "$CI_LOG_DIR/conform.log" 2>&1; then
        stage_fail conform "must-accept or must-reject disagreements" "$CI_LOG_DIR/conform.log"
    fi
    grep -E "y_ must accept|n_ must reject" "$CI_LOG_DIR/conform.log" | sed 's/^/      /'
    # Default mode is reported, not asserted: the number grammar is opt-in by design.
    "$CI_BUILD_DIR/jsom_conformance" 2>&1 | grep -E "n_ must reject" | sed 's/^/      default mode: /'
    stage_pass conform
}

stage_tidy() {
    stage_begin "tidy"
    require_tool clang-tidy tidy || return 0
    # find_program() results are CACHED: a build dir configured before clang-tidy was
    # installed keeps the "not found" branch, so re-configure before trusting this.
    cmake -S . -B "$CI_BUILD_DIR" > /dev/null 2>&1
    cmake --build "$CI_BUILD_DIR" --target tidy > "$CI_LOG_DIR/tidy.log" 2>&1
    local findings
    findings=$(grep -cE "warning:|error:" "$CI_LOG_DIR/tidy.log" || true)
    if [ -f "$CI_TIDY_BASELINE" ]; then
        local new_findings
        new_findings=$(comm -13 \
            <(sort "$CI_TIDY_BASELINE") \
            <(grep -E "warning:|error:" "$CI_LOG_DIR/tidy.log" | sed 's/:[0-9]*:[0-9]*:/:/' | sort) | wc -l)
        if [ "$new_findings" -gt 0 ]; then
            stage_fail tidy "$new_findings new finding(s) vs $CI_TIDY_BASELINE" "$CI_LOG_DIR/tidy.log"
        fi
        printf '    no new findings vs %s (%s total)\n' "$CI_TIDY_BASELINE" "$findings"
    else
        printf '    %s finding(s), no baseline file (%s)\n' "$findings" "$CI_TIDY_BASELINE"
        if [ "$findings" -gt 0 ]; then
            stage_fail tidy "findings with no baseline to compare against" "$CI_LOG_DIR/tidy.log"
        fi
    fi
    stage_pass tidy
}

stage_pristine() {
    stage_begin "pristine (does HEAD build on its own?)"
    local tmp
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/jsom-pristine-XXXXXX")
    RUN_TMP_DIRS+=("$tmp")
    git archive HEAD | tar -x -C "$tmp" \
        || stage_fail pristine "git archive failed"
    local file_count
    file_count=$(find "$tmp" -type f | wc -l)
    printf '    HEAD checked out: %s files\n' "$file_count"

    cmake -S "$tmp" -B "$tmp/build" > "$CI_LOG_DIR/pristine-configure.log" 2>&1 \
        || stage_fail pristine "a fresh clone of HEAD does not even configure (a needed file is not committed)" "$CI_LOG_DIR/pristine-configure.log"
    cmake --build "$tmp/build" -j "$CI_JOBS" > "$CI_LOG_DIR/pristine-build.log" 2>&1 \
        || stage_fail pristine "a fresh clone of HEAD does not build" "$CI_LOG_DIR/pristine-build.log"
    "$tmp/build/jsom_tests" > "$CI_LOG_DIR/pristine-tests.log" 2>&1 \
        || stage_fail pristine "tests fail on a fresh clone of HEAD" "$CI_LOG_DIR/pristine-tests.log"
    "$tmp/build/jsom" version | sed 's/^/      /'
    tail -n 2 "$CI_LOG_DIR/pristine-tests.log" | sed 's/^/      /'
    if [ "$CI_KEEP_TMP" != "1" ]; then
        rm -rf "$tmp"
    else
        printf '    kept: %s (CI_KEEP_TMP=1)\n' "$tmp"
    fi
    stage_pass pristine
}

cleanup() {
    local dir
    if [ "$CI_KEEP_TMP" != "1" ]; then
        for dir in "${RUN_TMP_DIRS[@]:-}"; do
            [ -n "$dir" ] && [ -d "$dir" ] && rm -rf "$dir"
        done
    fi
}
trap cleanup EXIT

# ---------------------------------------------------------------- dispatch
while [ $# -gt 0 ]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --list)
            printf 'default stages: %s\n' "$CI_DEFAULT_STAGES"
            printf 'stages: tree format build tests asan fuzz conform tidy pristine\n'
            exit 0 ;;
        --require-clean) REQUIRE_CLEAN=1 ;;
        --strict-tools) CI_STRICT_TOOLS=1 ;;
        -*) printf 'unknown option: %s (try --help)\n' "$1" >&2; exit 2 ;;
        *) STAGES_REQUESTED+=("$1") ;;
    esac
    shift
done

if [ ${#STAGES_REQUESTED[@]} -eq 0 ]; then
    # shellcheck disable=SC2206
    STAGES_REQUESTED=($CI_DEFAULT_STAGES)
fi

START=$(date +%s)
for stage in "${STAGES_REQUESTED[@]}"; do
    if ! declare -F "stage_$stage" > /dev/null; then
        printf 'unknown stage: %s (try --list)\n' "$stage" >&2
        exit 2
    fi
    "stage_$stage"
done
ELAPSED=$(( $(date +%s) - START ))
printf '\nall %s stage(s) passed in %ss\n' "${#STAGES_REQUESTED[@]}" "$ELAPSED"
