# Changelog

All notable changes to this project are documented in this file. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Entries are generated from
Conventional Commits by Release Please.

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
