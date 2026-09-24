// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Reader for tracks in OGC KML 2.2 files: `parse_kml`.
///
/// It reads `<gx:Track>` (the Google extension namespace) and `<LineString>` geometries
/// wherever they sit in the folder, placemark or multi-geometry hierarchy.
///
/// @see OGC KML 2.2, https://www.ogc.org/standard/kml/

#pragma once

#include <nmeasim/core/track/track.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace nmeasim::core::track {

/// Parses the text of a KML 2.2 file into a track.
///
/// The whole document under `<kml>` is traversed. Every `<gx:Track>` and every
/// `<LineString>` is concatenated in document order, wherever it sits (`<Document>`,
/// `<Folder>`, `<Placemark>`, `<MultiGeometry>`, `<gx:MultiTrack>`). Points, polygons,
/// styles and `<TimeStamp>` are ignored. Namespace prefixes are ignored.
///
/// - `<gx:Track>`: each `<gx:coord>` holds `lon lat [alt]` separated by whitespace and takes
///   the time of the `<when>` at the same index. Surplus `<when>` elements are ignored; a
///   point without a matching `<when>` has no time, which makes the track untimed.
/// - `<LineString>`: its `<coordinates>` hold `lon,lat[,alt]` tuples separated by
///   whitespace. The points have no time.
///
/// The third value, when present, is the elevation in metres; one that is not a number is
/// treated as absent, and values after the third are ignored. The result is a
/// `TrackKind::KmlTrack` when the file has at least one `<gx:Track>`, otherwise a
/// `TrackKind::KmlLineString`. The track name is set at the first geometry: the `<name>` of
/// the most recent named `<Placemark>` met so far, else the first non-empty `<name>` of a
/// `<Document>` or `<Folder>` met so far; while both are empty, the next geometry tries
/// again.
///
/// The file is rejected, with `error` set to a one-line reason, when:
/// - it is not well-formed XML: `Invalid XML at offset N: <pugixml description>`, where `N`
///   is the byte offset of the error;
/// - the root element is not `<kml>`: `Not a KML document: the root element is not <kml>`;
/// - a `<when>` cannot be parsed, including an empty one:
///   `<when> N: 'TEXT' is not an ISO 8601 time`;
/// - a coordinate has fewer than two values: `coordinate N: expected longitude and latitude`;
/// - its longitude or latitude is not a number: `coordinate N: 'LON,LAT' is not numeric`;
/// - it is out of range: `coordinate N: LAT, LON is out of range`, latitude first;
/// - no geometry yields a point: `No <gx:Track> or <LineString> geometry found`.
///
/// `N` is the 1-based number of the `<when>` or coordinate within its own `<gx:Track>` or
/// `<LineString>`, not within the file.
///
/// @param xml The complete file content, in any encoding pugixml detects. It is copied and
///        need not outlive the call.
/// @param error Receives the reason on failure; left unchanged on success. May be null.
/// @return The track, with at least one point, or `std::nullopt` when the file is rejected.
/// @see OGC KML 2.2, elements LineString and coordinates; Google KML extensions, gx:Track.
[[nodiscard]] std::optional<Track> parse_kml(std::string_view xml, std::string* error);

}  // namespace nmeasim::core::track
