# Packaging

Recipes that turn a release build into downloadable packages. Everything here is free of
code-signing costs; see [ADR 0005](../docs/adr/0005-unsigned-distribution.md). The packages
are produced by `cpack` from the CMake install target (`cmake/Packaging.cmake`); the
[build guide](../docs/how-to/build-from-source.md#installing-and-packaging) shows how to run it
locally.

| Path | Purpose |
| --- | --- |
| `icons/` | Application icon: `nmeasimulatorx.svg` is the source, the PNG, ICO and ICNS files are rendered from it by `tools/render_icons.py` |
| `cpack_project_config.cmake` | Per-generator package names (the Windows ZIP gets the `-portable` suffix) |
| `linux/*.desktop`, `linux/*.metainfo.xml.in` | Desktop entry and AppStream metadata; the release list is generated from `CHANGELOG.md` |
| `linux/appimage.cmake` | CPack External generator script that runs `linuxdeploy` on the staged install tree |
| `linux/AppRun` | AppImage entry point: starts the desktop application, or `nmeasim` when called through a link of that name or with `nmeasim` as first argument |

| Platform | Package | Tooling |
| --- | --- | --- |
| Windows | `NMEASimulatorX-<version>-win64.exe` installer and `NMEASimulatorX-<version>-win64-portable.zip` | `windeployqt`, CPack NSIS and ZIP |
| macOS | `NMEASimulatorX-<version>-macos-<arch>.dmg`, ad-hoc signed | `macdeployqt`, `codesign --sign -`, CPack DragNDrop |
| Linux | `NMEASimulatorX-<version>-<arch>.AppImage` | `linuxdeploy` with the Qt plugin, CPack External |
