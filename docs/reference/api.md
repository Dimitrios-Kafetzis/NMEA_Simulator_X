# C++ API

The C++ reference documents every namespace, class, function, data member, signal and file
of the simulator, generated from the comments in the source code each time this site is
built. It is split into four parts:

| Part | Contents |
| --- | --- |
| [Library API](cpp/library.md) | The public interface of the two libraries that other programs can link: `core`, the simulation engine and every encoder, with no Qt dependency, and `io`, the profiles, transports and runner built on Qt's non-GUI modules. |
| [Library internals](cpp/internals.md) | The implementation files and private headers of both libraries, with the helpers local to them. Not part of the library interface. |
| [Applications](cpp/applications.md) | The desktop application (`nmeasim::app`) and the `nmeasim` command-line tool. |
| [Test suite](cpp/tests.md) | The test files, with their test cases, fixtures and helpers. |

The [architecture overview](../explanation/architecture.md) explains how the parts fit
together, and the Library API page names good starting points.

| Library | CMake target | Namespace | Depends on |
| --- | --- | --- | --- |
| `core` | `nmeasim::core` | `nmeasim::core` | C++20 standard library, GeographicLib, pugixml |
| `io` | `nmeasim::io` | `nmeasim::io` | `core`, Qt Core, Network, SerialPort, WebSockets |

The libraries are linked statically into the desktop application and the command-line
tool. Their interfaces follow [Semantic Versioning](https://semver.org/) only in so far as
the applications do; programs that embed them should pin a release.

## Reading the reference

- Every page links to the declaration on GitHub at the commit the site was built from, and
  its *Edit* button opens the source file whose comments produced it.
- Units are part of the names: `speed_kn`, `depth_m`, `bearing_deg`, `period_ms`. Angles
  are degrees true unless marked magnetic, times are UTC.
- Unless a page says otherwise, an object is used by one thread at a time, and a `QObject`
  is used only from the thread that created it.

The rules the comments follow are in
[Documentation comments](../development/coding-standards.md#documentation-comments), and
the way the reference is generated in [ADR 0017](../adr/0017-cpp-reference-in-mkdocs.md).

## Building the reference locally

```bash
doxygen --version                  # 1.18 or later; CI uses 1.18.0
pip install -r docs/requirements.txt
mkdocs serve                       # or: mkdocs build --strict
```

The MkDocs hook in `tools/doxygen/mkdocs_hook.py` runs Doxygen with `tools/doxygen/Doxyfile`,
fails on missing or malformed documentation comments, and generates the pages with
`tools/doxygen/cppreference.py`. Without Doxygen, the build replaces the reference with
placeholder pages. To look at the generated Markdown directly:

```bash
doxygen tools/doxygen/Doxyfile
python3 tools/doxygen/cppreference.py build/doxygen/xml build/reference
```
