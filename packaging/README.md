# Packaging

This directory will hold the recipes that turn a release build into downloadable packages.
They are produced by the release workflow in milestone M5 and are all free of code-signing
costs; see [ADR 0005](../docs/adr/0005-unsigned-distribution.md).

| Platform | Artifact | Tooling |
| --- | --- | --- |
| Windows | `NMEASimulatorX-<version>-win64.exe` installer and `-portable.zip` | `windeployqt`, NSIS |
| Windows | winget and Scoop manifests | `manifests/winget`, `manifests/scoop` |
| macOS | `NMEASimulatorX-<version>-macos.dmg` (universal, ad-hoc signed) | `macdeployqt`, `codesign --sign -` |
| macOS | Homebrew cask | `manifests/homebrew` |
| Linux | `NMEASimulatorX-<version>-x86_64.AppImage` | `linuxdeploy` with the Qt plugin |
| Linux | `.deb` and `.rpm` | CPack |
| Linux | Flatpak manifest for Flathub | `flatpak/io.github.dimitrios_kafetzis.NMEASimulatorX.yml` |
