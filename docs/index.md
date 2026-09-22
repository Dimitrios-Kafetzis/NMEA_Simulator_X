# NMEA Simulator X

NMEA Simulator X is a free, open-source **NMEA 0183 and Signal K data stream simulator** for
Windows, macOS and Linux. It behaves like a vessel under way and streams navigation,
environment, propulsion and AIS data over serial, TCP, UDP and WebSocket connections, so that
chart plotters, instrument displays, gateways and marine software can be developed and tested
on a desk.

!!! warning "Pre-alpha"
    The project is under construction. Follow the [roadmap](development/roadmap.md) to see
    which milestone is in progress.

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
- **Platforms:** Windows 10 and later, macOS 13 and later, Linux with glibc 2.35 or later.
- **License:** GPL-3.0. Qt is linked dynamically under the LGPL v3.
- **Distribution:** unsigned, free downloads from GitHub Releases plus package managers.
- **Headless mode:** the `nmeasim` command-line tool runs the same engine without a display.
