// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Web Mercator (EPSG:3857) tile arithmetic for slippy maps, the scheme OpenStreetMap uses.
///
/// Tile coordinates at zoom `z` run from 0 to 2^z along both axes, with (0, 0) at the
/// north-west corner of the world (latitude `kMaxLatitudeDeg`, longitude -180) and y growing
/// southwards; the integer part names a tile and the fraction is the place inside it. Pixel
/// coordinates are tile coordinates times `kTileSize`, so a widget only needs to know where
/// its centre lies in pixels at the current zoom. All functions here work on whole zoom
/// levels; `MapWidget` scales their results for fractional zoom.
///
/// @see https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames

#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

#include <QPointF>

#include <compare>

namespace nmeasim::app::map {

/// Edge length of a square raster tile in pixels, as served by OpenStreetMap-style servers.
constexpr int kTileSize{256};
/// Lowest zoom level the map widget allows; at level 1 the world is 2 by 2 tiles.
constexpr int kMinZoom{1};
/// Highest zoom level the map widget allows, the deepest level the OpenStreetMap standard
/// tile layer serves.
constexpr int kMaxZoom{19};
/// Northern and southern limit of Web Mercator in degrees, `atan(sinh(pi))`.
///
/// The projection is undefined at the poles; this latitude makes the projected world square.
/// Positions beyond it are clamped to it.
constexpr double kMaxLatitudeDeg{85.05112878};

/// Address of one tile: its zoom level and its column and row at that level.
///
/// Valid keys have `zoom` in [0, 30] and `x` and `y` in [0, 2^zoom); nothing checks this.
/// Keys are ordered by zoom, then x, then y.
struct TileKey {
    int zoom{0};  ///< Zoom level of the tile.
    int x{0};     ///< Column, counted eastwards from longitude -180.
    int y{0};     ///< Row, counted southwards from the northern limit `kMaxLatitudeDeg`.

    /// Compares two keys member by member in the order zoom, x, y.
    ///
    /// @return The ordering of the first member that differs, or equality.
    auto operator<=>(const TileKey&) const = default;
};

/// Returns the number of tiles along one axis at a zoom level, 2^`zoom`.
///
/// @param zoom Whole zoom level; clamped to [0, 30] so that the result fits in an `int`.
/// @return The number of tiles per axis, from 1 to 2^30.
[[nodiscard]] int tiles_at(int zoom) noexcept;

/// Returns the fractional tile coordinates of a position at a zoom level.
///
/// @param position Position to project. The latitude is clamped to [-`kMaxLatitudeDeg`,
///     `kMaxLatitudeDeg`]; the longitude is not wrapped, so one outside [-180, 180] gives an
///     x outside [0, 2^`zoom`].
/// @param zoom Whole zoom level, clamped as `tiles_at` does.
/// @return Tile coordinates, x eastwards and y southwards, both in [0, 2^`zoom`] for
///     positions in range.
[[nodiscard]] QPointF tile_coordinates(core::geo::Position position, int zoom) noexcept;

/// Returns the pixel coordinates of a position at a zoom level.
///
/// @param position Position to project, treated as `tile_coordinates` treats it.
/// @param zoom Whole zoom level, clamped as `tiles_at` does.
/// @return Pixel coordinates in the world image, which is 2^`zoom` times `kTileSize` pixels
///     square: the tile coordinates times `kTileSize`.
[[nodiscard]] QPointF pixel_coordinates(core::geo::Position position, int zoom) noexcept;

/// Returns the position at fractional tile coordinates at a zoom level, the inverse of
/// `tile_coordinates`.
///
/// @param tile Tile coordinates, x eastwards and y southwards. Values outside [0, 2^`zoom`]
///     are neither wrapped nor clamped: they give longitudes outside [-180, 180] and
///     latitudes beyond `kMaxLatitudeDeg`.
/// @param zoom Whole zoom level, clamped as `tiles_at` does.
/// @return The position in degrees.
[[nodiscard]] core::geo::Position position_of_tile(QPointF tile, int zoom) noexcept;

/// Returns the position at pixel coordinates at a zoom level, the inverse of
/// `pixel_coordinates`.
///
/// @param pixel Pixel coordinates in the world image, treated as `position_of_tile` treats
///     tile coordinates.
/// @param zoom Whole zoom level, clamped as `tiles_at` does.
/// @return The position in degrees.
[[nodiscard]] core::geo::Position position_of_pixel(QPointF pixel, int zoom) noexcept;

/// Returns the tile containing fractional tile coordinates, with x wrapped around the
/// antimeridian.
///
/// @param tile Tile coordinates. The x may lie outside [0, 2^`zoom`) and is wrapped into it,
///     as the world repeats east and west.
/// @param zoom Whole zoom level, stored in the key as given.
/// @return The key of the tile. Its y is the floor of `tile.y()` clamped to [0, 2^`zoom`),
///     so a point north or south of the projected world gives the first or last row.
[[nodiscard]] TileKey tile_at(QPointF tile, int zoom) noexcept;

/// Returns the tile one zoom level up (one level less detailed) that contains a tile.
///
/// @param key Tile whose parent is wanted.
/// @return The key at `key.zoom` - 1 whose area covers `key` and three sibling tiles. A key
///     at zoom 0 or below has no parent and yields the whole-world tile (0, 0) at zoom 0.
[[nodiscard]] TileKey parent_of(TileKey key) noexcept;

/// Wraps a longitude into [-180, 180), as the world repeats east and west.
///
/// @param longitude_deg Longitude in degrees, positive east; any finite value.
/// @return The same meridian in [-180, 180): 180 gives -180 and 190 gives -170.
[[nodiscard]] double wrap_longitude(double longitude_deg) noexcept;

/// Returns the ground distance one pixel covers at a latitude and zoom level.
///
/// This is the Web Mercator ground resolution: the equatorial circumference of WGS 84
/// divided by the world width in pixels and multiplied by the cosine of the latitude. It
/// holds along both axes at that latitude.
///
/// @param latitude_deg Latitude in degrees, positive north; clamped to
///     [-`kMaxLatitudeDeg`, `kMaxLatitudeDeg`].
/// @param zoom Whole zoom level, clamped as `tiles_at` does.
/// @return Metres per pixel, for example about 156543 at zoom 0 on the equator.
[[nodiscard]] double metres_per_pixel(double latitude_deg, int zoom) noexcept;

}  // namespace nmeasim::app::map
