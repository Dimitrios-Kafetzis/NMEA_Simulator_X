# NMEA 0183 sentences

This page will list every sentence the simulator emits, grouped by instrument, with the field
layout, the talker ID used by default and an example line produced by the simulator.
Each sentence is added to this page in the same change that implements it, together with a
golden-file test.

## Framing rules that apply to every sentence

- A sentence starts with `$` (parametric) or `!` (encapsulated, used by AIS) followed by a
  two-character talker ID and a three-character sentence formatter.
- Fields are separated by commas. Empty fields are permitted and mean *no data*.
- The checksum is the XOR of every byte between the start delimiter and `*`, written as two
  upper-case hexadecimal digits.
- The total length including `$`, checksum and the terminating `<CR><LF>` never exceeds
  82 bytes. When a sentence would exceed this, numeric precision is reduced in a documented
  order instead of emitting a non-compliant line.
- When the IEC 61162-450 option is enabled each sentence is preceded by a TAG block of the
  form `\s:<source>,c:<unix time>*hh\`.

## Planned sentence set

| Group | Sentences |
| --- | --- |
| GNSS | RMC, GGA, GLL, GSA, GSV, VTG, ZDA |
| Heading and speed | HDG, HDM, HDT, VHW, VBW, ROT |
| Depth and water | DPT, DBT, MTW |
| Wind | MWV (apparent), MWD (true) |
| Route and autopilot | APB, RMB, XTE |
| Steering | RSA |
| AIS | VDO, VDM |
| Propulsion | RPM, XDR |
| User defined | Any sentence entered manually, checksum recalculated |
