#!/usr/bin/env bash
# release.sh — JSOM's release process, in one place and in that order.
#
# WHY THIS EXISTS: the same reason as FSMgine's and FSMTable's. Packages were once built
# by a script outside this repository, so nothing in the repo said how a release is made
# and the packages could drift from the install rules. This script is the process, and
# `packages` is the half that turns the declared version into files the pool can serve.
#
#   tools/release.sh status            what version this repo declares, and what is unreleased
#   tools/release.sh prepare [--apply] [--set X.Y.Z]
#                                      propose the next version from the commits since the
#                                      last tag; --apply writes it to CMakeLists.txt
#   tools/release.sh notes [--open]    draft the release notes, compatibility table first
#   tools/release.sh packages          build and consume the deb and rpm packages on THIS
#                                      machine, stage them for the pool, and print the one
#                                      step that needs another machine (arm64)
#   tools/release.sh publish --yes     tag and publish on GitHub (refuses unless everything
#                                      is in order, including a filled-in compatibility table)
#
# THE VERSION HAS ONE HOME: `project(JSOM VERSION x.y.z)` in CMakeLists.txt. The installed
# header <jsom/version.hpp> is generated from it at configure time, the CMake package version
# is generated from it, and the packaged version is generated from it, so they cannot drift.
#
# WHAT THIS SCRIPT WILL NOT DO: write your compatibility table. The mechanical parts are
# inferable from git; how an existing user's code or packages are affected is a judgement,
# and a generated answer would be a confidently wrong one. It scaffolds the table and blocks
# publish until you fill it in.
#
# ORDER MATTERS, and it is deliberate: build and verify every artifact, publish to the pool,
# confirm the pool serves the version, and only THEN tag. A tag that exists while an
# architecture cannot install the version is a claim the packages do not support.
#
# Everything here is local. No GitHub Actions, no CI service: the gate is scripts/gate.sh.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1

NOTES_DIR=".release"          # gitignored: notes are drafts until published on GitHub
PROJECT="JSOM"
STAGING="$HOME/hermes-workspace/pkg-repo/staging"
PUBLISH_DIR="$HOME/hermes-workspace/pkg-repo"

die()  { printf 'release: %s\n' "$*" >&2; exit 1; }
info() { printf '  %s\n' "$*"; }
head_() { printf '\n== %s ==\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1; }

declared_version() {
    sed -n "s/^project(${PROJECT} VERSION \([0-9][0-9.]*\).*/\1/p" CMakeLists.txt | head -1
}
latest_tag() { git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null || true; }
first_commit() { git rev-list --max-parents=0 HEAD | tail -1; }
range_since_tag() { local t; t=$(latest_tag); printf '%s..HEAD' "${t:-$(first_commit)}"; }

# The commits that break compatibility, if any: either the `!` marker or a BREAKING line.
breaking_commits() {
    git log --format='%h%x09%s' "$(range_since_tag)" \
        | grep -E '\t[a-z]+(\([^)]*\))?!:' || true
}

# Harri's convention for his own libraries: while a library has no consumers, a breaking
# change is a MINOR bump. JSOM has consumers (jsonTools, Computo, Permuto), so a breaking
# change is treated as MINOR too — the honest MAJOR is the caller's call via --set.
recommend_bump() {
    if [ -n "$(breaking_commits)" ]; then echo minor
    elif git log --format='%s' "$(range_since_tag)" | grep -qE '^feat(\([^)]*\))?:'; then echo minor
    else echo patch; fi
}

bump_version() { # bump_version <version> <major|minor|patch>
    local v="$1" kind="$2"
    local maj min pat
    IFS=. read -r maj min pat <<< "$v"
    case "$kind" in
        major) printf '%d.0.0' "$((maj + 1))" ;;
        minor) printf '%d.%d.0' "$maj" "$((min + 1))" ;;
        patch) printf '%d.%d.%d' "$maj" "$min" "$((pat + 1))" ;;
    esac
}

