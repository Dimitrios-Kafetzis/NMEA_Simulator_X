cmake_minimum_required(VERSION 3.25)

set(NMEASIM_VERSION "0.5.0") # x-release-please-version

project(NMEASimulatorX
    VERSION ${NMEASIM_VERSION}
    DESCRIPTION "NMEA 0183 and Signal K data stream simulator"
    HOMEPAGE_URL "https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

option(NMEASIM_BUILD_APP "Build the Qt Widgets desktop application" ON)
option(NMEASIM_BUILD_CLI "Build the command-line simulator" ON)
option(NMEASIM_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(NMEASIM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(NMEASIM_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(NMEASIM_ENABLE_COVERAGE "Instrument first-party code for gcov coverage" OFF)

include(ProjectWarnings)
include(ProjectSanitizers)
include(ProjectCoverage)
include(GNUInstallDirs)

# Reverse-DNS identifier shared by the desktop file, the AppStream metadata, the Flatpak and the
# macOS bundle.
set(NMEASIM_APP_ID "io.github.dimitrios_kafetzis.NMEASimulatorX")
if(WIN32)
    # Windows packages keep the executables, the Qt libraries and the runtime in one folder.
    set(NMEASIM_INSTALL_BINDIR ".")
else()
    set(NMEASIM_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
endif()

add_subdirectory(src/core)

if(NMEASIM_BUILD_CLI OR NMEASIM_BUILD_APP)
    add_subdirectory(src/io)
endif()
if(NMEASIM_BUILD_CLI)
    add_subdirectory(src/cli)
endif()
if(NMEASIM_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(NMEASIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(Packaging)

message(STATUS "${PROJECT_NAME} ${PROJECT_VERSION}: app=${NMEASIM_BUILD_APP} cli=${NMEASIM_BUILD_CLI} "
               "tests=${NMEASIM_BUILD_TESTS} werror=${NMEASIM_WARNINGS_AS_ERRORS} "
               "sanitizers=${NMEASIM_ENABLE_SANITIZERS} coverage=${NMEASIM_ENABLE_COVERAGE}")
