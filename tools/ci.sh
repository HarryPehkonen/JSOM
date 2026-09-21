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
#   tools/ci.sh --write-tidy-baseline   # accept the tidy findings you inherited
#   tools/ci.sh --help
#
#   git config core.hooksPath .githooks     # one-time, per clone, enables the hooks
#
# Configuration lives in .ci.env (gitignored, optional); every knob has a default
# here, so the repo works with no config at all. See .ci.env.example.
#
# Exit status: 0 only if every stage that ran passed. A failing stage stops the run,
# prints why, and leaves its full output in .ci-logs/<stage>.log. The last line of a run
# is always GATE PASSED or GATE FAILED, and a failure names every requested stage that
# never ran (BLOCK <stage>), so a stage that did not run is never read as one that passed.

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
CI_DEFAULT_STAGES=${CI_DEFAULT_STAGES:-"tree format kitprobes build tests consumer asan fuzz tsan std cli conform tidy pristine"}
CI_TIDY_BASELINE=${CI_TIDY_BASELINE:-.ci/tidy-baseline.txt}

if [ -f .ci.env ]; then
    # shellcheck disable=SC1091
    . ./.ci.env
fi

REQUIRE_CLEAN=0
WRITE_TIDY_BASELINE=0
STAGES_REQUESTED=()

# ---------------------------------------------------------------- plumbing
RESULT_LINES=()
RAN_STAGES=()
FAILED_STAGE=""
RUN_TMP_DIRS=()

usage() {
    # The whole leading comment block: every line from 2 to the first blank one. Derived
    # rather than hard-coded, so adding a line to the header cannot silently cut it off.
    sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
    cat <<'EOF'

Stages:
  tree        worktree clean? (only enforced with --require-clean) + .gitignore audit
  format      clang-format drift — dry run, reports files that need reformatting
  kitprobes   the kit fixes this copy claims to carry, held to their contracts: every
              script in tools/kit-probes/ checks one kit fix in THIS gate script by name
              and by behaviour (offline, no kit checkout, no build, <1 s). A missing
              directory SKIPs: it means this copy carries no probe yet, not that it is
              behind. The rule: KIT-REVISION-CONVENTION.md
  build       cmake configure + build, zero warnings (-Werror)
  tests       ./<build>/jsom_tests
  consumer    JSOM's own defaults (CMAKE_BUILD_TYPE, CMAKE_INSTALL_PREFIX, the compile
              database) stay JSOM's: a stub consumer that asks for nothing gets CMake's
              defaults and no database, while JSOM's own `cmake -B build` still gets
              Release. Configure-only, offline. The guard: tools/consumer_probe.sh
  asan        build-asan (-DJSOM_SANITIZE=ON) + the same tests under ASan+UBSan
  fuzz        libFuzzer smoke run, CI_FUZZ_SECONDS seconds, both configurations
  tsan        ThreadSanitizer: tests/thread_safety_probe.cpp, const reads from 4 threads
  std         tools/std_probe.cpp compiled and run as C++17, C++20 and C++23
  cli         the `jsom` binary: exit codes, validation, pointer ops, bad flags
  coverage    line coverage of the library (gcov); opt-in, reports but never fails
  conform     RFC 8259 suite: asserts the default verdict (n_ 188/188), reports loose
  tidy        clang-tidy against CI_TIDY_BASELINE (no NEW findings). Findings compare
              line-blind and clone-blind — capture the baseline with
              --write-tidy-baseline, never by hand (see .ci.env.example)
  pristine    git archive HEAD -> temp dir -> configure, build, test: proves the
              COMMITTED tree is complete (catches files that are uncommitted or ignored)

Options:
  --require-clean   make the tree stage fail when the worktree is dirty (used by pre-push)
  --strict-tools    a missing tool (clang-format/tidy) fails instead of skipping
  --write-tidy-baseline
                    accept every finding tidy reports now into CI_TIDY_BASELINE (runs the
                    build and tidy stages first); prints, and IS NOT, a gate pass
  --list            list the default stages and exit
  --help            this text
EOF
}

