#include <nmeasim/core/geo/route.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nmeasim::core::geo {

namespace {

/// Mean earth radius used by the spherical cross-track formulas.
constexpr double kEarthRadiusM{6371008.8};

double radians(double degrees) noexcept {
    return degrees * std::numbers::pi / 180.0;
}

}  // namespace

LegSolution solve_leg(Position origin, Position destination, Position vessel) {
    LegSolution solution;
    const auto leg = inverse(origin, destination);
    const auto to_destination = inverse(vessel, destination);
    const auto from_origin = inverse(origin, vessel);
    solution.leg_bearing_deg = leg.initial_bearing_deg;
    solution.leg_length_m = leg.distance_m;
    solution.bearing_deg = to_destination.initial_bearing_deg;
    solution.distance_m = to_destination.distance_m;

    if (from_origin.distance_m <= 0.0) {
        solution.along_track_m = 0.0;
        solution.cross_track_m = 0.0;
        return solution;
    }
    const double angular_distance = from_origin.distance_m / kEarthRadiusM;
    const double bearing_difference =
        radians(from_origin.initial_bearing_deg) - radians(leg.initial_bearing_deg);
    const double cross_track =
        std::asin(std::clamp(std::sin(angular_distance) * std::sin(bearing_difference), -1.0, 1.0));
    solution.cross_track_m = cross_track * kEarthRadiusM;
    const double along = std::acos(
        std::clamp(std::cos(angular_distance) / std::max(std::cos(cross_track), 1e-12), -1.0, 1.0));
    solution.along_track_m = (std::cos(bearing_difference) < 0.0 ? -along : along) * kEarthRadiusM;
    return solution;
}

}  // namespace nmeasim::core::geo
