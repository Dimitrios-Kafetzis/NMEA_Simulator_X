# SPDX-License-Identifier: GPL-3.0-only
# Script run with `cmake -P` on every build by the custom target nmeasim_version_description
# (src/core/CMakeLists.txt). It writes OUTPUT from INPUT with NMEASIM_VERSION_DESCRIPTION set
# to VERSION for a build of the tag v<VERSION> or of a tree without git metadata, and to
# "VERSION (git describe)" otherwise. configure_file() leaves OUTPUT untouched when the text
# is unchanged, so only a new commit or a change in the dirty state triggers a recompile.
#
# Expects these -D variables:
#   SOURCE_DIR  Directory in which `git describe` runs, the source tree.
#   VERSION     The project version, for example 1.1.0.
#   INPUT       The template, src/core/src/version.cpp.in.
#   OUTPUT      The file to write, version.cpp in the build tree.

set(NMEASIM_VERSION_DESCRIPTION "${VERSION}")

find_package(Git QUIET)
if(GIT_FOUND)
    # Only release tags (v followed by a digit) count; --always still yields the abbreviated
    # commit when no tag is reachable, as in a shallow clone. A failure, for example outside a
    # git work tree, leaves the plain version.
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*" --dirty --always
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE result)
    if(result EQUAL 0 AND NOT describe STREQUAL "" AND NOT describe STREQUAL "v${VERSION}")
        set(NMEASIM_VERSION_DESCRIPTION "${VERSION} (${describe})")
    endif()
endif()

configure_file("${INPUT}" "${OUTPUT}" @ONLY)
