# NMEA Simulator X

[![CI](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/actions/workflows/ci.yml/badge.svg)](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/actions/workflows/ci.yml)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://isocpp.org/)
[![Qt 6](https://img.shields.io/badge/Qt-6-41CD52.svg)](https://www.qt.io/)

**NMEA Simulator X** is a free, open-source NMEA 0183 and Signal K data stream simulator for
Windows, macOS and Linux. It mimics a vessel under way, including position, heading, speed,
depth, wind, engine and AIS data, and streams the result over serial, TCP, UDP and WebSocket
so that chart plotters, instrument displays and marine software can be developed and tested
without leaving the desk.

> **Status: pre-alpha.** The project is being built milestone by milestone; see the
> [roadmap](docs/development/roadmap.md). Nothing is ready for end users yet.

## Planned capabilities

- **Three simulation modes.** Seed values with periodic deltas and manual overrides,
  following a GPX or KML track or route, and replaying a recorded log.
- **NMEA 0183 output.** GNSS, heading, speed, depth, wind, autopilot, AIS and propulsion
  sentences, each individually switchable, with configurable talker IDs and strict
  82-character compliance. Optional IEC 61162-450 TAG blocks.
- **Signal K output.** Delta messages over TCP and WebSocket with the standard hello handshake.
- **ViewSync output.** UDP packets for Google Earth and Liquid Galaxy installations.
- **Transports.** Serial port, TCP server and client, UDP unicast, broadcast and multicast
  with interface selection, WebSocket server and file logging. Several outputs at once.
- **Headless operation.** The same engine runs as a command-line tool for scripts and CI.
- **Settings that persist.** Named profiles survive restarts and upgrades.

## Installation

Installers and portable packages for every platform will be published on the
[releases page](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/releases) once the first
milestone that has a usable interface is reached. The builds are unsigned, so each operating
system shows a one-time warning on first launch; the
[installation guide](docs/how-to/install.md) explains what to click.

## Building from source

Prerequisites: a C++20 compiler, CMake 3.25 or newer, Ninja, [vcpkg](https://vcpkg.io) and
Qt 6.10 with the Serial Port and WebSockets modules.

```bash
export VCPKG_ROOT=/path/to/vcpkg
export QT_ROOT_DIR=/path/to/Qt/6.10.3/gcc_64   # or msvc2022_64, macos
cmake --workflow --preset dev
```

The full walkthrough for each platform is in
[docs/how-to/build-from-source.md](docs/how-to/build-from-source.md).

## Documentation

The documentation lives in [`docs/`](docs/) and is organised as tutorials, how-to guides,
reference material and explanations. Architecture decisions are recorded in
[`docs/adr/`](docs/adr/). The site is published at
<https://dimitrios-kafetzis.github.io/NMEA_Simulator_X/> once the repository is public.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) for the workflow,
coding standards and the definition of done, and the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Acknowledgements

The feature set is modelled on the closed-source
[NMEASimulator](https://github.com/panaaj/nmeasimulator) by panaaj, which served the marine
software community for years. NMEA Simulator X is an independent implementation and shares no
code with it.

## License

NMEA Simulator X is licensed under the [GNU General Public License v3.0](LICENSE).
Qt is used under the LGPL v3 and is linked dynamically.
