# 0006 Documentation standard

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

The project owner requires documentation that follows industry best practice for both users
and contributors. What structure and tooling should be used?

## Decision drivers

- Different readers need different documents: newcomers, operators, integrators, contributors.
- Documentation must be versioned with the code and checked in CI.
- API documentation should be generated from source, not maintained by hand.
- Decisions must be traceable.

## Considered options

1. A single long README.
2. Diátaxis-structured MkDocs site with Material theme, MADR decision records and Doxygen.
3. Sphinx with Breathe and Exhale.

## Decision outcome

Option 2.

- **Structure:** [Diátaxis](https://diataxis.fr/): tutorials, how-to guides, reference,
  explanation, plus a *Decisions* section for ADRs and a *Development* section.
- **Tooling:** MkDocs with the Material theme, built with `--strict` in CI and published to
  GitHub Pages. Diagrams are Mermaid in Markdown so they are diffable.
- **API reference:** Doxygen with `doxygen-awesome-css`, generated from `///` comments in the
  public headers, linked from the Reference section (added with the first stable API).
- **Decisions:** MADR-format ADRs under `docs/adr/`, immutable once accepted.
- **Changelog:** Keep a Changelog format, generated from Conventional Commits.

Sphinx was rejected because its C++ pipeline is heavier to maintain and MkDocs Material
offers better authoring ergonomics for the non-API content, which is the majority.

### Consequences

- Every feature change updates the relevant reference page in the same pull request.
- Public headers must carry Doxygen comments.
- The documentation build failing breaks CI.
