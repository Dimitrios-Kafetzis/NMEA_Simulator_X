#pragma once

/// Geodesic calculations on the WGS84 ellipsoid.
///
/// All angles are in decimal degrees, all distances in metres. Bearings are measured
/// clockwise from true north and normalised to the half-open range [0, 360).
namespace nmeasim::core::geo {

/// A geographic position. Latitude is positive north, longitude positive east.
struct Position {
    double latitude_deg{0.0};
    double longitude_deg{0.0};
};

/// Result of the inverse geodesic problem between two positions.
struct InverseSolution {
    /// Length of the shortest path along the ellipsoid surface.
    double distance_m{0.0};
    /// True bearing at the start of the path.
    double initial_bearing_deg{0.0};
    /// True bearing at the end of the path.
    double final_bearing_deg{0.0};
};

/// Normalises a bearing to [0, 360).
[[nodiscard]] double normalize_bearing(double bearing_deg) noexcept;

/// Solves the inverse problem: distance and bearings from `from` to `to`.
[[nodiscard]] InverseSolution inverse(Position from, Position to);

/// Solves the direct problem: the position reached by travelling `distance_m` metres from
/// `from` along the initial true bearing `bearing_deg`.
[[nodiscard]] Position direct(Position from, double bearing_deg, double distance_m);

}  // namespace nmeasim::core::geo
