#!/usr/bin/env bash
#
# package — build JSOM's packages, then CONSUME them the way a user would.
#
# Not a gate.toml stage: the tiers are about the library, this is about what the
# packages install on a machine, and it costs a couple of minutes. Run it before
# publishing (PACKAGING.md), on each architecture, after any change to the install
# rules, the CMake configs or the packaging:
#
#     scripts/package.sh            # both layouts
#     scripts/package.sh deb        # Debian packages only
#     scripts/package.sh rpm        # Fedora packages only
#
# Failures are collected rather than fatal: one run shows everything that is
# wrong, not just the first thing. The exit status is 0 only when every check
# passed, and the scratch tree is removed on success and left behind on failure
# for inspection.
#
# What it asserts, and why each assertion exists:
#
#   1. The runtime/development split is right, per distribution. The SONAME link
#      (libjsom.so.3) rides in the RUNTIME package — a hand-rolled deb once shipped
#      only libjsom.so.3.1.2 and every dynamically linked consumer died at startup
#      with "libjsom.so.3: cannot open shared object file" while the package itself
#      looked complete — while the bare libjsom.so symlink, the headers, the CMake
#      configs, the pkg-config file and jsom(3) belong to the development package.
#      That split is re-proved by SABOTAGE at the end: the link is deleted from a
#      COPY of the package, and a gate that cannot see the deletion is not a gate.
#   2. Both layouts build: the Debian multiarch libdir and Fedora's lib64, each
#      packaged with its own generator. CMAKE_INSTALL_PREFIX=/usr is required —
#      GNUInstallDirs derives the libdir from the prefix at configure time, and the
#      repository's default prefix (~/.local) would install the library to plain
#      /usr/lib, which is not the multiarch layout Debian packaging expects. The
#      multiarch path is verified from the built packages, never assumed.
#   3. Every path the shipped CMake configs resolve to actually exists, computed
#      with the same directory math CMake itself uses. This is the check that
#      catches a config generated for one libdir and installed into another: it
#      points at a library that does not exist and no file listing shows it. Both
#      the include directory and the imported library are covered, in both the
#      *Targets.cmake prefix math and the JSOMConfig.cmake PACKAGE_PREFIX_DIR math.
#   4. A real consumer, two ways: find_package + CMake and pkg-config, each
#      configuring, linking and RUNNING against the extracted packages, each
#      printing a value it read through an RFC 6901 JSON Pointer. The CMake consumer
#      is configured with CMAKE_FIND_USE_CMAKE_SYSTEM_PATH=OFF so a copy of JSOM
#      already installed on the host cannot answer for the package under test —
#      without that flag this step can pass while testing nothing.
#   5. Metadata and identity: Depends/Requires from dpkg-shlibdeps and CPack rather
#      than by hand, the shlibs entry that names libjsom3 as the soname's provider,
#      and the version read back from the BUILT packages and compared with the
#      project() line in CMakeLists.txt (the single source of truth).
#   6. The man pages render, and the text names the command and the CMake target a
#      consumer links — the same names this script's own consumer uses.
#
# NOTE ON PIPES: every check captures its command's output into a variable and
# greps that, never `cmd | grep -q` — under `set -o pipefail` a match makes grep
# exit early, the producer dies of SIGPIPE (141), and pipefail reports the match
# as a failure, roughly one run in five.
#
set -uo pipefail
cd "$(dirname "$0")/.."

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
FAILURES=0
note() { printf '  %s\n' "$*"; }
bad()  { printf 'FAIL: %s\n' "$*" >&2; FAILURES=$((FAILURES + 1)); }
have() { command -v "$1" >/dev/null 2>&1; }

# expect_* take a label, a haystack and a needle; each records a failure instead
# of exiting, so one run reports every problem.
expect_in() { # expect_in <label> <haystack> <needle>
    if grep -qF -- "$3" <<<"$2"; then note "ok: $1"; else bad "$1 (expected '$3')"; fi
}
expect_not_in() { # expect_not_in <label> <haystack> <needle>
    if grep -qF -- "$3" <<<"$2"; then bad "$1 ('$3' must not be there)"; else note "ok: $1"; fi
}
expect_path() { # expect_path <label> <path list> <exact path>   (whole-line match)
    if grep -qxF -- "$3" <<<"$2"; then note "ok: $1"; else bad "$1 ($3 is missing)"; fi
}
expect_path_absent() { # expect_path_absent <label> <path list> <exact path>
    if grep -qxF -- "$3" <<<"$2"; then bad "$1 ($3 must not be there)"; else note "ok: $1"; fi
}

