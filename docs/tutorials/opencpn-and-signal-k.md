# Feeding OpenCPN and a Signal K server at once

In this tutorial you build one profile with two outputs: NMEA 0183 sentences for the
[OpenCPN](https://opencpn.org/) chart plotter and Signal K deltas for a
[Signal K server](https://github.com/SignalK/signalk-server), both from the same simulated
vessel at the same time. You then watch the vessel in both programs, steer it and see both
follow. It takes about twenty minutes.

You need NMEA Simulator X ([install it](../how-to/install.md)), OpenCPN 5.8 or later and a
Signal K server. Everything can run on one computer; the steps say where addresses change
when it does not.

## 1. Start a Signal K server

If you do not have one yet, the quickest way is the official container image:

```bash
docker run -d --name signalk -p 3000:3000 signalk/signalk-server
```

or, with Node.js 20 or later installed, `npm install -g signalk-server` followed by
`signalk-server`. Open <http://localhost:3000> in a browser and create the administrator
account when asked. The server's own port is 3000, so the simulator must not use it.

## 2. Build the profile

The profile needs two outputs, each with its own encoding:

| Output | Type | Port | Encoding | Consumer |
| --- | --- | --- | --- | --- |
| 1 | TCP server | 10110 | NMEA 0183 | OpenCPN |
| 2 | TCP server | 8375 | Signal K, every 500 ms | Signal K server |

=== "Desktop application"

    1. Start *NMEA Simulator X*. The default profile already has the first output, a TCP
       server on port 10110 carrying NMEA 0183.
    2. Open *File → Settings...* and the *Outputs* tab. Click *Add*, choose *TCP server*,
       set *Port* to `8375`, *Encoding* to *Signal K deltas* and *Period* to `500` ms. Leave
       the *Signal K* context on the vessel with the AIS MMSI.
    3. On the *Simulation* tab set *Name* to `OpenCPN and Signal K`, then click *OK*.
    4. Save the profile with *File → Save profile as...*, for example as
       `opencpn-and-signal-k.json`.

=== "Command line"

    `--encoding` applies to every output given on the command line, so two outputs with
    different encodings need a profile. Create the default one and replace its `outputs`:

    ```bash
    nmeasim profile init opencpn-and-signal-k.json
    ```

    ```json
    "name": "OpenCPN and Signal K",
    "outputs": [
      { "type": "tcp-server", "port": 10110 },
      { "type": "tcp-server", "port": 8375, "encoding": "signalk", "period_ms": 500 }
    ],
    ```

    Check it with `nmeasim profile show opencpn-and-signal-k.json`, which prints the
    profile with every default filled in, or an error that names the wrong key.

## 3. Start the stream

=== "Desktop application"

    Press **Start** (++f5++). The *Outputs* panel lists both TCP servers as *open* with no
    clients yet.

=== "Command line"

    ```bash
    nmeasim run --profile opencpn-and-signal-k.json
    ```

    The tool prints one line per output and keeps running until ++ctrl+c++:

    ```text
    output: TCP server on 0.0.0.0:10110
    output: TCP server on 0.0.0.0:8375
    running profile 'OpenCPN and Signal K' with a 100 ms tick; press Ctrl+C to stop
    ```

If OpenCPN or the Signal K server runs on another computer, note this computer's address
with `nmeasim interfaces` and allow the two ports through its firewall.

## 4. Connect OpenCPN

1. In OpenCPN open *Options → Connections* and click *Add Connection*.
2. Choose *Network*, protocol *TCP*, address `localhost` (or this computer's address) and
   port `10110`. Leave the input filter empty and click *OK*.
3. Close the options. Within a second the own-ship icon appears off Athens, the GPS status
   in the toolbar turns green and the heading and course lines point north-east.

The *Outputs* panel, or the table in the desktop application, now shows one client on port
10110.

## 5. Connect the Signal K server

1. In the Signal K administration page open *Server → Data Connections* and click *Add*.
2. Set *Data Type* to *Signal K*, *Signal K Source* to *TCP*, *Host* to `localhost` (or
   this computer's address) and *Port* to `8375`. Give it an *ID* such as `nmeasim` and
   click *Apply*, then restart the server when it asks.
3. Open *Data Browser*. The simulated vessel appears under the context
   `vessels.urn:mrn:imo:mmsi:239000001`, with `navigation.position`,
   `navigation.speedOverGround`, `environment.depth.belowTransducer`, the wind and the two
   engines under `propulsion.port` and `propulsion.starboard`, updated twice a second.

The deltas belong to the MMSI of the profile's AIS data, 239000001 by default. To make the
simulated vessel the server's own vessel, so that plugins and the instrument panel use it,
enter the same MMSI under *Server → Settings → Vessel*.

## 6. Steer and watch both follow

Click the dashboard of the desktop application and press ++right++ a few times: the heading
override rises by one degree per press. OpenCPN's own ship turns, and in the Data Browser
`navigation.headingTrue` and `navigation.courseOverGroundTrue` change with it. Press ++up++
to speed up and watch `navigation.speedOverGround` follow. Both consumers read the same
simulated vessel; only the encoding differs.

To try the autopilot paths, hold ++shift++ and click the map to set a destination. OpenCPN
then receives APB, RMB and XTE, and the server gets
`navigation.courseRhumbline.nextPoint` and `navigation.courseRhumbline.crossTrackError`.

## 7. Filter what each consumer gets

Each output has its own filter. On the *Outputs* tab, select the Signal K output and enter
`navigation, environment.depth` as *Sentence filter*: the server now receives only the
navigation and depth paths. The filter of the NMEA 0183 output takes sentence ids instead,
for example `RMC, GGA, HDT, DPT` for a plotter that only needs a fix, heading and depth.

## What you built

One profile that feeds two different kinds of consumer from the same simulation. The same
pattern adds a third output, for example a UDP broadcast for tablets or a ViewSync output for
Google Earth, without affecting the other two.

## Where next

- [Profile file format](../reference/profile.md#outputs): every output key.
- [Signal K output](../reference/signalk.md): the delta layout and every path.
- [Connect a Signal K server](../how-to/signal-k.md): WebSocket output, contexts and filters.
- [Set a destination](../how-to/set-a-destination.md) for the autopilot sentences.
