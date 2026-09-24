# Signal K output

An output with the `signalk` encoding sends [Signal K](https://signalk.org/) delta messages
built from the vessel state instead of NMEA 0183 sentences, one JSON document per line (or
per WebSocket text frame). A WebSocket server output greets every client with the Signal K
*hello* message first, so a Signal K server or a web instrument can treat the simulator as a
Signal K source. The design is recorded in
[ADR 0014](../adr/0014-multi-encoding-outputs.md) and the
[how-to guide](../how-to/signal-k.md) walks through connecting a server.

## Hello message

```json
{"name":"NMEASimulatorX","version":"0.5.0","self":"vessels.urn:mrn:imo:mmsi:239000001","roles":["master","main"],"timestamp":"2026-09-22T12:34:56.780Z"}
```

`self` is the context of the deltas that follow; `timestamp` is the wall-clock time the
client connected.

## Delta message

```json
{"context":"vessels.urn:mrn:imo:mmsi:239000001","updates":[{"source":{"label":"nmeasim","type":"simulator"},"timestamp":"2026-09-22T12:34:56.780Z","values":[{"path":"navigation.datetime","value":"2026-09-22T12:34:56.780Z"},{"path":"navigation.position","value":{"longitude":23.7275,"latitude":37.9838,"altitude":12.3}},{"path":"navigation.speedOverGround","value":3.3438889}]}]}
```

One delta is sent per output period (see the [profile reference](profile.md#outputs)) with
one update whose `timestamp` is the simulated clock. The `context` is configurable per
output: the default `vessels.urn:mrn:imo:mmsi:<MMSI>` uses the AIS MMSI, and any other
Signal K context such as `aircraft.urn:mrn:signalk:uuid:...` can be given. The output's
sentence filter applies to paths: a filter entry admits the path it names and every path
below it, so `navigation, environment.wind` sends only those groups. Entries name whole path
segments, so `navigation.speed` does not admit `navigation.speedThroughWater`, and case is
ignored.

## Paths

Every value uses the units of the Signal K specification: metres, metres per second, radians,
kelvin, hertz. Relative angles are in the range -π to π, positive to starboard.

| Path | Value | Condition |
| --- | --- | --- |
| `navigation.datetime` | Simulated UTC time, ISO 8601 | always |
| `navigation.position` | `{longitude, latitude, altitude}` in degrees and metres | with a fix |
| `navigation.courseOverGroundTrue` | rad | with a fix |
| `navigation.courseOverGroundMagnetic` | rad | with a fix |
| `navigation.speedOverGround` | m/s | with a fix |
| `navigation.headingTrue` | rad | always |
| `navigation.headingMagnetic` | rad | always |
| `navigation.magneticVariation` | rad, east positive | always |
| `navigation.magneticDeviation` | rad, east positive | always |
| `navigation.speedThroughWater` | m/s | always |
| `navigation.rateOfTurn` | rad/s, starboard positive | always |
| `navigation.gnss.type` | `"GPS"` | always |
| `navigation.gnss.methodQuality` | `"GNSS Fix"`, `"DGNSS fix"` or `"no GPS"` | always |
| `navigation.gnss.satellites` | satellites in use, 0 without a fix | always |
| `navigation.gnss.horizontalDilution`, `positionDilution` | HDOP, PDOP | with a fix |
| `navigation.gnss.antennaAltitude`, `geoidalSeparation` | m | with a fix |
| `navigation.courseRhumbline.nextPoint.position` | destination `{longitude, latitude}` | with a destination |
| `navigation.courseRhumbline.nextPoint.bearingTrue` | rad | with a destination |
| `navigation.courseRhumbline.nextPoint.distance` | m | with a destination |
| `navigation.courseRhumbline.nextPoint.velocityMadeGood` | m/s towards the destination | with a destination |
| `navigation.courseRhumbline.previousPoint.position` | leg origin | with a destination |
| `navigation.courseRhumbline.bearingTrackTrue` | rad, origin to destination | with a destination |
| `navigation.courseRhumbline.crossTrackError` | m, positive to the right of the leg | with a destination |
| `environment.depth.belowTransducer` | m | always |
| `environment.depth.surfaceToTransducer`, `belowSurface` | m | transducer offset ≥ 0 |
| `environment.depth.transducerToKeel`, `belowKeel` | m | transducer offset < 0 |
| `environment.water.temperature` | K | always |
| `environment.wind.speedTrue` | m/s | always |
| `environment.wind.directionTrue` | rad, direction the wind comes from | always |
| `environment.wind.angleTrueWater` | rad relative to the bow | always |
| `environment.wind.speedApparent` | m/s | always |
| `environment.wind.angleApparent` | rad relative to the bow | always |
| `steering.rudderAngle` | rad, starboard positive | always |
| `propulsion.<id>.revolutions` | Hz (revolutions per second), 0 when stopped | per engine |
| `propulsion.<id>.temperature` | coolant temperature, K | per engine |
| `propulsion.<id>.state` | `"started"` or `"stopped"` | per engine |

The engine `<id>` is the engine label lower-cased with the word *engine* and all
punctuation removed: *Port engine* becomes `port`. A label that leaves nothing becomes
`engine1`, `engine2`, ... in profile order.

## Checking the output

The repository ships `tools/check_signalk_stream.py`, which reads deltas from standard input,
checks that every line is a JSON document of the shape above and that every path is one of
the paths listed here with a value of the right JSON type. CI runs it on every platform.

```bash
nmeasim run --stdout --encoding signalk --quiet --duration 3 | python3 tools/check_signalk_stream.py
```
