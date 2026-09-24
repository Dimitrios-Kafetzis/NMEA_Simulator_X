#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Summarise a gcovr JSON summary per library and enforce the core coverage target.

Reads the file that `gcovr --json-summary` writes and adds up the line and function counts
of the files under `src/core`, `src/io`, `src/cli` and `src/app`; files elsewhere are
ignored. Prints a Markdown report to standard output: a table with the line and function
coverage of every library that has files in the summary, a sentence saying whether the line
coverage of `core` meets `--core-target` (90 percent by default, the target of ADR 0008),
and a collapsed list of the `core` files whose line coverage is below `--list-below` (also
90 by default). CI appends the report to the job summary. No file is written.

Usage:
    gcovr --root . --filter src/ --json-summary coverage.json
    python3 tools/coverage_summary.py coverage.json | tee -a "$GITHUB_STEP_SUMMARY"

Exit status:
    0 when the line coverage of `core` meets the target, 1 when it is below, and 2 for
    invalid arguments. A missing or malformed summary file ends the script with a traceback
    and status 1.
"""

import argparse
import json
import sys
from collections import defaultdict

#: Libraries reported, in table order, by their directory under `src/`.
LIBRARIES = ("core", "io", "cli", "app")


def percent(covered: int, total: int) -> float:
    """Return a coverage ratio as a percentage.

    Args:
        covered: Number of lines or functions executed at least once.
        total: Number of instrumented lines or functions.

    Returns:
        The percentage in [0, 100]; 100 when `total` is zero, since nothing is left
        uncovered.
    """
    return 100.0 * covered / total if total else 100.0


def library_of(filename: str) -> str | None:
    """Return the library a source file belongs to.

    Args:
        filename: The file name as gcovr reports it, relative to the gcovr root, with forward
            slashes or, from a Windows build, backslashes.

    Returns:
        The library name from `LIBRARIES` when the file lies below `src/<library>/`, or None
        for any other file.
    """
    parts = filename.replace("\\", "/").split("/")
    if len(parts) > 2 and parts[0] == "src" and parts[1] in LIBRARIES:
        return parts[1]
    return None


def main() -> int:
    """Print the coverage report and compare the line coverage of `core` with the target.

    Returns:
        The exit status: 0 when the target is met, 1 when it is not.

    Raises:
        OSError: The summary file cannot be read.
        json.JSONDecodeError: The summary file is not JSON.
        KeyError: The summary lacks the `files` list or a count gcovr writes for every file.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("summary", help="gcovr --json-summary output")
    parser.add_argument("--core-target", type=float, default=90.0)
    parser.add_argument("--list-below", type=float, default=90.0,
                        help="list core files whose line coverage is below this value")
    args = parser.parse_args()

    with open(args.summary, encoding="utf-8") as handle:
        files = json.load(handle)["files"]

    totals: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    weak: list[tuple[float, str, int, int]] = []
    for entry in files:
        library = library_of(entry["filename"])
        if library is None:
            continue
        for key in ("line_total", "line_covered", "function_total", "function_covered"):
            totals[library][key] += entry[key]
        line_percent = percent(entry["line_covered"], entry["line_total"])
        if library == "core" and line_percent < args.list_below:
            weak.append((line_percent, entry["filename"], entry["line_covered"], entry["line_total"]))

    core = totals["core"]
    core_percent = percent(core["line_covered"], core["line_total"])
    passed = core_percent >= args.core_target

    print("## Coverage\n")
    print("| Library | Lines | Line coverage | Functions | Function coverage |")
    print("| --- | ---: | ---: | ---: | ---: |")
    for library in LIBRARIES:
        if library not in totals:
            continue
        t = totals[library]
        print(f"| `{library}` | {t['line_covered']} / {t['line_total']} "
              f"| {percent(t['line_covered'], t['line_total']):.1f} % "
              f"| {t['function_covered']} / {t['function_total']} "
              f"| {percent(t['function_covered'], t['function_total']):.1f} % |")
    print()
    verdict = "meets" if passed else "is below"
    print(f"`core` line coverage {core_percent:.1f} % {verdict} the {args.core_target:.0f} % "
          "target of ADR 0008.")
    if weak:
        print(f"\n<details><summary>core files below {args.list_below:.0f} %</summary>\n")
        print("| File | Lines | Coverage |")
        print("| --- | ---: | ---: |")
        for line_percent, filename, covered, total in sorted(weak):
            print(f"| `{filename}` | {covered} / {total} | {line_percent:.1f} % |")
        print("\n</details>")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
