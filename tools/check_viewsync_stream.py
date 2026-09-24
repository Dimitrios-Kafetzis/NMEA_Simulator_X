#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Validate a stream of ViewSync packets, the camera updates Google Earth accepts.

Reads one packet per line from standard input and checks it against the layout on the
ViewSync reference page (docs/reference/viewsync.md): ten comma-separated fields, which are
a counter, latitude, longitude, altitude, heading, tilt, roll, start time, end time and
planet. A packet fails when it does not have ten fields or its numeric fields do not parse,
when the counter is not one more than the previous packet's, when the latitude is outside
[-90, 90] or the longitude outside [-180, 180] degrees, when the heading is outside [0, 360)
or the tilt outside [0, 90] degrees, when the start and end times, in seconds since
0001-01-01T00:00:00Z, differ or lie before the Unix epoch, and when the planet is not empty,
`sky`, `mars` or `moon`. The altitude and the roll are only parsed.

The script prints one line per problem and a summary to standard output, and writes no
files. Blank lines are skipped. It takes no arguments.

Usage:
    nmeasim run --stdout --quiet --duration 2 --encoding viewsync \
        | python3 tools/check_viewsync_stream.py

Exit status:
    0 when every packet passes, 1 when a packet fails or no packet was read.
"""
import sys

#: Seconds from 0001-01-01T00:00:00Z to the Unix epoch, the offset between Unix time and the
#: time Google Earth counts from the year 1. A smaller time points to a sender that wrote Unix
#: time without the offset.
SECONDS_BEFORE_UNIX_EPOCH = 62135596800


def main() -> int:
    """Check the packets read from standard input and print the summary.

    Returns:
        The exit status: 0 when the stream passes, 1 when it fails.
    """
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
        # Continue from the packet received, so that one gap is reported once, not for every
        # packet after it.
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