cmd_status() {
    git rev-parse --git-dir >/dev/null 2>&1 || die "not a git repository"
    local v tag n
    v=$(declared_version); [ -n "$v" ] || die "no version in CMakeLists.txt"
    tag=$(latest_tag)
    head_ "JSOM release status"
    info "declared version : $v   (CMakeLists.txt project(), the only home)"
    info "newest tag       : ${tag:-none}"
    if [ -n "$tag" ] && [ "$(git rev-list -n1 "$tag")" = "$(git rev-parse HEAD)" ]; then
        info "HEAD             : tagged $tag"
    else
        info "HEAD             : untagged"
    fi
    if [ -n "$tag" ]; then
        info "unreleased       : $(git rev-list --count "$tag..HEAD") commit(s) since $tag"
    fi
    if [ -n "$tag" ] && [ "$v" != "${tag#v}" ]; then
        info "pending release  : $v is declared but untagged — THIS is the release to make"
        info "                   next: scripts/gate.sh, tools/release.sh packages, then publish"
    else
        info "recommended bump : $(recommend_bump)  ->  would make $(bump_version "$v" "$(recommend_bump)")"
    fi
    local b; b=$(breaking_commits)
    if [ -n "$b" ]; then
        info "note             : breaking commits present; by convention that is a MINOR bump"
        head_ "commits marked breaking"
        printf '%s\n' "$b" | sed 's/^/  /'
    fi
    if [ -n "$(git status --porcelain)" ]; then
        info "working tree     : DIRTY — commit before releasing"
    else
        info "working tree     : clean"
    fi
    info "gate             : scripts/gate.sh  (all stages; the pre-push hook runs them too)"
    info "version header   : generated at configure time, <jsom/version.hpp>"
    info "packages         : tools/release.sh packages (deb + rpm here; arm64 builds on the Pi)"
}

cmd_prepare() {
    local apply=0 v kind next set=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --apply)   apply=1 ;;
            --set)     shift; set="${1:-}" ;;
            --set=*)   set="${1#--set=}" ;;
            *)         die "unknown prepare option: $1" ;;
        esac
        shift
    done
    v=$(declared_version)
    if [ -n "$set" ]; then
        # A deliberate bump: the convention below (breaking -> minor) is a default, not a rule.
        printf '%s' "$set" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$' || die "--set wants X.Y.Z, got '$set'"
        kind="set"; next="$set"
    else
        kind=$(recommend_bump); next=$(bump_version "$v" "$kind")
    fi
    head_ "prepare"
    local t; t=$(latest_tag); info "commits since ${t:-the first commit}:"
    git log --format='  %h %s' "$(range_since_tag)" | head -25
    info ""
    info "recommended: $kind  ->  $next   (declared today: $v)"
    if [ -z "$set" ] && [ -n "$(latest_tag)" ] && [ "$v" != "$(latest_tag | sed 's/^v//')" ]; then
        info "NOTE: $v is declared but untagged, so a release at $v looks pending. Bumping now"
        info "      skips past it; --apply is only right if you mean to abandon $v."
    fi
    if [ "$apply" != "1" ]; then
        info "re-run with --apply to write it to CMakeLists.txt"
        return 0
    fi
    grep -q "^project(${PROJECT} VERSION ${v} " CMakeLists.txt \
        || die "CMakeLists.txt does not declare $v: refusing to guess"
    sed -i "s/^project(${PROJECT} VERSION ${v} /project(${PROJECT} VERSION ${next} /" CMakeLists.txt
    grep -q "^project(${PROJECT} VERSION ${next} " CMakeLists.txt \
        || die "the bump did not land: check CMakeLists.txt"
    info ""
    info "wrote $next to CMakeLists.txt — the generated <jsom/version.hpp> follows on rebuild"
    info "next: cmake/CPack.cmake's JSOM_PACKAGE_RELEASE must be 1 for a NEW upstream version"
    info "      (bump it only for a packaging-only fix of the same version), then:"
    info "      scripts/gate.sh, commit, push, tools/release.sh packages, notes, publish"
}