# ---------------------------------------------------------------------------
# which layouts, and the tools they need
# ---------------------------------------------------------------------------
case "${1:-both}" in
    deb)     WANT_DEB=1; WANT_RPM=0 ;;
    rpm)     WANT_DEB=0; WANT_RPM=1 ;;
    both|"") WANT_DEB=1; WANT_RPM=1 ;;
    -h|--help)
        echo "usage: ${0##*/} [deb|rpm|both]" ; exit 0 ;;
    *) echo "usage: ${0##*/} [deb|rpm|both]" >&2; exit 2 ;;
esac

# A missing compiler or cpack means there is nothing to verify here, not that the
# packages are broken: SKIP, the way the other stages skip an absent tool.
no_build=""
have cmake || no_build="$no_build cmake"
have cpack || no_build="$no_build cpack"
if ! have c++ && ! have gcc; then no_build="$no_build c++"; fi
if [ -n "$no_build" ]; then
    echo "SKIP: cannot build packages, missing:$no_build"
    exit 0
fi
CXX=$(have c++ && echo c++ || echo gcc)

# A tool the REQUESTED layout needs is a different matter: FAIL, loudly.
need="scdoc gzip readelf pkg-config python3"
[ "$WANT_DEB" = 1 ] && need="$need dpkg dpkg-deb dpkg-shlibdeps"
[ "$WANT_RPM" = 1 ] && need="$need rpmbuild rpm rpm2cpio cpio"
missing=""
for tool in $need; do have "$tool" || missing="$missing $tool"; done
if [ -n "$missing" ]; then
    echo "FAIL: the requested layout needs tools that are missing:$missing" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# version (single source of truth) and the platform names
