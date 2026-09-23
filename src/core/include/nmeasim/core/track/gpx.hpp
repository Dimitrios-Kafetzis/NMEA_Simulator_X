#pragma once

#include <nmeasim/core/track/track.hpp>

#include <optional>
#include <string>
#include <string_view>

/// GPX 1.0 and 1.1 reader.
///
/// Every `<trkpt>` of every `<trkseg>` of every `<trk>` is concatenated in document order.
/// When the file has no track points the `<rtept>` of every `<rte>` are used instead.
/// `<ele>`, `<time>`, `<course>` (degrees true) and `<speed>` (metres per second) are read
/// when present, whether as direct children (GPX 1.0) or inside `<extensions>`. Namespace
/// prefixes are ignored, so `gpx:trkpt` and `trkpt` are the same element.
namespace nmeasim::core::track {

/// Parses GPX text. On failure returns nullopt and sets `error` to a one-line reason.
[[nodiscard]] std::optional<Track> parse_gpx(std::string_view xml, std::string* error);

}  // namespace nmeasim::core::track