cmd_notes() {
    local open=0 v tag
    for arg in "$@"; do [ "$arg" = "--open" ] && open=1; done
    v=$(declared_version); tag=$(latest_tag)
    mkdir -p "$NOTES_DIR"
    local out="$NOTES_DIR/notes-v$v.md"
    {
        printf '# %s v%s\n\n' "$PROJECT" "$v"
        printf '_Released %s from `%s`._\n\n' "$(date +%F)" "$(git rev-parse --short HEAD)"
        printf '## Compatibility\n\n'
        printf '| for code that… | before | now |\n'
        printf '| :--- | :--- | :--- |\n'
        printf '| TODO: compiles against the public headers (signatures, headers, macros) | | |\n'
        printf '| TODO: behaves the same at run time for the same input | | |\n'
        printf '| TODO: builds and packages (CMake targets, find_package version, options) | | |\n'
        printf '| TODO: anything removed, renamed, or newly required — and say explicitly when something still exists but CHANGED | | |\n'
        printf '| TODO: ABI/soname, if anything crosses the library boundary | | |\n\n'
        if [ -n "$(breaking_commits)" ]; then
            printf '## Breaking changes\n\n'
            breaking_commits | sed 's/^[0-9a-f]*\t/- /'
            printf '\n'
        fi
        printf '## Commits since %s\n\n' "${tag:-the first commit}"
        git log --format='- %s (%h)' "$(range_since_tag)"
        printf '\n## Packages\n\n'
        printf 'Published for amd64 and arm64 (.deb) and x86_64 (.rpm). Fill in what the pool\n'
        printf 'serves once it is checked, and name any architecture that is still missing.\n\n'
        printf '## Verification\n\n'
        printf 'The full local gate is green at this commit: `scripts/gate.sh`\n\n'
        printf '```\nscripts/gate.sh tree format build tests consumer asan fuzz tsan std cli conform docs tidy pristine\n```\n\n'
        printf 'And `scripts/package.sh` builds the deb and rpm layouts, then CONSUMES them: a real\n'
        printf 'consumer is configured against the extracted packages with\n'
        printf '`CMAKE_FIND_USE_CMAKE_SYSTEM_PATH=OFF` and run, and the split is sabotage-tested.\n\n'
        printf 'No counts here on purpose: they drift. Read the gate output for the numbers.\n'
    } > "$out"
    head_ "notes"
    info "drafted $out"
    info "THE COMPATIBILITY TABLE IS A PLACEHOLDER — fill it in yourself; release.sh publish refuses while it says TODO."
    [ "$open" = "1" ] && ${EDITOR:-vi} "$out"
    return 0
}

