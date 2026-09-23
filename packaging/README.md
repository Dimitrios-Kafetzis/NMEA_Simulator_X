# Packaging

Recipes that turn a release build into downloadable packages. Everything here is free of
code-signing costs; see [ADR 0005](../docs/adr/0005-unsigned-distribution.md). The packages
are produced by `cpack` from the CMake install target (`cmake/Packaging.cmake`); the
[build guide](../docs/how-to/build-from-source.md#installing-and-packaging) shows how to run it
locally. The release workflow (`.github/workflows/release.yml`) runs `cpack` on every platform,
checks the packages and attaches them to the GitHub release; see
[ADR 0015](../docs/adr/0015-release-workflow.md) and the
[release process](../docs/development/release-process.md).

| Path | Purpose |
| --- | --- |
| `icons/` | Application icon: `nmeasimulatorx.svg` is the source, the PNG, ICO and ICNS files are rendered from it by `tools/render_icons.py` |
| `cpack_project_config.cmake` | Per-generator package names (the Windows ZIP gets the `-portable` suffix) |
| `linux/*.desktop`, `linux/*.metainfo.xml.in` | Desktop entry and AppStream metadata; the release list is generated from `CHANGELOG.md` |
| `linux/appimage.cmake` | CPack External generator script that runs `linuxdeploy` on the staged install tree |
| `linux/AppRun` | AppImage entry point: starts the desktop application, or `nmeasim` when called through a link of that name or with `nmeasim` as first argument |
| `flatpak/*.yml` | Flatpak manifest on the KDE 6.10 runtime; builds the working tree (instructions in the file) |

| Platform | Package | Tooling |
| --- | --- | --- |
| Windows | `NMEASimulatorX-<version>-win64.exe` installer and `NMEASimulatorX-<version>-win64-portable.zip` | `windeployqt`, CPack NSIS and ZIP |
| macOS | `NMEASimulatorX-<version>-macos-<arch>.dmg`, ad-hoc signed | `macdeployqt`, `codesign --sign -`, CPack DragNDrop |
| Linux | `NMEASimulatorX-<version>-<arch>.AppImage` (x86_64, aarch64) | `linuxdeploy` with the Qt plugin, CPack External |
| Linux | `NMEASimulatorX-<version>-x86_64.flatpak` bundle | `flatpak-builder` with `flatpak/*.yml` |

Linux formats are explained in [ADR 0016](../docs/adr/0016-linux-packages.md).
