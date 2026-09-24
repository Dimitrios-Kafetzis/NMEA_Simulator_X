#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Fill in the package-manager manifests for a release.

Reads the templates under packaging/manifests/ (winget, Scoop, Homebrew) and replaces their
`@NAME@` placeholders with the version, the release tag, the release date and the SHA-256
checksums of the Windows installer, the Windows portable ZIP and the two macOS disk images;
the `# Template:` comment lines are dropped. It also derives the Flathub manifest from
packaging/flatpak/, replacing the source that builds the working tree with a git source at
the release tag and its commit. The files are written below `--output` (build/manifests in
the repository by default) in the layout of the target repositories:

    winget/manifests/d/DimitriosKafetzis/NMEASimulatorX/<version>/*.yaml
    scoop/bucket/nmeasimulatorx.json
    homebrew/Casks/nmeasimulatorx.rb
    flathub/io.github.dimitrios_kafetzis.NMEASimulatorX.yml

Without `--checksums` the SHA256SUMS.txt of the GitHub release is downloaded; without
`--commit` the commit is read from the local tag, which must have been fetched. The release
workflow runs the script with every argument; a maintainer can run it with the tag alone:

    python3 source/packaging/manifests/update_manifests.py --tag "$tag" \
        --checksums dist/SHA256SUMS.txt --commit "$COMMIT" --output manifests
    python3 packaging/manifests/update_manifests.py --tag v1.0.0 --output manifests

Only the Python standard library is used. The how-to docs/how-to/submit-package-manifests.md
explains how each file is submitted. The script prints the path of every file it writes.

Exit status:
    0 when every manifest was written; 1 when the checksum file lacks a package or the commit
    is not a full SHA-1 hash, and when the download, git or a template fails, in which case
    a traceback is printed; 2 for invalid arguments, including a tag not of the form
    `vX.Y.Z` with an optional pre-release suffix and a date not of the form `YYYY-MM-DD`.
"""

import argparse
import datetime
import pathlib
import re
import subprocess
import sys
import urllib.request

#: Root of the repository.
ROOT = pathlib.Path(__file__).resolve().parents[2]
#: Directory of the winget, Scoop and Homebrew templates.
TEMPLATES = ROOT / "packaging" / "manifests"
#: Flatpak manifest that builds the working tree, from which the Flathub manifest is derived.
FLATPAK_MANIFEST = ROOT / "packaging" / "flatpak" / "io.github.dimitrios_kafetzis.NMEASimulatorX.yml"
#: GitHub repository that publishes the releases, as owner/name.
REPOSITORY = "Dimitrios-Kafetzis/NMEA_Simulator_X"
#: winget package identifier, which also names the winget manifest files.
WINGET_ID = "DimitriosKafetzis.NMEASimulatorX"

#: Package file name of every checksum placeholder, with `{version}` standing for the version.
PACKAGES = {
    "SHA256_WIN64_EXE": "NMEASimulatorX-{version}-win64.exe",
    "SHA256_WIN64_ZIP": "NMEASimulatorX-{version}-win64-portable.zip",
    "SHA256_MACOS_ARM64": "NMEASimulatorX-{version}-macos-arm64.dmg",
    "SHA256_MACOS_X86_64": "NMEASimulatorX-{version}-macos-x86_64.dmg",
}

#: Release tag: `v` and a semantic version with an optional pre-release suffix, captured
#: without the `v`.
TAG_PATTERN = re.compile(r"^v(\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+)?)$")
#: A `- type: dir` source of the Flatpak manifest with its more deeply indented keys.
DIR_SOURCE = re.compile(r"^(?P<indent> *)- type: dir\n(?:(?P=indent)  .*\n)+", re.MULTILINE)


def read_checksums(text: str) -> dict[str, str]:
    """Parse the checksum list written by `sha256sum`.

    Args:
        text: Lines of a hexadecimal digest, white space and a file name, which may carry the
            `*` that marks binary mode. Blank lines are skipped.

    Returns:
        The lower-case digest of every file, by file name.

    Raises:
        ValueError: A non-blank line has no file name after the digest.
    """
    checksums = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        digest, name = line.split(maxsplit=1)
        checksums[name.lstrip("*").strip()] = digest.lower()
    return checksums


def download_checksums(tag: str) -> str:
    """Download the checksum list attached to a GitHub release.

    Args:
        tag: The release tag, such as `v1.0.0`.

    Returns:
        The content of the release's SHA256SUMS.txt.

    Raises:
        urllib.error.URLError: The download failed or timed out after 60 seconds; an
            `HTTPError` when the release or the file does not exist.
    """
    url = f"https://github.com/{REPOSITORY}/releases/download/{tag}/SHA256SUMS.txt"
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read().decode("utf-8")


def tag_commit(tag: str) -> str:
    """Return the commit a local git tag points to.

    Args:
        tag: The tag name, such as `v1.0.0`.

    Returns:
        The full SHA-1 hash of the commit, dereferencing an annotated tag.

    Raises:
        subprocess.CalledProcessError: git failed, for example because the tag is unknown.
    """
    result = subprocess.run(["git", "rev-list", "-n", "1", tag], cwd=ROOT, capture_output=True,
                            text=True, check=True)
    return result.stdout.strip()


def fill(template: str, values: dict[str, str]) -> str:
    """Replace the placeholders of a template and drop its template comments.

    Args:
        template: Text with `@NAME@` placeholders, where NAME consists of upper-case letters,
            digits and underscores.
        values: The replacement text of every placeholder, by NAME.

    Returns:
        The filled text without the lines that start with `# Template:`, ending in a newline.

    Raises:
        KeyError: The template contains a placeholder that `values` lacks.
    """

    def replace(match: re.Match) -> str:
        """Return the value of the placeholder a match found.

        Args:
            match: A match of `@NAME@` whose first group is NAME.

        Returns:
            The value of NAME.

        Raises:
            KeyError: `values` has no NAME.
        """
        key = match.group(1)
        if key not in values:
            raise KeyError(f"no value for placeholder @{key}@")
        return values[key]

    filled = re.sub(r"@([A-Z0-9_]+)@", replace, template)
    return "\n".join(line for line in filled.splitlines()
                     if not line.startswith("# Template:")) + "\n"


def flathub_manifest(tag: str, commit: str) -> str:
    """Derive the Flathub manifest of a release from the Flatpak manifest.

    The comment block before the `id:` key, which explains how to build the working tree, is
    replaced by a two-line header naming the release, and the first `- type: dir` source is
    replaced by a git source at the same indentation.

    Args:
        tag: The release tag, such as `v1.0.0`.
        commit: The full SHA-1 hash of the commit the tag points to.

    Returns:
        The text of the Flathub manifest.

    Raises:
        ValueError: The Flatpak manifest has no `- type: dir` source or no top-level `id:`
            key.
    """
    text = FLATPAK_MANIFEST.read_text(encoding="utf-8")
    match = DIR_SOURCE.search(text)
    if match is None:
        raise ValueError(f"no '- type: dir' source in {FLATPAK_MANIFEST}")
    indent = match.group("indent")
    git_source = (f"{indent}- type: git\n"
                  f"{indent}  url: https://github.com/{REPOSITORY}.git\n"
                  f"{indent}  tag: {tag}\n"
                  f"{indent}  commit: {commit}\n")
    # The leading comments explain how to build the working tree, which does not apply to
    # the Flathub copy.
    header_end = text.index("\nid:")
    header = ("# Flathub manifest of NMEA Simulator X " + tag + ", generated by\n"
              "# packaging/manifests/update_manifests.py from packaging/flatpak/.\n")
    return header + text[header_end + 1:match.start()] + git_source + text[match.end():]


def main() -> int:
    """Fill in every manifest of the release given on the command line.

    Returns:
        The exit status: 0 when every file was written, 1 when a checksum is missing or the
        commit is not a full SHA-1 hash.

    Raises:
        OSError: The checksum file or a template cannot be read, or an output cannot be
            written.
        urllib.error.URLError: The checksum list cannot be downloaded.
        subprocess.CalledProcessError: The tag's commit cannot be read from git.
        KeyError: A template contains a placeholder without a value.
        ValueError: The Flatpak manifest has no directory source to replace.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tag", required=True, help="release tag, e.g. v1.0.0")
    parser.add_argument("--checksums", type=pathlib.Path,
                        help="SHA256SUMS.txt of the release (downloaded when omitted)")
    parser.add_argument("--commit", help="commit of the tag (read from git when omitted)")
    parser.add_argument("--date", default=datetime.datetime.now(datetime.timezone.utc).date()
                        .isoformat(), help="release date, YYYY-MM-DD (default: today, UTC)")
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "build" / "manifests")
    args = parser.parse_args()

    match = TAG_PATTERN.match(args.tag)
    if match is None:
        parser.error(f"tag {args.tag} is not of the form vX.Y.Z")
    version = match.group(1)
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", args.date):
        parser.error(f"date {args.date} is not of the form YYYY-MM-DD")

    text = (args.checksums.read_text(encoding="utf-8") if args.checksums
            else download_checksums(args.tag))
    checksums = read_checksums(text)
    values = {"VERSION": version, "TAG": args.tag, "RELEASE_DATE": args.date}
    missing = []
    for key, pattern in PACKAGES.items():
        name = pattern.format(version=version)
        if name in checksums:
            values[key] = checksums[name]
        else:
            missing.append(name)
    if missing:
        print("SHA256SUMS.txt lacks: " + ", ".join(missing), file=sys.stderr)
        return 1
    # winget writes installer hashes in upper case.
    values["SHA256_WIN64_EXE"] = values["SHA256_WIN64_EXE"].upper()
    commit = args.commit or tag_commit(args.tag)
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        print(f"commit {commit!r} is not a full SHA-1", file=sys.stderr)
        return 1

    outputs = {
        pathlib.Path("scoop/bucket/nmeasimulatorx.json"): TEMPLATES / "scoop/nmeasimulatorx.json",
        pathlib.Path("homebrew/Casks/nmeasimulatorx.rb"): TEMPLATES / "homebrew/nmeasimulatorx.rb",
    }
    winget_dir = pathlib.Path("winget/manifests/d/DimitriosKafetzis/NMEASimulatorX") / version
    for suffix in ("", ".installer", ".locale.en-US"):
        name = f"{WINGET_ID}{suffix}.yaml"
        outputs[winget_dir / name] = TEMPLATES / "winget" / name

    for relative, template in outputs.items():
        target = args.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(fill(template.read_text(encoding="utf-8"), values), encoding="utf-8")
        print(target)
    target = args.output / "flathub" / FLATPAK_MANIFEST.name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(flathub_manifest(args.tag, commit), encoding="utf-8")
    print(target)
    return 0


if __name__ == "__main__":
    sys.exit(main())
