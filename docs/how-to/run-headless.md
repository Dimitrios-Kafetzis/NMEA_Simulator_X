# Run the simulator headless

The command-line tool runs the full simulation without a display. Typical uses are feeding a
chart plotter on another machine, generating test data in CI, or producing a log file.

## Feed a chart plotter over TCP

1. Find the address of this machine: `nmeasim interfaces`.
2. Start the stream: `nmeasim run --tcp-server 10110`.
3. In the plotter, add a TCP connection to that address on port 10110. OpenCPN: *Options →
   Connections → Add Connection → Network, TCP, port 10110*.
4. Stop with Ctrl+C.

## Broadcast to every device on the network

```bash
nmeasim run --udp 255.255.255.255:10110
```

To send from a specific interface, or to derive the subnet broadcast address instead of the
limited one, use a profile with a `udp` output whose `interface` names the interface and whose
`address` is empty.

## Save a profile and tune it

```bash
nmeasim profile init harbour.json
```

Edit the file (see the [profile reference](../reference/profile.md)), then:

```bash
nmeasim run --profile harbour.json
```

Command-line output options replace the profile's outputs for that run, so the same profile
can go to TCP in one run and to a serial port in the next.

## Follow a recorded track

```bash
nmeasim run --track samples/saronic-gulf.gpx --tcp-server 10110
```

The vessel sails the GPX or KML file on its own timestamps and the run ends at the last
point. Add `--loop` to sail it again and again, `--speed 8 --ignore-timestamps` to sail it
at a set speed, or use a route file without timestamps, which is always sailed at `--speed`.
See [Follow a track](follow-a-track.md) for the details and the profile keys.

## Record a session

```bash
nmeasim run --record monday.log --tcp-server 10110 --duration 3600
```

`--record` writes every sentence with a timestamp to the file, in addition to the outputs;
the file is truncated at the start. To record only some sentences, use a profile with a
`log` output and a `filter` instead.

## Replay a log

```bash
nmeasim run --replay monday.log --tcp-server 10110
```

The sentences are sent again, unchanged, on their original cadence, and the run ends when
the log does. A log captured by another program works the same way: lines that carry a
timestamp use it, lines that do not are timed from the RMC, GGA, GLL or ZDA time fields, and
a log without any time information is played at a fixed interval (`--replay-interval`).
The [log file reference](../reference/log-format.md) lists the accepted line shapes.

## Generate a fixture for automated tests

```bash
nmeasim run --stdout --quiet --duration 10 --rate 1000 > fixture.nmea
```

The stream is deterministic for a given profile and random seed, apart from the timestamps
when `start_time` is `"now"`. Set `start_time` in the profile for fully reproducible files.

## Validate the stream with an independent parser

The repository ships `tools/check_nmea_stream.py`, which checks every sentence with the
third-party `pynmea2` parser:

```bash
pip install pynmea2
nmeasim run --stdout --quiet --duration 3 | python3 tools/check_nmea_stream.py --expect 21
```

CI runs this on every platform.

## Run in the background on Linux

```bash
nohup nmeasim run --profile harbour.json --quiet > /dev/null 2> nmeasim.err &
```

The tool stops cleanly on `SIGINT` and `SIGTERM`, so `kill %1` or a systemd unit with the
default stop signal is enough.
