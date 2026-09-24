// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Loading a track from a file, with the reader chosen from the file name: `parse_track` and
/// `load_track`.

#pragma once

#include <nmeasim/core/track/track.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace nmeasim::core::track {

/// Parses track file content with the reader that matches the file name's extension.
///
/// The extension is everything after the last `.` of `file_name`, compared
/// case-insensitively: `gpx` selects `parse_gpx` and `kml` selects `parse_kml`. The content
/// is not inspected to guess the format.
///
/// @param file_name The file name or path; only its extension is used.
/// @param content The complete file content.
/// @param error Receives the reason on failure; left unchanged on success. May be null. For
///        any other extension, including none, the reason is
///        `Unsupported track file type '.EXT'; expected .gpx or .kml` with the extension in
///        lower case; otherwise it is the reader's reason.
/// @return The track, or `std::nullopt` when the extension is not supported or the reader
///         rejects the content.
[[nodiscard]] std::optional<Track> parse_track(std::string_view file_name, std::string_view content,
                                               std::string* error);

/// Reads a GPX or KML file from disk and parses it with `parse_track`.
///
/// When the file gives the track no name, the name becomes the last component of `path`,
/// extension included (for example `passage.gpx`).
///
/// @param path Path of the file, in the encoding `std::ifstream` expects.
/// @param error Receives the reason on failure; left unchanged on success. May be null. It
///        is `Cannot read PATH` when the file cannot be opened, and `PATH: REASON` with the
///        reason from `parse_track` when its content is rejected.
/// @return The track, or `std::nullopt` on failure.
[[nodiscard]] std::optional<Track> load_track(const std::string& path, std::string* error);

}  // namespace nmeasim::core::track
