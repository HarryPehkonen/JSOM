# CPack — three packages, one component split, named per each distribution's
# convention.
#
# Debian:   jsom          (CLI)           libjsom3        (runtime)   libjsom-dev   (development)
# Fedora:   jsom          (CLI)           jsom-libs       (runtime)   jsom-devel    (development)
#
# Why the split is not cosmetic: the COMPONENT annotations on the install rules in
# CMakeLists.txt put the SONAME link (libjsom.so.3) in the runtime package via
# NAMELINK_COMPONENT, and the bare .so symlink, the headers, the generated
# version.hpp, the CMake package configs, the pkg-config file and the jsom(3) man
# page in the development package. A hand-rolled deb that shipped only the library
# file and no soname link made every dynamically linked consumer die at startup
# with
#   libjsom.so.3: cannot open shared object file
# while the package itself looked complete. That is what this layout prevents, and
# scripts/package.sh consumes the built packages to prove it.
#
# The library file name is jsom (OUTPUT_NAME on the jsom_lib target), so the
# shared library installs as libjsom.so.3 and the pkg-config/CMake consumers link
# -ljsom — not -ljsom_lib, which is the name the internal target used to leak into
# the installed tree.
#
# Versioning: PROJECT_VERSION is the library version (one home: the project() line
# in CMakeLists.txt). The packaging release below is separate, so a
# packaging-only fix ships as 3.1.2-2 without pretending the library changed.
# See PACKAGING.md.

set(JSOM_PACKAGE_RELEASE 1)
set(JSOM_PACKAGE_VERSION "${PROJECT_VERSION}-${JSOM_PACKAGE_RELEASE}")

# This tree builds STATIC libraries by default (BUILD_SHARED_LIBS unset), which is
# what the tests and the fuzz targets want. Packaging the other way round is not a
# preference: with static targets there is nothing for the runtime component to
# hold, and CPack silently produces an EMPTY libjsom3/jsom-libs package while the
# .a archive lands in the development package. Refuse that loudly instead of
# shipping broken packages.
if(NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR
        "Packaging JSOM requires shared libraries: configure with -DBUILD_SHARED_LIBS=ON. "
        "Without it the runtime package would be empty and the .a archive would ship in the "
        "development package. See PACKAGING.md.")
endif()

set(CPACK_PACKAGE_NAME "jsom")
set(CPACK_PACKAGE_VENDOR "Harri Pehkonen")
set(CPACK_PACKAGE_CONTACT "Harri Pehkonen <harry.pehkonen@gmail.com>")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/HarryPehkonen/JSOM")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "JSOM — JavaScript Object Models (C++17 JSON library)")
# Not CPack boilerplate: this text is what `dnf info jsom` shows a user.
set(CPACK_PACKAGE_DESCRIPTION
    "JSOM is a C++17 JSON library with RFC 6901 JSON Pointer navigation. The RFC 8259 "
    "grammar is enforced while scanning, numbers are kept as text until a value is asked "
    "for so the original formatting survives a round trip, and path lookups read the "
    "document directly with no cache — so a const document can be navigated from several "
    "threads at once. The packages ship the shared library (libjsom.so.3); the jsom(1) "
    "command-line processor is a separate package.")

set(CPACK_COMPONENTS_ALL cli runtime dev)
set(CPACK_COMPONENT_CLI_DISPLAY_NAME "Command-line processor")
set(CPACK_COMPONENT_CLI_DESCRIPTION
    "The jsom(1) command-line JSON processor: format, validate and JSON Pointer operations.")
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime library")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION
    "The JSOM shared library (libjsom.so.3) with its soname link.")
set(CPACK_COMPONENT_DEV_DISPLAY_NAME "Development files")
set(CPACK_COMPONENT_DEV_DESCRIPTION
    "Headers, the generated version.hpp, CMake package configs, pkg-config metadata, the linker symlink and the jsom(3) man page.")
set(CPACK_COMPONENT_DEV_DEPENDS runtime)

# One package per component on each side.
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

# The component description is what each package shows as its own Description, so
# give both generators explicit single-line ones (a newline inside a description is
# what turned one into '; and the jsom(3) man page' in an earlier build).
set(CPACK_RPM_CLI_PACKAGE_SUMMARY "JSOM command-line JSON processor")
set(CPACK_RPM_CLI_PACKAGE_DESCRIPTION
    "The jsom(1) command-line processor: format, validate and navigate JSON with RFC 6901 JSON Pointer support.")
set(CPACK_RPM_RUNTIME_PACKAGE_SUMMARY "JSOM shared library")
set(CPACK_RPM_RUNTIME_PACKAGE_DESCRIPTION
    "The JSOM shared library libjsom.so.3 with its soname link. Install jsom-devel to build against it.")
set(CPACK_RPM_DEV_PACKAGE_SUMMARY "JSOM development files")
set(CPACK_RPM_DEV_PACKAGE_DESCRIPTION
    "Headers, the generated version.hpp, CMake package configs, pkg-config metadata, the linker symlink and the jsom(3) man page for JSOM.")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE ${JSOM_PACKAGE_RELEASE})
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)   # Depends: from dpkg-shlibdeps, not by hand
# The CLI links libjsom.so.3 — a library this same project produces, so it is in
# neither the system package database nor any search path dpkg-shlibdeps knows.
# Measured failure without the directory below:
#   dpkg-shlibdeps: error: cannot find library libjsom.so.3 needed by ./usr/bin/jsom
#   dpkg-shlibdeps: error: cannot continue due to the error above
# CPack passes this list to dpkg-shlibdeps -l, so the private library resolves from
# the build tree; --ignore-missing-info (which CPack also passes) then stops the
# absence of a system shlibs entry for it from being fatal. The CLI's own dependency
# on the runtime package stays explicit below, because a build-tree path is not a
# package name.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${CMAKE_CURRENT_BINARY_DIR}")
# Generating a shlibs control file in the library package is what lets the soname
# declare libjsom3 as its provider, so a later external package in this pool can
# depend on JSOM the ordinary way instead of by hand.
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY "=")
set(CPACK_DEBIAN_CLI_PACKAGE_NAME "jsom")
set(CPACK_DEBIAN_CLI_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_CLI_PACKAGE_DEPENDS "libjsom3 (>= ${JSOM_PACKAGE_VERSION})")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "libjsom3")
set(CPACK_DEBIAN_DEV_PACKAGE_NAME "libjsom-dev")
set(CPACK_DEBIAN_DEV_PACKAGE_SECTION "libdevel")
set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "libjsom3 (= ${JSOM_PACKAGE_VERSION})")

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE ${JSOM_PACKAGE_RELEASE})
set(CPACK_RPM_PACKAGE_LICENSE "Unlicense")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_CLI_PACKAGE_NAME "jsom")
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "jsom-libs")
set(CPACK_RPM_DEV_PACKAGE_NAME "jsom-devel")
set(CPACK_RPM_CLI_PACKAGE_REQUIRES "jsom-libs = ${JSOM_PACKAGE_VERSION}")
set(CPACK_RPM_DEV_PACKAGE_REQUIRES "jsom-libs = ${JSOM_PACKAGE_VERSION}")
# A package that ships a directory the `filesystem` package owns cannot be
# installed at all ("file /usr/share/man/man1 conflicts with file from package
# filesystem"), and CPack's RPM generator lists those directories by default.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/usr/share/man"
    "/usr/share/man/man1"
    "/usr/share/man/man3"
    "/usr/lib64/cmake"
    "/usr/lib64/pkgconfig"
    "/usr/include/jsom")

include(CPack)
