# Roadmap

Each milestone ends with a tagged pre-release and updated documentation. Milestones are
tracked as GitHub milestones with one issue per deliverable.

| Milestone | Deliverables | Target version |
| --- | --- | --- |
| **M0 Foundations** | Repository scaffold, CMake presets, vcpkg manifest, core/io/cli/app targets, unit tests, CI on three platforms, documentation site, ADRs 0001-0008 | 0.1.0 |
| **M1 Headless core** | Vessel model and delta simulation, NMEA 0183 encoder registry with the full GNSS, heading, speed, depth, wind and rudder sentence set, scheduler with per-sentence rates, all transports, profiles, `nmeasim run` | 0.2.0 |
| **M2 Desktop application** | Dashboard, settings dialog, console, map with tile cache, manual overrides, keyboard steering | 0.3.0 |
| **M3 Tracks and logs** | GPX track and route following, KML tracks, interpolation without timestamps, log recording and replay with pause, step and seek, replay of plain third-party NMEA logs | 0.4.0 |
| **M4 Protocols** | Signal K delta and hello, ViewSync, AIS VDO/VDM, APB/RMB/XTE with map destination, IEC 61162-450 TAG blocks, custom sentences, RPM/XDR propulsion | 0.5.0 |
| **M5 Release engineering** | Installers and images for all platforms, ad-hoc macOS signing, package-manager manifests, Doxygen API reference, tutorials, release workflow, arm64 Linux build | 1.0.0 |

## After 1.0

- AIS targets and the TTM sentence.
- Scenario scripting.
- NMEA 2000 over CAN with fast packets.
