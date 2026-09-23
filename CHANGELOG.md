# Changelog

All notable changes to this project are documented in this file. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Entries are generated from
Conventional Commits by Release Please.

## [1.1.0](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v1.0.1...v1.1.0) (2026-09-23)


### Features

* **app:** add a compass rose and a wind dial to the dashboard ([#40](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/40)) ([eb2a51b](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/eb2a51bb3c5132fb00026f2f04025a60989fb1cb))
* **app:** add night bridge and daylight themes with icons, status lights and a coloured console ([#38](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/38)) ([137d2fb](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/137d2fb445e6fcabb1d7da0b74f2045860dc6388))
* **app:** zoom the map smoothly and add scale bar, zoom buttons and position readout ([#41](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/41)) ([331b0d1](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/331b0d133e78f23105d43a25bcb97093179953ef))


### Bug Fixes

* **app:** zoom the map with touchpads and stop drawing a line when the vessel is moved ([#37](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/37)) ([5354d8c](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/5354d8c7f7bde270af62c6fcd9a68a1602de3212))


### Documentation

* show the redesigned interface in both themes ([#42](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/42)) ([a9b057b](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/a9b057b8c2e830e153a764d2cee6f3e63a36037e))

## [1.0.1](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v1.0.0...v1.0.1) (2026-09-23)


### Bug Fixes

* **packaging:** run the AppImage natively on Wayland ([#35](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/35)) ([804d17e](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/804d17e9c270ea44cfa9fd35d8444c7cd027e01c))

## [1.0.0](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v0.5.0...v1.0.0) (2026-09-23)


### Features

* **build:** add install rules, CPack packaging and a git describe version ([#26](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/26)) ([7bef30b](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/7bef30b42620d64bc90d52921bd74679c69642f7))


### Bug Fixes

* **core:** keep sentence periods on schedule ([#32](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/32)) ([8a0c0e8](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/8a0c0e8bd474ba35ca30de88afd2c3281171b4bb))


### Documentation

* document the 1.0 release ([#33](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/33)) ([f1514c7](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/f1514c785ace1f9d48cbf124f2089a32e2acd726))
* publish a Doxygen API reference for core and io ([#28](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/28)) ([e3a9c30](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/e3a9c307273c49c05fab769ac5422e10460762aa))

## [0.5.0](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v0.4.0...v0.5.0) (2026-09-23)


### Features

* **app:** set a destination on the map, edit engines, AIS, custom sentences and output encodings ([#25](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/25)) ([e0ddb82](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/e0ddb82264ffb54d5a41372d2d63d0f66aedda66))
* **core:** add AIS own-vessel VDO and VDM messages ([#22](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/22)) ([37e4942](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/37e494223a5b8c93dcaf23d3fca6b99f10041300))
* **core:** add autopilot, cross-track and propulsion sentences ([#20](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/20)) ([c5d4711](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/c5d4711356014004afe13110b069dbae3b384b0d))
* **core:** add Signal K, ViewSync, TAG block and custom sentence encoders ([#23](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/23)) ([b475c84](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/b475c844f9b22f2135105bf1363e96b3718831e3))
* **io:** add per-output encodings, TAG blocks, custom sentences, destination and AIS to the profile and CLI ([#24](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/24)) ([e4999fa](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/e4999fa3cb0d148902c8e02922dcf0ca171d2eca))

## [0.4.0](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v0.3.0...v0.4.0) (2026-09-23)


### Features

* **app:** open tracks and logs, add transport controls and draw the route ([#19](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/19)) ([51fb82d](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/51fb82d4e3ebed34944582c0121c89cf4b5d4fd2))
* **core:** add GPX and KML track parsing and the track-following source ([#15](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/15)) ([12f5404](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/12f5404b15c841bd76f123f22380dd61c14e476a))
* **core:** add the NMEA decoder, log parsing and the replay source ([#17](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/17)) ([6e00bbe](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/6e00bbe7c96e14a22d95eefc9f5aa53176796a3d))
* **io:** add track and replay profile modes, log recording and the CLI flags ([#18](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/18)) ([dcea1be](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/dcea1be180f237e5f330667433b38a41a91020c5))

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
