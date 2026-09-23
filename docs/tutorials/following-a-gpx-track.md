# Following a GPX track

In this tutorial you make the simulated vessel sail a track recorded on a real trip, watch
it on the map, jump around in it and hand the stream to a chart plotter. It takes about ten
minutes. You need the desktop application and a GPX file; the repository ships one under
`samples/saronic-gulf.gpx`, a passage from Piraeus past Aegina to Agistri.

## 1. Open the track

Start *NMEA Simulator X* and choose *File → Open track...* (++ctrl+t++). Pick
`saronic-gulf.gpx`. The current profile switches to track mode: the status bar reads
*Stopped - Default (track saronic-gulf.gpx)*, the map shows the track as a green line with a
marker on every point, and the vessel sits on its first point off Piraeus. The seek slider
in the toolbar becomes active and the label next to it shows `00:00 / 3:01:36`, the length
of the track from its timestamps.

Notice that the override boxes on the dashboard are greyed out: while a file drives the
vessel, the position, course and speed come from the file.

## 2. Sail it

Press **Start** (++f5++). The vessel leaves the harbour at the pace the track was recorded,
about seven knots, and the red sailed track grows over the green one. The *Time (UTC)* tile
shows the time recorded in the file rather than the wall clock, because the track has
timestamps. Type `RMC` in the console filter to see position, speed and course change with
every sentence.

## 3. Jump ahead

Drag the seek slider to about two thirds of the way. The vessel jumps to that point of the
track at once, the label shows the new elapsed time and the stream continues from there.
Press **Step** (++f7++): the run pauses and each further press advances the simulation by
one tick. Untick *Pause* (++f6++) to continue.

## 4. Sail it your way

Open *File → Settings...*. In the *Mode* group the track is already selected. Untick
*Follow the track's own timestamps* and set *Speed without timestamps* to 20 knots, then
press *OK*: the same track is now sailed at twenty knots and the clock follows the wall
clock. Tick *Start again at the end* to let it loop.

A route file has no timestamps at all: open `samples/saronic-route.gpx` to see the vessel
sail from waypoint to waypoint at the configured speed.

## 5. Connect a plotter

The TCP server on port 10110 is still part of the profile, so a chart plotter connected as
in [Your first simulated voyage](first-voyage.md#6-connect-a-chart-plotter) sees the vessel
follow the track. This is a convenient way to replay a real trip through software under
test, with the timing, positions and courses of the original.

## 6. Keep it

*File → Save profile as...* stores the mode, the file and the options with the profile, so
`nmeasim run --profile passage.json` sails the same track headless. The file path is saved
as given; keep the track next to the profile and use a relative path in the file to move
them together.

## Where next

- [Track files](../reference/track-files.md) lists exactly what is read from GPX and KML.
- [Follow a track](../how-to/follow-a-track.md) covers the command line and the profile keys.
- [Recording and replaying a log](recording-and-replaying-a-log.md) captures a session and
  plays it back.
