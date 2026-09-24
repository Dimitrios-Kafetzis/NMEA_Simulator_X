#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Validate an NMEA 0183 stream with an independent parser.

Reads sentences from standard input, one per line, and checks each against NMEA 0183
(IEC 61162-1). A parametric sentence (`$`) is parsed with pynmea2, a parser developed
independently of this project, in strict mode: the sentence must match its framing, carry a
checksum that is the XOR of the characters between `$` and `*`, and use a formatter pynmea2
knows. pynmea2 does not validate the field values. An encapsulated sentence (`!`, used by
AIS) is outside pynmea2's scope, so only its checksum is checked here; `check_ais_stream.py`
decodes its payload. Every sentence must also fit the 82-character limit of the standard.

The script prints one line per failing sentence and a summary with the number of sentences,
failures and the distinct formatters seen to standard output, and writes no files. Blank
lines are skipped.

Usage:
    nmeasim run --stdout --quiet --duration 3 --enable MWV-T \
        | python3 tools/check_nmea_stream.py --expect 21
    nmeasim run --replay tests/fixtures/logs/recorded.log --stdout --quiet \
        | python3 tools/check_nmea_stream.py --expect 8

Exit status:
    0 when every sentence passes and at least `--expect` distinct formatters were seen, 1
    when a sentence fails, no sentence was read or too few formatters were seen, and 2 for
    invalid arguments.
"""
import argparse
import sys

import pynmea2

#: Maximum length of a sentence in characters, counting the start delimiter (`$` or `!`) and
#: the closing CR LF, as NMEA 0183 specifies.
MAX_LENGTH = 82


def checksum_ok(sentence: str) -> bool:
    """Return whether a sentence carries a valid NMEA 0183 checksum.

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


def main() -> int:
    """Check the sentences read from standard input and print the summary.

    Returns:
        The exit status: 0 when the stream passes, 1 when it fails.
    """
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
        # The limit counts the CR LF, which a line may lack or carry as LF only; adding two
        # to the stripped length measures every line as if it ended in CR LF.
        length = len(line.rstrip("\r\n")) + 2
        if length > MAX_LENGTH:
            print(f"too long ({length} characters with CR LF): {line.strip()}")
            failures += 1
            continue
        if line.startswith("!"):
            # Encapsulated sentences (AIS) are outside pynmea2's scope; their framing is
            # checked here and their payload by check_ais_stream.py.
            if not checksum_ok(line.strip()):
                print(f"bad checksum: {line.strip()}")
                failures += 1
                continue
            formatters.add(line[3:6])
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
