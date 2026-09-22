#include <nmeasim/core/geo/geodesic.hpp>
#include <nmeasim/core/physics/wind.hpp>

#include <cmath>
#include <numbers>

namespace nmeasim::core::physics {

namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kRadToDeg = 180.0 / std::numbers::pi;

}  // namespace

ApparentWind apparent_wind(double true_direction_deg, double true_speed_kn, double heading_true_deg,
                           double speed_kn) noexcept {
    // Velocity vectors in an east/north frame. The wind blows *from* its direction, so its
    // velocity points the opposite way.
    const double wind_from = true_direction_deg * kDegToRad;
    const double wind_east = -true_speed_kn * std::sin(wind_from);
    const double wind_north = -true_speed_kn * std::cos(wind_from);

    const double heading = heading_true_deg * kDegToRad;
    const double vessel_east = speed_kn * std::sin(heading);
    const double vessel_north = speed_kn * std::cos(heading);

    // Apparent wind velocity is the true wind velocity minus the vessel velocity.
    const double apparent_east = wind_east - vessel_east;
    const double apparent_north = wind_north - vessel_north;

    ApparentWind result;
    result.speed_kn = std::hypot(apparent_east, apparent_north);
    if (result.speed_kn < 1e-9) {
        result.direction_true_deg = geo::normalize_bearing(true_direction_deg);
        result.angle_relative_deg = geo::normalize_bearing(true_direction_deg - heading_true_deg);
        return result;
    }
    // The direction the apparent wind comes from is opposite to its velocity vector.
    result.direction_true_deg =
        geo::normalize_bearing(std::atan2(-apparent_east, -apparent_north) * kRadToDeg);
    result.angle_relative_deg =
        geo::normalize_bearing(result.direction_true_deg - heading_true_deg);
    return result;
}

}  // namespace nmeasim::core::physics
