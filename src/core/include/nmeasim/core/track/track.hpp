// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The in-memory track: `Track`, its points and the kind of file element it came from.
///
/// A `Track` is what the GPX and KML readers (`parse_gpx`, `parse_kml`, `load_track`)
/// produce and what `simulation::TrackSource` sails.

#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/// Tracks and routes read from GPX and KML files for the track-following mode, part of the
/// Qt-free `nmeasim::core` library.
///
/// It holds the `Track` model, the readers `parse_gpx` (GPX 1.0 and 1.1) and `parse_kml`
/// (OGC KML 2.2 with the Google `gx` extensions), and `parse_track` and `load_track`, which
/// pick the reader from the file extension. Every reader concatenates all geometries of a
/// file into one `Track` and reports failure as `std::nullopt` plus a one-line reason; none
/// of them throws for malformed input. The accepted elements are listed in
/// docs/reference/track-files.md.
///
/// @see GPX 1.1, https://www.topografix.com/GPX/1/1/
/// @see OGC KML 2.2, https://www.ogc.org/standard/kml/
namespace nmeasim::core::track {

/// One point of a track, with the optional values the file recorded for it.
///
/// Only `position` is always present; every optional member is empty when the file does not
/// carry the value or carries one that is not a number.
struct TrackPoint {
    /// Latitude and longitude on WGS 84, validated by the readers to [-90, 90] and
    /// [-180, 180] degrees.
    geo::Position position;
    /// Elevation in metres as written in the file: GPX `<ele>` or the third KML coordinate.
    std::optional<double> elevation_m;
    /// UTC time at which the point was recorded, with millisecond precision: GPX `<time>` or
    /// the KML `<when>` at the same index as the `<gx:coord>`.
    std::optional<std::chrono::system_clock::time_point> time;
    /// Course over ground in degrees true, as recorded (GPX `<course>` only). Not
    /// range-checked or normalised.
    std::optional<double> course_deg;
    /// Speed over ground in knots, converted from the metres per second of GPX `<speed>`
    /// (GPX only). Not range-checked.
    std::optional<double> speed_kn;
};

/// The file format and element a track's points came from, for display and for choosing
/// defaults.
enum class TrackKind {
    /// GPX `<trkpt>` points of `<trk>`/`<trkseg>` elements. Display name `GPX track`.
    GpxTrack,
    /// GPX `<rtept>` points of `<rte>` elements, used when the file has no track point.
    /// Display name `GPX route`.
    GpxRoute,
    /// KML file with at least one `<gx:Track>` element that provides points, whether or not
    /// it also holds `<LineString>` elements. Display name `KML track`.
    KmlTrack,
    /// KML file whose points all come from `<LineString>` elements. Display name `KML line`.
    KmlLineString,
};

/// Returns the display name of a track kind, such as `GPX route`.
///
/// @param kind The kind to name.
/// @return A static string, valid for the lifetime of the program: `GPX track`,
///         `GPX route`, `KML track` or `KML line`, and `track` for a value outside the
///         enumeration.
[[nodiscard]] const char* to_string(TrackKind kind) noexcept;

/// A sequence of points to follow, read from one GPX or KML file.
///
/// All segments, routes or geometries of the source file are already concatenated in
/// document order; nothing is reordered or deduplicated. A track returned by the readers has
/// at least one point; a default-constructed one has none.
struct Track {
    /// Name given in the file; empty when it has none. `load_track` replaces an empty name
    /// with the file name.
    std::string name;
    /// The file format and element the points came from.
    TrackKind kind{TrackKind::GpxTrack};
    /// The points in sailing order.
    std::vector<TrackPoint> points;
    /// Number of source elements that were concatenated: `<trkseg>` elements for a GPX
    /// track, `<rte>` elements for a GPX route, `<gx:Track>` and `<LineString>` elements for
    /// KML. Elements without points are counted too.
    std::size_t segment_count{0};

    /// Tells whether the track can be followed on its own timing.
    ///
    /// @return True when there is at least one point, every point carries a time and the
    ///         times never decrease (equal consecutive times are allowed); false otherwise,
    ///         including for an empty track.
    [[nodiscard]] bool has_timestamps() const noexcept;
    /// Returns the time from the first to the last point.
    ///
    /// @return The difference between the last and the first point time, truncated to
    ///         milliseconds, or `std::nullopt` when `has_timestamps()` is false.
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const;
    /// Returns the length of the track along the WGS 84 ellipsoid.
    ///
    /// @return The sum of the geodesic distances between consecutive points in metres,
    ///         ignoring elevation; 0 for fewer than two points.
    /// @see geo::inverse
    [[nodiscard]] double length_m() const;
};

}  // namespace nmeasim::core::track
