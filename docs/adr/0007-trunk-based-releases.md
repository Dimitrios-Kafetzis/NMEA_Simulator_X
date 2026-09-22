# 0007 Trunk-based development, Conventional Commits and automated releases

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

The project needs a branching model, a commit convention and a release mechanism that scale
from a single maintainer to outside contributors without manual bookkeeping.

## Decision drivers

- `main` must always be releasable.
- Changelog and version numbers should be derived, not typed.
- Contributors should be able to follow the rules from a single page.

## Considered options

1. Git Flow with long-lived `develop` and release branches.
2. Trunk-based development with short-lived branches, squash merges, Conventional Commits
   and Release Please.

## Decision outcome

Option 2.

- **Branching:** short-lived feature branches off `main`, pull requests required, squash merge,
  branch protection with required CI checks.
- **Commits:** [Conventional Commits](https://www.conventionalcommits.org/) with scopes
  `core`, `io`, `cli`, `app`, `docs`, `build`, `ci`. The squash-merged pull request title is the
  commit message.
- **Versioning:** [Semantic Versioning](https://semver.org/). Before 1.0, `feat` bumps the
  minor version and `fix` bumps the patch version.
- **Releases:** Release Please maintains a release pull request from the commit history.
  Merging it creates the tag and GitHub release; the release workflow then builds and attaches
  the packages for every platform.

Git Flow was rejected as unnecessary ceremony for a project without parallel maintenance
lines.

### Consequences

- The version lives in `CMakeLists.txt` and `vcpkg.json`, both updated by Release Please.
- Repository settings must allow Actions to create pull requests.
- Hotfixes are ordinary pull requests to `main` followed by a release.
