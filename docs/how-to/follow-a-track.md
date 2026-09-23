# Follow a track

Make the simulated vessel sail a GPX track, a GPX route or a KML track. The
[track file reference](../reference/track-files.md) lists what is read from each format
and the [simulation model](../explanation/simulation-model.md#track-mode) explains how the
vessel moves between the points.

## From the command line

```bash
nmeasim run --track samples/saronic-gulf.gpx --tcp-server 10110
```

- A **timed** track (every point has a `<time>`) is sailed on its own timing; speed and
  course follow from the timestamps unless the points carry `<speed>` and `<course>`.
- An **untimed** track or a **route** is sailed at `--speed` knots (6 by default), or at
  the recorded point speeds when the file has them.
- `--ignore-timestamps` sails a timed track at `--speed` instead.
- `--loop` starts again at the first point; without it the run ends at the last point.

## From a profile

Set the mode and the file in the `simulation` object, then run the profile as usual:

```json
"simulation": {
  "mode": "track",
  "track": { "path": "saronic-gulf.gpx", "speed_kn": 7.5, "use_timestamps": true, "loop": false }
}
```

A relative `path` is resolved against the directory that holds the profile, so a profile
and its track can be moved together. Everything the track does not carry, such as depth,
wind and the GNSS quality, comes from the profile's `seed`; the `variation` values are not
used while following a track. See the [profile reference](../reference/profile.md#simulationtrack).

## Checking what will be sailed

The track is loaded when the profile is applied, and a file that cannot be read or has no
points stops the run with a message naming the point at fault. A quick way to check a file
is to sail it to standard output for a few seconds:

```bash
nmeasim run --track my-track.gpx --stdout --quiet --duration 3 | head
```
