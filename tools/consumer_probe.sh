#!/usr/bin/env bash
#
# consumer probe — the cache-scope guard (kanban t_772eabd9).
#
# JSOM is consumed as a SUBDIRECTORY: `FetchContent_MakeAvailable(JSOM)` (Computo) or
# `add_subdirectory(<checkout>)` (jsonTools' gate). CMake's cache is global, so a
# subproject that writes it with FORCE configures its CONSUMER too. JSOM did exactly that
# for CMAKE_BUILD_TYPE and CMAKE_INSTALL_PREFIX, and Computo's Pages workflow — which
# passes no build type, exactly like `cmake -S . -B build` — inherited Release and
# compiled at -O3 for months. That is how a gcc 13/14 -Wmaybe-uninitialized false positive
# in libstdc++'s <variant> machinery took Computo's only production target offline
# (kanban t_93e9a66b, 2026-09-20).
#
# The guard it now has: both cache writes are gated on JSOM_IS_TOP_LEVEL, so JSOM's own
# `cmake -B build` still gets Release (the point of OPTIMIZATIONS.md #2) and a consumer
# that asks for nothing keeps CMake's defaults. This probe holds the tree to that:
#
#   1. consumer, no flags at all  -> cache untouched (no build type, /usr/local)
#   2. consumer, compile database -> its OWN TU is in the database and is not at -O3;
#                                    JSOM's aid (CMAKE_EXPORT_COMPILE_COMMANDS) is
#                                    JSOM-local too, so it must be ASKED for
#   3. consumer, -DCMAKE_BUILD_TYPE=Debug -> an explicit type still wins
#   4. consumer via add_subdirectory (jsonTools' shape) -> cache untouched
#   5. JSOM alone, top level      -> Release, still, and still -O3 for its own TUs
#
# Configure-only: the cache and compile_commands.json are both written at configure time,
# so nothing is built. Offline: the consumer's FetchContent is pointed at THIS checkout
# with -DFETCHCONTENT_SOURCE_DIR_JSOM, the documented override for its git URL.
#
#   bash tools/consumer_probe.sh          # measure; prints ok/FAIL, exits non-zero on FAIL
#   bash tools/consumer_probe.sh --keep   # keep the scratch tree at .ci/consumer-probe
#
# Run by the `consumer` stage of tools/ci.sh on every gate run (see its usage text).

set -uo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
KEEP=0
case "${1:-}" in
    --keep) KEEP=1 ;;
    "") ;;
    *) printf 'usage: %s [--keep]\n' "$(basename "$0")" >&2; exit 2 ;;
esac

SCRATCH="$REPO_ROOT/.ci/consumer-probe"
FAILED=0
CHECKS=0

ok() {
    CHECKS=$((CHECKS + 1))
    printf 'ok   %s\n' "$1"
}

bad() {
    CHECKS=$((CHECKS + 1))
    FAILED=1
    printf 'FAIL %s — %s\n' "$1" "$2"
}

# expect <name> <actual> <expected>
expect() {
    if [ "$2" = "$3" ]; then
        ok "$1"
    else
        bad "$1" "expected '$3', got '$2'"
    fi
}

cache_var() { # cache_var <build dir> <cache variable>
    grep -m1 "^$2:" "$1/CMakeCache.txt" 2>/dev/null | cut -d= -f2-
}

# The compile-database entry for one source file. CMake gives every translation unit both
# a "file" key and a "command" line, and the command line ends with `-c <absolute source>`
# for every generator this repo uses — so the entry is addressed by the source path, with
# no assumption about the order of the keys inside the JSON object.
db_has_source() { # db_has_source <database> <absolute source path>
    grep -qF "\"$2\"" "$1"
}

db_command_for() { # db_command_for <database> <absolute source path>
    grep -F -- "$2" "$1" | grep -m1 '"command"'
}

configure() { # configure <build dir> <source dir> [cmake args...]
    local dir="$1" src="$2"
    shift 2
    cmake -S "$src" -B "$dir" "$@" > "$SCRATCH/$(basename "$dir").configure.log" 2>&1
}

