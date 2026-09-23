# Track files

The simulator can follow a track read from a GPX or KML file. This page lists exactly what
is read from each format. How the vessel moves along the points is explained in the
[simulation model](../explanation/simulation-model.md#track-mode).

## Common rules

- The file type is chosen from the extension, `.gpx` or `.kml`, case-insensitively.
- Namespace prefixes are ignored: `trkpt` and `gpx:trkpt` are the same element.
- All geometries of a file are concatenated in document order into one track. Nothing
  is reordered or deduplicated.
- Latitude must be within ±90 and longitude within ±180 degrees; anything else is
  rejected with the point number.
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
| `<metadata>/<name>`, `<name>`, `<trk>/<name>`, `<rte>/<name>` | Track name, first one found in that order; the file name when none |
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
| `<Placemark>/<name>`, `<Document>/<name>` | Track name of the first placemark holding geometry, else the document name |
| `<Point>`, `<Polygon>`, styles, `<TimeStamp>`, `<gx:angles>` | Ignored |

The third coordinate, when present, is the altitude in metres. A `gx:Track` with fewer
`<when>` than `<gx:coord>` elements leaves the remaining points without a time, which makes
the track untimed.

## Samples

The repository ships sample files under `samples/`:

| File | Contents |
| --- | --- |
| `saronic-gulf.gpx` | A timed GPX track from Piraeus around Aegina, two segments |
| `saronic-route.gpx` | The same passage as a GPX route without times |
| `saronic-gulf.kml` | The passage as a KML `gx:Track` |
