#!/usr/bin/env python3
"""Validate the AIS sentences of an NMEA 0183 stream with an independent decoder.

Reads sentences from standard input, decodes every `!..VDO` and `!..VDM` sentence with
pyais (multi-sentence messages are reassembled by their sequential id), and checks that at
least one position report (message type 1, 2 or 3) and one static data report (type 5) were
decoded. Optional expectations compare the decoded MMSI, name and call sign.

Usage:
    nmeasim run --stdout --quiet --duration 3 | python3 tools/check_ais_stream.py \
        --mmsi 239000001 --name "NMEA SIMULATOR X" --callsign SIMX
"""
import argparse
import sys

from pyais import decode
from pyais.exceptions import InvalidNMEAMessageException, MissingMultipartMessageException


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mmsi", type=int, help="expected MMSI of every message")
    parser.add_argument("--name", help="expected vessel name in the static data report")
    parser.add_argument("--callsign", help="expected call sign in the static data report")
    parser.add_argument("--no-static", action="store_true",
                        help="do not require a static data report")
    args = parser.parse_args()

    fragments: dict[str, list[str]] = {}
    decoded = []
    failures = 0
    for raw in sys.stdin.buffer:
        line = raw.decode("ascii", errors="replace").strip()
        if not line.startswith("!") or line[3:6] not in ("VDO", "VDM"):
            continue
        fields = line.split(",")
        total, number, sequence = int(fields[1]), int(fields[2]), fields[3]
        key = f"{line[1:6]}:{sequence}"
        fragments.setdefault(key, []).append(line)
        if number < total:
            continue
        parts = fragments.pop(key)
        try:
            message = decode(*parts)
        except (InvalidNMEAMessageException, MissingMultipartMessageException) as error:
            print(f"decode error: {error}: {parts}")
            failures += 1
            continue
        decoded.append(message)

    positions = [m for m in decoded if m.msg_type in (1, 2, 3)]
    statics = [m for m in decoded if m.msg_type == 5]
    print(f"decoded {len(decoded)} AIS messages ({len(positions)} position reports, "
          f"{len(statics)} static data reports), {failures} failures")
    if failures or not positions or (not statics and not args.no_static):
        return 1
    for message in decoded:
        if args.mmsi is not None and message.mmsi != args.mmsi:
            print(f"unexpected MMSI {message.mmsi}")
            return 1
    for message in statics:
        if args.name is not None and message.shipname != args.name:
            print(f"unexpected name '{message.shipname}'")
            return 1
        if args.callsign is not None and message.callsign != args.callsign:
            print(f"unexpected call sign '{message.callsign}'")
            return 1
    sample = positions[0]
    print(f"position {sample.lat:.4f} {sample.lon:.4f}, course {sample.course}, "
          f"speed {sample.speed} kn, heading {sample.heading}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
