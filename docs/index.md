# NMEA Simulator X

NMEA Simulator X is a free, open-source **NMEA 0183 and Signal K data stream simulator** for
Windows, macOS and Linux. It behaves like a vessel under way and streams navigation,
environment, propulsion and AIS data over serial, TCP, UDP and WebSocket connections, so that
chart plotters, instrument displays, gateways and marine software can be developed and tested
on a desk.

![A running simulation in the night bridge theme: compass, wind dial, instruments, map and console](assets/screenshots/main-window.png)

**Get started:** [install it](how-to/install.md) on Windows, macOS or Linux, then follow
[your first simulated voyage](tutorials/first-voyage.md).

## How this documentation is organised

The documentation follows the [Diátaxis](https://diataxis.fr/) framework, so each section
answers a different kind of question.

| Section | Answers | Start here if you |
| --- | --- | --- |
| [Tutorials](tutorials/index.md) | *How do I get started?* | Have never used the simulator before |
| [How-to guides](how-to/index.md) | *How do I achieve X?* | Need to install, configure or connect something |
| [Reference](reference/index.md) | *What exactly does this do?* | Need sentence formats, settings and CLI flags |
| [Explanation](explanation/index.md) | *Why is it like this?* | Want to understand the design and requirements |
| [Decisions](adr/index.md) | *Why was this chosen?* | Are contributing and need the reasoning behind the architecture |
| [Development](development/index.md) | *How do I contribute?* | Want to build, test, release or extend the project |

## Key facts

- **Language and toolkit:** C++20 with Qt 6 Widgets. No web technologies.
- **Platforms:** Windows 10 1809 and later (x64), macOS 13.3 and later (Apple Silicon and
  Intel), Linux x86_64 and aarch64 as an AppImage (glibc 2.39 or later) or as a Flatpak on any
  distribution.
- **License:** GPL-3.0. Qt is linked dynamically under the LGPL v3.
- **Distribution:** unsigned, free downloads from GitHub Releases with SHA-256 checksums,
  plus manifests for winget, Scoop, Homebrew and Flathub.
- **API:** the `core` and `io` libraries are documented in the [C++ API reference](reference/api.md).
- **Headless mode:** the `nmeasim` command-line tool runs the same engine without a display.