log() { printf '%s\n' "$*"; }

ci_begin() {
    local name="$1"
    printf '\n\033[1m==> %s\033[0m\n' "$name"
}

ci_pass() {
    RESULT_LINES+=("pass  $1")
}

ci_skip() {
    RESULT_LINES+=("SKIP  $1 ($2)")
    printf '    SKIP: %s\n' "$2"
}

ci_fail() {
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
    printf '\nGATE FAILED\n' >&2
    exit 1
}

summary() {
    printf '\n--- ci summary ---\n'
    local line
    for line in "${RESULT_LINES[@]}"; do printf '  %s\n' "$line"; done
    if [ -n "$FAILED_STAGE" ]; then
        printf '  stopped at: %s\n' "$FAILED_STAGE"
        # A stage that never ran because an earlier one failed must not read as a stage
        # that passed. Once the build fails, the stages behind it have nothing trustworthy
        # to say, so they are named as blocked rather than silently omitted.
        local s r ran
        for s in "${STAGES_REQUESTED[@]:-}"; do
            [ -n "$s" ] || continue
            ran=0
            for r in "${RAN_STAGES[@]:-}"; do
                [ "$r" = "$s" ] && ran=1
            done
            if [ "$ran" = "0" ] && [ "$s" != "$FAILED_STAGE" ]; then
                printf '  BLOCK %s (did not run: the run stopped at %s)\n' "$s" "$FAILED_STAGE"
            fi
        done
    fi
}

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

