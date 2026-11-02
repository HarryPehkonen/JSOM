# Packaging JSOM

JSOM is published as Debian and Fedora packages from this repository. The packages
are produced by CPack from the install rules in `CMakeLists.txt`, so the installed
layout and the packages can no longer disagree — which is what the hand-written
builder that produced the first `jsom`/`libjsom-dev` packages could not promise.

## What ships

| distribution | package | contents |
|---|---|---|
| Debian | `jsom` | `/usr/bin/jsom`, `jsom(1)` |
| Debian | `libjsom3` | `libjsom.so.3`, `libjsom.so.3.1.2`, and the soname declaration (`shlibs`) |
| Debian | `libjsom-dev` | headers, generated `version.hpp`, CMake configs, `jsom.pc`, `libjsom.so`, `jsom(3)` |
| Fedora | `jsom` | `/usr/bin/jsom`, `jsom(1)` |
| Fedora | `jsom-libs` | `libjsom.so.3`, `libjsom.so.3.1.2` |
| Fedora | `jsom-devel` | headers, generated `version.hpp`, CMake configs, `jsom.pc`, `libjsom.so`, `jsom(3)` |

The package names follow each distribution's convention rather than one shared
scheme: Debian requires a shared-library package to carry the soname's major
(`libjsom3`), while Fedora keeps the project name for the CLI and puts the runtime
library in a `-libs` subpackage.

### Why the split is not cosmetic

`NAMELINK_COMPONENT` in the install rules puts the **soname link** in the runtime
package and the **bare `.so`** in the development package. That single annotation is
the difference between a working consumer and a broken one: a package that ships
only `libjsom.so.3.1.2` and forgets `libjsom.so.3` makes every dynamically linked
program die at startup with

    libjsom.so.3: cannot open shared object file

while the package itself looks complete. `scripts/package.sh` consumes the built
packages to prove the split holds, rather than trusting a file listing.

### What is *not* packaged

- **The static archive.** A default in-tree build (`cmake -S . -B build`) produces a
  static `libjsom.a`; the packages ship the shared library only. Consumers that want
  static linking build JSOM from source, as jsonTools, Computo and Permuto already do
  through FetchContent.
- Tests, benchmarks and fuzz targets. They are built by the development gate, never
  installed.

## Building the packages

```bash
cmake -S . -B build-pkg \
      -DJSOM_ENABLE_PACKAGING=ON \
      -DBUILD_SHARED_LIBS=ON \
      -DJSOM_BUILD_TESTS=OFF \
      -DJSOM_BUILD_BENCHMARKS=OFF \
      -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-pkg -j"$(nproc)"
(cd build-pkg && cpack -G DEB)     # or -G RPM
```

- `-DBUILD_SHARED_LIBS=ON` is required, not preferred: `cmake/CPack.cmake` refuses a
  static packaging build, because with static targets the runtime package is empty
  and the archive silently lands in the development package.
- `-DCMAKE_INSTALL_PREFIX=/usr` is required too. This repository defaults its install
  prefix to `~/.local` for development, and `GNUInstallDirs` derives
  `CMAKE_INSTALL_LIBDIR` from the prefix at configure time — with the default prefix
  the library installs to `/usr/lib` instead of Debian's multiarch
  `/usr/lib/x86_64-linux-gnu`.
- The man pages are rendered from `man/*.scd` by scdoc during the build; a broken page
  fails the build. scdoc is required for a package build and optional for a
  development build.

## Versioning

The library version has exactly one home: the `project(JSOM VERSION x.y.z)` line in
`CMakeLists.txt`. Everything else is generated from it — `<jsom/version.hpp>`, the
CMake package version, the man pages' stated version and the package versions.

The **packaging release** is a separate counter, `JSOM_PACKAGE_RELEASE` in
`cmake/CPack.cmake`. A packaging-only fix (a corrected control field, a moved file, a
fixed description) ships as `3.1.2-2` without pretending the library or the CLI
changed. The packaging release is **not** a git tag: the tag names the source
revision (`v3.1.2`), the packaging release names the build of it.

