#!/usr/bin/env python3
"""Validate a ViewSync packet stream: ten comma-separated fields per line, a counter that
increases by one, coordinates in range and times counted from the year 1.

Usage:
    nmeasim run --stdout --encoding viewsync --quiet --duration 2 | python3 tools/check_viewsync_stream.py
"""
import sys

SECONDS_BEFORE_UNIX_EPOCH = 62135596800


def main() -> int:
    packets = 0
    failures = 0
    expected_counter = None
    for raw in sys.stdin.buffer:
        line = raw.decode("ascii", errors="replace").strip()
        if not line:
            continue
        packets += 1
        fields = line.split(",")
        if len(fields) != 10:
            print(f"expected 10 fields, got {len(fields)}: {line}")
            failures += 1
            continue
        try:
            counter = int(fields[0])
            latitude, longitude, altitude, heading, tilt, roll = (float(f) for f in fields[1:7])
            start, end = int(fields[7]), int(fields[8])
        except ValueError as error:
            print(f"bad number: {error}: {line}")
            failures += 1
            continue
        if expected_counter is not None and counter != expected_counter:
            print(f"counter {counter}, expected {expected_counter}")
            failures += 1
        expected_counter = counter + 1
        if not (-90 <= latitude <= 90 and -180 <= longitude <= 180):
            print(f"position out of range: {line}")
            failures += 1
        if not (0 <= heading < 360) or not (0 <= tilt <= 90):
            print(f"heading or tilt out of range: {line}")
            failures += 1
        if start != end or start < SECONDS_BEFORE_UNIX_EPOCH:
            print(f"times must be equal and counted from year 1: {line}")
            failures += 1
        if fields[9] not in ("", "sky", "mars", "moon"):
            print(f"unknown planet {fields[9]!r}")
            failures += 1
    print(f"checked {packets} packets, {failures} failures")
    return 1 if failures or packets == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