cmd_packages() {
    local v; v=$(declared_version)
    head_ "packages v$v"
    [ -x scripts/package.sh ] || die "scripts/package.sh is missing: cannot build or verify packages"
    have cmake || die "cmake not installed"
    have cpack || die "cpack not installed"
    [ -z "$(git status --porcelain)" ] || die "working tree is dirty: commit before building packages"
    info "building and consuming both layouts on this machine (this is the deb + rpm half)…"
    ./scripts/package.sh || die "scripts/package.sh failed: fix it before publishing anything"
    local out="$STAGING/${PROJECT,,}-$v"
    rm -rf "$out"; mkdir -p "$out"
    local deb_dir="build-package-deb" rpm_dir="build-package-rpm"
    # scripts/package.sh deliberately leaves no artifacts (it keeps scratch only on failure),
    # so build the publishable set explicitly into known directories.
    cmake -S . -B "$deb_dir" -DJSOM_ENABLE_PACKAGING=ON -DBUILD_SHARED_LIBS=ON \
          -DJSOM_BUILD_TESTS=OFF -DJSOM_BUILD_BENCHMARKS=OFF -DCMAKE_INSTALL_PREFIX=/usr \
          >/dev/null || die "deb packaging configure failed"
    cmake --build "$deb_dir" -j"$(nproc)" >/dev/null || die "deb packaging build failed"
    ( cd "$deb_dir" && cpack -G DEB >/dev/null ) || die "cpack -G DEB failed"
    cmake -S . -B "$rpm_dir" -DJSOM_ENABLE_PACKAGING=ON -DBUILD_SHARED_LIBS=ON \
          -DJSOM_BUILD_TESTS=OFF -DJSOM_BUILD_BENCHMARKS=OFF -DCMAKE_INSTALL_PREFIX=/usr \
          -DCMAKE_INSTALL_LIBDIR=lib64 >/dev/null || die "rpm packaging configure failed"
    cmake --build "$rpm_dir" -j"$(nproc)" >/dev/null || die "rpm packaging build failed"
    ( cd "$rpm_dir" && cpack -G RPM >/dev/null ) || die "cpack -G RPM failed"
    cp "$deb_dir"/*.deb "$rpm_dir"/*.rpm "$out"/ || die "could not stage the artifacts"
    info "staged $(ls -1 "$out" | wc -l) artifacts in $out:"
    ls -1 "$out" | sed 's/^/    /'
    head_ "the arm64 half (needs the Pi; nothing here can build it)"
    info "git archive --format=tar HEAD | ssh hermes2 'mkdir -p /tmp/pkg && tar -x -C /tmp/pkg'"
    info "ssh hermes2 'cd /tmp/pkg && ./scripts/package.sh deb'   # builds AND consumes on arm64"
    info "ssh hermes2 'cd /tmp/pkg/build-package-deb && cpack -G DEB'  # keep the artifacts"
    info "scp hermes2:/tmp/pkg/build-package-deb/*.deb $out/"
    head_ "publish (only after every architecture is staged)"
    info "cd $PUBLISH_DIR && ./publish-apt.sh add staging/${PROJECT,,}-$v/*.deb"
    info "cd $PUBLISH_DIR && ./publish-rpm.sh add staging/${PROJECT,,}-$v/*.rpm"
    info "then confirm the pool serves $v on each machine before tagging."
}

cmd_publish() {
    local yes=0 v tag notes
    for arg in "$@"; do [ "$arg" = "--yes" ] && yes=1; done
    v=$(declared_version); notes="$NOTES_DIR/notes-v$v.md"
    head_ "publish v$v"
    [ -z "$(git status --porcelain)" ] || die "working tree is dirty: commit first"
    [ "$(git rev-parse --abbrev-ref HEAD)" = "main" ] || die "not on main"
    [ -z "$(git describe --tags --exact-match 2>/dev/null)" ] || die "HEAD is already tagged"
    git rev-parse "v$v" >/dev/null 2>&1 && die "tag v$v already exists"
    git rev-parse origin/main >/dev/null 2>&1 || die "no origin/main to compare against"
    [ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || die "HEAD differs from origin/main: push first"
    have gh || die "gh not installed"
    [ -f "$notes" ] || { info "no draft yet: generating"; cmd_notes; }
    grep -q 'TODO' "$notes" && die "the compatibility table in $notes is still a placeholder"
    [ "$yes" = "1" ] || die "this publishes publicly: re-run with --yes"
    # The draft may have been written days ago, so stamp it at publish time: BOTH the date and
    # the commit being released. Re-dating while keeping the old commit told a small public lie
    # on a release once ("from <the commit before the release commit>").
    sed -i "s/^_Released .* from \`[0-9a-f]*\`\._$/_Released $(date +%F) from \`$(git rev-parse --short HEAD)\`._/" "$notes"
    info "tagging and publishing…"
    git tag -a "v$v" -m "$PROJECT v$v"
    git push origin "v$v" || die "could not push the tag"
    gh release create "v$v" --title "$PROJECT v$v" --notes-file "$notes" \
        || die "tag pushed, but creating the GitHub release failed: run gh release create manually"
    info "published v$v"
}

usage() {
    sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
}

cmd="${1:-status}"; shift || true
case "$cmd" in
    status)  cmd_status "$@" ;;
    prepare) cmd_prepare "$@" ;;
    notes)   cmd_notes "$@" ;;
    packages) cmd_packages "$@" ;;
    publish) cmd_publish "$@" ;;
    help|-h|--help) usage ;;
    *) die "unknown command '$cmd' (try: status, prepare, notes, packages, publish)" ;;
esac
