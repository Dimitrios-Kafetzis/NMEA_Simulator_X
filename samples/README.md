# Sample data

Sample tracks and recorded logs used by the tutorials and the documentation. The files under
`tests/fixtures/` are the small, sometimes deliberately broken inputs used by the automated
tests; the files here are meant to be opened in the application.

| File | Contents |
| --- | --- |
| `saronic-gulf.gpx` | A timed GPX track from Piraeus past Aegina to Agistri at about 7 knots, in two segments |
| `saronic-route.gpx` | The Piraeus to Aegina passage as a GPX route without timestamps |
| `saronic-gulf.kml` | The same passage as a KML `gx:MultiTrack` |

Load a track with *File → Open track...* in the desktop application or with
`nmeasim run --track samples/saronic-gulf.gpx`. See the
[track file reference](../docs/reference/track-files.md) for what is read from each format.
