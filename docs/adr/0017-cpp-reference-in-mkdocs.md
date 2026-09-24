# 0017 C++ reference generated from Doxygen XML into the MkDocs site

- Status: accepted
- Date: 2026-09-24

## Context and problem statement

[ADR 0006](0006-documentation-standard.md) publishes the C++ API reference as Doxygen's own
HTML under `/api/`. That reference covers only the public headers of `core` and `io`. It
has Doxygen's look instead of the site theme, has no dark mode, sits outside the site
navigation and is not found by the site search. The implementation files, the desktop
application, the command-line tool and the tests are not documented at all. The project now
documents every source file (see
[Documentation comments](../development/coding-standards.md#documentation-comments)). How is
the C++ reference generated from those comments and shown inside the MkDocs site?

## Decision drivers

- One site: the reference uses the Material theme in both colour schemes, appears in the
  navigation under *Reference* and is found by the site search.
- A page per namespace, class or struct, and file, with signatures, parameter and return
  tables, notes, inherited members, links between types and links to the source.
- The public library API (`core`, `io`) is clearly separated from the internals of the
  libraries, the applications (`app`, `cli`) and the tests.
- Reproducible builds: deterministic output, no network access during the build, pinned
  versions, `mkdocs build --strict` green, a build time of seconds rather than minutes.
- Low maintenance risk: tools that are maintained, or small enough to maintain ourselves.
- No published link breaks.

## Considered options

Maintenance status as of 2026-09-24.

1. **Keep Doxygen HTML under `/api/` and theme it** (for example with
   `doxygen-awesome-css`). It would look closer to the site but would still be a separate
   site: no shared navigation, no shared search, its own dark-mode switch.
2. **[MkDoxy](https://github.com/JakubAndrysek/MkDoxy)**, a MkDocs plugin that runs Doxygen
   and renders its XML with Jinja templates. Last release 1.2.8 on 2025-08-29, 37 open issues,
   among them enumerator descriptions missing from the output (opened 2026-05-21). Its page
   layout and navigation are fixed per Doxygen project; separating the public API from the
   internals would take several projects, each with its own Doxygen run, and links between
   them would not resolve.
3. **[doxybook2](https://github.com/matusnovak/doxybook2)** and its forks, a C++ program
   that converts Doxygen XML into Markdown for MkDocs and other generators. The original
   repository was archived in 2022. The forks carry occasional commits (the most recent in
   February 2026) but none publishes releases or has users to speak of. It would be a native
   binary to download or build in CI, outside `docs/requirements.txt`.
4. **[mkdocstrings](https://mkdocstrings.github.io/)**, the standard way to render API
   documentation in MkDocs, actively maintained (1.0.6, July 2026). Its handlers cover
   Python, C (`mkdocstrings-c` 0.1.0, which parses C with pycparser), shell, Crystal and VBA.
   There is no C++ handler, and parsing C++20 is far beyond what a new handler could do.
5. **A small generator in this repository** that reads Doxygen's XML output and produces
   Markdown pages through the existing MkDocs hook, using only the Python standard library
   and the MkDocs plugin API.

## Decision outcome

Option 5. Doxygen stays the C++ parser, which it does well and which CI already pins; the
part that the other options get wrong for this project, turning its output into pages that
fit the site, is a few hundred lines of Python that the project controls. Doxygen's XML
schema is versioned with Doxygen, and the Doxygen version is pinned, so the generator
changes only when the project decides to upgrade.

### Doxygen configuration

`tools/doxygen/Doxyfile` reads every C++ file under `src/` and `tests/`, including the
`.cpp` files, the `.in` templates, private members and anonymous namespaces, and writes XML
only. Warnings include undocumented entities (`WARN_IF_UNDOCUMENTED`), undocumented
parameters and return values (`WARN_NO_PARAMDOC`) and undocumented enumerators, and they
are errors.

### Generator

The MkDocs hook `tools/doxygen/mkdocs_hook.py` runs Doxygen once per build, and the
generator next to it turns the XML into Markdown pages that MkDocs renders like any other
page. The pages are generated in memory; nothing is written into `docs/`. The navigation
subtree is generated too and replaces the *C++ API* entry of `mkdocs.yml`. The generated
reference lives under `/reference/cpp/`, with the existing `/reference/api/` page as its
overview:

| Part | Contents | Example URL |
| --- | --- | --- |
| Library API | Namespaces, types and headers of `src/core/include` and `src/io/include` | `/reference/cpp/nmeasim/core/simulation/Simulation/` |
| Library internals | The `.cpp` files and private headers of `core` and `io`, with their file-local helpers | `/reference/cpp/files/src/core/src/nmea0183/encoders.cpp/` |
| Applications | `nmeasim::app` and the command-line tool | `/reference/cpp/nmeasim/app/MainWindow/` |
| Test suite | Test files, their fixtures, helpers and test case names | `/reference/cpp/files/tests/core/geo/geodesic_test.cpp/` |

- A namespace, class or struct page lives at its qualified name with `::` replaced by `/`.
  A file page lives at its repository path under `files/`. A type declared in an anonymous
  namespace lives under the page of the file that declares it.
- A type page shows the brief, the header to include, base and derived classes, the
  detailed description, a summary table per member section (types, functions, signals,
  slots, data members, by access), then every member with its signature, description, a
  parameter table with linked types, the return value, exceptions, pre- and postconditions,
  notes and warnings, and a link to its line on GitHub. Members inherited from other
  documented classes are listed with links. Qt base classes link to the Qt documentation.
- Source links point at the commit being built, so they match the documented code.
- The output depends only on the sources, the commit and the pinned tools: entries are
  sorted, and no dates or absolute paths are written.
- Nothing is downloaded during the build. Doxygen is a pinned binary checked by SHA-256 in
  CI, and every Python package in `docs/requirements.txt` is pinned to an exact version.

### The `/api/` pages

The Doxygen HTML is no longer generated. Each of the 288 HTML pages it published is
replaced by a small page that redirects to the new page of the same entity: classes,
structs and namespaces to their pages, headers to their file pages, and indexes to the
Library API overview. The list of published pages is frozen in the repository, so the
redirects do not depend on the old generator.

### Enforcement

- The documentation build fails on any Doxygen warning. While the comments are being
  written, directory by directory, the hook fails only on warnings from the directories
  already completed; the list shrinks to nothing when every directory is done.
- The hook also checks that every C++ file in a completed directory starts with the
  licence line and has a file comment.
- The macOS job compiles with Clang's `-Wdocumentation` under `-Werror`, which checks that
  `@param` names match the parameters. Qt and vcpkg headers are imported targets and
  therefore system headers, so only first-party comments are checked.
- The documentation workflow runs on changes to `src/`, `tests/`, `tools/` and the
  documentation tooling, so the published reference follows the code.

### Consequences

- The reference is part of the site: one theme, one navigation, one search, one build.
- Every C++ comment is visible to readers, including those in implementation files and
  tests, which raises the bar for their wording.
- The generator is project code: it is documented and kept small, and an upgrade of
  Doxygen is tested against it before the pin moves.
- The reference adds several hundred pages to the navigation; they sit in collapsed
  sections under *Reference › C++ API*.
- The *API reference* part of ADR 0006 is replaced by this decision; its other decisions
  stand.

## More information

- [Documentation comments](../development/coding-standards.md#documentation-comments)
- [Doxygen XML output](https://www.doxygen.nl/manual/customize.html#xmlgenerator)
