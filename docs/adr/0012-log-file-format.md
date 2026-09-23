# 0012 Log file format: timestamped plain-text sentences

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

Milestone M3 records every emitted sentence to a file and replays such files later with
pause, step and seek. The reference application records and replays its own logs only;
one of its users' unresolved requests was to replay plain NMEA logs captured by other
software, and the requirements page lists that as an improvement. What does the simulator
write, and what does it accept?

## Decision drivers

- A recording must reproduce the original cadence on replay, including the gaps between
  sentences inside one round.
- Logs captured by other tools (plotters, gateways, `nc`, serial loggers) must replay
  without conversion. Those files come with no timestamps, with a leading date-time, with a
  Unix time, or with IEC 61162-450 TAG blocks.
- The file must stay readable and editable with any text editor and greppable with
  standard tools, and must be usable as input for other software, not only this one.
- Parsing lives in `core` (ADR 0002), so no Qt classes.

## Considered options

1. A binary or JSON container with a per-sentence timestamp.
2. Plain text, one sentence per line, prefixed with an ISO 8601 UTC timestamp, with
   `#` comment lines for metadata; a reader that also accepts lines without a prefix and
   derives their timing from the sentences themselves.
3. Plain sentences only, replayed at a fixed rate.

## Decision outcome

Option 2. The recorder writes

```text
# NMEA Simulator X log 1
# recorded: 2026-09-23T10:00:00.000Z
# profile: Harbour
2026-09-23T10:00:00.000Z $GPRMC,100000.10,A,3759.0281,N,02343.6502,E,6.5,45.0,230926,4.6,E,A*0D
2026-09-23T10:00:00.021Z $GPGGA,100000.10,3759.0281,N,02343.6502,E,1,08,0.9,0.0,M,0.0,M,,*59
```

The timestamp is the wall clock at the moment the sentence was written, with millisecond
resolution, so a replay reproduces the real cadence rather than the simulated clock, which
is inside the sentences anyway. The header carries the format version so that a later
change can be detected. A file with the timestamps stripped (`cut -d' ' -f2-`) is a valid
plain NMEA log for any other tool.

The reader accepts, line by line and mixed freely: an ISO 8601 or Unix time prefix, a TAG
block whose `c:` parameter gives the time, or a bare sentence. Offsets are derived in this
order of preference: from the absolute timestamps when any line has one; otherwise from the
UTC time fields inside RMC, GGA, GLL and ZDA sentences, with sentences that carry no time
sharing the offset of the last one that does and midnight wrap-around handled; otherwise
from a fixed interval, 100 ms by default. Offsets never decrease, so a log with clock
adjustments still replays forward. Lines without a sentence, and sentences with a wrong
checksum, are skipped and counted; a file with no sentence at all is rejected.

Option 1 was rejected because it would make the logs useless for other tools and
unreadable in an editor. Option 3 was rejected because it loses the cadence of recorded
data and cannot represent bursts.

### Consequences

- Recording is a transport (`log` output type) and so works from the CLI, from profiles and
  from the desktop application with the usual per-output sentence filter.
- Replay timing quality depends on the input: recorded logs are exact, plain logs are as
  good as the time fields in them, and a depth-only log replays at a constant rate.
- Replayed sentences are identified by their formatter (for example `MWV`) rather than a
  registry id, so output filters written for `MWV-R` do not match replayed wind sentences.
- The whole log is read into memory; a full day at 10 Hz is a few hundred megabytes of
  text and still fine, multi-gigabyte logs are out of scope.

## More information

- [Log file reference](../reference/log-format.md)
- [ADR 0011 Track and replay sources](0011-track-and-replay-sources.md)