For 3.1.2 the release read `2` rather than `1`, for a measured Fedora reason. The pool
already carried `jsom 3.1.2-1.fc44` — published with a Fedora dist tag by the hand builder
this repository supersedes — and rpm's own comparison ranks that release *above* a plain
`1`, as printed by the machine that consumes it:

    rpm --eval '%{lua: print(rpm.vercmp("3.1.2-1.fc44","3.1.2-1"))}'    ->  1

so a package numbered `1` would never have been offered as an upgrade to a machine that
already had one, and republishing `3.1.2-1.fc44` was not an option: a pool entry must never
change under its own name-version-release. **A new upstream version starts over at 1** —
3.2.0 ships as `3.2.0-1` in every form.

## Verification

```bash
scripts/package.sh              # both layouts
scripts/package.sh deb          # one layout
```

It builds the packages, then checks them the way a user would experience them:

- the three packages exist with the expected names for the layout;
- the runtime package contains `libjsom.so.3` and `libjsom.so.3.1.2` and *not* the
  bare `libjsom.so`, while the development package contains the bare symlink,
  the headers, the four CMake config files, the pkg-config file and `jsom(3)`;
- every path the installed CMake configs resolve to exists inside the extracted
  development package;
- a real consumer — `find_package(JSOM CONFIG REQUIRED)`, link `JSOM::jsom`, parse a
  document and read it through a JSON Pointer — is configured, built and **run**
  against the *extracted packages*, with `CMAKE_FIND_USE_CMAKE_SYSTEM_PATH=OFF` so a
  system copy cannot satisfy the test;
- the same consumer is built through `pkg-config` and run;
- the shipped man pages render and name the right things;
- the packaged version equals the version in `CMakeLists.txt`.

## Releasing

`tools/release.sh` is the whole process, in this order:

```bash
tools/release.sh status                          # declared version, newest tag, what is unreleased
tools/release.sh prepare --set X.Y.Z --apply     # writes the version to CMakeLists.txt
tools/release.sh notes                           # drafts .release/notes-vX.Y.Z.md (gitignored)
tools/release.sh packages                        # builds AND consumes both layouts here, then stages them
tools/release.sh publish --yes                   # tag and create the GitHub release
```

`publish` refuses to run while the notes' compatibility table still says TODO, unless the
tree is clean, `main` is the branch, HEAD equals `origin/main`, and the tag does not already
exist. `packages` runs `scripts/package.sh` first and prints the arm64 half to run on the Pi,
because nothing on this machine can build it.

The order is deliberate and matches the fleet rule: **build and verify every artifact,
publish to the pool, confirm the pool serves the version, and only then tag.** A tag that
exists while an architecture cannot install the version is a claim the packages do not
support.

Publishing into the pool (`pehkonen.dev`) is done by `pkg-repo/publish-apt.sh` and
`pkg-repo/publish-rpm.sh` on the host that holds the signing key. Both accept a whole set in
one pass (`add FILE [FILE ...]`) so the pool never serves a partially updated set. amd64 and
arm64 build natively on the machines that hold them; armhf is built with the armv6 baseline
in a Raspbian chroot, because Debian's own armhf baseline is armv7 and would not run on the
armv6 print server.

## CLI help-text defects (fixed in 3.2.0)

Three defects were found while writing `jsom(1)`, and the man page documented the real
behaviour until the CLI could be fixed:

- `jsom pointer --help` advertised `--max-depth`, `--include-values` and `--format`, but
  `pointer` rejects every `--` argument after a subcommand
  (`Unknown option: --format=json`, exit 1). The flags were stale, left over from the
  removed path cache; **3.2.0 removes them from the help.**
- `jsom format --help` printed `--inline-arrays` default 10 and `--max-width` default 120
  (the bare struct defaults), while `jsom format` starts from the *pretty* preset: inline
  arrays 8, max width 100. **3.2.0 prints what the command uses**, from the same constants
  the preset uses, so the two cannot drift again.
- `echo '{"a":1}' | jsom pointer get /a` failed with `Error: Cannot open file: /a`: the
  pointer operand was taken for a filename, so the stdin form `jsom(1)` documents did not
  work for pointer reads (nor for `pointer list`). **3.2.0 reads standard input when no
  file is given**, and `tools/cli_smoke.sh` covers file, stdin and the RFC 6901 escapes.
