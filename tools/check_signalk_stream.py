#!/usr/bin/env python3
"""Validate a Signal K delta stream against the paths the simulator documents.

Reads one JSON document per line from standard input and checks that every line is a delta
of the documented shape (context, updates with source, timestamp and values), that every
path is one the reference page lists, and that every value has the JSON type the Signal K
specification gives that path. Optionally checks the context and the number of deltas.

Usage:
    nmeasim run --stdout --encoding signalk --quiet --duration 3 | python3 tools/check_signalk_stream.py
"""
import argparse
import json
import re
import sys

NUMBER = (int, float)
POSITION = "position"
TEXT = str

# Path pattern -> expected value kind. Propulsion ids are free-form.
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
TIMESTAMP = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$")


def check_value(kind, value) -> bool:
    if kind is POSITION:
        return (isinstance(value, dict) and isinstance(value.get("latitude"), NUMBER)
                and isinstance(value.get("longitude"), NUMBER))
    if kind is TEXT:
        return isinstance(value, str)
    return isinstance(value, NUMBER) and not isinstance(value, bool)


def check_delta(document, context: str | None) -> list[str]:
    problems = []
    if context is not None and document.get("context") != context:
        problems.append(f"context is {document.get('context')!r}, expected {context!r}")
    elif not isinstance(document.get("context"), str):
        problems.append("missing context")
    updates = document.get("updates")
    if not isinstance(updates, list) or not updates:
        return problems + ["missing updates"]
    for update in updates:
        if not isinstance(update.get("source"), dict) or "label" not in update["source"]:
            problems.append("update without source label")
        if not TIMESTAMP.match(str(update.get("timestamp"))):
            problems.append(f"bad timestamp {update.get('timestamp')!r}")
        for entry in update.get("values", []):
            path, value = entry.get("path"), entry.get("value")
            kinds = [kind for pattern, kind in EXPECTED.items() if re.fullmatch(pattern, str(path))]
            if not kinds:
                problems.append(f"unknown path {path!r}")
            elif not check_value(kinds[0], value):
                problems.append(f"wrong value type for {path!r}: {value!r}")
    return problems


def main() -> int:
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
        for update in document.get("updates", []):
            for entry in update.get("values", []):
                seen.add(entry.get("path"))
    missing = [path for path in args.require_path if path not in seen]
    for path in missing:
        print(f"path never sent: {path}")
    print(f"checked {deltas} deltas with {len(seen)} distinct paths, {failures} failures")
    if failures or missing or deltas < args.min_deltas:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
