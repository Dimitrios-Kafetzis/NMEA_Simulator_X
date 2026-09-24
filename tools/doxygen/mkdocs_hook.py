# SPDX-License-Identifier: GPL-3.0-only
"""MkDocs hook that generates the C++ reference from the source comments (ADR 0017).

When MkDocs loads its configuration, the hook runs Doxygen with `tools/doxygen/Doxyfile`,
checks its warnings, turns the XML into Markdown pages with `cppreference.py` and replaces
the *C++ API* entry of the navigation with the generated subtree. The pages are added to the
build as generated files, so MkDocs renders, themes, links and indexes them like the
hand-written ones. After the build, the hook writes a redirect at every page the former
Doxygen HTML reference published under `api/`.

Doxygen treats every warning as an error (`WARN_AS_ERROR = FAIL_ON_WARNINGS`): an undocumented
entity, parameter, return value or enumerator, or a malformed comment, fails the build, and the
hook lists every warning with its file and line. So does a C++ file under `src/` or `tests/`
without the licence line or a file comment.

Without Doxygen the reference is replaced by placeholder pages, unless
`NMEASIM_REQUIRE_DOXYGEN=1`, which CI sets. Set `DOXYGEN` to use a Doxygen binary that is not
in `PATH`.
"""

from __future__ import annotations

import logging
import os
import posixpath
import re
import shutil
import subprocess
import sys
from pathlib import Path

from mkdocs.exceptions import PluginError
from mkdocs.structure.files import File

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cppreference  # noqa: E402  (the hook directory is only importable after the line above)

log = logging.getLogger("mkdocs.hooks.cppreference")

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "build" / "doxygen"
WARNINGS = OUTPUT / "warnings.log"
LEGACY_PAGES = Path(__file__).resolve().parent / "legacy-api-pages.txt"

#: Directories whose C++ files must start with the licence line and a file comment.
SOURCE_DIRECTORIES = ("src", "tests")

#: First line of every C++ file.
LICENCE_LINE = "// SPDX-License-Identifier: GPL-3.0-only"

PLACEHOLDER = """# {title}

This build of the documentation was made without Doxygen, so the generated C++ reference is
missing. Install Doxygen 1.18 or later and build again, or read the
[published reference](https://dimitrios-kafetzis.github.io/NMEA_Simulator_X/reference/api/).
"""

_reference: cppreference.Reference | None = None


def project_version() -> str:
    """Return the project version from the top-level `CMakeLists.txt`."""
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r'set\(NMEASIM_VERSION "([^"]+)"\)', text)
    return match.group(1) if match else ""


def git_ref() -> str:
    """Return the commit being built, which source links point at; `main` outside git."""
    sha = os.environ.get("GITHUB_SHA")
    if sha:
        return sha
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True,
                            text=True, check=False)
    return result.stdout.strip() if result.returncode == 0 else "main"


def run_doxygen(doxygen: str) -> None:
    """Run Doxygen with the reference configuration.

    Args:
        doxygen: Path of the Doxygen executable.

    Raises:
        PluginError: Doxygen reported warnings, which are errors, or failed otherwise. The
            message lists every warning with its file and line.
    """
    environment = dict(os.environ, NMEASIM_VERSION=project_version())
    result = subprocess.run([doxygen, "tools/doxygen/Doxyfile"], cwd=ROOT, env=environment,
                            capture_output=True, text=True, check=False)
    warnings = [text for _, text in read_warnings(WARNINGS)]
    if warnings:
        raise PluginError("Documentation errors reported by Doxygen:\n" + "\n".join(warnings))
    if result.returncode != 0:
        raise PluginError("Doxygen failed:\n" + (result.stderr or result.stdout).strip())


def read_warnings(path: Path) -> list[tuple[str, str]]:
    """Read a Doxygen warning log.

    Args:
        path: The log written through `WARN_LOGFILE`.

    Returns:
        Pairs of the repository path a warning refers to and its full text, which may span
        several lines.
    """
    warnings: list[tuple[str, str]] = []
    if not path.is_file():
        return warnings
    prefix = f"{ROOT.as_posix()}/"
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^(/?[^:\s]+):\d+: ", line)
        if match:
            file = match.group(1).removeprefix(prefix)
            warnings.append((file, line.replace(prefix, "")))
        elif warnings and line.strip():
            file, text = warnings[-1]
            warnings[-1] = (file, f"{text}\n{line}")
    return warnings


def check_file_headers(reference_files: list[str]) -> list[str]:
    """Return the C++ files under `src/` and `tests/` that lack the licence line or a file comment.

    Args:
        reference_files: Repository paths of the files Doxygen documented with a comment.
    """
    problems = []
    documented = set(reference_files)
    for directory in SOURCE_DIRECTORIES:
        for path in sorted((ROOT / directory).rglob("*")):
            relative = path.relative_to(ROOT).as_posix()
            if not relative.endswith((".hpp", ".cpp", ".hpp.in", ".cpp.in")):
                continue
            first = path.read_text(encoding="utf-8").split("\n", 1)[0]
            if first != LICENCE_LINE:
                problems.append(f"{relative}:1: the first line must be `{LICENCE_LINE}`")
            if relative not in documented:
                problems.append(f"{relative}:1: no `/// @file` comment with a brief description")
    return problems


