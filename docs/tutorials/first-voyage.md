# Your first simulated voyage

In this tutorial you start the desktop application, watch it emit NMEA 0183 sentences, steer
the vessel from the keyboard and connect a chart plotter to the stream. It takes about ten
minutes and needs nothing but the application itself.

## 1. Start the application

Launch *NMEA Simulator X*. The window opens with the default profile: a vessel off Athens,
every standard sentence enabled and one TCP server output on port 10110.

The window has four areas:

- The **dashboard** in the centre shows the position, time, GNSS status, heading, speeds,
  depth, water temperature, altitude and wind as instrument tiles.
- The **Console** panel at the bottom lists every sentence as it is sent.
- The **Outputs** panel on the right lists each configured output with its state, the number
  of connected clients and the sentences and bytes sent.
- The **toolbar** holds the profile and simulation actions.

Panels can be dragged to another edge, floated or closed from the *View* menu. The layout is
remembered the next time you start.

## 2. Start the simulation

Press **Start** in the toolbar or press ++f5++. The status bar changes to *Running*, the
console starts scrolling and the *Outputs* panel shows the TCP server as *open*.

Type `RMC` in the console filter to show only the recommended minimum sentence. Tick *Pause*
in the console to freeze the view while the simulation keeps running; untick it to catch up.

## 3. Take the helm

Click anywhere on the dashboard so the window has keyboard focus, then:

| Key | Effect |
| --- | --- |
| ++up++ / ++down++ | Speed over ground plus or minus 0.1 kn |
| ++shift+up++ / ++shift+down++ | Speed over ground plus or minus 1 kn |
| ++left++ / ++right++ | Heading minus or plus 1 degree |
| ++shift+left++ / ++shift+right++ | Heading minus or plus 10 degrees |

Every nudge pins the value: the *Override* box of that tile becomes ticked and the value stops
drifting. Untick the box to let it drift again.

Now enable **Steering mode** in the toolbar. The arrow keys move the rudder instead of the
heading, and the vessel turns at a rate proportional to the rudder angle, as a real hull would.
Watch the *Rate of turn* tile.

## 4. Override an instrument

Tick *Override* on the *Depth* tile and type `4.5`. The next DPT and DBT sentences in the
console carry the new depth. Untick *Fix* on the *GNSS* tile: RMC and GLL switch to their
void form and GGA reports quality 0, exactly as a receiver that lost its fix would.

## 5. Connect a chart plotter

Any application that accepts NMEA 0183 over TCP can read the stream. With OpenCPN on the
same machine:

1. Open *Options → Connections → Add Connection*.
2. Choose *Network*, protocol *TCP*, address `127.0.0.1`, port `10110`.
3. Apply. The own-ship symbol appears off Piraeus and moves with your keyboard input.

To check the stream without a plotter, on Linux or macOS run:

```bash
nc 127.0.0.1 10110
```

## 6. Save your setup

Choose *File → Save profile as...* and store the profile as `first-voyage.json`. The file
holds the seed values, the sentence schedule and the outputs, and can be run headless later
with `nmeasim run --profile first-voyage.json`. The application reopens the last profile on
the next launch.

## Where next

- [Desktop application reference](../reference/desktop-app.md) lists every action, shortcut
  and panel.
- [Run the simulator headless](../how-to/run-headless.md) does the same job from a terminal.
- [Profile file format](../reference/profile.md) explains the file you just saved.
