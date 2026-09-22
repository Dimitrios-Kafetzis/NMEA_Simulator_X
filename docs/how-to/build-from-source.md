# Build from source

## Prerequisites

| Tool | Version | Notes |
| --- | --- | --- |
| C++ compiler | MSVC 2022 17.8+, GCC 13+, Clang 16+ or Apple Clang 15+ | C++20 including `<format>` |
| CMake | 3.25 or newer | Presets are used throughout |
| Ninja | any recent | Optional on Windows if you use the Visual Studio preset |
| vcpkg | current `master` | Provides GeographicLib, CLI11 and Catch2 through `vcpkg.json` |
| Qt | 6.11.2 | Modules: Core, Gui, Widgets, Network, SerialPort, WebSockets |
| Python | 3.10+ | Only for `clang-format` and the documentation site |

Two environment variables tell the presets where the toolchain lives:

- `VCPKG_ROOT` points at your vcpkg checkout.
- `QT_ROOT_DIR` points at the Qt kit directory that contains `bin/` and `lib/cmake/`,
  for example `~/Qt/6.11.2/gcc_64`, `C:\Qt\6.11.2\msvc2022_64` or `~/Qt/6.11.2/macos`.

## Getting Qt

Either use the [Qt Online Installer](https://www.qt.io/download-open-source) and select the
*Qt Serial Port* and *Qt WebSockets* additional libraries, or use
[aqtinstall](https://github.com/miurahr/aqtinstall), which is what the CI does:

```bash
pip install aqtinstall
aqt install-qt linux desktop 6.11.2 linux_gcc_64 -m qtserialport qtwebsockets -O ~/Qt
# windows: aqt install-qt windows desktop 6.11.2 win64_msvc2022_64 -m qtserialport qtwebsockets -O C:\Qt
# macOS:   aqt install-qt mac desktop 6.11.2 clang_64 -m qtserialport qtwebsockets -O ~/Qt
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
    export QT_ROOT_DIR=~/Qt/6.11.2/gcc_64      # ~/Qt/6.11.2/macos on macOS
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
    set QT_ROOT_DIR=C:\Qt\6.11.2\msvc2022_64
    cmake --preset dev-windows
    cmake --build --preset dev-windows
    ctest --preset dev-windows
    ```

=== "Windows (Visual Studio)"

    ```bat
    set VCPKG_ROOT=C:\vcpkg
    set QT_ROOT_DIR=C:\Qt\6.11.2\msvc2022_64
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
| `release` | Optimised build without tests, used for packaging |
| `ci-linux`, `ci-windows`, `ci-macos` | What CI runs: warnings are errors and, on Linux, sanitizers are on |

Options can be overridden on the command line, for example
`cmake --preset dev -DNMEASIM_BUILD_APP=OFF` to skip the Qt Widgets application.

## Formatting and static analysis

```bash
pip install clang-format
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
