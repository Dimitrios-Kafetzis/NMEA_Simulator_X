# Changelog

All notable changes to this project are documented in this file. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Entries are generated from
Conventional Commits by Release Please.

## [1.1.1](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/compare/v1.1.0...v1.1.1) (2026-09-24)


### Bug Fixes

* **app:** instrument tiles follow the rudder limit and round coordinates ([#81](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/81)) ([b0c4728](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/b0c47281482e75e461757202481186010eeed8a9))
* **app:** main window profile loading, menu order, saving and keys ([#75](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/75)) ([3e42787](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/3e42787a794be5cf74b06b6aef15740723bfbb40))
* **app:** resolve the system colour scheme and skip unchanged themes ([#91](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/91)) ([c012496](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/c0124965f6e7e7b26123d1dc89cc45452ed55e9b))
* **app:** settings dialog validation and full random seed range ([#88](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/88)) ([1d39b68](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/1d39b681467a69a4bf7d13538d2248660eb64bb7))
* **app:** start file dialogs next to paths relative to the profile ([#94](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/94)) ([266e21d](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/266e21dcfc9f747dd49659a6da30567578f933bc))
* **app:** stop re-requesting failed map tiles ([#65](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/65)) ([e229669](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/e22966975f669539b0a22206a7e939b35cc9f4fd))
* **app:** wrap map longitudes and credit the configured tile server ([#69](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/69)) ([f627632](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/f627632c6ba290e5ff1f1b42cf0fa0c3c8b162b9))
* **cli:** apply output options to profile outputs and keep profile loop settings ([#82](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/82)) ([338a650](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/338a6508909dd1668b93edccf0fa8b2b23c70821))
* **cli:** exit with code 2 on every argument error and parse values strictly ([#77](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/77)) ([b729067](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/b729067daba5a2b2275b2a473af35ed423c4cc33))
* **core:** advance the date when a time-only sentence crosses midnight ([#64](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/64)) ([ce7dcbd](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/ce7dcbd4d2af533d2e0b3df36ae057074d0957f2))
* **core:** check log TAG checksums and intervals, fix time steps back ([#83](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/83)) ([b24a4c8](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/b24a4c8019c1e5ccadce0962bac43ab864ed1e0f))
* **core:** clamp position decimals and enforce one sentence-length limit ([#73](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/73)) ([ba8e2b5](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/ba8e2b5d9822547bfe4e5bbcd35ec427f06a7546))
* **core:** end empty replays, loop one-point tracks, align loop timing ([#66](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/66)) ([84d1b93](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/84d1b93ebdff1552b921b67055c38d5c2d1ec382))
* **core:** exclude deviation from the magnetic heading in HDM and VHW ([#59](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/59)) ([eefa8fa](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/eefa8fa63fa622c16d86ae71856404235dbb31c4))
* **core:** GPX and KML track names, point numbers and file type errors ([#78](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/78)) ([58a954e](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/58a954e9799cf6a1da8bf11f86a6640fa90ed1d9))
* **core:** keep reserved characters out of sentence text fields ([#76](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/76)) ([c1d86eb](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/c1d86eb1958587f339a3636c11241f7a10b9784e))
* **core:** keep the RMB leg origin when the waypoint name is empty ([#60](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/60)) ([9de7d33](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/9de7d332bd5961f4752277f4491792344c4d9ab4))
* **core:** let a cleared override drift back and report the value in use ([#61](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/61)) ([36ee7a7](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/36ee7a74f82173e47869bca394ba8af7ca209e44))
* **core:** read the time of GNS, GST, GBS and GRS in replay ([#67](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/67)) ([be6cd8f](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/be6cd8fa6a6b4f98ea4766c94f4858050d37e387))
* **core:** reject a fractional second after 24:00:00 in ISO 8601 times ([#79](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/79)) ([64e742a](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/64e742a0421e77b6bbb32bf91e10204cc33884db))
* **core:** reject invalid coordinates, dates, ZDA fields and ROT status ([#70](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/70)) ([88041af](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/88041afa7cc7f751b5cd1a626fe126a6f609c60c))
* **core:** sanitise the ViewSync planet name ([#86](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/86)) ([75f3061](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/75f3061b941bd64059c75575b5a32b1f63f80ec8))
* **core:** send the M.1371-5 AIS version, heading and ROT sentinels ([#84](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/84)) ([652b8ea](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/652b8ea010ba193c9fb638e9314878c7cf2ec65c))
* **core:** validate custom sentence ids and talkers ([#72](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/72)) ([9127df4](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/9127df494ca925b6f5c82c9a85b8081104000e61))
* **io:** build TAG blocks with prepend_tag_block ([#74](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/74)) ([393b4c6](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/393b4c675a61d032b7c3cf68247b04d4c9656608))
* **io:** count sentences and state messages separately ([#87](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/87)) ([a51422f](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/a51422f883955e43ece6b7c1bae259410122b25d))
* **io:** current Signal K hello, pause state on stop, failed recordings ([#80](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/80)) ([0cc244b](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/0cc244b3e79ef05f2e627890776c723e231ae9d2))
* **io:** match Signal K paths by whole segment ([#85](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/85)) ([89cddcc](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/89cddcc447d0b94eda866551d636f17557819a1a))
* **io:** one UDP interface lookup, reject IPv6, drop dead serial handler ([#92](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/92)) ([1b1fb27](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/1b1fb2729fa3dfa5f97c6cb7951a65abefb6399f))
* **io:** range-check integer values in profiles ([#53](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/53)) ([1e3e8a4](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/1e3e8a484ec89fd23ed782fad9cd80219e454ded))
* **io:** report refused TCP client connections ([#90](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/90)) ([64f6c37](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/64f6c37066af6e1fa2baf534b6e016140cf0641e))
* **io:** resolve relative profile paths at use and save them as written ([#71](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/71)) ([eaa0517](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/eaa05178499676eba90761d17b4ccd6ed45c9e1e))
* **io:** validate the output and serial settings of profiles ([#62](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/62)) ([7a6889a](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/7a6889a4c9f9bb85c0f022ea0b8a84f10cb57ba2))
* **io:** validate the seed and sentence settings of profiles ([#56](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/56)) ([3e796a8](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/3e796a8af143a0ce7f9e494a0276d5c6b27f6ed3))
* **io:** write output files in binary mode and truncate them once ([#89](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/89)) ([d343260](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/d3432602fcd0a4335c28177c7e556604e7cf17ce))
* **tools:** report malformed AIS, Signal K and coverage input as failures ([#54](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/54)) ([612f4b4](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/612f4b4009c919f8d64e4e69cfbb9c004871a782))


### Documentation

* **app:** document every file of the desktop application and the command-line tool ([#48](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/48)) ([db0ed54](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/db0ed5470982893f32ac256c9aa2e9cab3913218))
* **core:** document every file of the core library ([#46](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/46)) ([dacc2e0](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/dacc2e003e10b6027c90cccf88027167e5e703f9))
* correct reference pages that disagree with the code ([#51](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/51)) ([6a068b3](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/6a068b3228772add56cb14fc36d5aeaf7e283e3e))
* **dev:** define the documentation-comment standard and record ADR 0017 ([#43](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/43)) ([aa3bea6](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/aa3bea63cc5073f549df2b33b8e8cad6746699b3))
* **io:** document every file of the io library ([#47](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/47)) ([7171e5e](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/7171e5e600fde436c56a56d218744acde923ceef))
* **tests:** document every test file, fixture and helper ([#49](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/49)) ([8b76962](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/8b76962f59a62479fa7b085de5b82aede69fa887))
* **tools:** document the Python tools, CMake files and workflows; enforce everywhere ([#50](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/50)) ([6f7aaf7](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/commit/6f7aaf7f7f09b1e2bad535d6ccc418baae8f4922))

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
