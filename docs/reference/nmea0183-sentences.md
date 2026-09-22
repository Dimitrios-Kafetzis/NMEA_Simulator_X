# NMEA 0183 sentences

Every sentence the simulator emits, grouped by instrument. Each entry lists the registry
identifier used in profiles, the default talker, the field layout and an example line
produced from the standard test fixture (a vessel off Athens at 12:34:56.78 UTC on
22 September 2026, making 6.5 knots on course 047.3). The examples are the golden values the
test suite checks; a change to any example is a change to the output.

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
- When the IEC 61162-450 option is enabled (milestone M4) each sentence is preceded by a TAG
  block of the form `\s:<source>,c:<unix time>*hh\`.

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

