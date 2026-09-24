#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Validate a Signal K delta stream against the paths the simulator documents.

Reads one JSON document per line from standard input and checks that each is a delta of the
Signal K specification 1.7.0 (https://signalk.org/specification/1.7.0/doc/) in the shape the
simulator sends: a `context` string and a non-empty `updates` list whose entries carry a
`source` object with a `label`, a UTC `timestamp` with milliseconds and a list of `values`.
Every path must be one that docs/reference/signalk.md lists, and every value must have the
JSON type the specification gives that path: a number, a string, or a position object with
numeric `latitude` and `longitude`; a JSON `true` or `false` is not a number. Units and
ranges are not checked. Optional arguments require a context, a minimum number of deltas and
paths that must appear at least once.

The script prints one line per problem, each path that was required but never sent and a
summary to standard output, and writes no files. Blank lines are skipped.

Usage:
    nmeasim run --stdout --quiet --duration 2 --encoding signalk \
        --destination 37.7466,23.4275,AEGINA | python3 tools/check_signalk_stream.py \
        --context vessels.urn:mrn:imo:mmsi:239000001 --require-path navigation.position
    python3 tools/check_signalk_stream.py --min-deltas 1 < tests/fixtures/signalk/delta.jsonl

Exit status:
    0 when every line passes, 1 when a line is not a JSON object, a delta has a problem
    (among them an update or value entry that is not an object), a required path was never
    sent or fewer than `--min-deltas` deltas were read, and 2 for invalid arguments.
"""
import argparse
import json
import re
import sys

#: Value kind of a JSON number, as an `isinstance` class tuple.
NUMBER = (int, float)
#: Value kind of a position object with numeric `latitude` and `longitude`.
POSITION = "position"
#: Value kind of a JSON string.
TEXT = str

#: Value kind of every documented path, by a regular expression that must match the whole
#: path. Propulsion ids are free-form, so any lower-case alphanumeric id is accepted.
EXPECTED = {
    r"navigation\.datetime": TEXT,
    r"navigation\.position": POSITION,
    r"navigation\.courseOverGround(True|Magnetic)": NUMBER,
    r"navigation\.speedOverGround": NUMBER,
    r"navigation\.heading(True|Magnetic)": NUMBER,
    r"navigation\.magnetic(Variation|Deviation)": NUMBER,
    r"navigation\.speedThroughWater": NUMBER,
    r"navigation\.rateOfTurn": NUMBER,
    r"navigation\.gnss\.(type|methodQuality)": TEXT,
    r"navigation\.gnss\.satellites": NUMBER,
    r"navigation\.gnss\.(horizontalDilution|positionDilution|antennaAltitude|geoidalSeparation)": NUMBER,
    r"navigation\.courseRhumbline\.(nextPoint|previousPoint)\.position": POSITION,
    r"navigation\.courseRhumbline\.nextPoint\.(bearingTrue|distance|velocityMadeGood)": NUMBER,
    r"navigation\.courseRhumbline\.(bearingTrackTrue|crossTrackError)": NUMBER,
    r"environment\.depth\.(belowTransducer|surfaceToTransducer|belowSurface|transducerToKeel|belowKeel)": NUMBER,
    r"environment\.water\.temperature": NUMBER,
    r"environment\.wind\.(speedTrue|directionTrue|angleTrueWater|speedApparent|angleApparent)": NUMBER,
    r"steering\.rudderAngle": NUMBER,
    r"propulsion\.[a-z0-9]+\.(revolutions|temperature)": NUMBER,
    r"propulsion\.[a-z0-9]+\.state": TEXT,
}
#: Update timestamp: ISO 8601 UTC with exactly three decimals of a second, the form the
#: simulator writes. The specification itself accepts any ISO 8601 date-time.
TIMESTAMP = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$")


def is_number(value) -> bool:
    """Return whether a decoded JSON value is a number.

    Args:
        value: Any decoded JSON value.

    Returns:
        True for an integer or a floating-point number. JSON true and false decode to bool, a
        subclass of int, and are not numbers in Signal K, so they return False.
    """
    return isinstance(value, NUMBER) and not isinstance(value, bool)


def check_value(kind, value) -> bool:
    """Return whether a delta value has the expected JSON type.

    Args:
        kind: The expected kind: `NUMBER`, `POSITION` or `TEXT`.
        value: The decoded `value` of a delta entry.

    Returns:
        True when the value is a number other than a boolean for `NUMBER`, a string for
        `TEXT`, or an object whose `latitude` and `longitude` are numbers other than booleans
        for `POSITION`.
    """
    if kind is POSITION:
        return (isinstance(value, dict) and is_number(value.get("latitude"))
                and is_number(value.get("longitude")))
    if kind is TEXT:
        return isinstance(value, str)
    return is_number(value)


def check_delta(document, context: str | None) -> list[str]:
    """Check one decoded document against the delta shape and the documented paths.

    Args:
        document: The decoded JSON value of one line.
        context: The context every delta must have, or None to accept any string.

    Returns:
        One human-readable description per problem found; empty when the delta is valid.
        When the document is not an object, or `updates` is missing, empty or not a list, the
        document is not checked further; an update or value entry that is not an object, or
        `values` that is not a list, is one problem and is not checked further.
    """
    if not isinstance(document, dict):
        return [f"not a JSON object: {type(document).__name__}"]
    problems = []
    if context is not None and document.get("context") != context:
        problems.append(f"context is {document.get('context')!r}, expected {context!r}")
    elif not isinstance(document.get("context"), str):
        problems.append("missing context")
    updates = document.get("updates")
    if not isinstance(updates, list) or not updates:
        return problems + ["missing updates"]
    for update in updates:
        if not isinstance(update, dict):
            problems.append(f"update is not an object: {update!r}")
            continue
        if not isinstance(update.get("source"), dict) or "label" not in update["source"]:
            problems.append("update without source label")
        if not TIMESTAMP.match(str(update.get("timestamp"))):
            problems.append(f"bad timestamp {update.get('timestamp')!r}")
        values = update.get("values", [])
        if not isinstance(values, list):
            problems.append(f"values is not a list: {values!r}")
            continue
        for entry in values:
            if not isinstance(entry, dict):
                problems.append(f"value entry is not an object: {entry!r}")
                continue
            path, value = entry.get("path"), entry.get("value")
            kinds = [kind for pattern, kind in EXPECTED.items() if re.fullmatch(pattern, str(path))]
            if not kinds:
                problems.append(f"unknown path {path!r}")
            elif not check_value(kinds[0], value):
                problems.append(f"wrong value type for {path!r}: {value!r}")
    return problems


def sent_paths(document) -> set:
    """Return the paths of the value entries of one decoded document.

    Args:
        document: The decoded JSON value of one line, valid or not.

    Returns:
        The `path` of every value entry that is an object, in every update that is an object
        with a `values` list; empty for a document that is not a delta.
    """
    paths = set()
    updates = document.get("updates") if isinstance(document, dict) else None
    for update in updates if isinstance(updates, list) else []:
        values = update.get("values") if isinstance(update, dict) else None
        for entry in values if isinstance(values, list) else []:
            if isinstance(entry, dict):
                paths.add(entry.get("path"))
    return paths


def main() -> int:
    """Check the deltas read from standard input and print the summary.

    Returns:
        The exit status: 0 when the stream passes, 1 when it fails.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--context", help="expected context of every delta")
    parser.add_argument("--min-deltas", type=int, default=1, help="minimum number of deltas")
    parser.add_argument("--require-path", action="append", default=[],
                        help="a path that must appear in at least one delta (repeatable)")
    args = parser.parse_args()

    deltas = 0
    failures = 0
    seen = set()
    for raw in sys.stdin.buffer:
        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            continue
        try:
            document = json.loads(line)
        except json.JSONDecodeError as error:
            print(f"not JSON: {error}: {line[:80]}")
            failures += 1
            continue
        deltas += 1
        problems = check_delta(document, args.context)
        for problem in problems:
            print(f"delta {deltas}: {problem}")
        failures += len(problems)
        seen |= sent_paths(document)
    missing = [path for path in args.require_path if path not in seen]
    for path in missing:
        print(f"path never sent: {path}")
    print(f"checked {deltas} deltas with {len(seen)} distinct paths, {failures} failures")
    if failures or missing or deltas < args.min_deltas:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
