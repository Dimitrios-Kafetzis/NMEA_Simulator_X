# Coding standards

These rules are enforced by `.clang-format`, `.clang-tidy` and code review.

## Language

- C++20, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- Prefer the standard library. Use `std::format`, `std::string_view`, `std::optional`,
  `std::span` and ranges where they make code clearer.
- No exceptions for control flow. Encoders and parsers return `std::optional` or a result
  type; exceptions are reserved for programming errors and unrecoverable I/O failures.
- No raw `new`/`delete`. Qt objects with a parent are the one exception, since Qt owns them.

## Naming

| Element | Style | Example |
| --- | --- | --- |
| Namespaces | `lower_case` | `nmeasim::core::nmea0183` |
| Types | `CamelCase` | `SerialPortDescriptor` |
| Functions and methods | `lower_case` | `append_checksum` |
| Variables and parameters | `lower_case` | `distance_m` |
| Private and protected members | trailing underscore | `port_name_` |
| Constants and `constexpr` | `k` prefix, `CamelCase` | `kMaxSentenceLength` |
| Files | `lower_case` with `.hpp`/`.cpp` | `serial_ports.hpp` |

Units are part of the name: `speed_kn`, `depth_m`, `bearing_deg`, `period_ms`.

## Layout

- `clang-format` with the repository configuration; 4-space indent, 100-column limit.
- One class per header where practical. Headers use `#pragma once`.
- Include order: own header, project headers, Qt, third party, standard library.
- Public headers under `src/<lib>/include/nmeasim/<lib>/` carry Doxygen `///` comments on
  every public symbol.

## Qt usage

- `src/core` never includes Qt.
- Use `QStringLiteral` for literals, `QString::fromStdString` at the boundary with `core`.
- Prefer the functor-based `connect` overloads over string-based signals and slots.
- GUI code never blocks the event loop; long work goes to the engine thread.

## Tests

- Every public function in `core` has a unit test.
- Test files mirror the source path: `src/core/src/geo/geodesic.cpp` is tested by
  `tests/core/geo/geodesic_test.cpp`.
- Use Catch2 tags: `[nmea0183]`, `[geo]`, `[io]`, `[integration]`.
