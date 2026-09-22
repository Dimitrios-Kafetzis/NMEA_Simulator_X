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
            [--stdout] [--tcp-server PORT]... [--udp HOST:PORT]... [--websocket PORT]...
            [--serial DEVICE[@BAUD]]... [--file PATH]...
            [--enable ID]... [--disable ID]...
```

| Option | Description |
| --- | --- |
| `-p`, `--profile FILE` | Profile to run. Without it the built-in default profile is used: a vessel off Athens with a TCP server on port 10110. |
| `-d`, `--duration SECONDS` | Stop after this many seconds. `0`, the default, runs until Ctrl+C. |
| `-r`, `--rate MS` | Send every sentence at this period, overriding the per-sentence periods of the profile. |
| `-q`, `--quiet` | Suppress the status lines written to standard error. |
| `--stdout` | Write sentences to standard output. |
| `--tcp-server PORT` | Serve sentences to any number of TCP clients on this port. Repeatable. |
| `--udp HOST:PORT` | Send one datagram per sentence. `255.255.255.255` selects broadcast. Repeatable. |
| `--websocket PORT` | Serve sentences as WebSocket text frames on this port. Repeatable. |
| `--serial DEVICE[@BAUD]` | Write to a serial device, 4800 baud unless given. Repeatable. |
| `--file PATH` | Append sentences to a file. Repeatable. |
| `--enable ID`, `--disable ID` | Turn a sentence on or off by registry id, for example `--enable MWV-T` or `--disable GSV`. Repeatable. |

When any output option is given, the outputs of the profile are replaced by those from the
command line; sentence options are applied on top of the profile's sentence settings.

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
