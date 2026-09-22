# 0001 Native C++20 with Qt 6 Widgets

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

The simulator must run natively on Windows, macOS and Linux, drive serial ports and network
sockets, render a dashboard with a map, and remain approachable to contributors. The reference
application was built with web technologies packaged as a desktop app, which produced large
binaries and a closed codebase. Which language and UI toolkit should NMEA Simulator X use?

## Decision drivers

- Native look, small footprint and low resource usage.
- One codebase and one language for all three platforms.
- Mature serial, TCP, UDP and WebSocket support.
- No web technologies in the application.
- Zero licensing cost for an open-source project.

## Considered options

1. C++20 with Qt 6 Widgets.
2. C++20 with Qt 6 Quick (QML).
3. C++ with wxWidgets.
4. C++ with Dear ImGui.
5. TypeScript with Electron or Tauri.

## Decision outcome

Option 1, **C++20 with Qt 6 Widgets**. Qt is the only C++ toolkit that provides a native GUI
together with serial port, networking and WebSocket modules that behave identically on all
three platforms. Widgets keeps every line of the application in C++, whereas QML would
introduce a JavaScript-flavoured declarative layer. wxWidgets lacks first-party serial and
WebSocket support, Dear ImGui does not look native, and web stacks were ruled out by the
project owner.

The minimum Qt version is 6.11 and the language standard is C++20 without extensions.

### Consequences

- Qt is linked dynamically under the LGPL v3; packages must bundle the Qt runtime.
- The map view is a custom tile widget on `QGraphicsView`, since QtLocation is QML-only and
  QtWebEngine would reintroduce a web runtime.
- Contributors need a Qt installation; the build guide documents `aqtinstall` for this.
