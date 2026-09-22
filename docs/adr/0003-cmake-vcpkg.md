# 0003 CMake presets and vcpkg manifest mode

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

The project needs a build that works identically for contributors on three operating systems
and in CI, with third-party C++ libraries that are not always available from system package
managers.

## Decision drivers

- One command to configure, build and test on any platform.
- Reproducible dependency versions.
- Fast CI through binary caching.
- Qt installed separately, since building Qt from source is prohibitively slow.

## Considered options

1. CMake with `FetchContent` for every dependency.
2. CMake with vcpkg in manifest mode.
3. CMake with Conan 2.

## Decision outcome

Option 2. `CMakePresets.json` defines developer, release and CI presets that reference the
vcpkg toolchain through `VCPKG_ROOT` and locate Qt through `QT_ROOT_DIR`. `vcpkg.json` pins
the dependencies (GeographicLib, CLI11, Catch2 as a `tests` feature). vcpkg is preinstalled
on GitHub-hosted runners and supports binary caching, which keeps CI fast. Qt itself is
installed with `aqtinstall`, not vcpkg, because vcpkg would rebuild Qt from source.

`FetchContent` was rejected because it rebuilds every dependency in every build directory and
offers no caching. Conan was rejected as a second package ecosystem to learn for little gain.

### Consequences

- Contributors set two environment variables and run `cmake --workflow --preset dev`.
- Adding a dependency means editing `vcpkg.json` and the relevant `CMakeLists.txt`.
- CI caches `vcpkg-binary-cache/` keyed on the hash of `vcpkg.json`.
