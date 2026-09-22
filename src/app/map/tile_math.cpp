#include "tile_math.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nmeasim::app::map {

namespace {

constexpr double kEarthCircumferenceM{40075016.686};

double to_radians(double degrees) noexcept {
    return degrees * std::numbers::pi / 180.0;
}

double to_degrees(double radians) noexcept {
    return radians * 180.0 / std::numbers::pi;
}

}  // namespace

int tiles_at(int zoom) noexcept {
    return 1 << std::clamp(zoom, 0, 30);
}

QPointF tile_coordinates(core::geo::Position position, int zoom) noexcept {
    const double n = tiles_at(zoom);
    const double latitude = std::clamp(position.latitude_deg, -kMaxLatitudeDeg, kMaxLatitudeDeg);
    const double latitude_rad = to_radians(latitude);
    const double x = (position.longitude_deg + 180.0) / 360.0 * n;
    const double y =
        (1.0 - std::log(std::tan(latitude_rad) + 1.0 / std::cos(latitude_rad)) / std::numbers::pi) /
        2.0 * n;
    return {x, y};
}

QPointF pixel_coordinates(core::geo::Position position, int zoom) noexcept {
    return tile_coordinates(position, zoom) * kTileSize;
}

core::geo::Position position_of_tile(QPointF tile, int zoom) noexcept {
    const double n = tiles_at(zoom);
    const double longitude = tile.x() / n * 360.0 - 180.0;
    const double latitude =
        to_degrees(std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * tile.y() / n))));
    return {latitude, longitude};
}

core::geo::Position position_of_pixel(QPointF pixel, int zoom) noexcept {
    return position_of_tile(pixel / kTileSize, zoom);
}

TileKey tile_at(QPointF tile, int zoom) noexcept {
    const int n = tiles_at(zoom);
    int x = static_cast<int>(std::floor(tile.x())) % n;
    if (x < 0) {
        x += n;
    }
    const int y = static_cast<int>(std::floor(tile.y()));
    return {zoom, x, y};
}

TileKey parent_of(TileKey key) noexcept {
    return {key.zoom - 1, key.x >> 1, key.y >> 1};
}

double metres_per_pixel(double latitude_deg, int zoom) noexcept {
    const double latitude = std::clamp(latitude_deg, -kMaxLatitudeDeg, kMaxLatitudeDeg);
    return kEarthCircumferenceM * std::cos(to_radians(latitude)) /
           (static_cast<double>(tiles_at(zoom)) * kTileSize);
}

}  // namespace nmeasim::app::map
