# Architecture decision records

Significant decisions are recorded as ADRs using the
[MADR](https://adr.github.io/madr/) template. An ADR is never edited after acceptance; a new
ADR supersedes it instead. Number the next record sequentially and add it to `mkdocs.yml`.

| ID | Title | Status |
| --- | --- | --- |
| [0001](0001-cpp-qt-widgets.md) | Native C++20 with Qt 6 Widgets | Accepted |
| [0002](0002-layered-libraries.md) | Layered libraries with a Qt-free core | Accepted |
| [0003](0003-cmake-vcpkg.md) | CMake presets and vcpkg manifest mode | Accepted |
| [0004](0004-gpl-license.md) | GPL-3.0 license | Accepted |
| [0005](0005-unsigned-distribution.md) | Unsigned distribution through free channels | Accepted |
| [0006](0006-documentation-standard.md) | Documentation standard | Accepted |
| [0007](0007-trunk-based-releases.md) | Trunk-based development, Conventional Commits and automated releases | Accepted |
| [0008](0008-testing-strategy.md) | Testing strategy | Accepted |
| [0009](0009-desktop-shell.md) | Desktop shell: one main window with dock panels, tested offscreen | Accepted |
| [0010](0010-map-tiles.md) | Map view: painted raster tiles with a disk cache | Accepted |
| [0011](0011-track-and-replay-sources.md) | Track following and log replay as sources with a seekable transport interface | Accepted |

## Template

```markdown
# NNNN Title

- Status: proposed | accepted | deprecated | superseded by [NNNN](NNNN-title.md)
- Date: YYYY-MM-DD

## Context and problem statement

## Decision drivers

## Considered options

## Decision outcome

### Consequences

## More information
```
