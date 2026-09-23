# 0015 Release workflow: one reusable workflow, CPack on every platform, called by Release Please

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

ADR 0007 hands releases to Release Please: merging its pull request creates the tag `vX.Y.Z`
and the GitHub release. ADR 0005 requires unsigned but launchable packages for Windows,
macOS and Linux with SHA-256 checksums. Something has to build those packages for every
release, attach them to it, and be testable before a release happens. How is that workflow
laid out and triggered?

## Decision drivers

- A release made by Release Please must get its packages without a manual step.
- Tags and releases created with the repository's `GITHUB_TOKEN`, which Release Please uses,
  do not start other workflows; a workflow listening for `push: tags` or `release: published`
  would never run for them.
- The packaging must be exercised before a real release: on a branch that changes it, and on
  a release candidate that produces a real pre-release.
- Package contents should come from one definition that developers can run locally, not
  from copy commands scattered through YAML.
- Every package is checked on the runner that built it before anything is published.
- Zero cost: GitHub-hosted runners only, which are free for public repositories.

## Considered options

1. A release workflow triggered by `push: tags`, with a personal access token given to
   Release Please so that its tags start workflows.
2. Package jobs appended to the Release Please workflow.
3. A separate `release.yml` with four triggers: `workflow_call` from the Release Please
   workflow, `push` of a `v*` tag, `workflow_dispatch` for an existing tag, and `push` to a
   branch that touches packaging files as a dry run.

## Decision outcome

Option 3.

- **Triggers.** `release-please.yml` calls `release.yml` with the new tag when
  `release_created` is true. Release candidates are tagged by hand (`v1.0.0-rc.1`); the tag
  push runs the same workflow, which creates the release as a pre-release. A manual dispatch
  rebuilds and re-uploads the packages of an existing tag. A push to any branch other than
  `main` that changes the workflow, `packaging/`, `cmake/` or a `CMakeLists.txt` builds every
  package with the version `<project version>-dryrun.<commit>` and keeps them as a workflow
  artifact without publishing anything.
- **Layout.** A `prepare` job derives the tag, the package version and whether to publish,
  and refuses a final tag that does not match the version in `CMakeLists.txt`. One job per
  platform and architecture builds the `release` preset (`release-windows` on Windows) from a
  full clone of the tag, so that `git describe` names the tag, then runs `cpack` and checks
  its output: the Windows portable ZIP and a silent install of the NSIS installer, the
  mounted macOS disk image (ad-hoc signature, architecture, minimum macOS version), the
  AppImage (command-line tool, `nmeasim` link, a headless start of the desktop application)
  and the Flatpak bundle, installed and run. A `publish` job collects everything, writes
  `SHA256SUMS.txt` and attaches it all to the release, creating the release for a tag pushed
  by hand.
- **Runners.** `windows-2022`; `macos-15` for arm64 and `macos-15-intel` for x86_64, since a
  separate native build per architecture avoids building the vcpkg dependencies twice for a
  universal binary and lets each package be run on its own architecture; `ubuntu-24.04` and
  `ubuntu-24.04-arm` for the x86_64 and aarch64 AppImages; the Flathub KDE builder container
  for the Flatpak.
- **Packaging definition.** `cmake --install` and `cpack` do the work on every platform
  (`cmake/Packaging.cmake`): windeployqt and macdeployqt run from Qt's deploy script at
  install time, the macOS bundle is signed ad hoc at install time, CPack's NSIS, ZIP and
  DragNDrop generators build the Windows and macOS packages, and a CPack External generator
  script runs linuxdeploy for the AppImage. The workflow only provides tools and checks
  results.
- **Pinned tools.** Qt, linuxdeploy, its Qt plugin and the Flatpak runtime are pinned to
  exact versions in the workflow and the manifest.

Option 1 was rejected because a long-lived personal token is a secret to rotate and a
security liability for a single-maintainer project. Option 2 was rejected because it cannot
be run for a hand-made release candidate or as a dry run without a release.

### Consequences

- A release is complete only when the `packages` job of the Release Please run has finished;
  the release checklist in the release process says how to confirm it.
- Changes to packaging are tested by pushing a branch; the dry-run artifact can be downloaded
  and tried on real machines before merging.
- A release candidate leaves a pre-release and a tag behind that Release Please could take
  for the latest version; both are deleted once the candidate has been checked.
- The macOS packages need macOS 13.3 or later (the deployment target that `std::format`
  requires in Apple's C++ library).

## More information

- [Release process](../development/release-process.md)
- [ADR 0016](0016-linux-packages.md) for the choice of Linux formats.
