# SPDX-License-Identifier: GPL-3.0-only
"""Test the stream checkers of `tools/` on valid and malformed streams.

Covers `check_nmea_stream.py`, `check_ais_stream.py` and `check_signalk_stream.py`. Each test
runs a checker as CI does, as a separate Python process reading the stream from standard
input, and checks its exit status and output: a malformed stream must end in exit status 1
with the problem named on standard output, never in a traceback or in a pass. The valid
streams are the fixtures `tests/fixtures/ais/own_vessel.nmea` and
`tests/fixtures/signalk/delta.jsonl`; the malformed ones are built from them in the tests,
with checksums computed by `with_checksum`. Needs pynmea2 and pyais, like the checkers.

Usage:
    python3 -m unittest discover -s tools/tests -v
"""
import json
import subprocess
import sys
import unittest
from pathlib import Path

#: Root of the repository, two levels above this file.
ROOT = Path(__file__).resolve().parents[2]
#: A valid AIS stream: a position report and a two-sentence static data report.
AIS_FIXTURE = (ROOT / "tests" / "fixtures" / "ais" / "own_vessel.nmea").read_text("ascii")
#: A valid Signal K stream of one delta.
SIGNALK_FIXTURE = (ROOT / "tests" / "fixtures" / "signalk" / "delta.jsonl").read_text("utf-8")


def with_checksum(delimiter: str, body: str) -> str:
    """Return a sentence with its NMEA 0183 checksum and a line terminator.

    Args:
        delimiter: The start delimiter, `$` or `!`.
        body: The characters between the delimiter and the `*`.

    Returns:
        `<delimiter><body>*<checksum>` followed by CR LF, where the checksum is the XOR of
        the ASCII codes of `body` in two upper-case hexadecimal digits.
    """
    checksum = 0
    for byte in body.encode("ascii"):
        checksum ^= byte
    return f"{delimiter}{body}*{checksum:02X}\r\n"


def run_tool(script: str, stream: str, *arguments: str) -> subprocess.CompletedProcess:
    """Run a script of `tools/` on a stream and capture what it prints.

    Args:
        script: File name of the script in `tools/`.
        stream: Text written to the script's standard input.
        *arguments: Command-line arguments of the script.

    Returns:
        The finished process, with `returncode`, `stdout` and `stderr` as text.
    """
    # The stream is written as bytes, so that no platform translates its line terminators.
    result = subprocess.run([sys.executable, str(ROOT / "tools" / script), *arguments],
                            input=stream.encode("utf-8"), capture_output=True, check=False,
                            cwd=ROOT)
    return subprocess.CompletedProcess(result.args, result.returncode,
                                       result.stdout.decode("utf-8", errors="replace"),
                                       result.stderr.decode("utf-8", errors="replace"))


class CheckerTestCase(unittest.TestCase):
    """Base class with the assertion shared by the checker tests."""

    def assert_failure(self, result: subprocess.CompletedProcess, reason: str) -> None:
        """Assert that a checker failed cleanly and named the problem.

        Args:
            result: The finished checker process.
            reason: Text that the checker must have printed to standard output.
        """
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn(reason, result.stdout)


class CheckNmeaStreamTest(CheckerTestCase):
    """Tests of `check_nmea_stream.py`."""

    def test_valid_stream_passes(self) -> None:
        """Accept a stream of valid parametric and encapsulated sentences."""
        stream = with_checksum("$", "GPGLL,3758.43,N,02343.61,E,123456.78,A,A") + AIS_FIXTURE
        result = run_tool("check_nmea_stream.py", stream, "--expect", "2")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_too_long_sentence_reports_the_checked_length(self) -> None:
        """Report the length measured against the limit, CR LF included, not the raw line."""
        body = "GPTXT,01,01,02," + "X" * 62
        # 81 characters without the terminator, 83 with CR LF: one too many.
        line = with_checksum("$", body).rstrip("\r\n") + "\n"
        self.assertEqual(len(line), 82)
        result = run_tool("check_nmea_stream.py", line)
        self.assert_failure(result, "too long (83 characters with CR LF)")


