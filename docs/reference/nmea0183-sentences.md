# NMEA 0183 sentences

Every sentence the simulator emits, grouped by instrument. Each entry lists the registry
identifier used in profiles, the default talker, the field layout and an example line
produced from the standard test fixture (a vessel off Athens at 12:34:56.78 UTC on
22 September 2026, making 6.5 knots on course 047.3, with two engines and a destination named
AEGINA). The examples are the golden values the test suite checks; a change to any example is
a change to the output.

## Framing rules that apply to every sentence

- A sentence starts with `$` (parametric) or `!` (encapsulated, used by AIS) followed by a
  two-character talker ID and a three-character sentence formatter.
- Fields are separated by commas. Empty fields are permitted and mean *no data*.
- The checksum is the XOR of every byte between the start delimiter and `*`, written as two
  upper-case hexadecimal digits.
- The total length including `$`, checksum and the terminating `<CR><LF>` never exceeds
  82 bytes. When a sentence would exceed this, the number of decimals in latitude and
  longitude is reduced, one digit at a time down to two, instead of emitting a non-compliant
  line.
- Numbers never carry a leading `+`, never render negative zero and use a fixed number of
  decimals per field. Positions use four decimal minutes by default (0.19 m resolution).
- Talker IDs are configurable per sentence; the tables show the defaults.
- Text fields, such as the waypoint name, never contain the characters NMEA 0183 reserves
  (`,` `*` `$` `!` `\` `^` `~`), control characters or bytes outside ASCII: they are
  removed before the sentence is framed.
- When an output enables [TAG blocks](#tag-blocks) each sentence on that output is preceded
  by a TAG block of the form `\s:<source>,c:<unix time>*hh\`.

## Sentences that depend on optional state

The autopilot sentences APB, RMB and XTE are sent only while a destination is set; the
propulsion sentences RPM and XDR only for the engines configured. When there is nothing to
report the sentence is skipped for that round rather than sent with empty fields.

The AIS sentences VDO and VDM, which carry a six-bit encoded payload rather than fields, are
described on the [AIS reference page](ais.md).

## Behaviour without a GNSS fix

When the simulated receiver has no fix, RMC and GLL report status `V`, mode `N` and empty
position, speed and course fields; GGA reports quality `0`, zero satellites and empty
position and altitude; GSA reports fix type `1` with no satellites; VTG has empty values with
mode `N`; GSV reports zero satellites in view. Sentences that do not depend on the receiver
(heading, depth, wind, rudder) are unaffected.


## GNSS

### RMC: Recommended minimum navigation data

- **Registry id:** `RMC`
- **Default talker:** `GP`
- **Example:** `$GPRMC,123456.78,A,3759.0280,N,02343.6500,E,6.5,47.3,220926,4.6,E,A*06`

| # | Field |
| --- | --- |
| 1 | UTC time `hhmmss.ss` |
| 2 | Status: `A` valid, `V` receiver warning (no fix) |
| 3 | Latitude `ddmm.mmmm` |
| 4 | `N`/`S` |
| 5 | Longitude `dddmm.mmmm` |
| 6 | `E`/`W` |
| 7 | Speed over ground, knots |
| 8 | Course over ground, degrees true |
| 9 | Date `ddmmyy` |
| 10 | Magnetic variation, degrees |
| 11 | `E`/`W` |
| 12 | Mode: `A` autonomous, `D` differential, `N` no fix |

### GGA: GNSS fix data

- **Registry id:** `GGA`
- **Default talker:** `GP`
- **Example:** `$GPGGA,123456.78,3759.0280,N,02343.6500,E,1,08,0.9,12.3,M,34.5,M,,*50`

| # | Field |
| --- | --- |
| 1 | UTC time |
| 2 | Latitude |
| 3 | `N`/`S` |
| 4 | Longitude |
| 5 | `E`/`W` |
| 6 | Fix quality: `0` invalid, `1` GPS, `2` differential |
| 7 | Satellites in use (00-12) |
| 8 | HDOP |
| 9 | Altitude above mean sea level |
| 10 | `M` |
| 11 | Geoid separation |
| 12 | `M` |
| 13 | Age of differential data (empty) |
| 14 | Differential station id (empty) |

### GLL: Geographic position

- **Registry id:** `GLL`
- **Default talker:** `GP`
- **Example:** `$GPGLL,3759.0280,N,02343.6500,E,123456.78,A,A*66`

| # | Field |
| --- | --- |
| 1 | Latitude |
| 2 | `N`/`S` |
| 3 | Longitude |
| 4 | `E`/`W` |
| 5 | UTC time |
| 6 | Status `A`/`V` |
| 7 | Mode indicator |

### GSA: Active satellites and dilution of precision

- **Registry id:** `GSA`
- **Default talker:** `GP`
- **Example:** `$GPGSA,A,3,02,05,07,09,12,15,19,21,,,,,1.7,0.9,1.4*3D`

| # | Field |
| --- | --- |
| 1 | Mode: `A` automatic |
| 2 | Fix type: `1` none, `3` 3D |
| 3 | Twelve PRN slots for satellites in use |
| 4 | PDOP |
| 5 | HDOP |
| 6 | VDOP |

### GSV: Satellites in view (one sentence per four satellites)

- **Registry id:** `GSV`
- **Default talker:** `GP`
- **Example:** `$GPGSV,3,1,10,02,29,074,32,05,50,185,35,07,64,259,37,09,78,333,39*7B`

| # | Field |
| --- | --- |
| 1 | Total sentences |
| 2 | Sentence number |
| 3 | Satellites in view |
| 4 | Per satellite: PRN, elevation (degrees), azimuth (degrees), SNR (dB) |

### VTG: Course over ground and ground speed

- **Registry id:** `VTG`
- **Default talker:** `GP`
- **Example:** `$GPVTG,47.3,T,42.7,M,6.5,N,12.0,K,A*12`

| # | Field |
| --- | --- |
| 1 | Course, degrees true |
| 2 | `T` |
| 3 | Course, degrees magnetic |
| 4 | `M` |
| 5 | Speed, knots |
| 6 | `N` |
| 7 | Speed, km/h |
| 8 | `K` |
| 9 | Mode indicator |

## Time

### ZDA: UTC time and date

- **Registry id:** `ZDA`
- **Default talker:** `GP`
- **Example:** `$GPZDA,123456.78,22,09,2026,00,00*61`

| # | Field |
| --- | --- |
| 1 | UTC time |
| 2 | Day |
| 3 | Month |
| 4 | Year |
| 5 | Local zone hours (always 00) |
| 6 | Local zone minutes (always 00) |

## Heading

### HDG: Magnetic heading, deviation and variation

- **Registry id:** `HDG`
- **Default talker:** `HC`
- **Example:** `$HCHDG,40.4,0.0,E,4.6,E*70`

| # | Field |
| --- | --- |
| 1 | Magnetic sensor heading |
| 2 | Deviation, degrees |
| 3 | `E`/`W` |
| 4 | Variation, degrees |
| 5 | `E`/`W` |

### HDM: Magnetic heading

- **Registry id:** `HDM`
- **Default talker:** `HC`
- **Example:** `$HCHDM,40.4,M*19`

| # | Field |
| --- | --- |
| 1 | Heading, degrees magnetic |
| 2 | `M` |

### HDT: True heading

- **Registry id:** `HDT`
- **Default talker:** `HE`
- **Example:** `$HEHDT,45.0,T*1E`

| # | Field |
| --- | --- |
| 1 | Heading, degrees true |
| 2 | `T` |

### ROT: Rate of turn

- **Registry id:** `ROT`
- **Default talker:** `TI`
- **Example:** `$TIROT,-2.5,A*11`

| # | Field |
| --- | --- |
| 1 | Rate of turn, degrees per minute, negative to port |
| 2 | Status `A` valid |

## Speed

### VHW: Water speed and heading

- **Registry id:** `VHW`
- **Default talker:** `VW`
- **Example:** `$VWVHW,45.0,T,40.4,M,6.2,N,11.5,K*64`

| # | Field |
| --- | --- |
| 1 | Heading, degrees true |
| 2 | `T` |
| 3 | Heading, degrees magnetic |
| 4 | `M` |
| 5 | Speed through water, knots |
| 6 | `N` |
| 7 | Speed through water, km/h |
| 8 | `K` |

### VBW: Dual ground and water speed

- **Registry id:** `VBW`
- **Default talker:** `VW`
- **Example:** `$VWVBW,6.2,0.0,A,6.5,0.3,A,0.0,A,0.0,A*46`

| # | Field |
| --- | --- |
| 1 | Longitudinal water speed, knots |
| 2 | Transverse water speed, knots |
| 3 | Status |
| 4 | Longitudinal ground speed, knots |
| 5 | Transverse ground speed, knots |
| 6 | Status |
| 7 | Stern transverse water speed |
| 8 | Status |
| 9 | Stern transverse ground speed |
| 10 | Status |

## Depth

### DPT: Depth below transducer with offset

- **Registry id:** `DPT`
- **Default talker:** `SD`
- **Example:** `$SDDPT,12.4,0.5,*49`

| # | Field |
| --- | --- |
| 1 | Depth, metres |
| 2 | Transducer offset, metres: positive to water line, negative to keel |
| 3 | Maximum range scale (empty) |

### DBT: Depth below transducer

- **Registry id:** `DBT`
- **Default talker:** `SD`
- **Example:** `$SDDBT,40.7,f,12.4,M,6.8,F*0C`

| # | Field |
| --- | --- |
| 1 | Depth, feet |
| 2 | `f` |
| 3 | Depth, metres |
| 4 | `M` |
| 5 | Depth, fathoms |
| 6 | `F` |

### MTW: Water temperature

- **Registry id:** `MTW`
- **Default talker:** `YC`
- **Example:** `$YCMTW,21.5,C*0F`

| # | Field |
| --- | --- |
| 1 | Temperature, degrees Celsius |
| 2 | `C` |

## Wind

### MWV (MWV-R): Apparent wind angle and speed

- **Registry id:** `MWV-R`
- **Default talker:** `WI`
- **Example:** `$WIMWV,300.0,R,14.2,N,A*17`

| # | Field |
| --- | --- |
| 1 | Wind angle relative to the bow, 0-359 degrees |
| 2 | `R` relative (apparent) |
| 3 | Wind speed |
| 4 | `N` knots |
| 5 | Status `A` |

### MWV (MWV-T): True wind angle relative to the bow and true wind speed (disabled by default)

- **Registry id:** `MWV-T`
- **Default talker:** `WI`
- **Example:** `$WIMWV,225.0,T,12.0,N,A*13`

| # | Field |
| --- | --- |
| 1 | Wind angle relative to the bow |
| 2 | `T` theoretical (true) |
| 3 | Wind speed |
| 4 | `N` knots |
| 5 | Status `A` |

### MWD: True wind direction and speed

- **Registry id:** `MWD`
- **Default talker:** `WI`
- **Example:** `$WIMWD,270.0,T,265.4,M,12.0,N,6.2,M*6D`

| # | Field |
| --- | --- |
| 1 | Direction, degrees true |
| 2 | `T` |
| 3 | Direction, degrees magnetic |
| 4 | `M` |
| 5 | Speed, knots |
| 6 | `N` |
| 7 | Speed, m/s |
| 8 | `M` |

## Steering

### RSA: Rudder angle

- **Registry id:** `RSA`
- **Default talker:** `II`
- **Example:** `$IIRSA,-3.5,A,,V*52`

| # | Field |
| --- | --- |
| 1 | Starboard (or single) rudder angle, degrees, positive to starboard |
| 2 | Status `A` |
| 3 | Port rudder angle (empty) |
| 4 | Status `V` (not fitted) |

## Autopilot

The three sentences describe the leg from the point where the destination was set (the
origin) to the destination waypoint. The cross-track error is the vessel's distance from
that leg, with the side to steer towards to regain it; bearings are true; the arrival flag is
set inside the arrival circle around the destination and the perpendicular flag once the
vessel has passed the destination along the leg. See the
[simulation model](../explanation/simulation-model.md#destination) for the geometry.

### APB: Autopilot sentence B

- **Registry id:** `APB`
- **Default talker:** `GP`
- **Example:** `$GPAPB,A,A,1.62,R,N,V,V,220.5,T,AEGINA,225.2,T,225.2,T,A*54`

| # | Field |
| --- | --- |
| 1 | Status `A` (no LORAN-C blink or SNR warning) |
| 2 | Status `A` (no cycle lock warning) |
| 3 | Cross-track error magnitude, nautical miles, two decimals |
| 4 | Direction to steer: `L` or `R` |
| 5 | `N` nautical miles |
| 6 | Arrival circle entered: `A` inside, `V` outside |
| 7 | Perpendicular passed at the destination: `A` or `V` |
| 8 | Bearing from origin to destination, degrees |
| 9 | `T` |
| 10 | Destination waypoint id |
| 11 | Bearing from present position to destination, degrees |
| 12 | `T` |
| 13 | Heading to steer to destination, degrees (equal to field 11) |
| 14 | `T` |
| 15 | Mode indicator, as in RMC |

### RMB: Recommended minimum navigation to the destination

- **Registry id:** `RMB`
- **Default talker:** `GP`
- **Example:** `$GPRMB,A,1.62,R,,AEGINA,3744.7960,N,02325.6500,E,20.1,225.2,-6.5,V,A*56`

| # | Field |
| --- | --- |
| 1 | Status `A` |
| 2 | Cross-track error magnitude, nautical miles |
| 3 | Direction to steer: `L` or `R` |
| 4 | Origin waypoint id (empty: the origin is the point where the destination was set) |
| 5 | Destination waypoint id |
| 6 | Destination latitude `ddmm.mmmm` |
| 7 | `N`/`S` |
| 8 | Destination longitude `dddmm.mmmm` |
| 9 | `E`/`W` |
| 10 | Range to destination, nautical miles, at most 999.9 |
| 11 | Bearing to destination, degrees true |
| 12 | Destination closing velocity, knots: the component of the speed over ground towards the destination, negative when sailing away |
| 13 | Arrival status: `A` inside the arrival circle, `V` otherwise |
| 14 | Mode indicator, as in RMC |

### XTE: Cross-track error

- **Registry id:** `XTE`
- **Default talker:** `GP`
- **Example:** `$GPXTE,A,A,1.62,R,N,A*18`

| # | Field |
| --- | --- |
| 1 | Status `A` |
| 2 | Status `A` |
| 3 | Cross-track error magnitude, nautical miles |
| 4 | Direction to steer: `L` or `R` |
| 5 | `N` nautical miles |
| 6 | Mode indicator, as in RMC |

Waypoint ids are sent as configured after removing characters that NMEA 0183 reserves
(`,`, `*`, `$`, `!`, `\`, `^`, `~`), spaces and control characters, and truncating to 16
characters; an id that ends up empty is sent as `WPT`.

## Propulsion

One sentence per configured engine, in profile order. A stopped engine reports zero
revolutions. Engines are numbered from 1 in RPM and their XDR transducers are named
`ENGINE#0`, `ENGINE#1`, ... following the convention Signal K and common gateways expect.

### RPM: Engine revolutions

- **Registry id:** `RPM`
- **Default talker:** `ER`
- **Example:** `$ERRPM,E,1,1800.0,,A*56`

| # | Field |
| --- | --- |
| 1 | Source: `E` engine |
| 2 | Engine number, 1 for the first configured engine |
| 3 | Revolutions per minute |
| 4 | Propeller pitch, percent (empty: not simulated) |
| 5 | Status `A` |

### XDR: Transducer measurements

- **Registry id:** `XDR`
- **Default talker:** `ER`
- **Example:** `$ERXDR,C,82.0,C,ENGINE#0,T,1800.0,R,ENGINE#0*5C`

Two transducers per engine, each four fields:

| # | Field |
| --- | --- |
| 1 | Transducer type `C` (temperature) |
| 2 | Coolant temperature, degrees Celsius |
| 3 | Unit `C` |
| 4 | Transducer id `ENGINE#n` |
| 5 | Transducer type `T` (tachometer) |
| 6 | Revolutions per minute |
| 7 | Unit `R` |
| 8 | Transducer id `ENGINE#n` |

## TAG blocks

An IEC 61162-450 TAG block is a prefix in front of a sentence, enabled per output:

```text
\s:GP0001,c:1790080496*26\$HEHDT,45.0,T*1E
```

| Part | Meaning |
| --- | --- |
| `\` ... `\` | Delimits the block |
| `s:GP0001` | Source identifier, configured per output (`SIM0001` by default); printable characters except `,`, `*`, `\`, `!` and `$`, at most 15 |
| `c:1790080496` | Time of the sentence as Unix seconds of the simulated clock, or milliseconds when the output asks for them; can be left out |
| `*26` | Checksum of the text between the backslashes, computed like a sentence checksum |

The sentence after the block is unchanged. Recordings never carry TAG blocks; the replay
reader accepts them from other sources (see the [log format](log-format.md)).

## Custom sentences

The operator can add sentences of their own to the schedule, each with an id, a body and a
period. The body is written as it should appear on the wire without the checksum, for
example `$PXYZ,1,2,3` or `!AIVDM,1,1,,A,13aEOK?P00PD2wVMdLDRhgvL289?,0`; the leading `$` may
be left out, an old `*hh` and line terminator are ignored, and the checksum is computed
when the sentence is sent. A body is refused when it is empty, carries characters outside
printable ASCII or one of `$ ! \ ^ ~` inside, has no address of at least three letters or
digits, or would exceed 82 characters.

Custom sentences are emitted after the registry sentences of the same round, filtered by
their id like any other sentence (`CUSTOM-1`, `CUSTOM-2`, ... when no id is given) and
recorded like them. An id equal to a registry id is refused.
