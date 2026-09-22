# Release process

Releases are automated; the maintainer's job is to review and merge.

## From commit to release

1. Pull requests are squash-merged into `main` with Conventional Commit titles.
2. The *Release Please* workflow keeps a release pull request open on `main`. It proposes the
   next version from the commit types and drafts the changelog.
3. Merging the release pull request bumps the version in `CMakeLists.txt` and `vcpkg.json`,
   updates `CHANGELOG.md`, creates the tag `vX.Y.Z` and a GitHub release.
4. The release workflow (milestone M5) builds packages on Linux, Windows and macOS runners,
   ad-hoc signs the macOS bundle, generates SHA-256 checksums and attaches everything to the
   GitHub release.
5. Package-manager manifests under `packaging/manifests/` are updated with the new version and
   checksums, then submitted to winget, Scoop, Homebrew and Flathub.

## Release checklist

- [ ] CI is green on `main`.
- [ ] Manual check on real hardware or a chart plotter for each platform (release candidates).
- [ ] Documentation reflects every user-visible change.
- [ ] The installation guide screenshots still match current operating system dialogs.

## Repository settings required

- Actions: *Allow GitHub Actions to create and approve pull requests* enabled.
- Branch protection on `main`: pull request required, required checks `Linux (GCC)`,
  `Windows (MSVC)`, `macOS (Clang)`, `Code formatting`, `Documentation`.
- Pages: source *GitHub Actions*; repository variable `DEPLOY_DOCS=true` once public.