class CheckAisStreamTest(CheckerTestCase):
    """Tests of `check_ais_stream.py`."""

    def test_valid_stream_passes(self) -> None:
        """Decode the fixture's position report and static data report."""
        result = run_tool("check_ais_stream.py", AIS_FIXTURE, "--mmsi", "239000001")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("decoded 2 AIS messages", result.stdout)

    def test_bad_checksum_is_a_failure(self) -> None:
        """Count a sentence whose checksum does not match as a failure."""
        position = AIS_FIXTURE.splitlines()[0]
        self.assertTrue(position.endswith("*34"))
        result = run_tool("check_ais_stream.py", AIS_FIXTURE + position[:-2] + "35\n")
        self.assert_failure(result, "bad checksum")

    def test_sentence_with_too_few_fields_is_a_failure(self) -> None:
        """Count a VDM sentence that ends after its fragment count as a failure."""
        result = run_tool("check_ais_stream.py", AIS_FIXTURE + with_checksum("!", "AIVDM,1"))
        self.assert_failure(result, "malformed sentence")

    def test_fragment_number_that_is_not_a_number_is_a_failure(self) -> None:
        """Count a VDM sentence whose fragment number is not a whole number as a failure."""
        sentence = with_checksum("!", "AIVDM,1,x,,A,13SsIh@vA11dWJ`Eg0R1nAKh0000,0")
        result = run_tool("check_ais_stream.py", AIS_FIXTURE + sentence)
        self.assert_failure(result, "malformed sentence")

    def test_unknown_message_type_is_a_failure(self) -> None:
        """Count a message pyais does not know, here of type 63, as a failure."""
        sentence = with_checksum("!", "AIVDM,1,1,,A,w000000000000000000000000000,0")
        result = run_tool("check_ais_stream.py", AIS_FIXTURE + sentence)
        self.assert_failure(result, "UnknownMessageException")

    def test_surplus_fragment_is_a_failure(self) -> None:
        """Count a multi-sentence message with a repeated fragment as a failure."""
        position, first, second = AIS_FIXTURE.splitlines()
        stream = "\n".join([position, first, first, second]) + "\n"
        result = run_tool("check_ais_stream.py", stream)
        self.assert_failure(result, "TooManyMessagesException")

    def test_unterminated_message_is_a_failure(self) -> None:
        """Count a multi-sentence message whose last fragment never arrives as a failure."""
        first = AIS_FIXTURE.splitlines()[1]
        # The same first fragment under another sequential id, with its checksum recomputed.
        body = first[1:first.index("*")].replace(",2,1,6,", ",2,1,7,")
        result = run_tool("check_ais_stream.py", AIS_FIXTURE + with_checksum("!", body))
        self.assert_failure(result, "unterminated multi-sentence message")


class CheckSignalkStreamTest(CheckerTestCase):
    """Tests of `check_signalk_stream.py`."""

    def delta_with(self, path: str, value) -> str:
        """Return one line holding a valid delta with a single value.

        Args:
            path: The Signal K path of the value.
            value: The value, encoded as JSON.

        Returns:
            The delta as one line of JSON followed by a line feed.
        """
        delta = json.loads(SIGNALK_FIXTURE)
        delta["updates"][0]["values"] = [{"path": path, "value": value}]
        return json.dumps(delta) + "\n"

    def test_valid_stream_passes(self) -> None:
        """Accept the fixture and a position with numeric coordinates."""
        stream = SIGNALK_FIXTURE + self.delta_with(
            "navigation.position", {"latitude": 37.9, "longitude": 23.7})
        result = run_tool("check_signalk_stream.py", stream, "--min-deltas", "2")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_json_that_is_not_an_object_is_a_failure(self) -> None:
        """Count lines that are JSON arrays, numbers or strings as failures."""
        for line in ("[1, 2]", "42", '"text"', "null"):
            with self.subTest(line=line):
                result = run_tool("check_signalk_stream.py", SIGNALK_FIXTURE + line + "\n")
                self.assert_failure(result, "not a JSON object")

    def test_update_and_value_that_are_not_objects_are_failures(self) -> None:
        """Count an update, a values list or a value entry of the wrong JSON type."""
        context = "vessels.urn:mrn:imo:mmsi:239000001"
        update = json.loads(SIGNALK_FIXTURE)["updates"][0]
        cases = {
            "update is not an object": [42],
            "values is not a list": [dict(update, values=7)],
            "value entry is not an object": [dict(update, values=["navigation.position"])],
        }
        for reason, updates in cases.items():
            with self.subTest(reason=reason):
                line = json.dumps({"context": context, "updates": updates}) + "\n"
                result = run_tool("check_signalk_stream.py", line)
                self.assert_failure(result, reason)

    def test_boolean_coordinates_are_a_failure(self) -> None:
        """Reject JSON true and false as latitude and longitude."""
        stream = self.delta_with("navigation.position", {"latitude": True, "longitude": False})
        result = run_tool("check_signalk_stream.py", stream)
        self.assert_failure(result, "wrong value type for 'navigation.position'")


if __name__ == "__main__":
    unittest.main()
