# Track files

The simulator can follow a track read from a GPX or KML file. This page lists exactly what
is read from each format. How the vessel moves along the points is explained in the
[simulation model](../explanation/simulation-model.md#track-mode); how to load a track is
in the [how-to guide](../how-to/follow-a-track.md).

## Common rules

- The file type is chosen from the extension of the file name, `.gpx` or `.kml`,
  case-insensitively; a dot in a directory name does not count. A file name without an
  extension, or with another one, is rejected with a message that says so.
- Namespace prefixes are ignored: `trkpt` and `gpx:trkpt` are the same element.
- All geometries of a file are concatenated in document order into one track. Nothing
  is reordered or deduplicated.
- Latitude must be within ±90 and longitude within ±180 degrees; anything else is
  rejected with the point number. Points are numbered from 1 across the whole file, over
  all segments, tracks or geometries, in GPX and KML alike.
- Optional values that are not numbers, a GPX `<ele>`, `<course>` or `<speed>` or a KML
  altitude such as `high`, are ignored without an error: the point is read as if the value
  were absent.
- A track is **timed** when every point has a time and the times never decrease. One
  missing or out-of-order time makes the whole track untimed, and it is then sailed at the
  configured speed.
- Times are ISO 8601: `2026-09-23T10:00:00Z`, with optional fraction of a second and an
  optional `±hh:mm` offset. A time without a `Z` or offset is taken as UTC.
- A file that is not well-formed XML, has the wrong root element or contains no usable points
  is rejected with a one-line reason that names the offending point where possible.

## GPX

GPX 1.0 and 1.1 are read the same way.

| Element | Used as |
| --- | --- |
| `<trk>` / `<trkseg>` / `<trkpt lat="" lon="">` | Track points, every segment of every track, in order |
| `<rte>` / `<rtept lat="" lon="">` | Route points, used only when the file has no track points |
| `<trkpt>/<ele>` | Altitude in metres, interpolated between points |
| `<trkpt>/<time>` | Timestamp of the point |
| `<trkpt>/<course>` | Course over ground in degrees true (GPX 1.0) |
| `<trkpt>/<speed>` | Speed over ground in metres per second (GPX 1.0), converted to knots |
| `<extensions>/…/<speed>`, `<extensions>/…/<course>` | The same values in GPX 1.1 extensions, matched on the element name whatever the prefix, e.g. `gpxtpx:speed` |
| `<metadata>/<name>`, `<name>`, `<trk>/<name>`, `<rte>/<name>` | Track name, the first non-empty one in that order; the file name when none. `<name>` is the one directly under `<gpx>`, as GPX 1.0 writes it |
| `<wpt>` | Ignored |

A recorded speed is reported as the vessel speed while sailing the leg that starts at that
point. On an untimed track the recorded speed also sets the pace of that leg; on a timed
track the pace always comes from the timestamps.

## KML

KML 2.2 with the Google extension namespace `gx` is supported.

| Element | Used as |
| --- | --- |
| `<gx:Track>` | Track points: each `<gx:coord>` (`lon lat [alt]`, space separated) paired with the `<when>` at the same index |
| `<gx:MultiTrack>` | Its tracks, in order |
| `<LineString>/<coordinates>` | Untimed points: `lon,lat[,alt]` tuples separated by whitespace |
| `<MultiGeometry>`, `<Folder>`, `<Document>`, `<Placemark>` | Traversed; every track and line inside is used |
| `<Placemark>/<name>`, `<Document>/<name>`, `<Folder>/<name>` | Track name: the name of the placemark that holds the first track or line; else the name of the first named document or folder; while both are missing, the next track or line is tried, and the file name is used when none gives a name |
| `<Point>`, `<Polygon>`, styles, `<TimeStamp>`, `<gx:angles>` | Ignored |

The file is a *KML track* when at least one `gx:Track` provides points, and a *KML line*
otherwise, even when it holds an empty `gx:Track`. The third coordinate, when present, is
the altitude in metres. A `gx:Track` with fewer
`<when>` than `<gx:coord>` elements leaves the remaining points without a time, which makes
the track untimed.

## Samples

The repository ships sample files under `samples/`:

| File | Contents |
| --- | --- |
| `saronic-gulf.gpx` | A timed GPX track from Piraeus around Aegina, two segments |
| `saronic-route.gpx` | The same passage as a GPX route without times |
| `saronic-gulf.kml` | The passage as a KML `gx:Track` |
