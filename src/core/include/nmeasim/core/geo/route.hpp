// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Geometry of a leg between two waypoints as seen from the vessel.
///
/// `solve_leg` returns the bearings, distances and cross-track error that the autopilot
/// sentences APB, RMB and XTE and the Signal K `navigation.courseRhumbline` paths report.

#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

namespace nmeasim::core::geo {

/// Bearings and distances of a leg and of the vessel relative to it.
///
/// @see solve_leg
struct LegSolution {
    /// Initial true bearing of the leg from its origin to its destination, in [0, 360).
    double leg_bearing_deg{0.0};
    /// Length of the leg along the ellipsoid.
    double leg_length_m{0.0};
    /// Initial true bearing of the geodesic from the vessel to the destination, in [0, 360).
    double bearing_deg{0.0};
    /// Distance from the vessel to the destination along the ellipsoid.
    double distance_m{0.0};
    /// Signed distance of the vessel from the great circle through origin and destination:
    /// positive when the vessel is to the right (starboard) of the leg looking from origin to
    /// destination, negative to the left.
    double cross_track_m{0.0};
    /// Progress of the vessel along the leg, measured from the origin: negative behind the
    /// origin, greater than `leg_length_m` beyond the destination.
    double along_track_m{0.0};
};

/// Solves the leg from `origin` to `destination` for a vessel at `vessel`.
///
/// Bearings and the distances to the destination use the WGS 84 geodesic. The cross-track
/// and along-track distances use the spherical great-circle formulas on the mean earth
/// radius, accurate to well under one percent for legs of coastal length. A vessel exactly
/// at the origin has zero cross-track and along-track distances.
///
/// @param origin Start of the leg, normally the vessel's position when the destination was
///        set.
/// @param destination The waypoint the leg leads to.
/// @param vessel Current position of the vessel.
/// @return The leg geometry. When `origin` and `destination` coincide the leg has no
///         direction and the cross-track and along-track distances are measured against
///         whatever bearing the geodesic solution reports for a zero-length path.
[[nodiscard]] LegSolution solve_leg(Position origin, Position destination, Position vessel);

}  // namespace nmeasim::core::geo
