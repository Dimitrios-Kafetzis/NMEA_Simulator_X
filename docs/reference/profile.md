# Profile file format

A profile is a JSON document that describes one complete simulator setup: the vessel seed
values, how they drift, which sentences are sent how often, and where the output goes. The
desktop application and `nmeasim run --profile` read the same format.

`nmeasim profile init my.json` writes a complete default profile to start from. Every key is
optional except `schema_version`; missing keys take the defaults shown below.

## Versioning

```json
{ "schema_version": 3 }
```

`schema_version` is the version of the format the file was written with. The simulator reads
any older version and migrates it in memory; saving writes the current version. A file with a
newer version than the software understands is rejected with a clear message rather than
misread.

| Version | Introduced in | Change |
| --- | --- | --- |
| 1 | 0.2.0 | First format |
| 2 | 0.4.0 | `simulation.mode` gains `track` and `replay`, with the `simulation.track` and `simulation.replay` objects; the `log` output type. Version 1 files are always in `delta` mode and load unchanged. |
| 3 | 0.5.0 | `simulation.seed.destination` and `simulation.seed.ais`, `sentences.custom`, the output `encoding` values `signalk` and `viewsync` with their `period_ms`, `tag_block`, `signalk` and `viewsync` objects. Every new key has a default, so version 2 files load unchanged. |

## Top level

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `schema_version` | integer | required | Format version, currently `3` |
| `name` | string | `"Default"` | Display name of the profile |
| `simulation` | object | see below | Vessel seed and behaviour |
| `sentences` | object | see below | Sentence schedule |
| `outputs` | array | `[]` | Output channels |

## `simulation`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `mode` | string | `"delta"` | `delta` (seed values that drift), `track` (follow a file, see `track`) or `replay` (re-send a log, see `replay`) |
| `tick_ms` | integer | `100` | Length of one simulation step, 10 to 10000 |
| `start_time` | string | `"now"` | `"now"` or an ISO 8601 UTC date-time such as `"2026-09-22T12:34:56.780Z"` |
| `random_seed` | integer | `2026` | Seed of the random generator; the same seed reproduces the same run |
| `seed` | object | see below | Initial vessel values |
| `variation` | object | see below | How far and how fast each value drifts |
| `steering` | object | see below | Rudder behaviour |
| `track` | object | see below | Track-following settings, used when `mode` is `track` |
| `replay` | object | see below | Log replay settings, used when `mode` is `replay` |

In `track` and `replay` mode the `seed` still supplies the values the file does not carry
(depth, water temperature, wind, GNSS quality, engines) and `start_time` supplies the clock
until the file provides one. The `variation` values are not used in those modes.

### `simulation.seed`

| Key | Type | Default | Unit |
| --- | --- | --- | --- |
| `position.latitude`, `position.longitude` | number | `37.9838`, `23.7275` | decimal degrees, north and east positive |
| `altitude_m` | number | `0` | metres above mean sea level |
| `heading_true_deg` | number | `45` | degrees true |
| `speed_over_ground_kn` | number | `6.5` | knots |
| `magnetic_variation_deg` | number | `4.6` | degrees, east positive |
| `magnetic_deviation_deg` | number | `0` | degrees, east positive |
| `rudder_angle_deg` | number | `0` | degrees, starboard positive |
| `depth_m` | number | `12.4` | metres below transducer |
| `transducer_offset_m` | number | `0.5` | metres; positive to the water line, negative to the keel |
| `water_temperature_c` | number | `21.5` | degrees Celsius |
| `wind_true_direction_deg` | number | `270` | degrees true, the direction the wind blows from |
| `wind_true_speed_kn` | number | `12` | knots |
| `gnss.fix` | boolean | `true` | `false` simulates a receiver without a fix |
| `gnss.quality` | string | `"gps"` | `"invalid"`, `"gps"` or `"differential"` |
| `gnss.satellites_in_use`, `gnss.satellites_in_view` | integer | `8`, `10` | 0 to 12 |
| `gnss.hdop`, `gnss.pdop`, `gnss.vdop` | number | `0.9`, `1.7`, `1.4` | dilution of precision |
| `gnss.geoid_separation_m` | number | `0` | metres |
| `engines` | array | two engines | objects with `label`, `running`, `rpm`, `coolant_temperature_c`; the order gives the RPM engine numbers and the XDR transducer ids |
| `destination` | object or `null` | `null` | The waypoint the autopilot sentences describe, see below |
| `ais` | object | see below | AIS static data of the own vessel |

### `simulation.seed.destination`

