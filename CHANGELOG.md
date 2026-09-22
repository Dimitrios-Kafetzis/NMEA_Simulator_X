# Changelog

All notable changes to this project are documented in this file. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Entries are generated from
Conventional Commits by Release Please.

## [0.3.0](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v0.2.0...v0.3.0) (2026-09-22)


### Features

* **app:** add the desktop shell with dashboard, console and outputs panels ([#11](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/11)) ([723271b](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/723271bde13c15aa6014c7fc54eb4192fceaadb6))
* **app:** add the map view with cached OpenStreetMap tiles ([#14](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/14)) ([982ea7c](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/982ea7c7a0cf1cd57fb6791f95f9bf75fa9c9a52))
* **app:** add the settings dialog for simulation, sentences and outputs ([#13](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/13)) ([8feb377](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/8feb377b5fc67b1e21ce2a132b4f6320b3ac184a))

## [0.2.0](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v0.1.0...v0.2.0) (2026-09-22)


### Features

* **core:** add delta simulation source, apparent wind and sentence scheduler ([#10](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/10)) ([684b4de](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/684b4deacdfa902ec11e9fcb299ef911f08c0a62))
* **core:** add vessel state model and NMEA 0183 sentence encoders ([#5](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/5)) ([1844500](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/1844500aa91fd66b96eb8beb893e2d29ba6901df))
* **io:** add JSON profiles, the simulation runner and nmeasim run ([#8](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/8)) ([79c11aa](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/79c11aa4ed68215d63a1dcb60e322a5251999469))
* **io:** add TCP, UDP, WebSocket, serial and file transports ([#7](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/7)) ([36bd290](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/36bd29013cc2774842f30ec82fea72e63e01b098))

## 0.1.0 (2026-09-22)


### Features

* **core:** add NMEA 0183 checksum helpers and WGS84 geodesic solutions ([bc7f268](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/bc7f2680144378a956ea39d8207a97c72701c90f))
* **io:** enumerate serial ports and add CLI and desktop application skeletons ([e55472e](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/e55472e2cd7d7a42a3e22a1e842eae698d970f87))


### Documentation

* add documentation site, ADRs 0001-0008 and community files ([a150ba8](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/a150ba89a92e44d7ac976eacb63773583cf5d18c))

## [Unreleased]

### Added

- Project scaffold: CMake build with presets, vcpkg manifest, Qt 6 application and CLI
  targets, unit tests, continuous integration on Linux, Windows and macOS, and the
  documentation site.
- `nmeasim ports` lists the serial ports on the host.
- NMEA 0183 checksum computation and verification.
- WGS84 geodesic direct and inverse solutions.
