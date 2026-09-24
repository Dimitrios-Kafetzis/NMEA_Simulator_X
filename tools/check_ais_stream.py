#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Validate the AIS sentences of an NMEA 0183 stream with an independent decoder.

Reads lines from standard input and decodes every `!xxVDO` and `!xxVDM` sentence with
pyais, a decoder developed independently of this project, against the message layouts of
ITU-R M.1371. The fragments of a multi-sentence message are collected by talker, formatter
and sequential message id and decoded together once the last fragment arrives. Every other
line is ignored, so the simulator's full output can be piped in. pyais is called with its
default of not verifying checksums; `check_nmea_stream.py` checks the checksums of the same
sentences.

The stream passes when every message decodes and at least one position report (message 1, 2
or 3) and, unless `--no-static` is given, one static and voyage related data report
(message 5) were decoded. `--mmsi` is compared with every decoded message, `--name` and
`--callsign` with every message 5. The script prints a count of the decoded messages, one
line per problem and the first position report to standard output, and writes no files.

Usage:
    nmeasim run --stdout --quiet --duration 2 --enable VDM-POS --enable VDM-STATIC \
        | python3 tools/check_ais_stream.py --mmsi 239000001 --name "NMEA SIMULATOR X" \
        --callsign SIMX
    python3 tools/check_ais_stream.py --mmsi 239000001 < tests/fixtures/ais/own_vessel.nmea

Exit status:
    0 when the stream passes, 1 when a message fails to decode, a required message type is
    missing or an expectation is not met, and 2 for invalid arguments. A sentence with too
    few fields, and a pyais error other than a malformed sentence or a missing fragment, end
    the script with a traceback and status 1.
"""
import argparse
import sys

from pyais import decode
from pyais.exceptions import InvalidNMEAMessageException, MissingMultipartMessageException


def main() -> int:
    """Decode the AIS sentences read from standard input and check the expectations.

    Returns:
        The exit status: 0 when the stream passes, 1 when it fails.
    """
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
        # A fragment's sequential id is only unique per talker and formatter, so a VDO and a
        # VDM message in flight with the same id are kept apart.
        total, number, sequence = int(fields[1]), int(fields[2]), fields[3]
        key = f"{line[1:6]}:{sequence}"
        fragments.setdefault(key, []).append(line)
        if number < total:
            # pyais decodes a multi-sentence message only from all of its fragments at once.
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
