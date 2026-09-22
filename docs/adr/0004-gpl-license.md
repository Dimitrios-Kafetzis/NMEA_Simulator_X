# 0004 GPL-3.0 license

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

The project is published as free and open-source software. The reference application had no
public source and no license. Which license should NMEA Simulator X use?

## Decision drivers

- Keep the software and its derivatives free and open.
- Compatibility with Qt's open-source licensing (LGPL v3 / GPL v3).
- Compatibility with the other dependencies (GeographicLib: MIT, CLI11: BSD-3, Catch2: BSL-1.0).

## Considered options

1. GPL-3.0.
2. Apache-2.0.
3. MIT.

## Decision outcome

Option 1, **GPL-3.0-only**. It guarantees that improvements to the simulator remain available
to the community, aligns naturally with Qt's open-source terms and is compatible with every
dependency in use. Permissive licenses were considered but the project owner prefers the
copyleft guarantee.

### Consequences

- Every source file may carry an SPDX header `SPDX-License-Identifier: GPL-3.0-only`.
- Contributions are accepted under the same license, as stated in `CONTRIBUTING.md`.
- Qt is linked dynamically so that its LGPL obligations are met by shipping the Qt libraries
  alongside the application.
