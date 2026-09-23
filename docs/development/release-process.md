# Release process

Releases are automated; the maintainer's job is to review, merge and check. The reasoning is
in [ADR 0007](../adr/0007-trunk-based-releases.md) (versioning and Release Please) and
[ADR 0015](../adr/0015-release-workflow.md) (the release workflow).

## From commit to release

1. Pull requests are squash-merged into `main` with Conventional Commit titles.
2. The *Release Please* workflow keeps a release pull request open on `main`. It proposes the
   next version from the commit types and drafts the changelog.
3. Merging the release pull request bumps the version in `CMakeLists.txt` and `vcpkg.json`,
   updates `CHANGELOG.md` (from which the AppStream release list is generated), creates the
   tag `vX.Y.Z` and a GitHub release with the changelog as notes.
4. In the same *Release Please* run, the `packages` job calls the release workflow
   (`.github/workflows/release.yml`) for the new tag. It builds the `release` presets from the
   tag, runs `cpack` and checks every package on the runner that built it:

    | Job | Runner | Package | Checks |
    | --- | --- | --- | --- |
    | Windows x64 | `windows-2022` | `NMEASimulatorX-X.Y.Z-win64.exe` (NSIS), `NMEASimulatorX-X.Y.Z-win64-portable.zip` | ZIP extracted and `nmeasim` run; installer run silently and the installed `nmeasim` run |
    | macOS arm64, macOS x86_64 | `macos-15`, `macos-15-intel` | `NMEASimulatorX-X.Y.Z-macos-arm64.dmg`, `NMEASimulatorX-X.Y.Z-macos-x86_64.dmg` | Image mounted; ad-hoc signature verified; architecture and minimum macOS 13.3 checked; `nmeasim` run from the bundle |
    | Linux x86_64, Linux aarch64 | `ubuntu-24.04`, `ubuntu-24.04-arm` | `NMEASimulatorX-X.Y.Z-x86_64.AppImage`, `NMEASimulatorX-X.Y.Z-aarch64.AppImage` | `nmeasim` run through the AppImage and through a link; desktop application started headless |
    | Flatpak x86_64 | KDE 6.10 builder container | `NMEASimulatorX-X.Y.Z-x86_64.flatpak` | Built from `packaging/flatpak/`, bundle installed and `nmeasim` run |

5. The `publish` job writes `SHA256SUMS.txt` over all packages and attaches the packages and
   the checksum file to the GitHub release. It also fills the package-manager templates
   under `packaging/manifests/` with the version, the checksums and the tag's commit
   (`update_manifests.py`) and keeps them as the `package-manager-manifests-X.Y.Z` artifact.
6. The maintainer submits those manifests to winget, Scoop, the Homebrew tap and Flathub, as
   described in [Submit the package-manager manifests](../how-to/submit-package-manifests.md).

## Choosing the version

Release Please derives the version from the commits since the last release. Before 1.0 a
`feat` bumps the minor version (`bump-minor-pre-major`). To force a version, for example the
first stable release, end the squash commit message of the last pull request with a footer:

```text
Release-As: 1.0.0
```

## Release candidates

A release candidate exercises the whole workflow, including publishing, without a real
release:

```bash
git tag v1.0.0-rc.1 <commit>          # any commit, usually the tip of main or a branch
git push origin v1.0.0-rc.1
gh run watch "$(gh run list --workflow Release --limit 1 --json databaseId --jq '.[0].databaseId')"
gh release view v1.0.0-rc.1           # a pre-release with every package and SHA256SUMS.txt
```

Download the packages and try them on real machines. Then delete the pre-release and the
tag, so that Release Please does not take the candidate for the latest version:

```bash
gh release delete v1.0.0-rc.1 --cleanup-tag --yes
git tag -d v1.0.0-rc.1
```

## Dry runs

Pushing a branch that changes `.github/workflows/release.yml`, `packaging/`, `cmake/`,
`CMakeLists.txt`, `CMakePresets.json` or the application and CLI `CMakeLists.txt` runs the
release workflow as a dry run: every package is built and checked with the version
`<project version>-dryrun.<commit>`, and the result is kept as the workflow artifact
`dry-run-<version>`. Nothing is published.

## Rebuilding the packages of a release

*Actions → Release → Run workflow* with the tag, for example `v1.0.0`, builds the packages of
that tag again and replaces the files attached to its release (`gh release upload --clobber`).
Use it when a runner failure left a release without some of its packages.

## Release checklist

Before merging the release pull request:

- [ ] CI is green on `main` and on the release pull request (Release Please pull requests
      get no CI of their own; close and reopen the pull request to start it).
- [ ] The last dry run or release candidate of the release workflow succeeded, and its
      packages were started on real Windows, macOS and Linux machines, ideally with a chart
      plotter or OpenCPN connected.
- [ ] Documentation reflects every user-visible change, and `mkdocs build --strict` passes.
- [ ] The version proposed by Release Please is the intended one (see *Choosing the
      version*).

After merging:

- [ ] The *Release Please* run on `main` finished, including its `packages` job.
- [ ] `gh release view vX.Y.Z` lists seven packages and `SHA256SUMS.txt`, and the file names
      match the [installation guide](../how-to/install.md).
- [ ] A package downloaded from the release matches its line in `SHA256SUMS.txt`
      (`sha256sum --check --ignore-missing SHA256SUMS.txt`).
- [ ] The package-manager manifests from the release run are submitted (winget, Scoop on the
      first release, the Homebrew tap, Flathub).
- [ ] Any release candidate pre-releases and tags are deleted.

## Repository settings required

- Actions: *Allow GitHub Actions to create and approve pull requests* enabled.
- Branch protection on `main`: pull request required, required checks `Linux (GCC)`,
  `Windows (MSVC)`, `macOS (Clang)`, `Code formatting`, `Documentation`.
- Pages: source *GitHub Actions*; repository variable `DEPLOY_DOCS=true` once public.
