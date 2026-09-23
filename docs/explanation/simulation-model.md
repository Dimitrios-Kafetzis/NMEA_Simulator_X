# Simulation model

This page explains how the delta simulation mode produces a believable vessel from a handful
of seed values, how track mode moves the vessel along a file, and how sentences are scheduled
from the resulting state.

## Delta mode

The operator supplies a **seed state**: position, heading, speed, depth, water temperature,
true wind and a UTC start time. On every tick the source applies these rules, in order:

1. **Time** advances by the tick length.
2. **Heading** either follows the rudder (steering mode), stays pinned (override), or
   performs a bounded random walk around the seed heading. The walk step is at most
   `step_per_second × tick` degrees and the excursion never exceeds `amplitude` degrees from
   the seed, measured on an unwrapped angle so the bound also works across north. The rate of
   turn is derived from the heading change.
3. **Speed over ground** performs the same bounded walk around its seed unless overridden.
   Speed through water equals speed over ground because the model has no current or leeway.
4. **Depth, water temperature, true wind direction and true wind speed** perform bounded
   walks around their seeds unless overridden.
5. **Course over ground** equals the heading.
6. **Position** is moved along the course by `speed × tick` using the WGS84 geodesic direct
   solution, so the track is exact on the ellipsoid rather than a flat-earth approximation.
7. **Apparent wind** is recomputed from the true wind vector and the vessel's velocity
   through the water.

A variation with zero amplitude freezes that value. The random generator is seeded from the
profile, so the same profile always produces the same run, which keeps replays and tests
deterministic.

### Steering mode

In steering mode the rudder angle drives the rate of turn linearly:
`rate_of_turn = rudder_angle × turn_rate_per_rudder_deg` degrees per minute. With the default
gain of 0.6, ten degrees of rudder turn the vessel six degrees per minute. The rudder is
clamped to the configured maximum, 35 degrees by default.

### Overrides and nudges

Every controllable parameter can be **overridden** (pinned to a value, drift stops) or
**nudged** (moved by a delta and then pinned). Keyboard arrows in the desktop application
are nudges: up and down nudge speed, left and right nudge heading, or the rudder when
steering mode is on. Clearing an override lets the value drift again from where it is.

### Apparent wind

With the true wind blowing *from* direction `D` at speed `W` and the vessel moving on
heading `H` at speed `V` through the water, the apparent wind velocity is the true wind
velocity minus the vessel velocity. Its magnitude is the apparent wind speed and the
direction it comes from, minus the heading, is the apparent wind angle reported by MWV.
A head wind at 10 knots on a vessel making 5 knots is felt as 15 knots from dead ahead.

## Track mode

In track mode the vessel follows a [GPX or KML file](../reference/track-files.md). The
source reduces the file to a table of **legs**, one per pair of consecutive points, and gives
every leg a duration:

- On a **timed** track (every point has a timestamp and the times never decrease) the
  duration of a leg is the difference between the timestamps of its two points. The speed
  shown is the recorded point speed when the file has one, otherwise the leg length divided
  by its duration.
- On an **untimed** track the duration is the leg length divided by the speed to sail it at:
  the recorded point speed when the file has one, otherwise the speed configured in the
  profile (6 knots by default). Timestamps can also be ignored on request so that a recorded
  track is sailed at a chosen speed.

On every tick the elapsed time along the track grows by the tick length. The current leg is
found from the elapsed time, and the position is the WGS84 geodesic point at the elapsed
fraction of the leg length, so the file's point density never shows in the output: a track
with a point every minute still yields a smooth position at 10 Hz. Course over ground is the
recorded course of the leg's start point when there is one, otherwise the leg's initial
bearing; heading equals course and the rate of turn follows from the heading change.
Altitude is interpolated between points that carry an elevation.

The simulated clock follows the track's own timestamps on a timed track, and the profile
start time plus elapsed time otherwise. Depth, water temperature, wind and GNSS quality come
from the profile seed and do not drift; apparent wind is recomputed from the vessel's motion.

At the end of the track the source either **stops**, holding the last point with zero speed
and reporting that it is finished so that the run ends, or **loops** back to the first point.
A timed track that loops rewinds the clock to its first timestamp.

The operator can jump to any point and seek to any elapsed time; the state is recomputed at
once from the leg table, so seeking is as cheap as a tick.

## Sentence scheduling

Each sentence has its own **period** and **enabled** flag, seeded from the registry
defaults. The scheduler runs on the simulated clock:

- Everything enabled is due on the first tick.
- A sentence is due again once its period has elapsed since it was last emitted.
- The next emission is scheduled relative to the actual emission time, not the missed slot,
  so a host that stalls for ten seconds emits one round of sentences afterwards rather than
  ten.
- Sentences are returned in registry order, so a client always sees RMC before GGA within a
  round.

The host tick (100 ms by default) bounds the scheduling resolution: a 50 ms period requires a
50 ms tick. The desktop application refreshes its display on a separate, slower timer.
