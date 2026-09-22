#!/usr/bin/env python3
"""Validate an NMEA 0183 stream with an independent parser.

Reads sentences from standard input (one per line) and checks every one with pynmea2, which
is developed independently of this project. Exit status is non-zero when any sentence fails
to parse, has a bad checksum, exceeds the length limit, or when fewer than the expected
number of distinct sentence formatters were seen.

Usage:
    nmeasim run --stdout --quiet --duration 3 | python3 tools/check_nmea_stream.py --expect 19
"""
import argparse
import sys

import pynmea2

MAX_LENGTH = 82  # including the leading '$' and the trailing CR LF


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expect", type=int, default=0,
                        help="minimum number of distinct formatters expected")
    args = parser.parse_args()

    total = 0
    failures = 0
    formatters = set()
    for raw in sys.stdin.buffer:
        line = raw.decode("ascii", errors="replace")
        if not line.strip():
            continue
        total += 1
        if len(line.rstrip("\r\n")) + 2 > MAX_LENGTH:
            print(f"too long ({len(line)} bytes): {line.strip()}")
            failures += 1
            continue
        try:
            message = pynmea2.parse(line.strip(), check=True)
        except pynmea2.ParseError as error:
            print(f"parse error: {error}: {line.strip()}")
            failures += 1
            continue
        formatters.add(message.sentence_type)

    print(f"checked {total} sentences, {failures} failures, "
          f"{len(formatters)} formatters: {', '.join(sorted(formatters))}")
    if total == 0 or failures:
        return 1
    if len(formatters) < args.expect:
        print(f"expected at least {args.expect} distinct formatters")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
