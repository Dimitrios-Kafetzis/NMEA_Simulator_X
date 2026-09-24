# Log files

A log is a text file with one NMEA sentence per line. The simulator writes logs when
recording and reads them back for replay; it also reads plain logs written by other
software. The design rationale is in [ADR 0012](../adr/0012-log-file-format.md).

## What the recorder writes

```text
# NMEA Simulator X log 1
# recorded: 2026-09-23T10:00:00.000Z
# profile: Harbour
2026-09-23T10:00:00.000Z $GPRMC,100000.10,A,3759.0281,N,02343.6502,E,6.5,45.0,230926,4.6,E,A*0D
2026-09-23T10:00:00.021Z $GPGGA,100000.10,3759.0281,N,02343.6502,E,1,08,0.9,0.0,M,0.0,M,,*59
```

| Part | Meaning |
| --- | --- |
| `# NMEA Simulator X log 1` | First line; the number is the format version |
| `# key: value` | Header metadata: `recorded` (UTC start), `profile` (profile name) |
| `2026-09-23T10:00:00.000Z` | Wall-clock UTC time the sentence was written, millisecond resolution |
| `$GPRMC,...*0D` | The sentence exactly as sent, without the line terminator |

Lines end with a single line feed. Every sentence admitted by the output's filter is
written, so a recording can be limited to a subset of sentences like any other output.

## What the reader accepts

Each line is handled on its own, so the shapes below can be mixed in one file.

| Line shape | Time taken from |
| --- | --- |
| `2026-09-23T10:00:00.250Z $GPRMC,...` | The ISO 8601 prefix (`T` or space separator, optional fraction, `Z` or `±hh:mm`; no zone means UTC) |
| `2026-09-23 10:00:00,$GPRMC,...` | The same, with a comma, tab or space before the sentence |
| `1790416800.250 $GPRMC,...` | Unix seconds, or milliseconds when the number is too large for seconds |
| `\s:GP0001,c:1790416800*2E\$GPRMC,...` | The `c:` parameter of the IEC 61162-450 TAG block, in seconds or milliseconds |
| `[10:00:00] $GPRMC,...` or any other prefix | Ignored; the line counts as having no timestamp |
| `$GPRMC,...` | No timestamp on the line |
| `# ...` | Comment; `# key: value` lines become header metadata |
| Blank | Skipped, not counted |
| No `$` or `!` on the line | Skipped and counted |

A sentence with a checksum that does not match is skipped and counted. A sentence without a
checksum is accepted. The same holds for a TAG block: when it ends in `*hh`, the checksum of
the text between its backslashes must match, or the whole line is skipped and counted, and
a block without a checksum is accepted; a block that is not closed by a second backslash is
skipped and counted as well. Trailing `<CR>`, `<LF>` and spaces are ignored.

### How replay timing is derived

1. **Timestamps.** When at least one line carries an absolute time, every entry's offset is
   its time minus the first timestamp seen. Lines without a time share the offset of the
   previous line. Offsets never decrease: a line with an earlier time than its predecessor
   gets the predecessor's offset.
2. **Sentence times.** Otherwise, when sentences carry a UTC time field (RMC, GGA, GLL,
   ZDA, GNS, GST, GBS, GRS), offsets follow those times. Sentences without a time field share
   the offset of the last one that had it. A jump back of more than 12 hours is taken as
   crossing midnight into the next day. A shorter jump back holds the replay: the offset
   stays where it is until the times pass the latest time seen before the jump, and only
   the time beyond it is added, so no time is counted twice. The same sentences set the
   simulated time during replay (see [Replay behaviour](#replay-behaviour)).
3. **Fixed interval.** Otherwise the entries are spaced by a fixed interval, 100 ms by
   default, configurable per profile. A negative interval is rejected.

[ADR 0012](../adr/0012-log-file-format.md) names RMC, GGA, GLL and ZDA as the sentences whose
time fields are used; the reader also uses the other sentences listed above, and this page
is the reference for the current behaviour.

The duration of a log is the offset of its last entry. A looping replay starts the next
pass one duration after the previous one, so the first entry of a pass is sent together
with the last entry of the previous pass, and the time a tick runs past the end carries into
the next pass.

## Replay behaviour

Replayed sentences are sent as recorded, bytes unchanged, with `<CR><LF>` appended. Each
one is also decoded into the vessel state so that the dashboard and the map follow the
replay; sentences the decoder does not know (see below) pass through unchanged and leave
the state as it is.

| Formatter | Values decoded |
| --- | --- |
| RMC | Time and date, fix status, position, speed and course over ground, magnetic variation |
| GGA | Time, position, fix quality, satellites in use, HDOP, altitude, geoid separation |
| GLL | Position, time, fix status |
| GNS | Time, position, fix status and quality from the mode letters, satellites in use, HDOP, altitude, geoid separation |
| GST, GBS, GRS | Time only |
| GSA | Fix status, satellites in use, PDOP, HDOP, VDOP |
| GSV | Satellites in view |
| VTG | Course and speed over ground |
| ZDA | Time and date |
| HDT, HDG, HDM | True heading; HDG also sets deviation and variation |
| ROT | Rate of turn, when its status is `A` |
| VHW | Heading and speed through water |
| VBW | Speed through water |
| DPT, DBT | Depth below transducer, DPT also the transducer offset |
| MTW | Water temperature |
| MWV | Apparent (`R`) or true (`T`) wind angle and speed, unit converted to knots |
| MWD | True wind direction and speed |
| RSA | Rudder angle |
| RMB | Destination waypoint id and position; a new id starts the leg at the vessel's current position, the same id keeps the leg (an empty id is `WPT`) |
| APB, XTE | Recognised, nothing applied (they repeat what RMB carries) |
| RPM | Revolutions of engine `n` (`E` source, status `A`), creating engines up to `n`; running when above zero |
| XDR | `C`/`C` coolant temperature and `T`/`R` revolutions for transducer ids `ENGINE#n` |
| VDO, VDM | Passed through unchanged; the AIS payload is not decoded |

The simulated UTC time follows the time fields: RMC and ZDA with a valid date set the date and
the time, the other time fields only the time of day. A time of day more than 12 hours
earlier than the current time is taken as the next day, so a GGA just after midnight moves
to the new date without waiting for the next RMC or ZDA.

Replayed sentences are identified by their formatter (`MWV`, not `MWV-R`) for output
filters and the console filter.
