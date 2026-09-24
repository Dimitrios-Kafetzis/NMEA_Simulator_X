# SPDX-License-Identifier: GPL-3.0-only
# Top-level build of NMEA Simulator X. Configure it through a preset from CMakePresets.json, for
# example `cmake --workflow --preset dev`; the presets take the vcpkg toolchain from VCPKG_ROOT
# and find Qt through QT_ROOT_DIR (ADR 0003).
#
# The file sets the project version and C++20, declares the options below, adds src/core
# always, src/io when the command-line tool or the desktop application is built, src/cli,
# src/app and tests according to their options, and finally cmake/Packaging.cmake for
# `cmake --install` and `cpack`.
#
# Options: NMEASIM_BUILD_APP, NMEASIM_BUILD_CLI, NMEASIM_BUILD_TESTS (ON by default only when
# this is the top-level project), NMEASIM_WARNINGS_AS_ERRORS, NMEASIM_ENABLE_SANITIZERS and
# NMEASIM_ENABLE_COVERAGE. Variables set for the subdirectories: NMEASIM_VERSION,
# NMEASIM_APP_ID and NMEASIM_INSTALL_BINDIR.

cmake_minimum_required(VERSION 3.25)

# The single source of the version. Release Please rewrites the next line, which carries its
# marker comment, when a release pull request is merged (release-please-config.json); the
# release workflow and the documentation hook read the version from this line as well.
set(NMEASIM_VERSION "1.1.0") # x-release-please-version

project(NMEASimulatorX
    VERSION ${NMEASIM_VERSION}
    DESCRIPTION "NMEA 0183 and Signal K data stream simulator"
    HOMEPAGE_URL "https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# compile_commands.json for clangd, clang-tidy and other tools that need the compiler flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# A single-configuration build without a build type gets no optimisation flags at all, so it
# defaults to Release. Multi-configuration generators such as Visual Studio choose the
# configuration at build time instead.
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
# CMAKE_INSTALL_BINDIR, CMAKE_INSTALL_DATADIR and CMAKE_INSTALL_DOCDIR for the install rules.
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

# nmeasim::io needs Qt; a build of the core library and its tests alone needs no Qt at all.
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
