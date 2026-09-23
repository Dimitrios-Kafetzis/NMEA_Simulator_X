#pragma once

#include <nmeasim/core/track/track.hpp>

#include <optional>
#include <string>
#include <string_view>

/// KML 2.2 reader for tracks.
///
/// Every `<gx:Track>` (a list of `<when>` and `<gx:coord>` pairs) and every `<LineString>`
/// (a `<coordinates>` list of `lon,lat[,alt]` tuples) is concatenated in document order,
/// wherever it sits in the folder, placemark or multi-geometry hierarchy. Points, polygons
/// and styling are ignored.
namespace nmeasim::core::track {

/// Parses KML text. On failure returns nullopt and sets `error` to a one-line reason.
[[nodiscard]] std::optional<Track> parse_kml(std::string_view xml, std::string* error);

}  // namespace nmeasim::core::track
