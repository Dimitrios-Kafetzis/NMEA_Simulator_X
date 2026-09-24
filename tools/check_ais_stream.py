#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Validate the AIS sentences of an NMEA 0183 stream with an independent decoder.

Reads lines from standard input and decodes every `!xxVDO` and `!xxVDM` sentence with
pyais, a decoder developed independently of this project, against the message layouts of
ITU-R M.1371. The fragments of a multi-sentence message are collected by talker, formatter
and sequential message id and decoded together once the last fragment arrives. Every other
line is ignored, so the simulator's full output can be piped in. Before a sentence is
collected, its NMEA 0183 checksum is verified and its fragment count and number are read; a
sentence with a bad checksum or a malformed header is a failure and is not decoded.

A failure is counted for every sentence with a bad checksum, too few fields or a fragment
count or number that is not a whole number from 1 to 9, for every message pyais rejects
(malformed, of an unknown type, with missing or surplus fragments), and for every
multi-sentence message whose last fragment never arrives.

The stream passes when there is no failure and at least one position report (message 1, 2
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
    0 when the stream passes, 1 when a sentence or message fails, a required message type
    is missing or an expectation is not met, and 2 for invalid arguments.
"""
import argparse
import sys

from pyais import decode
from pyais.exceptions import AISBaseException

#: Highest fragment count of an encapsulated sentence: the count is a single digit in
#: IEC 61162-1.
MAX_FRAGMENTS = 9


def checksum_ok(sentence: str) -> bool:
    """Return whether a sentence carries a valid NMEA 0183 checksum.

    The same test as in `check_nmea_stream.py`, repeated so that this script needs pyais
    only.

    Args:
        sentence: The whole sentence from its start delimiter to its checksum, without the
            line terminator or surrounding whitespace.

    Returns:
        True when exactly two hexadecimal digits, in either case, follow the first `*` and
        equal the XOR of the ASCII codes of the characters between the start delimiter and
        that `*`; False otherwise, also when the `*` is missing.
    """
    body, star, given = sentence[1:].partition("*")
    if star != "*" or len(given) != 2:
        return False
    computed = 0
    for byte in body.encode("ascii", errors="replace"):
        computed ^= byte
    return f"{computed:02X}" == given.upper()


def fragment_header(sentence: str) -> tuple[int, int, str] | None:
    """Return the fragment count, fragment number and sequential id of a VDM or VDO sentence.

    Args:
        sentence: A whole `!xxVDM` or `!xxVDO` sentence without the line terminator.

    Returns:
        The count, the number and the sequential id (empty for a single-sentence message),
        or None when the sentence has fewer than the seven fields of the format or the count
        or number is not a whole number with 1 <= number <= count <= `MAX_FRAGMENTS`.
    """
    fields = sentence.split(",")
    if len(fields) < 7 or not fields[1].isdigit() or not fields[2].isdigit():
        return None
    total, number = int(fields[1]), int(fields[2])
    if not 1 <= number <= total <= MAX_FRAGMENTS:
        return None
    return total, number, fields[3]


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
        if not checksum_ok(line):
            print(f"bad checksum: {line}")
            failures += 1
            continue
        header = fragment_header(line)
        if header is None:
            print(f"malformed sentence: {line}")
            failures += 1
            continue
        # A fragment's sequential id is only unique per talker and formatter, so a VDO and a
        # VDM message in flight with the same id are kept apart.
        total, number, sequence = header
        key = f"{line[1:6]}:{sequence}"
        fragments.setdefault(key, []).append(line)
        if number < total:
            # pyais decodes a multi-sentence message only from all of its fragments at once.
            continue
        parts = fragments.pop(key)
        try:
            message = decode(*parts)
        except AISBaseException as error:
            # Every pyais error: a malformed sentence or payload, an unknown message type,
            # missing or surplus fragments.
            print(f"decode error: {type(error).__name__}: {error}: {parts}")
            failures += 1
            continue
        decoded.append(message)
    for parts in fragments.values():
        # The stream ended before the last fragment of these messages arrived.
        print(f"unterminated multi-sentence message: {parts}")
        failures += 1

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
