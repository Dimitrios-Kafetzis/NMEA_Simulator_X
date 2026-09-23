# Submit the package-manager manifests

This guide is for maintainers. After a release, winget, Scoop, Homebrew and Flathub need a
manifest that names the new version and the checksums of its packages. The files are
generated; submitting them is a pull request to each package manager's repository.

## Get the filled-in manifests

The release workflow generates them for every release. Download the
`package-manager-manifests-X.Y.Z` artifact from the *Release* run of the tag (for a release
made by Release Please, the `packages` job of the *Release Please* run):

```bash
gh run download <run id> --name package-manager-manifests-1.0.0 --dir manifests
```

Or generate them locally from the published release; the script downloads
`SHA256SUMS.txt` from the release and reads the commit from the tag:

```bash
git fetch --tags
python3 packaging/manifests/update_manifests.py --tag v1.0.0 --output manifests
```

The templates live under `packaging/manifests/` and the Flatpak manifest under
`packaging/flatpak/`; change those, never the generated copies. The output mirrors the layout
of the target repositories:

| File | Package manager | Package |
| --- | --- | --- |
| `winget/manifests/d/DimitriosKafetzis/NMEASimulatorX/<version>/*.yaml` | winget | NSIS installer |
| `scoop/bucket/nmeasimulatorx.json` | Scoop, *extras* bucket | Portable ZIP |
| `homebrew/Casks/nmeasimulatorx.rb` | Homebrew tap `dimitrios-kafetzis/tap` | Disk images, arm64 and x86_64 |
| `flathub/io.github.dimitrios_kafetzis.NMEASimulatorX.yml` | Flathub | Built from the tagged source |

## winget

The manifests go into [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).
Unsigned installers are accepted; the pipeline scans them with Microsoft Defender.

1. On Windows, validate and test-install the manifests:

    ```powershell
    winget validate --manifest manifests\winget\manifests\d\DimitriosKafetzis\NMEASimulatorX\1.0.0
    winget settings --enable LocalManifestFiles
    winget install --manifest manifests\winget\manifests\d\DimitriosKafetzis\NMEASimulatorX\1.0.0
    ```

2. Fork `microsoft/winget-pkgs`, copy the version folder to the same path in the fork, commit
   it on a new branch and open a pull request titled `New package: DimitriosKafetzis.NMEASimulatorX version 1.0.0`
   (later: `New version: ...`). Fill in the checklist of the pull request template.
3. The validation bot labels the pull request; a moderator merges it. Users then run
   `winget install DimitriosKafetzis.NMEASimulatorX`.

`wingetcreate submit --token <token> <folder>` does steps 2 and 3 in one command if you prefer.

## Scoop

The manifest is meant for the [ScoopInstaller/Extras](https://github.com/ScoopInstaller/Extras)
bucket, which hosts GUI applications.

1. Test it locally:

    ```powershell
    scoop install .\manifests\scoop\bucket\nmeasimulatorx.json
    nmeasim --version
    scoop uninstall nmeasimulatorx
    ```

2. Fork `ScoopInstaller/Extras`, copy the file to `bucket/nmeasimulatorx.json` and open a pull
   request titled `nmeasimulatorx: Add version 1.0.0`, following the bucket's contribution
   guide.
3. Once merged, later versions need no pull request: the bucket's update bot reads
   `checkver` and `autoupdate` from the manifest, finds new GitHub releases and takes the hash
   from their `SHA256SUMS.txt`.

## Homebrew

Since 1 September 2026 the official `homebrew/cask` repository disables casks whose
applications fail Gatekeeper, and `brew install --no-quarantine` no longer exists. NMEA
Simulator X is signed ad hoc but not notarised ([ADR 0005](../adr/0005-unsigned-distribution.md)),
so it cannot live in `homebrew/cask`. It is published in a tap of its own instead.

1. Once: create the public repository `Dimitrios-Kafetzis/homebrew-tap` with a `Casks/`
   folder and a README that says how to install.
2. Test the cask:

    ```bash
    brew tap-new --no-git dimitrios-kafetzis/local
    cp manifests/homebrew/Casks/nmeasimulatorx.rb "$(brew --repository)/Library/Taps/dimitrios-kafetzis/homebrew-local/Casks/"
    brew audit --cask --online dimitrios-kafetzis/local/nmeasimulatorx
    brew install --cask dimitrios-kafetzis/local/nmeasimulatorx
    brew uninstall --cask nmeasimulatorx && brew untap dimitrios-kafetzis/local
    ```

3. Copy `nmeasimulatorx.rb` to `Casks/` in `homebrew-tap`, commit and push. Users run
   `brew install --cask dimitrios-kafetzis/tap/nmeasimulatorx`; the cask's caveat reminds them
   of the one-time *Open Anyway* step.

## Flathub

A new application is submitted once through [flathub/flathub](https://github.com/flathub/flathub);
after acceptance it gets its own repository, `flathub/io.github.dimitrios_kafetzis.NMEASimulatorX`,
where updates are pull requests.

1. Build and lint the generated manifest locally:

    ```bash
    flatpak install -y flathub org.flatpak.Builder
    flatpak run --command=flathub-build org.flatpak.Builder --install \
        manifests/flathub/io.github.dimitrios_kafetzis.NMEASimulatorX.yml
    flatpak run --command=flatpak-builder-lint org.flatpak.Builder \
        manifest manifests/flathub/io.github.dimitrios_kafetzis.NMEASimulatorX.yml
    flatpak run --command=flatpak-builder-lint org.flatpak.Builder repo repo
    flatpak run io.github.dimitrios_kafetzis.NMEASimulatorX
    ```

2. First submission: fork `flathub/flathub`, create a branch from `new-pr`, add the manifest
   at the top level and open a pull request against `new-pr` titled
   `Add io.github.dimitrios_kafetzis.NMEASimulatorX`. Explain the permissions in the
   description: `--device=all` for serial ports, `--share=network` for the network outputs
   and map tiles, `--filesystem=home` for profiles, tracks and logs opened by path.
3. Reviewers build it with `bot, build`. After acceptance, log in to flathub.org with the
   GitHub account to verify the `io.github.dimitrios_kafetzis` identifier.
4. Updates: replace the manifest in `flathub/io.github.dimitrios_kafetzis.NMEASimulatorX` with
   the newly generated one and open a pull request; Flathub builds x86_64 and aarch64 and
   publishes on merge.

The Flathub build also reads the AppStream metadata from the tagged source; its release list
comes from `CHANGELOG.md`, so it is correct for every tag Release Please creates.