stub_consumer() {
    rm -rf "$SCRATCH"
    mkdir -p "$SCRATCH/consumer"
    cat > "$SCRATCH/consumer/CMakeLists.txt" <<'STUB'
cmake_minimum_required(VERSION 3.16)
project(consumer_stub LANGUAGES CXX)

# The consumer's own compile database: what a consumer's tidy gate reads.
if(STUB_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Two ways a real consumer takes JSOM in. Both enter it as a subdirectory, which is what
# makes JSOM's cache writes reach this project when they are not guarded.
option(STUB_ADD_SUBDIRECTORY "take JSOM in with add_subdirectory() instead of FetchContent" OFF)
if(STUB_ADD_SUBDIRECTORY)
    add_subdirectory("${STUB_JSOM_DIR}" jsom)
else()
    include(FetchContent)
    FetchContent_Declare(JSOM
        GIT_REPOSITORY https://github.com/HarryPehkonen/JSOM.git
        GIT_TAG main)
    # Offline: -DFETCHCONTENT_SOURCE_DIR_JSOM=<checkout> is the documented override.
    FetchContent_MakeAvailable(JSOM)
endif()

message(STATUS "STUB after JSOM: build type='${CMAKE_BUILD_TYPE}' prefix='${CMAKE_INSTALL_PREFIX}'")

add_executable(stub_app main.cpp)
target_link_libraries(stub_app PRIVATE JSOM::jsom)
STUB
    cat > "$SCRATCH/consumer/main.cpp" <<'STUB'
// The consumer's own translation unit: the flags CMake gives THIS file are the
// measurement. It includes JSOM's header, so the include path is exercised too.
#include <jsom/json_document.hpp>

int main() { return 0; }
STUB
}

stub_consumer

printf 'consumer probe — scratch: %s\n' "$SCRATCH"

# 1. The case that hurt: a consumer that passes no build type (Computo's Pages workflow).
if configure "$SCRATCH/build-clean" "$SCRATCH/consumer" -DFETCHCONTENT_SOURCE_DIR_JSOM="$REPO_ROOT"; then
    expect "consumer + no flags: cache CMAKE_BUILD_TYPE is not set by JSOM" \
        "$(cache_var "$SCRATCH/build-clean" CMAKE_BUILD_TYPE)" ""
    expect "consumer + no flags: cache CMAKE_INSTALL_PREFIX is not set by JSOM" \
        "$(cache_var "$SCRATCH/build-clean" CMAKE_INSTALL_PREFIX)" "/usr/local"
    if [ -f "$SCRATCH/build-clean/compile_commands.json" ]; then
        bad "consumer + no flags: JSOM writes no compile database into the consumer" \
            "$SCRATCH/build-clean/compile_commands.json exists (it is JSOM's aid, JSOM-local; the consumer asks for it itself)"
    else
        ok "consumer + no flags: JSOM writes no compile database into the consumer"
    fi
else
    bad "consumer + no flags: configure succeeds" "see $SCRATCH/build-clean.configure.log"
fi

# 2. A consumer that asks for the compile database (jsonTools' and Computo's gate shape).
if configure "$SCRATCH/build-db" "$SCRATCH/consumer" \
    -DFETCHCONTENT_SOURCE_DIR_JSOM="$REPO_ROOT" -DSTUB_EXPORT_COMPILE_COMMANDS=ON; then
    db="$SCRATCH/build-db/compile_commands.json"
    own_src="$SCRATCH/consumer/main.cpp"
    if db_has_source "$db" "$own_src"; then
        ok "consumer + compile database: the consumer's own TU is in it"
    else
        bad "consumer + compile database: the consumer's own TU is in it" \
            "not found in $db — a consumer's tidy gate would lint JSOM and skip its own sources"
    fi
    own_flags="$(db_command_for "$db" "$own_src")"
    if printf '%s' "$own_flags" | grep -qE -- '-O3|-DNDEBUG|-march=native'; then
        bad "consumer + compile database: the consumer's TU is not at JSOM's Release flags" \
            "inherited an optimized configuration: $(printf '%s' "$own_flags" | tr -s ' ')"
    else
        ok "consumer + compile database: the consumer's TU is not at JSOM's Release flags"
    fi
else
    bad "consumer + compile database: configure succeeds" "see $SCRATCH/build-db.configure.log"
fi

# 3. An explicitly chosen build type still wins (the documented escape hatch).
if configure "$SCRATCH/build-debug" "$SCRATCH/consumer" \
    -DFETCHCONTENT_SOURCE_DIR_JSOM="$REPO_ROOT" -DCMAKE_BUILD_TYPE=Debug; then
    expect "consumer + -DCMAKE_BUILD_TYPE=Debug: the explicit type wins" \
        "$(cache_var "$SCRATCH/build-debug" CMAKE_BUILD_TYPE)" "Debug"
else
    bad "consumer + -DCMAKE_BUILD_TYPE=Debug: configure succeeds" "see $SCRATCH/build-debug.configure.log"
fi

# 4. The other entry point: add_subdirectory() with a local checkout (jsonTools' gate).
if configure "$SCRATCH/build-subdir" "$SCRATCH/consumer" \
    -DSTUB_ADD_SUBDIRECTORY=ON -DSTUB_JSOM_DIR="$REPO_ROOT"; then
    expect "consumer via add_subdirectory: cache CMAKE_BUILD_TYPE is not set by JSOM" \
        "$(cache_var "$SCRATCH/build-subdir" CMAKE_BUILD_TYPE)" ""
else
    bad "consumer via add_subdirectory: configure succeeds" "see $SCRATCH/build-subdir.configure.log"
fi

# 5. The default OPTIMIZATIONS.md #2 is about: JSOM's own build keeps it.
if configure "$SCRATCH/build-jsom-top" "$REPO_ROOT" -DJSOM_BUILD_TESTS=OFF; then
    expect "JSOM alone, top level: Release is still the default" \
        "$(cache_var "$SCRATCH/build-jsom-top" CMAKE_BUILD_TYPE)" "Release"
    if grep -q -- '-O3' "$SCRATCH/build-jsom-top/compile_commands.json" 2>/dev/null; then
        ok "JSOM alone, top level: its own TUs are compiled at -O3"
    else
        bad "JSOM alone, top level: its own TUs are compiled at -O3" \
            "no -O3 in $SCRATCH/build-jsom-top/compile_commands.json"
    fi
else
    bad "JSOM alone, top level: configure succeeds" "see $SCRATCH/build-jsom-top.configure.log"
fi

printf '\n'
if [ "$FAILED" = "1" ]; then
    printf 'consumer probe: FAIL (%s checks run; logs and the scratch tree kept in %s)\n' "$CHECKS" "$SCRATCH"
    exit 1
fi
printf 'consumer probe: %s checks, all ok - JSOM defaults configure JSOM only\n' "$CHECKS"
if [ "$KEEP" = "0" ]; then
    rm -rf "$SCRATCH"
fi
exit 0