# ---------------------------------------------------------------- stages
stage_tree() {
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

stage_format() {
    ci_begin "format"
    require_tool clang-format format || return 0
    local -a sources=()
    mapfile -t sources < <(ci_sources)
    if clang-format --dry-run -Werror "${sources[@]}" > "$CI_LOG_DIR/format.log" 2>&1; then
        printf '    %s files conform to .clang-format\n' "${#sources[@]}"
        ci_pass format
        return 0
    fi
    grep -oE '^[^:]+\.(cpp|hpp)' "$CI_LOG_DIR/format.log" | sort -u | sed 's/^/      /'
    ci_fail format "clang-format drift (fix with: clang-format -i \$(git ls-files 'include/jsom/*.hpp' 'src/*.cpp' 'tests/*.cpp' 'tools/*.cpp' 'benchmarks/*.cpp'))" "$CI_LOG_DIR/format.log"
}

stage_build() {
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

stage_tests() {
    ci_begin "tests"
    "$CI_BUILD_DIR/jsom_tests" > "$CI_LOG_DIR/tests.log" 2>&1 \
        || ci_fail tests "test failures" "$CI_LOG_DIR/tests.log"
    tail -n 2 "$CI_LOG_DIR/tests.log" | sed 's/^/      /'
    ci_pass tests
}

# The cache-scope guard (card t_772eabd9): JSOM enters a consumer's build as a
# SUBDIRECTORY, and CMake's cache is global — so a FORCE cache write here configures the
# consumer too. That is how Computo's Pages workflow, which passes no build type, silently
# compiled at -O3 for months until a gcc 13/14 false positive in libstdc++'s <variant>
# took its only production target offline (t_93e9a66b). Both writes are now gated on
# JSOM_IS_TOP_LEVEL, and this stage is what keeps them that way: it configures stub
# consumers (FetchContent and add_subdirectory), asserts the cache stays theirs, and
# asserts JSOM's own Release default survives. Configure-only and offline; ~11 s.
stage_consumer() {
    ci_begin "consumer (JSOM's defaults configure JSOM only)"
    if ! bash "$REPO_ROOT/tools/consumer_probe.sh" > "$CI_LOG_DIR/consumer.log" 2>&1; then
        grep -E '^(ok|FAIL) ' "$CI_LOG_DIR/consumer.log" | sed 's/^/      /'
        ci_fail consumer "a consumer of this repo is configured by JSOM's own defaults — the FAIL lines above name which" "$CI_LOG_DIR/consumer.log"
    fi
    tail -n 1 "$CI_LOG_DIR/consumer.log" | sed 's/^/      /'
    ci_pass consumer
}

stage_asan() {
    ci_begin "asan"
    cmake -S . -B "$CI_ASAN_BUILD_DIR" -DJSOM_SANITIZE=ON > "$CI_LOG_DIR/asan-configure.log" 2>&1 \
        || ci_fail asan "cmake configure failed" "$CI_LOG_DIR/asan-configure.log"
    cmake --build "$CI_ASAN_BUILD_DIR" -j "$CI_JOBS" > "$CI_LOG_DIR/asan-build.log" 2>&1 \
        || ci_fail asan "sanitizer build failed" "$CI_LOG_DIR/asan-build.log"
    "$CI_ASAN_BUILD_DIR/jsom_tests" > "$CI_LOG_DIR/asan-tests.log" 2>&1 \
        || ci_fail asan "ASan/UBSan reported something" "$CI_LOG_DIR/asan-tests.log"
    tail -n 2 "$CI_LOG_DIR/asan-tests.log" | sed 's/^/      /'
    ci_pass asan
}

stage_fuzz() {
    ci_begin "fuzz (${CI_FUZZ_SECONDS}s smoke)"
    if ! command -v clang++ >/dev/null 2>&1; then
        ci_skip fuzz "clang++ not installed (libFuzzer needs clang)"
        return 0
    fi
    # The fuzzer now drives JSONFuzz's structure-aware generator, mutators and oracles,
    # so it is gated behind JSOM_BUILD_FUZZING (OFF by default, so the normal build and
    # the `pristine` stage never fetch the sibling repo). This stage configures its OWN
    # build dir with that option on. DEVIATION from the other stages: JSONFuzz comes in
    # via FetchContent pinned to a pushed commit, with CI_FUZZ_JSONFUZZ_DIR as the
    # offline override (the pattern JSONFuzz itself uses for nlohmann) — set it in
    # .ci.env to a local checkout so this stage needs no network.
    local fuzz_build="$CI_BUILD_DIR-fuzz"
    local jsonfuzz_opt=()
    if [ -n "${CI_FUZZ_JSONFUZZ_DIR:-}" ]; then
        jsonfuzz_opt=(-DJSONFUZZ_SOURCE_DIR="$CI_FUZZ_JSONFUZZ_DIR")
    fi
    cmake -S . -B "$fuzz_build" -DJSOM_BUILD_FUZZING=ON -DJSOM_BUILD_TESTS=OFF \
        -DCMAKE_CXX_COMPILER=clang++ "${jsonfuzz_opt[@]}" \
        > "$CI_LOG_DIR/fuzz-configure.log" 2>&1 \
        || ci_fail fuzz "fuzz configure failed" "$CI_LOG_DIR/fuzz-configure.log"
    cmake --build "$fuzz_build" --target fuzz_jsom -j "$CI_JOBS" \
        > "$CI_LOG_DIR/fuzz-build.log" 2>&1 \
        || ci_fail fuzz "fuzz target build failed" "$CI_LOG_DIR/fuzz-build.log"
    # Reach smoke FIRST: -runs=0 plays every seed and exits non-zero unless each ENABLED
    # reading reached its oracle laws. With the default (all three) this is what keeps every
    # reading live in the gate — "at least one input got there" is the guard that hid a blind
    # spot in a sibling repo, where 117 of 9,952 inputs reached the assertion and a class-shaped
    # sabotage still survived 4.2 M executions.
    if ! JSOM_FUZZ_REQUIRE_REACH=1 JSOM_FUZZ_READINGS="$CI_FUZZ_READINGS" \
            "$fuzz_build/fuzz_jsom" fuzz/seeds -runs=0 > "$CI_LOG_DIR/fuzz-smoke.log" 2>&1; then
        ci_fail fuzz "the -runs=0 reach smoke failed (a reading starved, or a finding)" \
            "$CI_LOG_DIR/fuzz-smoke.log"
    fi
    # Then the knob's own contract: exactly the named readings run, a deselected one does no
    # work, and a typo fails loudly instead of silently reducing coverage.
    if ! tools/fuzz_readings_smoke.sh "$fuzz_build/fuzz_jsom" \
            > "$CI_LOG_DIR/fuzz-readings.log" 2>&1; then
        ci_fail fuzz "the JSOM_FUZZ_READINGS contract failed" "$CI_LOG_DIR/fuzz-readings.log"
    fi
    grep -E "readings enabled" "$CI_LOG_DIR/fuzz-smoke.log" | tail -n 1 | sed 's/^/      /'
    mkdir -p corpus
    if JSOM_FUZZ_READINGS="$CI_FUZZ_READINGS" "$fuzz_build/fuzz_jsom" corpus fuzz/seeds \
            -dict=fuzz/jsom.dict -artifact_prefix=corpus/ \
            -max_total_time="$CI_FUZZ_SECONDS" > "$CI_LOG_DIR/fuzz.log" 2>&1; then
        grep -E "^Done |^#[0-9]+.*cov:" "$CI_LOG_DIR/fuzz.log" | tail -n 1 | sed 's/^/      /'
        ci_pass fuzz
        return 0
    fi
    ci_fail fuzz "the fuzzer found something (artifact in corpus/, add a regression test)" "$CI_LOG_DIR/fuzz.log"
}

stage_tsan() {
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

stage_std() {
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

stage_coverage() {
    # Informational, never a gate: a coverage threshold mostly teaches people to write
    # tests that raise the number. It is here so the number is one command away.
    ci_begin "coverage (gcov, library only)"
    if ! command -v gcov >/dev/null 2>&1; then
        ci_skip coverage "gcov not installed (it ships with gcc)"
        return 0
    fi
    if ! python3 "$REPO_ROOT/tools/coverage.py" > "$CI_LOG_DIR/coverage.log" 2>&1; then
        ci_fail coverage "coverage run failed" "$CI_LOG_DIR/coverage.log"
    fi
    grep -E "= [0-9.]+%$" "$CI_LOG_DIR/coverage.log" | sed 's/^/      /'
    grep -E "^    [a-z_]+\.[ch]pp" "$CI_LOG_DIR/coverage.log" | head -6 | sed 's/^/    /'
    ci_pass coverage
}

stage_cli() {
    ci_begin "cli (smoke tests for the jsom binary)"
    if ! bash "$REPO_ROOT/tools/cli_smoke.sh" "$CI_BUILD_DIR/jsom" \
        > "$CI_LOG_DIR/cli.log" 2>&1; then
        ci_fail cli "a CLI check failed" "$CI_LOG_DIR/cli.log"
    fi
    tail -n 1 "$CI_LOG_DIR/cli.log" | sed 's/^/      /'
    ci_pass cli
}

stage_conform() {
    ci_begin "conform"
    cmake --build "$CI_BUILD_DIR" --target jsom_conformance > "$CI_LOG_DIR/conform-build.log" 2>&1 \
        || ci_fail conform "conformance runner build failed" "$CI_LOG_DIR/conform-build.log"
    # The suite is a gate on the DEFAULT configuration: the number grammar is enforced
    # while scanning, so every judged class must pass with no flags.
    if ! "$CI_BUILD_DIR/jsom_conformance" > "$CI_LOG_DIR/conform.log" 2>&1; then
        ci_fail conform "must-accept or must-reject disagreements" "$CI_LOG_DIR/conform.log"
    fi
    grep -E "y_ must accept|n_ must reject" "$CI_LOG_DIR/conform.log" | sed 's/^/      /'
    # Loose mode is reported, not asserted: it is the documented extension mode, and the
    # number cases are expected to disagree there.
    "$CI_BUILD_DIR/jsom_conformance" 2>&1 | grep -E "n_ must reject" | sed 's/^/      default mode: /'
    ci_pass conform
}

stage_tidy() {
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
            ci_fail tidy "$findings finding(s) and no baseline file — accept them in one step with 'tools/ci.sh --write-tidy-baseline', or fix them; see .ci.env.example" "$CI_LOG_DIR/tidy.log"
        fi
    fi
    ci_pass tidy
}

# Capture the accepted-findings baseline. Not a stage: the list is compared against every
# finding in the log, not against the outcome of the run, and the run is EXPECTED to fail
# while there is no baseline yet. It runs the real stages as a child so there is exactly one
# definition of what a finding is (tidy_key) and of where tidy's log comes from.
write_tidy_baseline() {
    printf '\n\033[1m==> write the tidy baseline\033[0m (%s)\n' "$CI_TIDY_BASELINE"
    printf '    running the real build + tidy stages (build first: tidy refuses to run without\n'
    printf '    %s/compile_commands.json; treat the tidy failure below as expected)\n\n' "$CI_BUILD_DIR"
    bash "$0" build tidy > "$CI_LOG_DIR/write-baseline.log" 2>&1
    if [ ! -f "$CI_LOG_DIR/tidy.log" ]; then
        printf 'no %s was written, so there is nothing to capture — the run stopped before the tidy stage:\n' "$CI_LOG_DIR/tidy.log" >&2
        tail -n 20 "$CI_LOG_DIR/write-baseline.log" | sed 's/^/      /' >&2
        printf '    full log: %s\n' "$CI_LOG_DIR/write-baseline.log" >&2
        exit 1
    fi
    mkdir -p "$(dirname "$CI_TIDY_BASELINE")"
    grep -E 'warning:|error:' "$CI_LOG_DIR/tidy.log" | tidy_key | sort -u > "$CI_TIDY_BASELINE"
    local accepted
    accepted=$(grep -c . "$CI_TIDY_BASELINE" || true)
    printf '    %s finding(s) accepted into %s:\n' "${accepted:-0}" "$CI_TIDY_BASELINE"
    head -20 "$CI_TIDY_BASELINE" | sed 's/^/      /'
    if [ "${accepted:-0}" -eq 0 ]; then
        printf '\nNothing to accept: the tidy stage is clean, so delete %s and keep the stage strict.\n' "$CI_TIDY_BASELINE"
        exit 0
    fi
    printf '\nThis is NOT a gate pass: from now on the tidy stage tolerates exactly these findings\n'
    printf 'and fails on anything else. Commit the file — it is this repo'"'"'s accepted-findings list,
'
    printf 'and the tree stage fails on a file that is neither committed nor ignored:\n'
    printf '    git add %s && git commit -m "ci: accept the inherited clang-tidy findings"\n' "$CI_TIDY_BASELINE"
}

stage_pristine() {
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

cleanup() {
    local dir
    if [ "$CI_KEEP_TMP" != "1" ]; then
        for dir in "${RUN_TMP_DIRS[@]:-}"; do
            [ -n "$dir" ] && [ -d "$dir" ] && rm -rf "$dir"
        done
    fi
}
trap cleanup EXIT

# ---------------------------------------------------------------- kit probes
# A fix that must propagate ships a probe (docs/KIT-REVISION-CONVENTION.md). Each script
# in tools/kit-probes/ holds this gate to ONE kit fix's contract — name and behaviour, not
# bytes — and exits non-zero when the fix is absent. The directory IS the list of fixes
# this copy claims to carry, so absence fails HERE, in under a second, on the machine that
# would otherwise push the lag: no kit checkout, no network, no build.
#
# Why not a hash or a diff against the kit: the copies of this file are forks (a repo's
# adapted stages, its own defaults, 60-586 differing lines), and diff SIZE measures
# divergence, not lateness — a one-fix-behind copy is missing 27 kit lines while a
# verified current record is missing 125. See the convention for the measurement.
#
# It is name- and contract-level: a semantic regression INSIDE a function that is still
# present is not caught. That needs a real build and a real run, which is what the port
# did by hand; the probe is the cheap net, not the whole net.
stage_kitprobes() {
    ci_begin "kit probes (the fixes this copy claims to carry)"
    local self probe name failed=0
    self="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"
    if [ ! -d "$REPO_ROOT/tools/kit-probes" ]; then
        ci_skip kitprobes "no tools/kit-probes/ — this copy carries no kit probe yet"
        return 0
    fi
    for probe in "$REPO_ROOT"/tools/kit-probes/*.sh; do
        [ -f "$probe" ] || continue
        name="$(basename "$probe")"
        if bash "$probe" "$self" "$REPO_ROOT"; then
            printf '    ok   %s\n' "$name"
        else
            printf '    FAIL %s — this gate is missing that kit fix\n' "$name"
            failed=1
        fi
    done
    if [ "$failed" = "1" ]; then
        ci_fail kitprobes "a probe failed: this copy is behind a kit fix — port it from the kit (tools/kit-probes/ names which)"
    fi
    printf '    every probe in tools/kit-probes/ verified against this gate\n'
    ci_pass kitprobes
}

# ---------------------------------------------------------------- dispatch
while [ $# -gt 0 ]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --list)
            printf 'default stages: %s\n' "$CI_DEFAULT_STAGES"
            # Derived from the defined functions, so this line cannot drift from the
            # stages the script actually implements (it did: it kept listing the old set).
            printf 'stages:'
            for fn in $(declare -F | awk '{print $3}' | grep '^stage_' | sed 's/^stage_//' | sort); do
                printf ' %s' "$fn"
            done
            printf '\n   (opt-in, not in the default set: %s)\n' \
                "$(declare -F | awk '{print $3}' | grep '^stage_' | sed 's/^stage_//' \
                   | grep -vE "^($(printf '%s' "$CI_DEFAULT_STAGES" | tr ' ' '|'))$" \
                   | sort | tr '\n' ' ')"
            exit 0 ;;
        --require-clean) REQUIRE_CLEAN=1 ;;
        --strict-tools) CI_STRICT_TOOLS=1 ;;
        --write-tidy-baseline) WRITE_TIDY_BASELINE=1 ;;
        -*) printf 'unknown option: %s (try --help)\n' "$1" >&2; exit 2 ;;
        *) STAGES_REQUESTED+=("$1") ;;
    esac
    shift
done

# Accepting inherited findings is its own mode, not a stage: it produces no verdict about
# the tree, only a file (and it exits before the summary so it can never print
# "GATE PASSED" about a run whose tidy stage failed on purpose).
if [ "$WRITE_TIDY_BASELINE" = "1" ]; then
    write_tidy_baseline
    exit 0
fi

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
    # A stage that returns non-zero without reporting a verdict is not a pass, and a stage that
    # dies from a shell error cannot report anything at all — so neither is left to the summary.
    if ! "stage_$stage"; then
        FAILED_STAGE="$stage"
        summary
        printf 'FAILED: %s exited non-zero without reporting a verdict\n' "$stage" >&2
        printf '\nGATE FAILED\n' >&2
        exit 1
    fi
    RAN_STAGES+=("$stage")
done
ELAPSED=$(( $(date +%s) - START ))

# The verdict comes from what RAN, not from what was requested. A shell error can unwind out of
# the loop above without either guard seeing it — measured 2026-09-20 on Computo's fork: `set -u`
# plus `local -a sources` (declared, never filled) made "${#sources[@]}" an unbound-variable
# error, which aborted stage_format and the dispatch loop together, and the run then printed
# "all 10 stage(s) passed ... GATE PASSED" after executing one stage of ten (INCIDENTS.md). This
# comparison is the backstop for that whole class: if any requested stage did not run, the run
# fails.
if [ "${#RAN_STAGES[@]}" -ne "${#STAGES_REQUESTED[@]}" ]; then
    summary
    printf 'FAILED: %s of %s stage(s) did not run — the run ended early\n' \
        "$(( ${#STAGES_REQUESTED[@]} - ${#RAN_STAGES[@]} ))" "${#STAGES_REQUESTED[@]}" >&2
    printf '  ran: %s\n' "${RAN_STAGES[*]:-none}" >&2
    printf '\nGATE FAILED\n' >&2
    exit 1
fi

summary
printf '\nall %s stage(s) passed in %ss\nGATE PASSED\n' "${#STAGES_REQUESTED[@]}" "$ELAPSED"
