# Profile file format

A profile is a JSON document that describes one complete simulator setup: the vessel seed
values, how they drift, which sentences are sent how often, and where the output goes. The
desktop application and `nmeasim run --profile` read the same format.

`nmeasim profile init my.json` writes a complete default profile to start from. Every key is
optional except `schema_version`; missing keys take the defaults shown below.

## Versioning

```json
{ "schema_version": 2 }
```

`schema_version` is the version of the format the file was written with. The simulator reads
any older version and migrates it in memory; saving writes the current version. A file with a
newer version than the software understands is rejected with a clear message rather than
misread.

| Version | Introduced in | Change |
| --- | --- | --- |
| 1 | 0.2.0 | First format |
| 2 | 0.4.0 | `simulation.mode` gains `track` and `replay`, with the `simulation.track` and `simulation.replay` objects; the `log` output type. Version 1 files are always in `delta` mode and load unchanged. |

## Top level

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `schema_version` | integer | required | Format version, currently `2` |
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
| `engines` | array | two engines | objects with `label`, `running`, `rpm`, `coolant_temperature_c` |

### `simulation.seed.ais`

The AIS static data is added to the profile in a following milestone M4 change; until then
the [defaults](ais.md#defaults) are transmitted.

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

## `outputs`

Every entry has `type`, `enabled` (default `true`), `encoding` (only `"nmea0183"` yet) and
`filter`, a list of registry ids to send on that output (empty sends everything). The other
keys depend on the type.

| `type` | Keys |
| --- | --- |
| `tcp-server` | `bind_address` (default `0.0.0.0`), `port` (default `10110`) |
| `tcp-client` | `host` (default `127.0.0.1`), `port`, `reconnect_ms` (default `2000`) |
| `udp` | `mode` (`unicast`, `broadcast`, `multicast`), `address`, `port`, `interface`, `multicast_ttl` |
| `websocket-server` | `bind_address`, `port` |
| `serial` | `port_name` (required), `baud_rate` (default `4800`), `data_bits` (5 to 8), `parity` (`none`, `even`, `odd`, `mark`, `space`), `stop_bits` (`1`, `1.5`, `2`), `flow_control` (`none`, `hardware`, `software`) |
| `file` | `path` (required), `append` (default `true`) |
| `log` | `path` (required), `append` (default `true`); a timestamped recording in the [log format](log-format.md) |
| `stdout` | none |

```json
"outputs": [
  { "type": "tcp-server", "port": 10110 },
  { "type": "udp", "mode": "broadcast", "address": "", "port": 10110, "interface": "eth0" },
  { "type": "serial", "port_name": "/dev/ttyUSB0", "baud_rate": 38400, "filter": ["RMC", "GGA", "VTG"] }
]
```

Behaviour of each transport is described on the [transports reference](transports.md).
