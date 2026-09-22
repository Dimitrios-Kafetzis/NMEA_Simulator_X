# 0002 Layered libraries with a Qt-free core

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

Users of the reference application asked for command-line control and for the ability to
run without a display. Testing NMEA encoders through a GUI is slow and brittle. How should
the code be partitioned?

## Decision drivers

- Unit-test the simulation and encoders without Qt or a display.
- Ship a headless command-line tool from the same engine as the desktop app.
- Keep a single networking and serial stack rather than one for the CLI and one for the GUI.
- Make dependency direction obvious and enforceable.

## Considered options

1. One monolithic application target.
2. A Qt-free `core` library, a Qt-based non-GUI `io` library, and two thin hosts.
3. A Qt-free `core` and `io` using standalone Asio and a third-party serial library.

## Decision outcome

Option 2. `nmeasim::core` contains the vessel model, kinematics, geodesy, sources, parsers
and encoders and depends only on the standard library and GeographicLib. `nmeasim::io`
contains transports, scheduling, persistence and the control bus and depends on Qt Core,
Network, SerialPort and WebSockets, none of which need a display. The CLI links `core` and
`io`; the desktop application adds Qt Widgets.

Option 3 was rejected because it would duplicate what Qt Network and Qt SerialPort already
provide and add two more dependencies to maintain.

### Consequences

- `src/core` must never include a Qt header. Code review and, later, a CMake check enforce it.
- Tests for `core` run with Catch2 alone and are fast.
- The CLI can run on a headless server or in CI.
