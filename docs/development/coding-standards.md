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
- Every file carries documentation comments as described under
  [Documentation comments](#documentation-comments).

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
- The Python tools in `tools/` are tested with the standard library's `unittest` in
  `tools/tests/`, one `test_<subject>.py` file per subject. CI runs them on every platform,
  after the stream cross-check that installs their dependencies:

    ```bash
    pip install pynmea2==1.19.0 pyais==2.9.4
    python3 -m unittest discover -s tools/tests -v
    ```

## Documentation comments

Every source file in the repository is documented: C++ headers and implementation files,
tests, Python tools, CMake files and workflows. The [C++ reference](../reference/api.md) on
this site is generated from the C++ comments, as [ADR 0017](../adr/0017-cpp-reference-in-mkdocs.md)
describes, so a comment is written for a reader who sees it next to the signature but not
next to the code. The checks listed under [Enforcement](#enforcement) run in CI.

### Syntax

- Documentation comments are `///` lines in front of the entity they describe. A trailing
  `///<` is allowed after a data member or an enumerator when the whole comment fits on
  that line.
- Commands start with `@` (`@param`, never `\param`).
- The first sentence is the brief description: Doxygen ends it at the first full stop
  followed by a space. It must therefore contain no abbreviation such as "e.g." or "i.e.";
  write "for example" and "that is". Details follow after an empty `///` line.
- Comments are Markdown: backticks around code, parameter names, file names and literal
  values, `-` for lists. No HTML.
- British spelling, complete sentences, present tense. Functions are described in the
  third person: "Returns the checksum", not "Return the checksum".
- Lines stay within 100 columns. `clang-format` reflows comments, so reread a comment after
  formatting.
- Name another entity by its name, qualified where it is ambiguous; the reference turns the
  name into a link.

### Files

Every C++ file, including the `.in` templates that CMake configures, starts with the licence
identifier and a file comment whose first sentence says what the file provides:

```cpp
// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Geodesic distance, bearing and destination on the WGS 84 ellipsoid.
///
/// Optional paragraphs: the main entry points, the standard the file implements.

#pragma once
```

The licence line is a plain `//` comment so that it never becomes part of the
documentation.

### Namespaces

- Every named namespace is documented exactly once: in the header that declares its central
  type or is named after it, with a `///` comment in front of the `namespace` line. The
  enclosing namespaces that have no such header (`nmeasim`, `nmeasim::core`,
  `nmeasim::io`, `nmeasim::app`) are documented in `src/namespaces.dox`.
- The comment says what the namespace contains and, for a library namespace, which layer it
  belongs to.
- Anonymous namespaces are not documented themselves; every entity inside one is.

### Types

A class, struct, union, enumeration or type alias states:

- what it represents or is responsible for (the brief);
- its invariants: which combinations of member values are valid;
- ownership and lifetime: what it owns, and the objects it refers to that must outlive it;
- copy and move behaviour when it is not the obvious one, and why copying is disabled when
  it is;
- thread safety and, for a `QObject`, thread affinity, whenever they differ from the
  defaults below;
- the governing standard when it models a protocol structure (see [Standards](#standards));
- related types with `@see`.

Every template parameter has a `@tparam`.

### Functions

A function, constructor, operator or method has:

- a brief saying what it does or returns;
- `@param` for every parameter: its meaning, its unit when the name has no unit suffix, its
  valid range and what special values (null, empty, `std::nullopt`, zero) mean. Output and
  in-out parameters are marked `@param[out]` and `@param[in,out]`;
- `@return` for every non-void function: what is returned, including what an empty or
  failure value means. Constructors and `void` functions have none;
- `@throws` for every exception type the function throws or deliberately lets propagate,
  with the condition. A `noexcept` function has none;
- `@pre` for every condition the caller must ensure and whose violation is undefined
  behaviour or an assertion, and `@post` for guarantees about the object's state after the
  call that `@return` does not already state;
- `@note`, `@warning` and `@see` where they help: a surprising side effect, a performance
  cost, a related function.

Further rules:

- A function that emits Qt signals names them: "Emits `state_changed`."
- An override of a function documented in this repository inherits its documentation and
  is commented only where its behaviour is more specific. An override of a Qt or standard
  library function (`paintEvent`, `sizeHint`, `what`) is always documented, because its
  base is not part of this reference: the comment says what this override does.
- A defaulted special member gets a one-line comment ("Destroys the transport, closing it
  first."); a deleted one says why it is deleted.
- Documentation lives at the declaration. A definition in a `.cpp` file of a function
  declared in a header carries no `///` comment of its own, only `//` implementation
  comments; functions local to a `.cpp` file are documented where they are defined.

### Data members, enumerators and constants

Every data member (public, protected and private), enumerator and named constant states its
meaning, its unit when the name has none, its valid range and what its default or sentinel
values mean. An enumerator that maps to a protocol field names the value it is sent as. A
constant says where its value comes from: a standard, a measurement, a user-interface
choice.

### Qt signals and slots

- A signal is described as an event: "Emitted when ...". It states the condition, the
  meaning of every argument with `@param`, whether it is also emitted when the value did
  not change, and whether it is emitted synchronously from inside a public call.
- A slot is documented like any other function and additionally says what normally
  connects to it.

### Macros

Every macro states its purpose, documents each argument with `@param` and says what it
expands to when that matters to the caller.

### Units, ranges and defaults

Units follow the naming rules above (`speed_kn`, `depth_m`, `bearing_deg`). Comments add
what the name cannot carry:

- angles are degrees true unless marked magnetic; bearings and headings are in
  [0, 360), latitudes in [-90, 90] positive north, longitudes in [-180, 180] positive east;
- signed quantities say which sign means what (positive to starboard, positive rising);
- times are UTC; durations use `std::chrono` types;
- ranges are written in interval notation, and the comment says what happens outside the
  range: clamped, rejected with an empty result, or undefined behaviour;
- a default value is documented by its initialiser in the declaration; the comment explains
  why that value was chosen when it is not self-evident.

### Ownership, lifetime and threads

- Raw pointers and references are non-owning. A function or type that keeps one says who
  owns the object and how long it must live.
- A `std::unique_ptr` parameter transfers ownership; a Qt `parent` parameter makes the
  parent the owner, and the comment says so when the argument may be null.
- A function returning a reference or a view says how long it stays valid.
- Default thread safety, which comments do not repeat: an object is used by one thread at a
  time; distinct objects may be used from different threads; `const` member functions of
  core types may run concurrently. A `QObject` lives in the thread that created it and is
  used only from that thread. Anything else (a class moved to a worker thread, a signal
  emitted from another thread, a function safe to call from any thread) is stated.

### Standards

Code that implements an external specification cites it at the type or function concerned,
naming the edition where the code follows a particular one, and the section, message or
sentence where one applies:

| Area | Specification | Example citation |
| --- | --- | --- |
| NMEA 0183 sentences | NMEA 0183 (IEC 61162-1) | `@see NMEA 0183, sentence GGA.` |
| AIS messages | ITU-R M.1371-5 | `@see ITU-R M.1371-5, Annex 8, message 5.` |
| TAG blocks | IEC 61162-450 | `@see IEC 61162-450, TAG block parameter "c".` |
| Signal K | [Signal K specification 1.7.0](https://signalk.org/specification/1.7.0/doc/) | `@see https://signalk.org/specification/1.7.0/doc/data_model.html` |
| Geodesics | WGS 84; C. F. F. Karney, *Algorithms for geodesics*, 2013 | `@see GeographicLib::Geodesic` |
| Tracks | [GPX 1.1](https://www.topografix.com/GPX/1/1/), [OGC KML 2.2](https://www.ogc.org/standard/kml/) | `@see GPX 1.1, element trkpt.` |

Standards that are not freely available are cited by document and clause without a link.
Open specifications are linked.

### Implementation comments

- `//` comments inside functions and on implementation details explain *why*: the reason
  for a choice, a constraint that is not visible in the code, the source of a constant, a
  consequence that is easy to miss. They do not narrate *what* the next line does.
- A comment that contradicts the code is a bug; change both together.
- No commented-out code. Delete it; the history keeps it.
- A `TODO` names its issue: `// TODO(#123): ...`.
- A workaround cites the bug it works around, with a link where one exists.

### Tests

- The file comment of a test file names the unit under test and what the file covers,
  and names the fixtures it reads.
- `TEST_CASE` and `SECTION` names are the specification: complete statements of the
  expected behaviour. They carry no documentation comment; the reference lists them on the
  test file's page.
- Fixtures, helper functions and helper types are documented like production code.
- An expected value that is not obvious says where it comes from: the standard's worked
  example, an independent decoder, a calculation in Python.

### Python

- A script starts with its shebang line, if any, then
  `# SPDX-License-Identifier: GPL-3.0-only`, then the module docstring.
- Docstrings follow [PEP 257](https://peps.python.org/pep-0257/) with the Google layout: a
  one-line summary in the imperative mood ("Return the ...", as PEP 257 prescribes for
  Python), a blank line, a description, then `Args:`, `Returns:`, `Yields:` and `Raises:`
  sections.
- The module docstring says what the script does and how to run it. Every function, class
  and method, private helpers included, has a docstring.

### CMake

- Every `CMakeLists.txt` and `.cmake` file starts with `# SPDX-License-Identifier:
  GPL-3.0-only` and a comment saying what the file builds or defines and how it is used.
- Every `function()` and `macro()` is preceded by a comment describing its arguments, its
  results and its side effects. A script run with `cmake -P` lists the `-D` variables it
  expects.
- Every `option()` and cache variable has a help string. Non-obvious commands have a comment
  explaining why.

### Workflows

- Every workflow starts with `# SPDX-License-Identifier: GPL-3.0-only` and a comment block
  saying what the workflow does, what triggers it, and which permissions, secrets and
  repository variables it needs.
- A job whose `name` does not say what it is for, and a step whose script is not
  self-explanatory, have a comment explaining why they exist.

### Enforcement

| Check | Where | What it enforces |
| --- | --- | --- |
| Doxygen, warnings as errors (`tools/doxygen/Doxyfile`, run by `mkdocs build`) | *Documentation* job | Every file, namespace member, type, function, parameter, return value, data member and enumerator in `src/` and `tests/` is documented, private members and anonymous namespaces included; comments are well formed. |
| Clang `-Wdocumentation`, warnings as errors (`cmake/ProjectWarnings.cmake`) | *macOS (Clang)* job | `@param` and `@tparam` names match the declaration; no `@return` on a `void` function; commands are well formed. |
| File headers (`mkdocs build` for the C++ file comment, `tools/check_file_headers.py` for the licence line) | *Documentation* job | The licence line in every C++, Python, CMake, workflow and shell file, and a file comment in every C++ file. |
| `ruff check` with the pydocstyle rules of `ruff.toml` | *Documentation* job | Docstrings in the Google layout of PEP 257 in every Python module, class and function, with documented arguments. |

To run the checks locally before pushing:

```bash
NMEASIM_REQUIRE_DOXYGEN=1 mkdocs build --strict   # Doxygen and the C++ file comments
python3 tools/check_file_headers.py
ruff check .                                      # pip install ruff==0.16.8
```

Clang's `-Wdocumentation` runs in every build with Clang or AppleClang, for example the
`ci-macos` preset.

Checks that a machine cannot make (that a comment is specific, current and explains why)
are part of code review.
