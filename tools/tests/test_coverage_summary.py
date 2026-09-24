# SPDX-License-Identifier: GPL-3.0-only
"""Test `coverage_summary.py` on small gcovr JSON summaries written by the tests.

Each test writes a summary to a temporary directory, runs the script on it as CI does, as a
separate Python process, and checks its exit status and report. Reads no fixtures.

Usage:
    python3 -m unittest discover -s tools/tests -v
"""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

#: Root of the repository, two levels above this file.
ROOT = Path(__file__).resolve().parents[2]


def file_entry(filename: str, lines: int, covered: int) -> dict:
    """Return one file of a gcovr JSON summary.

    Args:
        filename: The file name relative to the gcovr root.
        lines: Number of instrumented lines, also used as the number of functions.
        covered: Number of lines, and of functions, executed at least once.

    Returns:
        The entry with the four counts that `coverage_summary.py` reads.
    """
    return {"filename": filename, "line_total": lines, "line_covered": covered,
            "function_total": lines, "function_covered": covered}


class CoverageSummaryTest(unittest.TestCase):
    """Tests of `coverage_summary.py`."""

    def summarise(self, files: list[dict]) -> subprocess.CompletedProcess:
        """Write a summary holding some files and run the script on it.

        Args:
            files: The `files` list of the summary.

        Returns:
            The finished process, with `returncode`, `stdout` and `stderr` as text.
        """
        with tempfile.TemporaryDirectory() as directory:
            summary = Path(directory) / "coverage.json"
            summary.write_text(json.dumps({"files": files}), encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(ROOT / "tools" / "coverage_summary.py"), str(summary)],
                capture_output=True, text=True, encoding="utf-8", check=False)

    def test_core_above_the_target_passes(self) -> None:
        """Pass when the lines of `src/core` are covered at 95 percent."""
        result = self.summarise([file_entry("src/core/src/a.cpp", 100, 95),
                                 file_entry("src/io/src/b.cpp", 10, 1)])
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("95.0 % meets", result.stdout)

    def test_core_below_the_target_fails(self) -> None:
        """Fail when the lines of `src/core` are covered at 50 percent."""
        result = self.summarise([file_entry("src/core/src/a.cpp", 100, 50)])
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("50.0 % is below", result.stdout)

    def test_summary_without_core_files_fails(self) -> None:
        """Fail, instead of reporting 100 percent, when no file lies under `src/core`."""
        for files in ([], [file_entry("src/io/src/b.cpp", 10, 10)],
                      [file_entry("src/core/src/empty.cpp", 0, 0)]):
            with self.subTest(files=files):
                result = self.summarise(files)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(result.returncode, 1, result.stdout)
                self.assertIn("no instrumented line under `src/core`", result.stdout)


if __name__ == "__main__":
    unittest.main()
