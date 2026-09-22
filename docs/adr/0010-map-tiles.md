# 0010 Map view: painted raster tiles with a disk cache

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

The desktop application shows the vessel on a map, lets the operator drag and zoom it, and
lets them move the vessel by pointing at a place. ADR 0001 ruled out QtLocation (QML only)
and QtWebEngine (a web runtime), and foresaw a custom tile widget. The reference application
depends on an internet connection for its map and fails without one, which is one of the
gaps this project set out to close. How are map tiles obtained, drawn and kept?

## Decision drivers

- The map must work with no network at all, on the tiles that were seen before.
- The tile server's usage policy must be respected: a real `User-Agent`, caching, and no
  bulk downloading.
- The widget must be testable offscreen without touching the network.
- Painting must stay cheap at the simulation's refresh rate.
- No new third-party dependency for something this small.

## Considered options

1. A `QGraphicsView` scene holding one pixmap item per tile.
2. A plain `QWidget` that computes the tiles covering its viewport in `paintEvent` and
   draws them with `QPainter`, with overlays drawn in the same pass.
3. Vector tiles rendered locally.

## Decision outcome

Option 2. A tile map is a pure function of centre, zoom and viewport size, so a painter
that walks the visible tile range is simpler and faster than keeping a scene graph in step
with the view, and the vessel, the track and the attribution are drawn in the same pass.
This refines the "tile widget on `QGraphicsView`" note in ADR 0001; the rest of that
decision stands.

Tiles follow the slippy map scheme (`{z}/{x}/{y}.png`, Web Mercator, 256 pixels). A
`TileCache` serves them from an in-memory `QCache`, then from `<cache dir>/tiles/z/x/y.png`
on disk, and finally by downloading from a configurable URL template that defaults to the
OpenStreetMap tile server. Downloads run at most four at a time, carry a `User-Agent` that
names the application and its project page, and every tile fetched is written to disk, so
the map keeps working offline for every area visited before. When a tile is missing the
widget draws the covering part of the nearest cached ancestor tile, and a plain grey tile
when there is none. Downloading can be switched off entirely from the *View* menu, and the
cache can be cleared.

### Consequences

- Only raster tiles are supported; a different tile source needs only a URL template.
- The map contacts a third-party server when downloading is on; the reference page states
  this and the OpenStreetMap attribution is always drawn.
- Tests write PNG tiles into a temporary cache directory and run the widget offline, so the
  map is covered on every CI platform.
- The disk cache is unbounded by design; the operator clears it from the menu.

## More information

- [OpenStreetMap tile usage policy](https://operations.osmfoundation.org/policies/tiles/)
- [Slippy map tile names](https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames)
- [Desktop application reference](../reference/desktop-app.md#map)