`null` (or a missing key) means no destination: APB, RMB and XTE are not sent and the Signal K
course paths are absent. See the [simulation model](../explanation/simulation-model.md#destination).

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `"WPT"` | Waypoint id, sent after removing NMEA reserved characters and truncating to 16 characters |
| `latitude`, `longitude` | number | required | Destination, decimal degrees |
| `origin_latitude`, `origin_longitude` | number | the seed position | Start of the leg, from which the cross-track error is measured |
| `arrival_radius_m` | number | `100` | Radius of the arrival circle |

```json
"destination": { "name": "AEGINA", "latitude": 37.7466, "longitude": 23.4275,
                 "origin_latitude": 38.0, "origin_longitude": 23.7, "arrival_radius_m": 100 }
```

### `simulation.seed.ais`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `mmsi` | integer | `239000001` | Maritime Mobile Service Identity, at most nine digits |
| `imo_number` | integer | `0` | IMO number, 0 for none |
| `name` | string | `"NMEA SIMULATOR X"` | Vessel name, 20 characters of the AIS alphabet |
| `call_sign` | string | `"SIMX"` | Call sign, 7 characters |
| `ship_type` | integer | `37` | Type of ship and cargo code, 0 to 255 |
| `dimension_to_bow_m`, `dimension_to_stern_m` | number | `12`, `4` | Metres from the antenna, at most 511 |
| `dimension_to_port_m`, `dimension_to_starboard_m` | number | `3`, `3` | Metres from the antenna, at most 63 |
| `draught_m` | number | `1.8` | Maximum static draught, at most 25.5 |
| `destination` | string | `""` | Voyage destination, 20 characters |
| `navigation_status` | integer | `0` | Navigational status code, 0 to 15 |
| `position_report_type` | integer | `1` | Message type of the position report: 1, 2 or 3 |

The fields are described on the [AIS reference page](ais.md). The MMSI also forms the default
Signal K context.

### `simulation.variation`

Each of `heading`, `speed`, `depth`, `water_temperature`, `wind_direction` and `wind_speed`
is an object with `amplitude` (how far the value may wander from its seed) and
`step_per_second` (how fast). Units are those of the value. An amplitude of `0` freezes the
value. See the [simulation model](../explanation/simulation-model.md).

| Value | Default amplitude | Default step per second |
| --- | --- | --- |
| `heading` | 2 degrees | 0.5 |
| `speed` | 0.3 knots | 0.1 |
| `depth` | 1.5 metres | 0.2 |
| `water_temperature` | 0.2 degrees | 0.05 |
| `wind_direction` | 10 degrees | 2 |
| `wind_speed` | 2 knots | 0.5 |

### `simulation.steering`

| Key | Default | Meaning |
| --- | --- | --- |
| `turn_rate_per_rudder_deg` | `0.6` | degrees per minute of turn for each degree of rudder |
| `max_rudder_angle_deg` | `35` | rudder limit |

### `simulation.track`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `path` | string | `""` | GPX or KML [track file](track-files.md). Required in `track` mode. A relative path is resolved against the directory of the profile file when the profile is loaded from disk. |
| `speed_kn` | number | `6` | Speed along legs whose points have neither timestamps nor a recorded speed; must be positive |
| `use_timestamps` | boolean | `true` | `false` ignores the track's timestamps and sails every leg at `speed_kn` or the recorded point speed |
| `loop` | boolean | `false` | Start again at the first point instead of stopping at the last |

### `simulation.replay`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `path` | string | `""` | [Log file](log-format.md) to replay. Required in `replay` mode; relative paths are resolved like `track.path`. |
| `loop` | boolean | `false` | Start again at the first entry instead of stopping at the last |
| `fixed_interval_ms` | integer | `100` | Spacing of the entries when the log has no time information at all, 1 to 60000 |

```json
"simulation": {
  "mode": "track",
  "track": { "path": "../tracks/saronic-gulf.gpx", "speed_kn": 7.5, "use_timestamps": true, "loop": false }
}
```

## `sentences`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `position_decimals` | integer | `4` | Decimal minutes in latitude and longitude, 2 to 8. Reduced automatically when a sentence would exceed 82 bytes. |
| `settings` | object | `{}` | Per-sentence overrides keyed by registry id |
| `custom` | array | `[]` | Sentences typed in by the operator, see below |

Each entry of `settings` may contain `enabled` (boolean), `talker` (two characters, empty for
the default) and `period_ms` (integer). Ids and defaults are listed by `nmeasim sentences`
and on the [sentence reference](nmea0183-sentences.md).

```json
"sentences": {
  "position_decimals": 4,
  "settings": {
    "RMC": { "enabled": true, "talker": "GN", "period_ms": 500 },
    "GSV": { "enabled": false }
  }
}
```

### `sentences.custom`

Each entry is one [custom sentence](nmea0183-sentences.md#custom-sentences):

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `id` | string | `""` | Identifier for filters and the console, upper-cased; empty gives `CUSTOM-n`; must not be a registry id |
| `body` | string | required | The sentence without checksum, for example `$PXYZ,1,2,3`; validated when the profile is loaded |
| `period_ms` | integer | `1000` | Emission period, 50 to 3600000 |
| `enabled` | boolean | `true` | |

```json
"sentences": {
  "custom": [
    { "id": "BARO", "body": "$IIXDR,P,1.013,B,BARO", "period_ms": 5000, "enabled": true }
  ]
}
```

## `outputs`

Every entry has `type`, `enabled` (default `true`), `encoding` and `filter`. The encoding
decides what the output carries ([ADR 0014](../adr/0014-multi-encoding-outputs.md)):

| `encoding` | Carries | `filter` matches | Extra keys |
| --- | --- | --- | --- |
| `nmea0183` (default) | The sentences the simulation emits | Registry ids and custom sentence ids; empty sends everything | `tag_block` |
| `signalk` | One [Signal K delta](signalk.md) built from the state every `period_ms` | Path prefixes such as `navigation` or `environment.wind`; empty sends every path | `period_ms`, `signalk` |
| `viewsync` | One [ViewSync packet](viewsync.md) built from the state every `period_ms` | not used | `period_ms`, `viewsync` |

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `period_ms` | integer | `1000` | Period of the Signal K or ViewSync messages, 50 to 3600000; the simulation tick bounds the resolution |
| `tag_block.enabled` | boolean | `false` | Prefix every sentence with an IEC 61162-450 [TAG block](nmea0183-sentences.md#tag-blocks) |
| `tag_block.source` | string | `"SIM0001"` | The `s:` source identifier |
| `tag_block.include_time` | boolean | `true` | Send the `c:` time |
| `tag_block.milliseconds` | boolean | `false` | Send `c:` in milliseconds instead of seconds |
| `signalk.context` | string | `""` | Signal K context; empty derives `vessels.urn:mrn:imo:mmsi:<mmsi>` from the AIS data |
| `signalk.source_label` | string | `"nmeasim"` | The `source.label` of every update |
| `viewsync.camera_altitude_m` | number | `500` | Camera height above the vessel |
| `viewsync.tilt_deg` | number | `60` | Camera tilt |
| `viewsync.roll_deg` | number | `0` | Camera roll |
| `viewsync.planet` | string | `""` | Empty for Earth, or `sky`, `mars`, `moon` |

The other keys depend on the type. Each type reads and checks only its own keys, so a key of
another type is ignored; a value outside the range given below, or a name that is not listed,
is rejected when the profile is loaded.

| `type` | Keys |
| --- | --- |
| `tcp-server` | `bind_address` (default `0.0.0.0`), `port` (default `10110`, 0 to 65535) |
| `tcp-client` | `host` (default `127.0.0.1`), `port`, `reconnect_ms` (default `2000`, 1 to 3600000) |
| `udp` | `mode` (`unicast` (default), `broadcast`, `multicast`), `address`, `port`, `interface`, `multicast_ttl` (default `1`, 1 to 255) |
| `websocket-server` | `bind_address`, `port` |
| `serial` | `port_name` (required), `baud_rate` (default `4800`, positive), `data_bits` (5 to 8, default `8`), `parity` (`none` (default), `even`, `odd`, `mark`, `space`), `stop_bits` (`"1"` (default), `"1.5"`, `"2"`, as strings), `flow_control` (`none` (default), `hardware`, `software`) |
| `file` | `path` (required), `append` (default `true`) |
| `log` | `path` (required), `append` (default `true`); a timestamped recording in the [log format](log-format.md) |
| `stdout` | none |

```json
"outputs": [
  { "type": "tcp-server", "port": 10110, "tag_block": { "enabled": true, "source": "GP0001" } },
  { "type": "udp", "mode": "broadcast", "address": "", "port": 10110, "interface": "eth0" },
  { "type": "serial", "port_name": "/dev/ttyUSB0", "baud_rate": 38400, "filter": ["RMC", "GGA", "VTG"] },
  { "type": "websocket-server", "port": 3000, "encoding": "signalk", "period_ms": 500 },
  { "type": "udp", "address": "192.168.1.20", "port": 42000, "encoding": "viewsync", "period_ms": 200 }
]
```

Behaviour of each transport is described on the [transports reference](transports.md).
