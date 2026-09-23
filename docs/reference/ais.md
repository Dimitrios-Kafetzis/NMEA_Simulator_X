# AIS own-vessel messages

The simulator transmits its own vessel as an AIS class A station: a position report
(ITU-R M.1371 message type 1, 2 or 3) and a static and voyage data report (message type 5).
Both are sent as VDO sentences, the form an AIS transponder uses for the own vessel, and
optionally also as VDM sentences, the form used for received targets, for consumers that
ignore VDO. The four registry entries are independent, so any combination can be enabled.
The design is recorded in [ADR 0013](../adr/0013-ais-encoder.md).

| Registry id | Formatter | Message | Default | Default period |
| --- | --- | --- | --- | --- |
| `VDO-POS` | `VDO` | Position report, type 1 (or 2, 3) | on | 2 s |
| `VDO-STATIC` | `VDO` | Static and voyage data, type 5 | on | 30 s |
| `VDM-POS` | `VDM` | The same position report framed as received | off | 2 s |
| `VDM-STATIC` | `VDM` | The same static data framed as received | off | 30 s |

The default talker is `AI`.

## Sentence framing

```text
!AIVDO,1,1,,A,13SsIh@vA11dWJ`Eg0R1nAKh0000,0*34
!AIVDO,2,1,6,A,53SsIh@00001<TmP000plD61<TmDh5@u:1P0000U1P43340Ht4PAAjCP@000,0*7A
!AIVDO,2,2,6,A,00000000000,2*20
```

| # | Field |
| --- | --- |
| 1 | Total number of sentences carrying this message |
| 2 | Number of this sentence, from 1 |
| 3 | Sequential message id, 0 to 9, shared by the sentences of one message; empty for a single-sentence message. Derived from the simulated clock (seconds modulo 10). |
| 4 | Radio channel, always `A` |
| 5 | Payload: the message bits in groups of six, each written as one character (`0`-`W`, `` ` ``-`w`) |
| 6 | Number of fill bits added to complete the last character, 0 to 5 |

The payload is split so that every sentence stays within 82 bytes: at most 60 payload
characters per sentence. A position report (168 bits, 28 characters) needs one sentence, a
static data report (424 bits, 71 characters) needs two.

Text fields use the six-bit ASCII alphabet (`@`, `A`-`Z`, `[\]^_`, space, `!` to `?`
including digits). Lower-case letters are sent upper-case, characters outside the alphabet
become `?`, and fields are padded with `@`.

## Position report (message types 1, 2 and 3)

| Bits | Field | Value sent |
| --- | --- | --- |
| 6 | Message type | `position_report_type` from the profile: 1 scheduled, 2 assigned, 3 in response to interrogation; anything else is sent as 1 |
| 2 | Repeat indicator | 0 |
| 30 | MMSI | `mmsi` |
| 4 | Navigational status | `navigation_status`: 0 under way using engine, 8 under way sailing, ... |
| 8 | Rate of turn | 4.733 × √(rate in °/min) with the rate's sign, clamped to ±126 |
| 10 | Speed over ground | tenths of a knot; 1022 for 102.2 kn and above; 1023 (not available) without a fix |
| 1 | Position accuracy | 1 with a differential fix, 0 otherwise |
| 28 | Longitude | 1/10000 minutes, east positive; 181° (not available) without a fix |
| 27 | Latitude | 1/10000 minutes, north positive; 91° (not available) without a fix |
| 12 | Course over ground | tenths of a degree; 3600 (not available) without a fix |
| 9 | True heading | whole degrees |
| 6 | Time stamp | UTC second of the simulated clock |
| 2 | Manoeuvre indicator | 0 (not available) |
| 3 | Spare | 0 |
| 1 | RAIM flag | 0 |
| 19 | Radio status | 0 |

## Static and voyage data (message type 5)

| Bits | Field | Value sent |
| --- | --- | --- |
| 6 | Message type | 5 |
| 2 | Repeat indicator | 0 |
| 30 | MMSI | `mmsi` |
| 2 | AIS version | 0 |
| 30 | IMO number | `imo_number`, 0 for none |
| 42 | Call sign | `call_sign`, 7 characters |
| 120 | Vessel name | `name`, 20 characters |
| 8 | Type of ship and cargo | `ship_type`, 0 to 255 (37 pleasure craft, 36 sailing, 70 cargo, 30 fishing) |
| 9 | Dimension to bow | `dimension_to_bow_m`, whole metres, at most 511 |
| 9 | Dimension to stern | `dimension_to_stern_m` |
| 6 | Dimension to port | `dimension_to_port_m`, at most 63 |
| 6 | Dimension to starboard | `dimension_to_starboard_m` |
| 4 | Position fix device | 1 (GPS) |
| 4, 5, 5, 6 | ETA month, day, hour, minute | 0, 0, 24, 60 (not available) |
| 8 | Maximum static draught | `draught_m` in tenths of a metre, at most 25.5 |
| 120 | Destination | `destination`, 20 characters, empty when not set |
| 1 | DTE | 0 (data terminal ready) |
| 1 | Spare | 0 |

## Defaults

The built-in profile transmits MMSI `239000001`, name `NMEA SIMULATOR X`, call sign `SIMX`,
ship type 37, dimensions 12/4/3/3 m and a draught of 1.8 m. Every value is set in the
`simulation.seed.ais` object of the [profile](profile.md#simulationseedais) and on the
*Vessel* tab of the settings dialog.

## Checking the output

The repository ships `tools/check_ais_stream.py`, which decodes the stream with the
third-party [pyais](https://github.com/M0r13n/pyais) library and checks the MMSI, name and
call sign; CI runs it on every platform. The golden lines above are also the fixture
`tests/fixtures/ais/own_vessel.nmea` that the same check reads.

```bash
pip install pyais
nmeasim run --stdout --quiet --duration 3 | python3 tools/check_ais_stream.py --mmsi 239000001
```
