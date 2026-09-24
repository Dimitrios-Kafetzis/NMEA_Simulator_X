// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Geodesic distance, bearing and destination on the WGS 84 ellipsoid.
///
/// `inverse` and `direct` solve the two geodesic problems with GeographicLib, which
/// implements C. F. F. Karney, *Algorithms for geodesics*, 2013, accurate to about 15
/// nanometres; `normalize_bearing` brings any angle into [0, 360).

#pragma once

/// Geographic positions and geodesics on the WGS 84 ellipsoid, part of the `nmeasim::core`
/// library.
///
/// It holds the `Position` type the whole simulator uses, the direct and inverse geodesic
/// problems, and the leg geometry the autopilot sentences and Signal K course paths report.
/// All angles are in decimal degrees, all distances in metres. Bearings are measured
/// clockwise from true north and normalised to the half-open range [0, 360).
///
/// @see GeographicLib::Geodesic
namespace nmeasim::core::geo {

/// A geographic position on the WGS 84 ellipsoid, without height.
///
/// Nothing is validated on construction; the functions of this namespace expect the ranges
/// below. The default is the intersection of the equator and the prime meridian.
struct Position {
    /// Latitude in degrees, in [-90, 90], positive north.
    double latitude_deg{0.0};
    /// Longitude in degrees, in [-180, 180], positive east.
    double longitude_deg{0.0};
};

/// Result of the inverse geodesic problem between two positions.
///
/// @see inverse
struct InverseSolution {
    /// Length of the shortest path along the ellipsoid surface; 0 for coincident positions.
    double distance_m{0.0};
    /// True bearing of the path at its start, in [0, 360).
    double initial_bearing_deg{0.0};
    /// True bearing of the path at its end, in [0, 360): the direction of travel on arrival,
    /// not the bearing back to the start.
    double final_bearing_deg{0.0};
};

/// Normalises an angle in degrees to the half-open range [0, 360).
///
/// @param bearing_deg Any finite angle in degrees; negative values count anticlockwise.
/// @return The equivalent angle in [0, 360), never `-0.0` and never exactly 360. A non-finite
///         argument gives NaN.
[[nodiscard]] double normalize_bearing(double bearing_deg) noexcept;

/// Solves the inverse geodesic problem: the distance and bearings from `from` to `to`.
///
/// @param from Start of the path; its latitude must be in [-90, 90].
/// @param to End of the path; its latitude must be in [-90, 90].
/// @return Distance in metres and initial and final true bearings in [0, 360).
/// @see GeographicLib::Geodesic::Inverse
[[nodiscard]] InverseSolution inverse(Position from, Position to);

/// Solves the direct geodesic problem: the position reached by travelling a distance along a
/// geodesic from a starting point.
///
/// @param from Starting position; its latitude must be in [-90, 90].
/// @param bearing_deg Initial true bearing at `from`, in degrees; any value is accepted.
/// @param distance_m Distance to travel along the ellipsoid; a negative distance travels
///        backwards along the same geodesic.
/// @return The position reached, with its longitude reduced to [-180, 180].
/// @see GeographicLib::Geodesic::Direct
[[nodiscard]] Position direct(Position from, double bearing_deg, double distance_m);

}  // namespace nmeasim::core::geo