# ---------------------------------------------------------------------------
VERSION=$(grep -oE 'project\(JSOM VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
[ -n "$VERSION" ] || { echo "FAIL: could not read project(JSOM VERSION x.y.z) from CMakeLists.txt" >&2; exit 1; }
SOVERSION=${VERSION%%.*}
RELEASE=$(grep -oE 'set\(JSOM_PACKAGE_RELEASE [0-9]+' cmake/CPack.cmake | grep -oE '[0-9]+')
[ -n "$RELEASE" ] || { echo "FAIL: could not read JSOM_PACKAGE_RELEASE from cmake/CPack.cmake" >&2; exit 1; }
PV="$VERSION-$RELEASE"
TRIPLET=$($CXX -dumpmachine)
DEBARCH=$(dpkg --print-architecture 2>/dev/null || echo "$(uname -m)")
RPMARCH=$(uname -m)
note "library $VERSION, packaging release $RELEASE (package version $PV)"
note "host triplet $TRIPLET, deb arch $DEBARCH, rpm arch $RPMARCH"

# ---------------------------------------------------------------------------
# scratch tree: kept on failure, removed on success
# ---------------------------------------------------------------------------
WORK=$(mktemp -d "${TMPDIR:-/tmp}/jsom-package-XXXXXX")
PKGS="$WORK/pkgs"
mkdir -p "$PKGS"
cleanup() {
    if [ "$FAILURES" -eq 0 ]; then
        rm -rf "$WORK"
    else
        echo "scratch kept for inspection: $WORK" >&2
    fi
}
trap cleanup EXIT

deb_paths() { dpkg-deb -c "$1" 2>/dev/null | awk '{print $6}'; }

build_layout() { # build_layout <name> <generator> <libdir|"">
    local name="$1" gen="$2" libdir="$3"
    local build="$WORK/build-package-$name"
    local args=(-S . -B "$build"
        -DCMAKE_BUILD_TYPE=Release
        -DJSOM_ENABLE_PACKAGING=ON
        -DBUILD_SHARED_LIBS=ON
        -DJSOM_BUILD_TESTS=OFF
        -DJSOM_BUILD_BENCHMARKS=OFF
        -DCMAKE_INSTALL_PREFIX=/usr)
    [ -n "$libdir" ] && args+=(-DCMAKE_INSTALL_LIBDIR="$libdir")
    echo "== $name layout (generator $gen${libdir:+, CMAKE_INSTALL_LIBDIR=$libdir})"
    if ! cmake "${args[@]}" > "$WORK/configure-$name.log" 2>&1; then
        tail -n 20 "$WORK/configure-$name.log" >&2; bad "$name: cmake configure failed"; return 1
    fi
    if ! cmake --build "$build" -j"$(nproc)" > "$WORK/build-$name.log" 2>&1; then
        tail -n 20 "$WORK/build-$name.log" >&2; bad "$name: cmake --build failed"; return 1
    fi
    if ! cpack -G "$gen" --config "$build/CPackConfig.cmake" -B "$PKGS" > "$WORK/cpack-$name.log" 2>&1; then
        tail -n 20 "$WORK/cpack-$name.log" >&2; bad "$name: cpack -G $gen failed"; return 1
    fi
    note "$name: configured, built and packaged"
    return 0
}

# The installed CMake package must export the target the man page and PACKAGING.md
# document. This is asserted, not assumed: a package that exports a different name
# makes every consumer that follows the documentation die at configure time.
exported_targets() { # exported_targets <installed root>
    find "$1/usr" -name 'JSOMTargets.cmake' -exec \
        grep -hoE 'add_library\(JSOM::[A-Za-z_][A-Za-z0-9_]*' {} \; 2>/dev/null \
        | sed 's/add_library(//' | sort -u
}

# Every path a shipped CMake config resolves to must exist in the extracted
# packages, computed with CMake's own directory math. The calls run against the
# INSTALLED trees (runtime + development extracted together), because the config's
# include directory lives in the development package and its imported library in
# the runtime one.
check_config_paths() { # check_config_paths <installed root> <label> <libdir>
    local root="$1" label="$2" libdir="$3"
    local before="$FAILURES" targets pops prefix rel files cfg rel_suffix parts
    for targets in $(find "$root" -name '*Targets.cmake' ! -name '*-release.cmake' ! -name '*-noconfig.cmake'); do
        # Reproduce CMake's math: one PATH pop turns the file into its directory,
        # then one per _IMPORT_PREFIX pop walks up to the install prefix.
        pops=$(grep -c 'get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)' "$targets")
        prefix=$(python3 - "$targets" "$pops" <<'PY'
import os, sys
path, pops = sys.argv[1], int(sys.argv[2])
for _ in range(pops + 1):
    path = os.path.dirname(path)
print(path)
PY
)
        [ "$prefix" = "$root/usr" ] || bad "$label: ${targets#$root} resolves its install prefix to $prefix, expected $root/usr"
        files=("$targets")
        for suffix in -release.cmake -noconfig.cmake; do
            [ -e "${targets%.cmake}$suffix" ] && files+=("${targets%.cmake}$suffix")
        done
        while read -r rel; do
            [ -n "$rel" ] || continue
            [ -e "$prefix/$rel" ] || bad "$label: ${targets#$root} points at $prefix/$rel, which no package installs"
        done < <(grep -hoE '\$\{_IMPORT_PREFIX\}/[^";]*' "${files[@]}" | sed 's|\${_IMPORT_PREFIX}/||' | sort -u)
    done
    # JSOMConfig.cmake computes its own PACKAGE_PREFIX_DIR from a relative climb
    # ("${CMAKE_CURRENT_LIST_DIR}/../../../../"); it must land on the same prefix.
    for cfg in $(find "$root" -name 'JSOMConfig.cmake'); do
        rel_suffix=$(grep -oE '\$\{CMAKE_CURRENT_LIST_DIR\}(/[.][.]/?)+' "$cfg" | head -1 | sed 's|\${CMAKE_CURRENT_LIST_DIR}||')
        [ -n "$rel_suffix" ] || continue
        parts=$(python3 -c "import os,sys; print(os.path.normpath(os.path.dirname(sys.argv[1]) + sys.argv[2]))" "$cfg" "$rel_suffix")
        [ "$parts" = "$root/usr" ] || bad "$label: ${cfg#$root} PACKAGE_PREFIX_DIR resolves to $parts, expected $root/usr"
    done
    # The include directory the configs carry must hold the real header.
    [ -f "$root/usr/include/jsom/jsom.hpp" ] || bad "$label: ${root##*/} has no usr/include/jsom/jsom.hpp for the configs' include directory"
    # And the imported library the configs carry must be there.
    [ -f "$root/usr/$libdir/libjsom.so.$VERSION" ] || bad "$label: the imported library usr/$libdir/libjsom.so.$VERSION is missing"
    [ "$FAILURES" -eq "$before" ] && note "$label: ${root#$WORK/} — configs resolve to $root/usr, include dir and imported library both present"
}

# The two documented ways of consuming the packages, both RUN, not just linked.
write_consumer() {
    local dir="$WORK/consumer"
    mkdir -p "$dir"
    cat > "$dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(jsom_consumer LANGUAGES CXX)
# The documented target is JSOM::jsom. JSOM_LINK_TARGET exists only so the gate can
# point at whatever the installed config really exports when that name is missing,
# which keeps the run proof alive while the naming defect is reported separately.
if(NOT DEFINED JSOM_LINK_TARGET)
    set(JSOM_LINK_TARGET JSOM::jsom)
endif()
find_package(JSOM CONFIG REQUIRED)
if(NOT TARGET ${JSOM_LINK_TARGET})
    message(FATAL_ERROR "the JSOM package does not provide ${JSOM_LINK_TARGET}")
endif()
add_executable(app main.cpp)
target_link_libraries(app PRIVATE ${JSOM_LINK_TARGET})
EOF
    cat > "$dir/main.cpp" <<'EOF'
#include <jsom/jsom.hpp>
#include <jsom/version.hpp>
#include <iostream>
int main() {
    auto doc = jsom::parse_document(R"({"users":[{"name":"Alice","age":30}]})");
    // Read through an RFC 6901 JSON Pointer, and report the generated version —
    // both come from the installed library, not from this file.
    std::cout << doc.at("/users/0/name").as<std::string>() << " " << JSOM_VERSION << "\n";
    return 0;
}
EOF
}

consume_root() { # consume_root <root> <libdir> <label> <tag> [extra cmake args...]
    local root="$1" libdir="$2" label="$3" tag="$4"; shift 4
    local build="$WORK/consume-$tag"
    if ! cmake -S "$WORK/consumer" -B "$build" -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_PREFIX_PATH="$root/usr" \
            -DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=OFF "$@" > "$WORK/consume-$tag.log" 2>&1; then
        tail -n 15 "$WORK/consume-$tag.log" >&2
        bad "$label: find_package(JSOM CONFIG REQUIRED) failed against the extracted packages"
        return
    fi
    if ! cmake --build "$build" >> "$WORK/consume-$tag.log" 2>&1; then
        tail -n 15 "$WORK/consume-$tag.log" >&2
        bad "$label: linking the find_package consumer failed"
        return
    fi
    local out rc
    out=$(LD_LIBRARY_PATH="$root/usr/$libdir" "$build/app" 2>&1); rc=$?
    if [ "$rc" -ne 0 ]; then
        bad "$label: the find_package consumer built but did not RUN: $out"; return
    fi
    if [ "$out" = "Alice $VERSION" ]; then
        note "ok: $label: find_package + link + run OK (reported '$out')"
    else
        bad "$label: the consumer ran but reported '$out', expected 'Alice $VERSION'"
    fi
}

check_pkgconfig() { # check_pkgconfig <root> <libdir> <label> <tag>
    local root="$1" libdir="$2" label="$3" tag="$4"
    local pc="$root/usr/$libdir/pkgconfig"
    local flags
    flags=$(PKG_CONFIG_PATH="$pc" PKG_CONFIG_SYSROOT_DIR="$root" \
            PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 PKG_CONFIG_ALLOW_SYSTEM_LIBS=1 \
            pkg-config --cflags --libs jsom)
    expect_in "$label: jsom.pc carries the include directory" "$flags" "-I$root/usr/include"
    expect_in "$label: jsom.pc carries the library directory" "$flags" "-L$root/usr/$libdir"
    expect_in "$label: jsom.pc names -ljsom" "$flags" "-ljsom"
    expect_in "$label: jsom.pc version" "$(PKG_CONFIG_PATH="$pc" pkg-config --modversion jsom)" "$VERSION"
    if ! $CXX -std=c++17 "$WORK/consumer/main.cpp" $flags -o "$WORK/pc-consumer-$tag" > "$WORK/pc-$tag.log" 2>&1; then
        tail -n 15 "$WORK/pc-$tag.log" >&2
        bad "$label: a pkg-config driven compile failed with: $flags"
        return
    fi
    local out rc
    out=$(LD_LIBRARY_PATH="$root/usr/$libdir" "$WORK/pc-consumer-$tag" 2>&1); rc=$?
    if [ "$rc" -ne 0 ]; then
        bad "$label: the pkg-config built consumer did not RUN: $out"
    elif [ "$out" = "Alice $VERSION" ]; then
        note "ok: $label: pkg-config compile + link + run OK"
    else
        bad "$label: pkg-config consumer reported '$out', expected 'Alice $VERSION'"
    fi
    return 0
}

# The pages ship as scdoc's rendered roff, gzipped. Render to plain text when
# groff/col are available (they strip the overstrike escapes and the generator's
# comment lines); otherwise check the roff source, which still carries the words.
if have groff && have col; then
    render_man() { gzip -dc "$1" | groff -man -Tutf8 2>/dev/null | col -b; }
else
    render_man() { gzip -dc "$1"; }
    note "groff/col not found: man pages checked as rendered roff source"
fi

write_consumer

# ---------------------------------------------------------------------------
# Debian layout
# ---------------------------------------------------------------------------
if [ "$WANT_DEB" = 1 ]; then
    build_layout deb DEB "" || true
    DEB_CLI="$PKGS/jsom_${PV}_${DEBARCH}.deb"
    DEB_RT="$PKGS/libjsom3_${PV}_${DEBARCH}.deb"
    DEB_DEV="$PKGS/libjsom-dev_${PV}_${DEBARCH}.deb"
    for f in "$DEB_CLI" "$DEB_RT" "$DEB_DEV"; do
        [ -f "$f" ] || bad "deb: expected package ${f##*/} was not produced"
    done

    if [ -f "$DEB_RT" ] && [ -f "$DEB_DEV" ] && [ -f "$DEB_CLI" ]; then
        echo "== deb: runtime/development split"
        L="lib/$TRIPLET"
        rt_paths=$(deb_paths "$DEB_RT")
        dev_paths=$(deb_paths "$DEB_DEV")
        cli_paths=$(deb_paths "$DEB_CLI")

        # runtime
        expect_path "deb: libjsom3 ships the versioned library" "$rt_paths" "./usr/$L/libjsom.so.$VERSION"
        expect_path "deb: libjsom3 ships the SONAME link" "$rt_paths" "./usr/$L/libjsom.so.$SOVERSION"
        expect_path_absent "deb: libjsom3 must not ship the bare linker symlink" "$rt_paths" "./usr/$L/libjsom.so"
        expect_not_in "deb: libjsom3 must not ship headers" "$rt_paths" "./usr/include/"
        expect_not_in "deb: libjsom3 must not ship CMake configs" "$rt_paths" "/cmake/"
        expect_not_in "deb: libjsom3 must not ship pkg-config metadata" "$rt_paths" ".pc"
        expect_not_in "deb: libjsom3 must not ship man pages" "$rt_paths" "/man"

        # development
        expect_path "deb: libjsom-dev ships the bare linker symlink" "$dev_paths" "./usr/$L/libjsom.so"
        expect_in "deb: the linker symlink points at the SONAME" "$(dpkg-deb -c "$DEB_DEV")" "libjsom.so -> libjsom.so.$SOVERSION"
        for h in jsom.hpp json_document.hpp json_pointer.hpp version.hpp; do
            expect_path "deb: libjsom-dev ships include/jsom/$h" "$dev_paths" "./usr/include/jsom/$h"
        done
        for c in JSOMConfig.cmake JSOMConfigVersion.cmake JSOMTargets.cmake JSOMTargets-release.cmake; do
            expect_path "deb: libjsom-dev ships cmake/JSOM/$c" "$dev_paths" "./usr/$L/cmake/JSOM/$c"
        done
        expect_path "deb: libjsom-dev ships the pkg-config file" "$dev_paths" "./usr/$L/pkgconfig/jsom.pc"
        expect_path "deb: libjsom-dev ships jsom(3)" "$dev_paths" "./usr/share/man/man3/jsom.3.gz"
        expect_path_absent "deb: libjsom-dev must not ship the runtime library file" "$dev_paths" "./usr/$L/libjsom.so.$VERSION"
        expect_path_absent "deb: libjsom-dev must not ship the SONAME link" "$dev_paths" "./usr/$L/libjsom.so.$SOVERSION"

        # the CLI package
        expect_path "deb: jsom ships the CLI binary" "$cli_paths" "./usr/bin/jsom"
        expect_path "deb: jsom ships jsom(1)" "$cli_paths" "./usr/share/man/man1/jsom.1.gz"
        cli_control=$(dpkg-deb -I "$DEB_CLI")
        expect_in "deb: the CLI package Depends on libjsom3" "$cli_control" "libjsom3"
        expect_in "deb: the CLI package is in section utils" "$cli_control" "Section: utils"
        expect_in "deb: the CLI package version equals CMakeLists.txt" "$cli_control" "Version: $PV"

        # control and the soname declaration
        rt_control=$(dpkg-deb -I "$DEB_RT")
        dev_control=$(dpkg-deb -I "$DEB_DEV")
        expect_in "deb: libjsom3 control version" "$rt_control" "Version: $PV"
        expect_in "deb: libjsom3 Section: libs" "$rt_control" "Section: libs"
        expect_in "deb: libjsom-dev Section: libdevel" "$dev_control" "Section: libdevel"
        expect_in "deb: libjsom-dev depends on the exact runtime version" "$dev_control" "libjsom3 (= $PV)"
        ctrl="$WORK/control-rt"; mkdir -p "$ctrl"
        dpkg-deb -e "$DEB_RT" "$ctrl" 2>/dev/null
        if [ -f "$ctrl/shlibs" ]; then
            expect_in "deb: libjsom3 declares its soname provider in shlibs" "$(cat "$ctrl/shlibs")" "libjsom3"
        else
            bad "deb: libjsom3 has no shlibs control file (CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS)"
        fi

        echo "== deb: consuming the packages as a user would"
        root="$WORK/install-deb"; mkdir -p "$root"
        dpkg-deb -x "$DEB_RT" "$root"
        dpkg-deb -x "$DEB_DEV" "$root"
        dpkg-deb -x "$DEB_CLI" "$root"

        check_config_paths "$root" "deb" "$L"

        names=$(exported_targets "$root")
        if grep -qxF "JSOM::jsom" <<<"$names"; then
            note "ok: deb: the CMake config exports the documented target JSOM::jsom"
        else
            bad "deb: the CMake config exports '$(paste -sd' ' <<<"$names")', not the documented JSOM::jsom — the man page and PACKAGING.md both name JSOM::jsom"
        fi
        link=JSOM::jsom
        grep -qxF "JSOM::jsom" <<<"$names" || link=$(head -1 <<<"$names")
        consume_root "$root" "$L" "deb" deb -DJSOM_LINK_TARGET="$link"
        check_pkgconfig "$root" "$L" "deb" deb

        echo "== deb: the packaged CLI runs and navigates"
        cli_ver=$(LD_LIBRARY_PATH="$root/usr/$L" "$root/usr/bin/jsom" version 2>&1)
        expect_in "deb: jsom version prints the library version" "$cli_ver" "$VERSION"
        # The CLI reads a named file, or stdin when none is given; a file is the
        # form that always resolves, so the gate uses it rather than depending on
        # the argument handling of a stdin run.
        printf '{"users":[{"name":"Alice","age":30}]}' > "$WORK/pointer.json"
        cli_out=$(LD_LIBRARY_PATH="$root/usr/$L" "$root/usr/bin/jsom" pointer get "/users/0/name" "$WORK/pointer.json" 2>&1)
        expect_in "deb: the packaged CLI reads a value through a JSON Pointer" "$cli_out" "Alice"
    fi
fi

# ---------------------------------------------------------------------------
# Fedora layout
# ---------------------------------------------------------------------------
if [ "$WANT_RPM" = 1 ]; then
    build_layout rpm RPM lib64 || true
    RPM_CLI="$PKGS/jsom-${PV}.${RPMARCH}.rpm"
    RPM_RT="$PKGS/jsom-libs-${PV}.${RPMARCH}.rpm"
    RPM_DEV="$PKGS/jsom-devel-${PV}.${RPMARCH}.rpm"
    for f in "$RPM_CLI" "$RPM_RT" "$RPM_DEV"; do
        [ -f "$f" ] || bad "rpm: expected package ${f##*/} was not produced"
    done

    if [ -f "$RPM_RT" ] && [ -f "$RPM_DEV" ] && [ -f "$RPM_CLI" ]; then
        echo "== rpm: runtime/development split"
        rpm_rt_files=$(rpm -qlp "$RPM_RT")
        rpm_dev_files=$(rpm -qlp "$RPM_DEV")
        rpm_cli_files=$(rpm -qlp "$RPM_CLI")
        [ -n "$rpm_rt_files" ] || bad "rpm: jsom-libs contains no files (a static build?)"

        expect_in "rpm: jsom-libs ships the versioned library" "$rpm_rt_files" "/usr/lib64/libjsom.so.$VERSION"
        expect_in "rpm: jsom-libs ships the SONAME link" "$rpm_rt_files" "/usr/lib64/libjsom.so.$SOVERSION"
        expect_not_in "rpm: jsom-libs must not ship headers" "$rpm_rt_files" "/usr/include/"
        expect_not_in "rpm: jsom-libs must not ship CMake configs" "$rpm_rt_files" "/cmake/"

        expect_in "rpm: jsom-devel ships the bare linker symlink" "$rpm_dev_files" "/usr/lib64/libjsom.so"
        expect_in "rpm: jsom-devel ships the CMake config" "$rpm_dev_files" "/usr/lib64/cmake/JSOM/JSOMConfig.cmake"
        expect_in "rpm: jsom-devel ships the exported targets" "$rpm_dev_files" "/usr/lib64/cmake/JSOM/JSOMTargets.cmake"
        expect_in "rpm: jsom-devel ships the pkg-config file" "$rpm_dev_files" "/usr/lib64/pkgconfig/jsom.pc"
        expect_in "rpm: jsom-devel ships jsom(3)" "$rpm_dev_files" "/usr/share/man/man3/jsom.3.gz"
        expect_in "rpm: jsom-devel ships the generated version header" "$rpm_dev_files" "/usr/include/jsom/version.hpp"
        if grep -qE '^/usr/share/man$|^/usr/share/man/man1$|^/usr/share/man/man3$' <<<"$rpm_dev_files"; then
            bad "rpm: jsom-devel ships /usr/share/man directories (they conflict with Fedora's filesystem package)"
        else
            note "ok: rpm: no /usr/share/man directory entries"
        fi

        expect_in "rpm: jsom ships the CLI" "$rpm_cli_files" "/usr/bin/jsom"
        expect_in "rpm: jsom ships jsom(1)" "$rpm_cli_files" "/usr/share/man/man1/jsom.1.gz"

        # metadata from the tools, not by hand
        expect_in "rpm: jsom-libs License" "$(rpm -qp --qf '%{LICENSE}' "$RPM_RT")" "Unlicense"
        expect_not_in "rpm: Description is not CPack boilerplate" "$(rpm -qp --qf '%{DESCRIPTION}' "$RPM_RT")" "created using CPack"
        expect_in "rpm: jsom requires the exact runtime version" "$(rpm -qp --requires "$RPM_CLI")" "jsom-libs = $PV"
        expect_in "rpm: jsom-devel requires the exact runtime version" "$(rpm -qp --requires "$RPM_DEV")" "jsom-libs = $PV"
        expect_in "rpm: jsom-libs version equals CMakeLists.txt" "$(rpm -qp --qf '%{VERSION}-%{RELEASE}' "$RPM_RT")" "$PV"

        echo "== rpm: consuming the packages as a user would"
        rpmroot="$WORK/install-rpm"; mkdir -p "$rpmroot"
        ( cd "$rpmroot" && rpm2cpio "$RPM_RT" | cpio -idm --quiet && rpm2cpio "$RPM_DEV" | cpio -idm --quiet ) \
            || bad "rpm: could not extract the packages with rpm2cpio | cpio"

        check_config_paths "$rpmroot" "rpm" "lib64"

        names=$(exported_targets "$rpmroot")
        if grep -qxF "JSOM::jsom" <<<"$names"; then
            note "ok: rpm: the CMake config exports the documented target JSOM::jsom"
        else
            bad "rpm: the CMake config exports '$(paste -sd' ' <<<"$names")', not the documented JSOM::jsom"
        fi
        # lib64 is not searched by Debian's CMake, so name the config directly; the
        # Fedora install itself is dnf on the laptop, which no script here mirrors.
        link=JSOM::jsom
        grep -qxF "JSOM::jsom" <<<"$names" || link=$(head -1 <<<"$names")
        consume_root "$rpmroot" "lib64" "rpm" rpm \
            -DJSOM_DIR="$rpmroot/usr/lib64/cmake/JSOM" -DJSOM_LINK_TARGET="$link"
    fi
fi

# ---------------------------------------------------------------------------
# the man pages: render, and check the text names the right things
# ---------------------------------------------------------------------------
if [ "$WANT_DEB" = 1 ] && [ -f "$WORK/install-deb/usr/share/man/man3/jsom.3.gz" ]; then
    echo "== deb: the shipped man pages render"
    man1=$(render_man "$WORK/install-deb/usr/share/man/man1/jsom.1.gz")
    man3=$(render_man "$WORK/install-deb/usr/share/man/man3/jsom.3.gz")
    expect_in "man: jsom(1) documents the format command" "$man1" "format"
    expect_in "man: jsom(1) documents the pointer command" "$man1" "pointer"
    expect_in "man: jsom(3) names the documented CMake target JSOM::jsom" "$man3" "JSOM::jsom"
    expect_in "man: jsom(3) names the runtime packages" "$man3" "libjsom3"
    expect_not_in "man: jsom(3) has no HTML entities" "$man3" "&lt;"
    # The pages state no numeric version of their own (the version has one home,
    # the project() line). If one ever appears, it must be this version.
    man_versions=$(printf '%s\n%s\n' "$man1" "$man3" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | sort -u)
    if [ -n "$man_versions" ]; then
        while read -r v; do
            [ "$v" = "$VERSION" ] || bad "man: a page states version $v, but CMakeLists.txt says $VERSION"
        done <<<"$man_versions"
        grep -qvxF "$VERSION" <<<"$man_versions" || note "ok: man: every version a page states is $VERSION"
    else
        note "ok: man: the pages state no version of their own (single-source rule)"
    fi
fi

# ---------------------------------------------------------------------------
# sabotage: prove the assertions above can fail
# ---------------------------------------------------------------------------
# Harri's rule: a gate that has never been seen to fail is not a gate. Two
# deletions, each on a throwaway COPY of a real package, each of which the gate
# must notice. The real packages are never touched.
if [ "${JSOM_PACKAGE_SKIP_SABOTAGE:-0}" != 1 ] && [ "$WANT_DEB" = 1 ] && [ -f "${DEB_RT:-}" ] && [ -f "${DEB_DEV:-}" ]; then
    echo "== sabotage: the gate must notice a broken package"
    SAB="$WORK/sabotage"; mkdir -p "$SAB"
    L="lib/$TRIPLET"
    # Link the consumer at whatever target the REAL package exports, so this
    # section is about the deleted file and not about the exported name.
    sab_link=$(head -1 <<<"$(exported_targets "$WORK/install-deb")")
    [ -n "$sab_link" ] || sab_link=JSOM::jsom

    # (i) the runtime package without its SONAME link.
    tree="$SAB/rt-tree"; rm -rf "$tree"
    dpkg-deb -R "$DEB_RT" "$tree" 2>/dev/null || bad "sabotage (i): could not unpack libjsom3"
    rm -f "$tree/usr/$L/libjsom.so.$SOVERSION"
    sab_rt="$SAB/libjsom3-no-soname.deb"
    dpkg-deb --build --root-owner-group "$tree" "$sab_rt" >/dev/null 2>&1 || bad "sabotage (i): could not repack libjsom3"
    if grep -qxF -- "./usr/$L/libjsom.so.$SOVERSION" <<<"$(deb_paths "$sab_rt")"; then
        bad "sabotage (i): removed libjsom.so.$SOVERSION but the split assertion still saw it"
    else
        note "sabotage (i): with libjsom.so.$SOVERSION removed, the 'libjsom3 ships the SONAME link' assertion fires"
    fi
    sab_root="$SAB/rt-root"; rm -rf "$sab_root"; mkdir -p "$sab_root"
    dpkg-deb -x "$sab_rt" "$sab_root"; dpkg-deb -x "$DEB_DEV" "$sab_root"
    if cmake -S "$WORK/consumer" -B "$SAB/rt-consumer" -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_PREFIX_PATH="$sab_root/usr" -DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=OFF \
            -DJSOM_LINK_TARGET="$sab_link" \
            > "$SAB/rt-consumer.log" 2>&1 \
       && cmake --build "$SAB/rt-consumer" >> "$SAB/rt-consumer.log" 2>&1; then
        out=$(LD_LIBRARY_PATH="$sab_root/usr/$L" "$SAB/rt-consumer/app" 2>&1); rc=$?
        if [ "$rc" -ne 0 ] && grep -qF "libjsom.so.$SOVERSION" <<<"$out"; then
            note "sabotage (i): the consumer dies at startup exactly as the split exists to prevent — $(head -1 <<<"$out")"
        else
            bad "sabotage (i): the consumer did not fail with 'libjsom.so.$SOVERSION: cannot open shared object file' (rc=$rc, out=$out)"
        fi
    else
        bad "sabotage (i): could not build the consumer against the sabotaged runtime package"
    fi

    # (ii) the development package without the bare linker symlink.
    tree="$SAB/dev-tree"; rm -rf "$tree"
    dpkg-deb -R "$DEB_DEV" "$tree" 2>/dev/null || bad "sabotage (ii): could not unpack libjsom-dev"
    rm -f "$tree/usr/$L/libjsom.so"
    sab_dev="$SAB/libjsom-dev-no-namelink.deb"
    dpkg-deb --build --root-owner-group "$tree" "$sab_dev" >/dev/null 2>&1 || bad "sabotage (ii): could not repack libjsom-dev"
    if grep -qxF -- "./usr/$L/libjsom.so" <<<"$(deb_paths "$sab_dev")"; then
        bad "sabotage (ii): removed libjsom.so but the linker-symlink assertion still saw it"
    else
        note "sabotage (ii): with libjsom.so removed, the 'libjsom-dev ships the bare linker symlink' assertion fires"
    fi
    sab_root="$SAB/dev-root"; rm -rf "$sab_root"; mkdir -p "$sab_root"
    dpkg-deb -x "$DEB_RT" "$sab_root"; dpkg-deb -x "$sab_dev" "$sab_root"
    sab_flags=$(PKG_CONFIG_PATH="$sab_root/usr/$L/pkgconfig" PKG_CONFIG_SYSROOT_DIR="$sab_root" \
                PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 PKG_CONFIG_ALLOW_SYSTEM_LIBS=1 pkg-config --cflags --libs jsom)
    if $CXX -std=c++17 "$WORK/consumer/main.cpp" $sab_flags -o "$SAB/sab-consumer" > "$SAB/dev-link.log" 2>&1; then
        bad "sabotage (ii): the pkg-config consumer still linked with no libjsom.so — the link check is blind"
    elif grep -qF "cannot find -ljsom" "$SAB/dev-link.log"; then
        note "sabotage (ii): with libjsom.so removed, the pkg-config link fails — the linker cannot resolve -ljsom"
    else
        bad "sabotage (ii): the link failed, but not with 'cannot find -ljsom': $(head -1 "$SAB/dev-link.log")"
    fi

    # the real packages were never touched
    expect_path "sabotage: the real libjsom3 still ships its SONAME link" "$(deb_paths "$DEB_RT")" "./usr/$L/libjsom.so.$SOVERSION"
    expect_path "sabotage: the real libjsom-dev still ships its linker symlink" "$(deb_paths "$DEB_DEV")" "./usr/$L/libjsom.so"
fi

# ---------------------------------------------------------------------------
# summary
# ---------------------------------------------------------------------------
echo
if [ "$FAILURES" -eq 0 ]; then
    echo "PASS: packages built and consumed, the split and the config paths verified, the consumers ran, and the sabotage proved the assertions fire"
    exit 0
fi
echo "FAIL: $FAILURES check(s) failed (see the FAIL lines above)" >&2
exit 1
