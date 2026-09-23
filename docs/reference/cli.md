# Command-line tool

`nmeasim` is the headless simulator. It links the same engine and transports as the desktop
application and needs no display, which makes it suitable for scripts, containers and CI.

## Synopsis

```text
nmeasim [OPTIONS] [SUBCOMMAND]
```

| Option | Description |
| --- | --- |
| `-h`, `--help` | Print help and exit. Every subcommand has its own `--help`. |
| `-V`, `--version` | Print the project name and version and exit |

## `nmeasim run`

Runs a simulation and streams it to the configured outputs until the duration elapses or
Ctrl+C is pressed.

```text
nmeasim run [--profile FILE] [--duration SECONDS] [--rate MS] [--quiet]
            [--track FILE [--speed KN] [--ignore-timestamps] | --replay FILE [--replay-interval MS]]
            [--loop] [--record PATH]
            [--stdout] [--tcp-server PORT]... [--udp HOST:PORT]... [--websocket PORT]...
            [--serial DEVICE[@BAUD]]... [--file PATH]...
            [--enable ID]... [--disable ID]...
            [--encoding nmea0183|signalk|viewsync] [--tag-block [--tag-source ID]]
            [--destination LAT,LON[,NAME]]
```

| Option | Description |
| --- | --- |
| `-p`, `--profile FILE` | Profile to run. Without it the built-in default profile is used: a vessel off Athens with a TCP server on port 10110. |
| `-d`, `--duration SECONDS` | Stop after this many seconds. `0`, the default, runs until Ctrl+C. |
| `-r`, `--rate MS` | Send every sentence at this period, overriding the per-sentence periods of the profile. |
| `--track FILE` | Follow a GPX or KML [track file](track-files.md) instead of running the delta simulation. Sets the profile's mode to `track`. |
| `--speed KN` | Speed in knots along legs whose points have neither timestamps nor a recorded speed; also the speed of a timed track with `--ignore-timestamps`. Default: the profile's `simulation.track.speed_kn`, 6. |
| `--ignore-timestamps` | Sail a timed track at `--speed` instead of on its own timing. |
| `--replay FILE` | Replay a [log file](log-format.md), recorded by the simulator or by other software, instead of simulating. Sets the mode to `replay`. Excludes `--track`. |
| `--replay-interval MS` | Spacing between the sentences of a log that carries no time information at all. Default: the profile's `simulation.replay.fixed_interval_ms`, 100. |
| `--loop` | Start the track or log again when its end is reached. Without it the run ends there. |
| `--record PATH` | Record every emitted sentence with a timestamp to a [log file](log-format.md), in addition to the outputs. The file is truncated first. |
| `-q`, `--quiet` | Suppress the status lines written to standard error. |
| `--stdout` | Write sentences to standard output. |
| `--tcp-server PORT` | Serve sentences to any number of TCP clients on this port. Repeatable. |
| `--udp HOST:PORT` | Send one datagram per sentence. `255.255.255.255` selects broadcast. Repeatable. |
| `--websocket PORT` | Serve sentences as WebSocket text frames on this port. Repeatable. |
| `--serial DEVICE[@BAUD]` | Write to a serial device, 4800 baud unless given. Repeatable. |
| `--file PATH` | Append sentences to a file. Repeatable. |
| `--enable ID`, `--disable ID` | Turn a sentence on or off by registry id, for example `--enable MWV-T` or `--disable GSV`. Repeatable. |
| `--encoding ENC` | What the outputs given on the command line carry: `nmea0183` (default), `signalk` deltas or `viewsync` packets, see the [profile reference](profile.md#outputs). Outputs from a profile keep their own encoding. |
| `--tag-block` | Prefix every sentence of the command-line outputs with an IEC 61162-450 [TAG block](nmea0183-sentences.md#tag-blocks). |
| `--tag-source ID` | Source identifier of the TAG block, `SIM0001` unless given. |
| `--destination LAT,LON[,NAME]` | Steer for a waypoint so that APB, RMB and XTE are sent and the Signal K course paths appear; the leg starts at the seed position. |

When any output option is given, the outputs of the profile are replaced by those from the
command line; `--record` adds a log output in either case. Sentence options are applied on
top of the profile's sentence settings, and `--track` or `--replay` replace the profile's
simulation mode.

A track or a log that is not looped ends the run by itself: the last sentence is sent, the
message `end of the track or log reached` is printed unless `--quiet` is given, and the tool
exits with status `0`.

Status lines go to standard error, sentences go only to the outputs, so
`nmeasim run --stdout --quiet` produces a clean stream that can be piped.

Exit status: `0` on a normal stop, `2` for invalid arguments or profile, `3` when no output
could be opened. An output that fails while others succeed is reported as a warning and the
run continues.

### Examples

```bash
# Stream to a chart plotter listening on TCP 10110 for one minute
nmeasim run --tcp-server 10110 --duration 60

# Broadcast on the local network at 2 Hz
nmeasim run --udp 255.255.255.255:10110 --rate 500

# Pipe a clean stream into another program
nmeasim run --stdout --quiet --duration 5 | python3 tools/check_nmea_stream.py

# Sail a recorded track on its own timing and send it to a plotter
nmeasim run --track samples/saronic-gulf.gpx --tcp-server 10110

# Sail a route without timestamps at 8 knots, forever
nmeasim run --track samples/saronic-route.gpx --speed 8 --loop --udp 255.255.255.255:10110

# Record a session, then replay it later
nmeasim run --record monday.log --duration 600
nmeasim run --replay monday.log --tcp-server 10110

# Replay a log captured by another program, spacing untimed sentences 200 ms apart
nmeasim run --replay plotter-capture.txt --replay-interval 200 --stdout --quiet

# Serve Signal K deltas to a Signal K server or web instrument on the usual port
nmeasim run --websocket 3000 --encoding signalk

# Drive Google Earth on another machine, with a destination for the course paths
nmeasim run --udp 192.168.1.20:42000 --encoding viewsync --destination 37.7466,23.4275,AEGINA

# Send sentences with TAG blocks, as an IEC 61162-450 gateway would
nmeasim run --tcp-server 10110 --tag-block --tag-source GP0001

# Run a saved profile but only GNSS sentences on a serial port
nmeasim run --profile harbour.json --serial /dev/ttyUSB0@38400 \
    --disable HDG --disable HDM --disable HDT --disable ROT --disable VHW --disable VBW \
    --disable DPT --disable DBT --disable MTW --disable MWV-R --disable MWD --disable RSA
```

## `nmeasim profile`

| Subcommand | Description |
| --- | --- |
| `profile init PATH [--force]` | Write the default profile to `PATH`. Refuses to overwrite unless `--force` is given. |
| `profile show [PATH]` | Validate, migrate and print a profile as JSON. Without `PATH` prints the built-in default. |

The file format is documented in the [profile reference](profile.md).

## `nmeasim sentences`

Lists every sentence the simulator can emit with its registry id, formatter, default talker,
group, default state and description.

## `nmeasim ports`

Lists the serial ports present on this machine with the device path, the driver description
and the manufacturer where the operating system provides them. No filtering is applied.

```text
$ nmeasim ports
PORT                         DESCRIPTION                          MANUFACTURER
/dev/ttyUSB0                 USB-Serial Controller                Prolific Technology Inc.
/dev/ttyS0
```

## `nmeasim interfaces`

Lists every IPv4 address of every interface that is up, with its subnet broadcast address.
Use the interface name in a profile's UDP output to choose the source interface.
