# Build from source

## Prerequisites

| Tool | Version | Notes |
| --- | --- | --- |
| C++ compiler | MSVC 2022 17.8+, GCC 13+, Clang 16+ or Apple Clang 15+ | C++20 including `<format>` |
| CMake | 3.25 or newer | Presets are used throughout |
| Ninja | any recent | Optional on Windows if you use the Visual Studio preset |
| vcpkg | current `master` | Provides GeographicLib, pugixml, CLI11 and Catch2 through `vcpkg.json` |
| Qt | 6.10.3 | Modules: Core, Gui, Widgets, Network, SerialPort, WebSockets |
| Python | 3.10+ | Only for `clang-format` and the documentation site |

Two environment variables tell the presets where the toolchain lives:

- `VCPKG_ROOT` points at your vcpkg checkout.
- `QT_ROOT_DIR` points at the Qt kit directory that contains `bin/` and `lib/cmake/`,
  for example `~/Qt/6.10.3/gcc_64`, `C:\Qt\6.10.3\msvc2022_64` or `~/Qt/6.10.3/macos`.

## Getting Qt

Either use the [Qt Online Installer](https://www.qt.io/download-open-source) and select the
*Qt Serial Port* and *Qt WebSockets* additional libraries, or use
[aqtinstall](https://github.com/miurahr/aqtinstall), which is what the CI does:

```bash
pip install aqtinstall
aqt install-qt linux desktop 6.10.3 linux_gcc_64 -m qtserialport qtwebsockets -O ~/Qt
# windows: aqt install-qt windows desktop 6.10.3 win64_msvc2022_64 -m qtserialport qtwebsockets -O C:\Qt
# macOS:   aqt install-qt mac desktop 6.10.3 clang_64 -m qtserialport qtwebsockets -O ~/Qt
```

## Getting vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh      # bootstrap-vcpkg.bat on Windows
export VCPKG_ROOT=~/vcpkg
```

The first configure builds the C++ dependencies from source, which takes a few minutes.
Later configures reuse vcpkg's binary cache.

## Building

=== "Linux and macOS"

    ```bash
    export VCPKG_ROOT=~/vcpkg
    export QT_ROOT_DIR=~/Qt/6.10.3/gcc_64      # ~/Qt/6.10.3/macos on macOS
    cmake --workflow --preset dev               # configure + build + test
    ```

    On Debian or Ubuntu, Qt needs the OpenGL and XKB development packages:

    ```bash
    sudo apt install build-essential ninja-build libgl1-mesa-dev libxkbcommon-dev
    ```

=== "Windows (Ninja)"

    Open a *x64 Native Tools Command Prompt for VS 2022* or run `vcvars64.bat`, then:

    ```bat
    set VCPKG_ROOT=C:\vcpkg
    set QT_ROOT_DIR=C:\Qt\6.10.3\msvc2022_64
    cmake --preset dev-windows
    cmake --build --preset dev-windows
    ctest --preset dev-windows
    ```

=== "Windows (Visual Studio)"

    ```bat
    set VCPKG_ROOT=C:\vcpkg
    set QT_ROOT_DIR=C:\Qt\6.10.3\msvc2022_64
    cmake --preset windows-vs
    start build\windows-vs\NMEASimulatorX.sln
    ```

Build output lands in `build/<preset>/`. The desktop application is
`build/<preset>/src/app/NMEASimulatorX` and the command-line tool is
`build/<preset>/src/cli/nmeasim`.

## Presets

| Preset | Purpose |
| --- | --- |
| `dev` | Debug build with tests, for daily work on Linux and macOS |
| `dev-windows` | The same with MSVC and Ninja |
| `windows-vs` | Generates a Visual Studio 2022 solution |
| `release` | Optimised build without tests, used for packaging on Linux and macOS |
| `release-windows` | The same with MSVC and statically linked vcpkg dependencies |
| `ci-linux`, `ci-windows`, `ci-macos` | What CI runs: warnings are errors and, on Linux, sanitizers and coverage instrumentation are on |

Options can be overridden on the command line, for example
`cmake --preset dev -DNMEASIM_BUILD_APP=OFF` to skip the Qt Widgets application.

## Installing and packaging

The install target lays out a runnable tree; `cpack` turns it into the release packages.

```bash
cmake --preset release                      # release-windows on Windows
cmake --build --preset release
cmake --install build/release --prefix ~/nmeasim-install
cpack --preset release                      # packages land in build/release/packages/
```

| Platform | Install layout | `cpack` output |
| --- | --- | --- |
| Windows | Executables, Qt libraries (copied by `windeployqt`) and the C++ runtime in one folder | NSIS installer `NMEASimulatorX-<version>-win64.exe` and portable `NMEASimulatorX-<version>-win64-portable.zip`; needs [NSIS](https://nsis.sourceforge.io/) in `PATH` |
| macOS | `NMEASimulatorX.app` with the Qt frameworks (copied by `macdeployqt`) and `nmeasim` in `Contents/MacOS`, signed ad hoc | Disk image `NMEASimulatorX-<version>-macos-<arch>.dmg` |
| Linux | `bin/`, a desktop file, AppStream metadata and icons under `share/`; Qt is not copied | AppImage `NMEASimulatorX-<version>-<arch>.AppImage`; needs `linuxdeploy` and `linuxdeploy-plugin-qt` in `PATH` and `QMAKE` set to Qt's `qmake` |

`-DNMEASIM_PACKAGE_VERSION=1.0.0-rc.1` changes the version in the file names without
changing the version compiled into the programs. Every package is accompanied by a
`.sha256` file.

The two screenshots in `docs/assets/screenshots/`, `main-window.png` (night theme) and
`main-window-day.png` (daylight theme), which the AppStream metadata also uses, are taken by
a hidden test:

```bash
NMEASIM_SCREENSHOT_DIR=docs/assets/screenshots build/dev/tests/nmeasim_app_tests "[.screenshot]"
```

## Measuring coverage

The `ci-linux` preset sets `NMEASIM_ENABLE_COVERAGE=ON`, which compiles first-party code with
`--coverage`. After running the tests (and, if you like, the command-line tool):

```bash
pip install gcovr
gcovr --root . --filter src/ --json-summary coverage.json --html-nested coverage/index.html
python3 tools/coverage_summary.py coverage.json
```

The summary lists line and function coverage per library and fails when `core` is below
the 90 percent line coverage that [ADR 0008](../adr/0008-testing-strategy.md) sets. CI
prints the same table in the summary of the *Linux (GCC)* job of every pull request and keeps
the HTML report as the `coverage-report` artifact.

## Formatting and static analysis

```bash
pip install clang-format==23.1.1   # the version CI uses; others format differently
git ls-files '*.cpp' '*.hpp' | xargs clang-format -i
```

`clang-tidy` reads `.clang-tidy` and `build/<preset>/compile_commands.json`:

```bash
run-clang-tidy -p build/dev src tests
```

## Building the documentation

```bash
pip install -r docs/requirements.txt
mkdocs serve          # live preview at http://127.0.0.1:8000
mkdocs build --strict # what CI checks
```

With [Doxygen](https://www.doxygen.nl/) 1.18 or later in `PATH`, the build also generates
the [C++ reference](../reference/api.md) from the comments in `src/` and `tests/`; without it
the reference is replaced by placeholder pages. CI sets `NMEASIM_REQUIRE_DOXYGEN=1`, which
turns a missing Doxygen into a failed build. Any Doxygen warning, such as an undocumented
function or parameter, fails the build; see
[Documentation comments](../development/coding-standards.md#documentation-comments) for the
rules and the other checks.
