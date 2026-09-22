# 0008 Testing strategy

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

The value of a simulator lies in the correctness of every byte it emits. Marine equipment is
often intolerant of small deviations such as a leading zero or an 83-character sentence.
How is correctness assured continuously?

## Decision drivers

- Every sentence must be provably compliant with the NMEA 0183 framing rules.
- Regressions in encoders must be caught before release.
- Tests must run on all three platforms in CI.
- Networking and serial code must be exercised, not only the pure core.

## Considered options

1. Manual testing against real chart plotters.
2. Layered automated tests: unit, golden-file, property-based, integration and end-to-end,
   with manual hardware testing reserved for release candidates.

## Decision outcome

Option 2, with these layers:

| Layer | Scope | Tooling |
| --- | --- | --- |
| Unit | Functions and classes in `core` and `io` | Catch2 v3 through CTest |
| Golden file | Every sentence builder compared to a reviewed example line | Catch2 with fixtures under `tests/fixtures/` |
| Independent cross-check | Emitted sentences parsed by a third-party NMEA parser and compared field by field | Added with the encoder registry |
| Property based | Invariants such as checksum correctness and the 82-byte limit for random inputs | Catch2 generators |
| Integration | Real TCP, UDP and WebSocket sockets on loopback; virtual serial pairs on Linux | Catch2 with Qt Test event loop |
| End to end | The desktop application driven through Qt Test | Added in milestone M2 |
| Sanitizers | AddressSanitizer and UndefinedBehaviorSanitizer on the Linux CI build | CMake option `NMEASIM_ENABLE_SANITIZERS` |

Coverage is measured on the Linux build and reported per pull request; the `core` library
targets 90 percent line coverage.

### Consequences

- A sentence is not merged without a golden-file test and a reference page.
- CI time grows with the test suite; integration tests are tagged so they can be filtered.
- Manual testing against hardware is a release checklist item, not a substitute for
  automation.
