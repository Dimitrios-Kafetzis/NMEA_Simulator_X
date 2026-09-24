// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Geodesic calculations on the WGS 84 ellipsoid, delegated to GeographicLib.

#include <nmeasim/core/geo/geodesic.hpp>

#include <GeographicLib/Geodesic.hpp>

#include <cmath>

namespace nmeasim::core::geo {

double normalize_bearing(double bearing_deg) noexcept {
    double normalized = std::fmod(bearing_deg, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    // A tiny negative remainder plus 360 rounds to exactly 360.0, which is outside the range.
    if (normalized >= 360.0) {
        normalized = 0.0;
    }
    // fmod keeps the sign of a zero: adding 0.0 turns -0.0 into +0.0.
    return normalized + 0.0;
}

InverseSolution inverse(Position from, Position to) {
    double distance_m{0.0};
    double initial_bearing_deg{0.0};
    double final_bearing_deg{0.0};
    GeographicLib::Geodesic::WGS84().Inverse(from.latitude_deg, from.longitude_deg, to.latitude_deg,
                                             to.longitude_deg, distance_m, initial_bearing_deg,
                                             final_bearing_deg);
    return {distance_m, normalize_bearing(initial_bearing_deg),
            normalize_bearing(final_bearing_deg)};
}

Position direct(Position from, double bearing_deg, double distance_m) {
    Position result;
    GeographicLib::Geodesic::WGS84().Direct(from.latitude_deg, from.longitude_deg, bearing_deg,
                                            distance_m, result.latitude_deg, result.longitude_deg);
    return result;
}

}  // namespace nmeasim::core::geo
