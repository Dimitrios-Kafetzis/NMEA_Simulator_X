#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

/// Geometry of a leg between two waypoints as seen from the vessel: bearings, distances and
/// the cross-track error the autopilot sentences report.
namespace nmeasim::core::geo {

struct LegSolution {
    /// Initial true bearing of the leg, from its origin to its destination.
    double leg_bearing_deg{0.0};
    /// Length of the leg along the ellipsoid.
    double leg_length_m{0.0};
    /// True bearing from the vessel to the destination.
    double bearing_deg{0.0};
    /// Distance from the vessel to the destination along the ellipsoid.
    double distance_m{0.0};
    /// Signed distance of the vessel from the leg: positive when the vessel is to the right
    /// (starboard) of the leg looking from origin to destination, negative to the left.
    double cross_track_m{0.0};
    /// Progress of the vessel along the leg, measured from the origin; negative behind the
    /// origin, greater than `leg_length_m` beyond the destination.
    double along_track_m{0.0};
};

/// Solves the leg from `origin` to `destination` for a vessel at `vessel`. Bearings and the
/// distances to the destination use the WGS84 geodesic; the cross-track and along-track
/// distances use the spherical great-circle formulas, accurate to well under one percent
/// for legs of coastal length.
[[nodiscard]] LegSolution solve_leg(Position origin, Position destination, Position vessel);

}  // namespace nmeasim::core::geo
