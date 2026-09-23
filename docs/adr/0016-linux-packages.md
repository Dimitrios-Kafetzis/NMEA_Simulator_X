# 0016 Linux packages: AppImage and Flatpak; deb and rpm deferred

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

ADR 0005 lists AppImage, deb, rpm and a Flathub submission for Linux. The desktop
application needs Qt 6.10 with Serial Port and WebSockets, and C++20 with `<format>`, which
means GCC 13 and its C++ library. No Debian, Ubuntu LTS or Fedora release ships Qt 6.10, and
the older distributions ship an older C++ library. Which Linux formats does the project
publish, and how old a system do they reach?

## Decision drivers

- One download that runs on current mainstream distributions without installing anything.
- A sandboxed, auto-updating channel for users who prefer a software centre.
- Serial ports, network sockets and files in the home folder must work.
- Both x86_64 and aarch64 (Raspberry Pi 4 and 5, Arm laptops) should be covered.
- No distribution-specific packaging that would have to bundle a private copy of Qt, which
  distribution policies reject and which conflicts with the system Qt.

## Considered options

1. deb and rpm packages that depend on the distribution's Qt.
2. deb and rpm packages that bundle Qt under `/opt`.
3. An AppImage with Qt bundled by linuxdeploy, plus a Flatpak on the KDE runtime.

## Decision outcome

Option 3.

- **AppImage**, x86_64 and aarch64, built on Ubuntu 24.04 with the Qt binaries used in CI
  and bundled by linuxdeploy and its Qt plugin (xcb, Wayland and offscreen platform plugins).
  It needs glibc 2.39 or later: Ubuntu 24.04, Debian 13, Fedora 40 and newer. The command-line
  tool is inside: `./NMEASimulatorX-<version>-<arch>.AppImage nmeasim ...`, or through a link
  named `nmeasim`.
- **Flatpak**, on `org.kde.Platform` 6.10, which provides Qt 6.10 with Serial Port and
  WebSockets on any distribution with Flatpak, including older ones the AppImage does not
  reach. The manifest lives in `packaging/flatpak/` and builds the working tree; the Flathub
  copy points at the release tag. It asks for network, all devices (serial ports) and the
  home folder. The release workflow builds it and attaches an x86_64 bundle; Flathub builds
  both architectures once the application is accepted there.
- **deb and rpm** are not published for 1.0. Option 1 is impossible until distributions ship
  Qt 6.10, and option 2 produces packages that no distribution would accept and that users
  would have to trust like an AppImage anyway. They are listed under *After 1.0* and will be
  reconsidered when Debian and Fedora carry a recent enough Qt.

This amends the Linux line of [ADR 0005](0005-unsigned-distribution.md); its other
decisions stand.

### Consequences

- The installation guide offers Flatpak first, then the AppImage, and names the glibc
  requirement.
- Building the AppImage on an older distribution would need GCC 13 there together with
  shipping its C++ library, which is fragile; the Flatpak covers those systems instead.
- The desktop file, AppStream metadata and icons installed by CMake serve the AppImage, the
  Flatpak and any future distribution package alike.

## More information

- [ADR 0015](0015-release-workflow.md) for the release workflow that builds both.
- [Install on Windows, macOS or Linux](../how-to/install.md)
