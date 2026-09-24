// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Reader for GPX 1.0 and 1.1 track and route files: `parse_gpx`.
///
/// @see GPX 1.1, https://www.topografix.com/GPX/1/1/

#pragma once

#include <nmeasim/core/track/track.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace nmeasim::core::track {

/// Parses the text of a GPX 1.0 or 1.1 file into a track.
///
/// Every `<trkpt>` of every `<trkseg>` of every `<trk>` is concatenated in document order
/// and the result is a `TrackKind::GpxTrack`. When the file has no track point, the
/// `<rtept>` of every `<rte>` are used instead and the result is a `TrackKind::GpxRoute`.
/// `<wpt>` elements are ignored. Namespace prefixes are ignored, so `gpx:trkpt` and `trkpt`
/// are the same element; both versions are read the same way.
///
/// For each point:
/// - the `lat` and `lon` attributes are required, in [-90, 90] and [-180, 180] degrees;
/// - `<time>` is optional; when present and not empty it must be an ISO 8601 time;
/// - `<ele>` (metres), `<course>` (degrees true) and `<speed>` (metres per second, converted
///   to knots) are read as direct children, as in GPX 1.0, or from `<extensions>` or one of
///   its child elements (for example `gpxtpx:TrackPointExtension`), as GPX 1.1 writers
///   store them. A value that is not a number is treated as absent, without an error.
///
/// The track name is the first non-empty one of `<metadata>/<name>`, `<gpx>/<name>` and the
/// `<trk>/<name>` of the tracks in order (or `<rte>/<name>` for a route).
///
/// The file is rejected, with `error` set to a one-line reason, when:
/// - it is not well-formed XML: `Invalid XML at offset N: <pugixml description>`, where `N`
///   is the byte offset of the error;
/// - the root element is not `<gpx>`: `Not a GPX document: the root element is not <gpx>`;
/// - a point has no valid `lat` or `lon`: `point N: missing or invalid lat/lon attribute`;
/// - a point is out of range: `point N: coordinates LAT, LON are out of range`;
/// - a `<time>` cannot be parsed: `point N: 'TEXT' is not an ISO 8601 time`;
/// - it has neither track nor route points:
///   `No track points (<trkpt>) or route points (<rtept>) found`.
///
/// `N` is the 1-based number of the point in the whole file, counted across segments and
/// tracks (or across routes), not within its segment.
///
/// @param xml The complete file content, in any encoding pugixml detects. It is copied and
///        need not outlive the call.
/// @param error Receives the reason on failure; left unchanged on success. May be null.
/// @return The track, with at least one point, or `std::nullopt` when the file is rejected.
/// @see GPX 1.1, elements trk, trkseg, trkpt, rte and rtept.
[[nodiscard]] std::optional<Track> parse_gpx(std::string_view xml, std::string* error);

}  // namespace nmeasim::core::track
