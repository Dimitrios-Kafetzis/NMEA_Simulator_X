# Connect a Signal K server

Feed a [Signal K](https://signalk.org/) server, or any Signal K client such as a web
instrument panel, with deltas built from the simulated vessel. The message and path layout is
on the [Signal K reference page](../reference/signalk.md).

## Serve deltas over WebSocket

Add a *WebSocket server* output with the `signalk` encoding. From the command line:

```bash
nmeasim run --websocket 3000 --encoding signalk
```

In the desktop application add a *WebSocket server* output on the *Outputs* tab of *File →
Settings...*, set its *Encoding* to *Signal K* and its port to 3000. Every client that
connects receives the *hello* message and then one delta per output period (1 s by default;
lower it in the output's *Period* field for smoother instruments).

Check the stream from a terminal with any WebSocket client, or with the check script shipped
in the repository:

```bash
nmeasim run --stdout --encoding signalk --quiet --duration 3 | python3 tools/check_signalk_stream.py
```

## Feed a Signal K server

The Signal K server reads NMEA 0183 and Signal K deltas from TCP, UDP and serial
connections through its *Data Connections* page.

1. In the server's admin interface open *Server → Data Connections → Add*.
2. Choose *Data Type: Signal K*, *Source: TCP client* (or *WebSocket*), enter the address
   of the machine running the simulator and the port of the output.
3. For a TCP source configure a *TCP server* output with the `signalk` encoding in the
   simulator, for example:

    ```json
    { "type": "tcp-server", "port": 8375, "encoding": "signalk", "period_ms": 500 }
    ```

4. Restart the server. The vessel appears in the *Data Browser* under the context
   `vessels.urn:mrn:imo:mmsi:239000001`, or whatever context the output is set to.

To feed the same server with NMEA 0183 instead, add a plain *TCP server* output on port
10110 and a *Data Type: NMEA 0183* connection; both can run at the same time.

## Choose the context

By default the deltas belong to `vessels.urn:mrn:imo:mmsi:<MMSI>` with the MMSI from the AIS
static data of the profile. Set `signalk.context` on the output (or the *Context* field in
the settings dialog) to another vessel URN, or to an aircraft context such as
`aircraft.urn:mrn:signalk:uuid:...` for a flight simulator feed; the hello message's `self`
follows the same setting.

## Send only some paths

The output's sentence filter lists path prefixes for a Signal K output: `navigation`
sends every navigation path, `environment.wind, propulsion` the wind and the engines.
