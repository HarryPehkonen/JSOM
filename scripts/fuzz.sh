#!/usr/bin/env bash
#
# fuzz — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.fuzz] cmd = "scripts/fuzz.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
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
    # -U: whatever a previous configure left in the cache cannot outrank the pin. The stage
    # tests the pinned revision unless .ci.env says otherwise, and saying otherwise is
    # explicit (CI_FUZZ_JSONFUZZ_DIR) rather than an accident of build-dir history. Measured
    # 2026-09-21: build-fuzz carried a cached JSONFUZZ_SOURCE_DIR, so the stage had been
    # fuzzing the sibling checkout while the log implied the pin.
    local jsonfuzz_opt=(-UJSONFUZZ_SOURCE_DIR -UFETCHCONTENT_SOURCE_DIR_JSONFUZZ)
    if [ -n "${CI_FUZZ_JSONFUZZ_DIR:-}" ]; then
        jsonfuzz_opt+=(-DJSONFUZZ_SOURCE_DIR="$CI_FUZZ_JSONFUZZ_DIR")
    fi
    cmake -S . -B "$fuzz_build" -DJSOM_BUILD_FUZZING=ON -DJSOM_BUILD_TESTS=OFF \
        -DCMAKE_CXX_COMPILER=clang++ "${jsonfuzz_opt[@]}" \
        > "$CI_LOG_DIR/fuzz-configure.log" 2>&1 \
        || ci_fail fuzz "fuzz configure failed" "$CI_LOG_DIR/fuzz-configure.log"
    cmake --build "$fuzz_build" --target fuzz_jsom -j "$CI_JOBS" \
        > "$CI_LOG_DIR/fuzz-build.log" 2>&1 \
        || ci_fail fuzz "fuzz target build failed" "$CI_LOG_DIR/fuzz-build.log"
    # Provenance: say which JSONFuzz revision this run fuzzed against. A finding's meaning
    # rests on the pin, and a build dir configured once with CI_FUZZ_JSONFUZZ_DIR keeps
    # using that checkout afterwards (FetchContent caches the source dir), so an offline
    # run can be ahead of or behind the pin without saying so. Print it rather than assume.
    # If the fetched revision looks older than the pin, wipe "$fuzz_build/_deps/jsonfuzz-src".
    local jf_dir="${CI_FUZZ_JSONFUZZ_DIR:-}" jf_where=""
    if [ -z "$jf_dir" ]; then
        jf_dir="$fuzz_build/_deps/jsonfuzz-src"; jf_where="fetched"
    else
        jf_where="local override (CI_FUZZ_JSONFUZZ_DIR)"
    fi
    if [ -d "$jf_dir" ]; then
        printf '      jsonfuzz: %s (%s), %s\n' \
            "$(git -C "$jf_dir" describe --tags --always 2>/dev/null || echo unknown)" \
            "$(git -C "$jf_dir" rev-parse --short HEAD 2>/dev/null || echo '?')" "$jf_where"
    else
        printf '      jsonfuzz: no source dir at %s — configure log has the reason\n' "$jf_dir"
    fi

    # Every archived finding in fuzz/regressions/ is replayed alongside the seeds, so a
    # regression of a bug this project has already paid for fails the gate in seconds
    # instead of waiting for a campaign to rediscover it.
    # Reach smoke FIRST: -runs=0 plays every seed and exits non-zero unless each ENABLED
    # reading reached its oracle laws. With the default (all three) this is what keeps every
    # reading live in the gate — "at least one input got there" is the guard that hid a blind
    # spot in a sibling repo, where 117 of 9,952 inputs reached the assertion and a class-shaped
    # sabotage still survived 4.2 M executions.
    if ! JSOM_FUZZ_REQUIRE_REACH=1 JSOM_FUZZ_READINGS="$CI_FUZZ_READINGS" \
            "$fuzz_build/fuzz_jsom" fuzz/regressions fuzz/seeds -runs=0 > "$CI_LOG_DIR/fuzz-smoke.log" 2>&1; then
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
    if JSOM_FUZZ_READINGS="$CI_FUZZ_READINGS" "$fuzz_build/fuzz_jsom" corpus fuzz/regressions fuzz/seeds \
            -dict=fuzz/jsom.dict -artifact_prefix=corpus/ \
            -max_total_time="$CI_FUZZ_SECONDS" > "$CI_LOG_DIR/fuzz.log" 2>&1; then
        grep -E "^Done |^#[0-9]+.*cov:" "$CI_LOG_DIR/fuzz.log" | tail -n 1 | sed 's/^/      /'
        ci_pass fuzz
        return 0
    fi
    ci_fail fuzz "the fuzzer found something (artifact in corpus/, add a regression test)" "$CI_LOG_DIR/fuzz.log"
}

run_stage "$@"
