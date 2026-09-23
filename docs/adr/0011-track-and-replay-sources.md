# 0011 Track following and log replay as sources with a seekable transport interface

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

Milestone M3 adds two ways of driving the vessel besides the delta simulation: following a
GPX track, GPX route or KML track, and replaying a recorded log. The reference application
offers both, but stops at the end of a track, cannot sail a track that has no timestamps,
and can only pause and step a replay. How are these modes fitted into the engine so that the
desktop application, the command-line tool and the tests drive them through one interface,
and how are the files parsed inside a Qt-free `core`?

## Decision drivers

- ADR 0002: the engine and its parsers live in `src/core` without Qt, so the CLI, the
  desktop application and the Catch2 tests share them.
- The output rate must be independent of the point density of a file: a track with a point
  every minute must still produce a smooth position at 10 Hz.
- Pause, step and seek must work the same for a track and for a log, and hosts must not need
  to know which one is running to show a progress slider.
- GPX and KML are XML; a hand-written XML tokenizer would be a liability.
- The delta simulation must keep working unchanged.

## Considered options

1. Give `Source` a minimal state-producing interface (as in M1) and let each host special-case
   track and log playback.
2. Extend `Source` with a small transport interface, `duration()`, `position()` and
   `seek()`, that endless sources implement trivially, and add `TrackSource` and
   `ReplaySource` behind it.
3. Implement track and replay playback in `nmeasim::io` on top of Qt's XML and file classes.

For XML parsing: a hand-written parser, Qt XML (in `io`), or pugixml in `core`.

## Decision outcome

Option 2, with pugixml.

`Source` gains three virtual members with harmless defaults: `duration()` returns nullopt
for an endless source, `position()` returns zero and `seek()` does nothing. A finite source
reports its length in simulated time, its elapsed time, and moves when asked. `finished()`
already existed. The `SimulationRunner` and the desktop transport controls are written
against these members only, so the seek slider works for a track, a log and any future
finite source, and is simply disabled for the delta simulation.

`TrackSource` reduces every track to a table of legs, each with a duration. For a timed track
the duration is the difference of the end points' timestamps, otherwise the leg length
divided by the recorded point speed or the configured speed. The position on the current leg
is the geodesic interpolation for the elapsed fraction of that duration, so the density of the
file never shows in the output. Course over ground is the recorded course when the file
carries one, otherwise the initial bearing of the leg, and heading follows course. At the end
the source either stops with zero speed and reports `finished()`, or loops. Timestamps can be
ignored on request so that a recorded track is sailed at a chosen speed. Environment values
(depth, wind, GNSS quality) come from the profile seed and do not drift.

`ReplaySource` is the second finite source. It produces sentences rather than a
state to encode, so `Source` also gains `provides_sentences()` and `take_sentences()`; the
`Simulation` returns the replayed sentences instead of consulting the scheduler when a source
provides them, and decodes them into a vessel state so that the dashboard and the map keep
working during a replay.

GPX and KML are read with [pugixml](https://pugixml.org/), a small MIT-licensed DOM parser
added through vcpkg and linked privately into `core`. Element names are compared without
their namespace prefix, so files written with or without a prefix parse identically. All
segments of all tracks are concatenated in document order; GPX routes are used when a file
has no track; KML `gx:Track` and `LineString` geometries are both accepted. `<time>`,
`<course>` and `<speed>` are honoured, the last two also inside GPX 1.1 extensions.

Option 1 was rejected because it would duplicate playback logic in every host and make the
CLI and the desktop application diverge. Option 3 was rejected because it would put
simulation behaviour in the Qt layer and out of reach of the fast core tests. A hand-written
XML parser was rejected as more code to maintain than the feature it serves.

### Consequences

- `core` has a second third-party dependency, pugixml, next to GeographicLib.
- Hosts implement pause, step and seek once, through `Source`.
- A track without timestamps needs a speed; the profile carries it and defaults to 6 knots.
- A timed track that loops rewinds the simulated clock to the track's first timestamp; a
  track without timestamps keeps a monotonic clock from the profile start time.
- Tracks and logs are read into memory in full; files of hundreds of thousands of points are
  fine, streaming multi-gigabyte logs is out of scope.

## More information

- [Track file reference](../reference/track-files.md)
- [Simulation model: track mode](../explanation/simulation-model.md#track-mode)
