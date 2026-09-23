# Script mode (cmake -P). Writes OUTPUT from INPUT with NMEASIM_VERSION_DESCRIPTION set to
# VERSION for a build of the tag v<VERSION> or of a tree without git metadata, and to
# "VERSION (git describe)" otherwise. configure_file() leaves OUTPUT untouched when the text
# is unchanged, so only a new commit or a change in the dirty state triggers a recompile.
#
# Inputs: SOURCE_DIR, VERSION, INPUT, OUTPUT.

set(NMEASIM_VERSION_DESCRIPTION "${VERSION}")

find_package(Git QUIET)
if(GIT_FOUND)
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
