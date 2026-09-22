# Command-line tool

`nmeasim` is the headless simulator. It links the same engine and transports as the desktop
application and needs no display.

## Synopsis

```text
nmeasim [OPTIONS] [SUBCOMMAND]
```

## Options

| Option | Description |
| --- | --- |
| `-h`, `--help` | Print help and exit |
| `-V`, `--version` | Print the project name and version and exit |

## Subcommands

### `ports`

Lists the serial ports present on this machine, one per line, with the device path,
the driver description and the manufacturer where the operating system provides them.
No filtering is applied.

```text
$ nmeasim ports
PORT                         DESCRIPTION                          MANUFACTURER
/dev/ttyUSB0                 USB-Serial Controller                Prolific Technology Inc.
/dev/ttyS0
```

Subcommands to run a simulation profile, follow a track and replay a log are added in
milestone M1.
