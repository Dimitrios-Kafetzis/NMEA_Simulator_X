# Contributing to NMEA Simulator X

Thank you for considering a contribution. This document is the short version; the
[developer documentation](docs/development/index.md) has the details.

## Ways to contribute

- Report bugs and request features through the
  [issue templates](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues/new/choose).
- Improve documentation under `docs/`.
- Implement an issue labelled `good first issue` or `help wanted`.
- Test pre-release builds on your hardware and report what you find.

## Development workflow

1. Fork the repository and create a branch from `main`, named like `feat/rot-sentence` or
   `fix/udp-broadcast-interface`.
2. Build and test locally with `cmake --workflow --preset dev`
   (see [Build from source](docs/how-to/build-from-source.md)).
3. Write tests for behaviour you add or change. Core logic lives in `src/core` and is tested
   under `tests/`.
4. Format C++ with `clang-format` and keep `clang-tidy` clean.
5. Commit using [Conventional Commits](https://www.conventionalcommits.org/):
   `feat(core): add ROT sentence encoder`, `fix(io): honour selected UDP interface`.
6. Open a pull request against `main` and fill in the template. CI must pass on Linux,
   Windows and macOS before review.

`main` is protected and always releasable. Pull requests are squash-merged, so the pull request
title becomes the commit message and must itself follow Conventional Commits.

## Definition of done

A change is done when:

- it builds warning-free on all three platforms,
- unit tests cover it and pass,
- user-visible behaviour is documented in `docs/`,
- any architectural decision is captured in an ADR under `docs/adr/`,
- the changelog entry can be derived from the commit message.

## Coding standards

See [docs/development/coding-standards.md](docs/development/coding-standards.md). In short:
C++20, `snake_case` functions and variables, `CamelCase` types, `k`-prefixed constants,
trailing underscore for private members, no raw `new`/`delete` outside Qt parent-child
ownership, and no Qt in `src/core`.

## Licensing of contributions

By submitting a contribution you agree that it is licensed under the
[GPL-3.0](LICENSE) like the rest of the project.
