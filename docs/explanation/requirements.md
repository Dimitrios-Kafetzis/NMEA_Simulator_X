# Requirements and feature parity

## Where the requirements come from

The functional scope of version 1.0 is **feature parity with the closed-source NMEASimulator
1.6.1 by panaaj**, reconstructed from its public README, its release notes from 1.0.0 to
1.6.1 and the requests and bug reports in its issue tracker. Its users' unresolved requests
become improvements in NMEA Simulator X. No code from that application is used.

## Simulation modes

| Requirement | Parity | Improvement over the reference |
| --- | --- | --- |
| Delta mode: seed values plus periodic change; keyboard arrows for speed and heading | Yes | Every value individually overridable |
| Steering mode with rudder angle | Yes | Rate of turn derived from rudder, exposed as ROT |
| Follow GPX track, GPX route and KML track; concatenate segments | Yes | Interpolate along legs at a set speed when timestamps are missing; jump to a point |
| Use `<time>`, `<course>` and `<speed>` from track points when present | Yes | |
| Record output to a log file and replay it with pause and step | Yes | Seek to a position; replay plain third-party NMEA logs as well |
| Autostart the stream on launch | Yes | |

## Data and sentences

| Requirement | Parity | Improvement over the reference |
| --- | --- | --- |
| GNSS: RMC, GGA, GLL, GSA, GSV, VTG, ZDA; fix on/off; altitude | Yes | Configurable satellite count and HDOP |
| Heading and speed: HDG, HDM, HDT, VHW, VBW | Yes | ROT sentence |
| Depth and water: DPT, DBT, MTW | Yes | |
| Wind: MWV apparent, MWD true | Yes | |
| Autopilot: APB, RMB with a destination set on the map | Yes | XTE sentence; sentences kept within 82 characters |
| Rudder: RSA | Yes | |
| AIS own vessel: VDO and VDM | Yes | Can be disabled independently |
| Propulsion: engine RPM and temperature | Yes | |
| Configurable talker ID | Yes | Per sentence as well as global |
| Manually entered additional sentences | Yes | Checksum recalculated automatically |
| IEC 61162-450 TAG block prefix | Yes | |
| Enable or disable individual sentences | Partial in reference | Full per-sentence control and per-sentence rate |
| Signal K delta output with hello message, context `vessels` or `aircraft` | Yes | |
| ViewSync UDP for Google Earth and Liquid Galaxy | Yes | |

## Transports

| Requirement | Parity | Improvement over the reference |
| --- | --- | --- |
| Serial port with detected and manually entered paths, custom baud rate | Yes | |
| TCP server | Yes | TCP client |
| UDP unicast, broadcast with interface selection, multicast | Yes | Explicit broadcast address override |
| WebSocket server | Yes | |
| Output interval down to 50 ms | Yes | Per-sentence rates |
| Several simultaneous outputs | No | Yes, each with its own filter |

## Platform and operations

| Requirement | Parity | Improvement over the reference |
| --- | --- | --- |
| Windows, macOS and Linux desktop builds | Yes (macOS lapsed in reference) | Built, checked and published by the release workflow for every release: Windows x64 installer and portable ZIP, macOS arm64 and x86_64 disk images, Linux x86_64 and aarch64 AppImages and a Flatpak |
| Installation without paid signing | Signed installers in reference | Ad-hoc signed macOS bundles, SHA-256 checksums, documented first-launch steps; manifests for winget, Scoop, a Homebrew tap and Flathub prepared for every release, submission pending |
| Settings survive restart | Unreliable in reference | Versioned profiles with migrations |
| Works without internet (map tiles) | Fragile in reference | Offline tile cache and graceful fallback |
| Command-line control | No | Headless CLI, shipped in every package; a remote control endpoint is not implemented |
| Open source | No | GPL-3.0 |

## Out of scope for 1.0

- NMEA 2000 over CAN, including fast packets. The encoder layer is designed to host it later.
- AIS targets other than own vessel, and the TTM sentence.
- Scenario scripting.
