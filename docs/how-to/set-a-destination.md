# Set a destination

Make the simulator steer for a waypoint so that the autopilot sentences APB, RMB and XTE
and the Signal K course paths describe the leg towards it. The sentences are listed on the
[sentence reference](../reference/nmea0183-sentences.md#autopilot) and the geometry is
explained in the [simulation model](../explanation/simulation-model.md#destination).

The vessel does not turn towards the destination by itself: steer it with the keyboard or
sail a track, and watch the cross-track error change.

## On the map

Hold ++shift++ and click the spot on the map, or right-click it and choose *Set destination
here*. A magenta diamond marks the waypoint, a dashed line shows the bearing from the vessel
and the *Destination* tile on the dashboard shows the bearing, the distance and the
cross-track error with the side to steer. The leg starts where the vessel was at that moment,
so the cross-track error is zero until the vessel leaves the line.

*Simulation → Clear destination*, or the same entry in the map's right-click menu, stops the
autopilot sentences.

The waypoint is named `WPT`. To rename it, or to type coordinates, open *File → Settings...*
and use the *Destination* group of the *Simulation* tab, which also sets the arrival circle
radius (100 m by default). Save the profile to keep the destination.

## From the command line

```bash
nmeasim run --tcp-server 10110 --destination 37.7466,23.4275,AEGINA
```

The leg starts at the profile's initial position. A profile carries the same information in
`simulation.seed.destination` ([reference](../reference/profile.md#simulationseeddestination)):

```json
"destination": { "name": "AEGINA", "latitude": 37.7466, "longitude": 23.4275,
                 "origin_latitude": 38.0, "origin_longitude": 23.7, "arrival_radius_m": 100 }
```

## What the consumer sees

- **APB** carries the cross-track error, the bearing of the leg and the bearing to the
  waypoint, and flags the arrival circle and the perpendicular at the waypoint.
- **RMB** adds the waypoint position, the range and the closing velocity.
- **XTE** carries the cross-track error alone.
- **Signal K** outputs publish `navigation.courseRhumbline.*`, including
  `nextPoint.position`, `nextPoint.distance` and `crossTrackError`.

OpenCPN shows the waypoint and the cross-track error on its dashboard instruments when an
autopilot sentence arrives; a Signal K server shows the course paths in its data browser.