def generate(config) -> cppreference.Reference | None:
    """Run Doxygen, check the documentation and generate the reference.

    Returns:
        The generated reference, or None when Doxygen is not available and not required.

    Raises:
        PluginError: Doxygen is required but missing, failed or reported warnings, or a C++
            file lacks its licence line or file comment.
    """
    doxygen = shutil.which(os.environ.get("DOXYGEN", "doxygen"))
    if doxygen is None:
        if os.environ.get("NMEASIM_REQUIRE_DOXYGEN") == "1":
            raise PluginError("Doxygen is required (NMEASIM_REQUIRE_DOXYGEN=1) but not found")
        log.info("Doxygen not found; the C++ reference is replaced by placeholder pages")
        return None
    shutil.rmtree(OUTPUT, ignore_errors=True)
    OUTPUT.mkdir(parents=True)

    run_doxygen(doxygen)
    reference = cppreference.build_reference(OUTPUT / "xml", ROOT,
                                             config["repo_url"].rstrip("/"), git_ref())
    errors = check_file_headers(reference.documented_files)
    if errors:
        raise PluginError("Documentation errors:\n" + "\n".join(errors))
    return reference


def placeholder() -> cppreference.Reference:
    """Return a reference made of placeholder pages, for builds without Doxygen."""
    pages = {part.page: PLACEHOLDER.format(title=part.title) for part in cppreference.PARTS}
    nav = [{"Overview": cppreference.OVERVIEW_PAGE}]
    nav += [{part.title: part.page} for part in cppreference.PARTS]
    return cppreference.Reference(pages, nav, {}, [], {})


def replace_nav(nav: list, reference_nav: list) -> bool:
    """Replace the navigation entry of the overview page with the generated subtree.

    Args:
        nav: The `nav` configuration, modified in place.
        reference_nav: The generated entries.

    Returns:
        Whether the overview page was found.
    """
    for index, entry in enumerate(nav):
        if isinstance(entry, dict):
            for title, value in entry.items():
                if value == cppreference.OVERVIEW_PAGE:
                    nav[index] = {title: reference_nav}
                    return True
                if isinstance(value, list) and replace_nav(value, reference_nav):
                    return True
    return False


def on_config(config):
    """Generate the reference once per MkDocs process and insert its navigation."""
    global _reference
    if _reference is None:
        _reference = generate(config) or placeholder()
    if not replace_nav(config["nav"], _reference.nav):
        raise PluginError(f"{cppreference.OVERVIEW_PAGE} is not in the navigation")
    return config


def on_files(files, config):
    """Add the generated pages, whose "Edit" links open the source file on GitHub."""
    for path, content in _reference.pages.items():
        file = File.generated(config, path, content=content)
        source = _reference.edit_sources.get(path)
        # MkDocs joins this to edit_uri, which ends in docs/; one level up is the repository.
        file.edit_uri = f"../{source}" if source else None
        files.append(file)
    return files


def redirect_target(page: str, pages: dict[str, str]) -> str:
    """Return the new page for a page of the former Doxygen reference.

    Args:
        page: Path of the former page below `api/`, such as `classnmeasim_1_1io_1_1Profile.html`.
        pages: Page of every compound of the new reference, by Doxygen id.

    Returns:
        The path of the new page, relative to the documentation root.
    """
    stem = page.removesuffix(".html").removesuffix("-members")
    if stem in pages:
        return pages[stem]
    if stem.endswith("_source"):
        header = stem.removesuffix("_source").replace("_8", ".").replace("__", "_")
        matches = sorted(path for path in pages.values()
                         if path.endswith(f"/{header}.md") and "/include/" in path)
        if len(matches) == 1:
            return matches[0]
    if stem == "index":
        return cppreference.OVERVIEW_PAGE
    return cppreference.PART_BY_KEY["library"].page


def on_post_build(config):
    """Write a redirect to the new reference at every page the former one published."""
    site = Path(config["site_dir"])
    names = [line for line in LEGACY_PAGES.read_text(encoding="utf-8").splitlines()
             if line and not line.startswith("#")]
    for name in names:
        target = redirect_target(name, _reference.compound_pages)
        url = posixpath.relpath(posixpath.dirname(target) if target.endswith("/index.md")
                                else target.removesuffix(".md"), "api") + "/"
        html = ('<!DOCTYPE html>\n<html lang="en"><head><meta charset="utf-8">'
                f'<title>Redirecting</title><link rel="canonical" href="{url}">'
                f'<meta http-equiv="refresh" content="0; url={url}"></head>'
                f'<body><p>The C++ reference has moved to <a href="{url}">{url}</a>.</p>'
                "</body></html>\n")
        path = site / "api" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(html, encoding="utf-8")
