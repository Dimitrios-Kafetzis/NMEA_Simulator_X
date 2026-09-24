#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that every source file of the repository starts with the licence identifier.

The coding standard (docs/development/coding-standards.md, "Documentation comments") requires
`SPDX-License-Identifier: GPL-3.0-only` as the first line of every C++, Python, CMake,
workflow and shell file, after the shebang line of an executable script. The files are the
ones git tracks, so the check needs a git checkout. The documentation build checks the C++
file comments themselves; this script covers every file type.

Run it from anywhere in the repository:

    python3 tools/check_file_headers.py

Exit status: 0 when every file carries the identifier, 1 otherwise, with one line per file
that does not.
"""

import subprocess
import sys
from pathlib import Path

#: The licence identifier every checked file starts with, after its comment marker.
IDENTIFIER = "SPDX-License-Identifier: GPL-3.0-only"

#: Tracked files to check, as git pathspecs, and the comment marker each file type uses.
PATTERNS = {
    "*.hpp": "//",
    "*.cpp": "//",
    "*.hpp.in": "//",
    "*.cpp.in": "//",
    "*.py": "#",
    "*.cmake": "#",
    "CMakeLists.txt": "#",
    "**/CMakeLists.txt": "#",
    ".github/workflows/*.yml": "#",
    ".github/versions.env": "#",
    "packaging/linux/AppRun": "#",
}


def tracked_files(root: Path, pattern: str) -> list[str]:
    """Return the tracked files that match a git pathspec.

    Args:
        root: Repository root.
        pattern: A git pathspec such as `*.py`.

    Returns:
        Repository-relative paths, sorted.
    """
    result = subprocess.run(["git", "ls-files", "--", pattern], cwd=root, check=True,
                            capture_output=True, text=True)
    return sorted(result.stdout.split())


def header_ok(text: str, marker: str) -> bool:
    """Return whether a file's text starts with the licence identifier.

    Args:
        text: Contents of the file.
        marker: Comment marker of the file type, `//` or `#`.

    Returns:
        True when the first line, or the second after a shebang line, is the marker followed
        by a space and the identifier.
    """
    lines = text.splitlines()
    if lines and lines[0].startswith("#!"):
        lines = lines[1:]
    return bool(lines) and lines[0] == f"{marker} {IDENTIFIER}"


def main() -> int:
    """Check every tracked source file and report those without the identifier.

    Returns:
        The process exit status: 0 when every file is correct, 1 otherwise.
    """
    root = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"], check=True,
                               capture_output=True, text=True).stdout.strip())
    checked: dict[str, str] = {}
    for pattern, marker in PATTERNS.items():
        for path in tracked_files(root, pattern):
            checked.setdefault(path, marker)
    problems = [path for path, marker in sorted(checked.items())
                if not header_ok((root / path).read_text(encoding="utf-8"), marker)]
    for path in problems:
        print(f"{path}:1: the file must start with `{checked[path]} {IDENTIFIER}`")
    print(f"{len(checked)} files checked, {len(problems)} without the licence identifier")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
