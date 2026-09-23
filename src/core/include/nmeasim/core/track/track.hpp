#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/// Tracks and routes loaded from GPX and KML files, ready for the track-following source.
namespace nmeasim::core::track {

/// One point of a track. Optional members are present only when the file carries them.
struct TrackPoint {
    geo::Position position;
    std::optional<double> elevation_m;
    std::optional<std::chrono::system_clock::time_point> time;
    /// Course over ground in degrees true, as recorded at this point.
    std::optional<double> course_deg;
    /// Speed over ground in knots, as recorded at this point.
    std::optional<double> speed_kn;
};

/// Where a track came from, for display and for choosing defaults.
enum class TrackKind {
    GpxTrack,
    GpxRoute,
    KmlTrack,
    KmlLineString,
};

[[nodiscard]] const char* to_string(TrackKind kind) noexcept;

/// A sequence of points to follow. Segments of the source file are already concatenated.
struct Track {
    std::string name;
    TrackKind kind{TrackKind::GpxTrack};
    std::vector<TrackPoint> points;
    /// Number of segments (GPX `<trkseg>`, KML geometries) that were concatenated.
    std::size_t segment_count{0};

    /// True when every point carries a time and the times never decrease, so that the track
    /// can be followed on its own timing.
    [[nodiscard]] bool has_timestamps() const noexcept;
    /// Time from the first to the last point, when `has_timestamps()`.
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const;
    /// Sum of the geodesic leg lengths.
    [[nodiscard]] double length_m() const;
};

}  // namespace nmeasim::core::track
