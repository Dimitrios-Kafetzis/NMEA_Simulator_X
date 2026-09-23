#pragma once

#include <nmeasim/core/track/track.hpp>

#include <optional>
#include <string>
#include <string_view>

/// Loading a track from a file, choosing the reader from the file name.
namespace nmeasim::core::track {

/// Parses `content` as GPX or KML according to the extension of `file_name` (`.gpx`, `.kml`,
/// case-insensitive). Any other extension is an error.
[[nodiscard]] std::optional<Track> parse_track(std::string_view file_name, std::string_view content,
                                               std::string* error);

/// Reads and parses a track file from disk.
[[nodiscard]] std::optional<Track> load_track(const std::string& path, std::string* error);

}  // namespace nmeasim::core::track
