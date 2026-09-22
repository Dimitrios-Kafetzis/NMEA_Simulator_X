#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

#include <QPointF>

#include <compare>

/// Web Mercator tile arithmetic for slippy maps (the scheme used by OpenStreetMap).
///
/// Tile coordinates at zoom `z` run from 0 to 2^z - 1 in both axes, with (0, 0) at the
/// north-west corner of the world. Pixel coordinates are tile coordinates times the tile
/// size, so a widget only needs to know where its centre is in pixels at the current zoom.
namespace nmeasim::app::map {

constexpr int kTileSize{256};
constexpr int kMinZoom{1};
constexpr int kMaxZoom{19};
/// Web Mercator is undefined at the poles; positions beyond this latitude are clamped.
constexpr double kMaxLatitudeDeg{85.05112878};

struct TileKey {
    int zoom{0};
    int x{0};
    int y{0};

    auto operator<=>(const TileKey&) const = default;
};

/// Number of tiles along one axis at a zoom level.
[[nodiscard]] int tiles_at(int zoom) noexcept;

/// Fractional tile coordinates of a position at a zoom level.
[[nodiscard]] QPointF tile_coordinates(core::geo::Position position, int zoom) noexcept;

/// Pixel coordinates of a position at a zoom level.
[[nodiscard]] QPointF pixel_coordinates(core::geo::Position position, int zoom) noexcept;

/// Position of fractional tile coordinates at a zoom level.
[[nodiscard]] core::geo::Position position_of_tile(QPointF tile, int zoom) noexcept;

/// Position of pixel coordinates at a zoom level.
[[nodiscard]] core::geo::Position position_of_pixel(QPointF pixel, int zoom) noexcept;

/// The tile containing fractional tile coordinates, with x wrapped around the antimeridian.
[[nodiscard]] TileKey tile_at(QPointF tile, int zoom) noexcept;

/// The tile one zoom level up that contains this tile.
[[nodiscard]] TileKey parent_of(TileKey key) noexcept;

/// Metres per pixel at a latitude and zoom level.
[[nodiscard]] double metres_per_pixel(double latitude_deg, int zoom) noexcept;

}  // namespace nmeasim::app::map
