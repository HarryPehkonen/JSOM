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

It currently reads `2`, not `1`, for a measured Fedora reason. The pool already
carried `jsom 3.1.2-1.fc44` — published with a Fedora dist tag by the hand builder
this repository supersedes — and rpm's own comparison ranks that release *above* a
plain `1`, as printed by the machine that consumes it:

    rpm --eval '%{lua: print(rpm.vercmp("3.1.2-1.fc44","3.1.2-1"))}'    ->  1

so a package numbered `1` would never have been offered as an upgrade to a machine
that already had one. The first packaging revision that can actually reach those
machines is therefore `2`. Both ecosystems carry the same release, because the forms
are meant to be interchangeable. Republishing `3.1.2-1.fc44` was not an option: a pool
entry must never change under its own name-version-release.

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

There is no release tool in this repository yet; `tools/release.sh` is being added
across the fleet (FSMgine and FSMTable already have it — a clean tree and
`HEAD == origin/main` are required, notes are drafted in a gitignored `.release/`,
and the annotated tag is created last). Until it lands here, a release is: bump the
`project()` line, build and verify the packages, publish them, then tag.

Publishing into the pool (`pehkonen.dev`) is done by `pkg-repo/publish-apt.sh` and
`pkg-repo/publish-rpm.sh` on the host that holds the signing key. The fleet rule is
that a release is only announced once every architecture carries the version — amd64
and arm64 build natively on the machines that hold them, and armhf is built with the
armv6 baseline in a Raspbian chroot, because Debian's own armhf baseline is armv7 and
would not run on the armv6 print server.

## Known issues in the CLI's own help text

Two defects were found while writing `jsom(1)`, and the man page documents the real
behaviour rather than the help text's claim:

- `jsom pointer --help` advertises `--max-depth`, `--include-values` and `--format`,
  but `pointer` rejects every `--` argument after a subcommand
  (`Unknown option: --format=json`, exit 1). The flags are stale.
- `jsom format --help` prints `--inline-arrays` default 10 and `--max-width` default
  120 (the struct defaults), while `jsom format` actually starts from the *pretty*
  preset: inline arrays 8, max width 100
  (`PRETTY_INLINE_ARRAY_SIZE` / `PRETTY_MAX_LINE_WIDTH` in
  `include/jsom/constants.hpp`).
