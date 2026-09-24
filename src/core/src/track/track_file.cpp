// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Choice of the track reader from the file extension, and reading track files from disk.

#include <nmeasim/core/track/gpx.hpp>
#include <nmeasim/core/track/kml.hpp>
#include <nmeasim/core/track/track_file.hpp>

#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace nmeasim::core::track {

namespace {

/// Returns the last component of a path.
///
/// Both the slash and the backslash count as separators, so that Windows paths give their
/// file name on every platform.
///
/// @param path A file name or path.
/// @return The text after the last separator of `path`, or all of `path` when it has none;
///         a view into `path`.
std::string_view base_name(std::string_view path) noexcept {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

/// Returns the extension of a file name in lower case.
///
/// @param file_name A file name without directory, as `base_name` returns it.
/// @return The text after the last `.` of `file_name`, converted to lower case with
///         `std::tolower` byte by byte; `std::nullopt` when there is no `.`.
std::optional<std::string> lowercase_extension(std::string_view file_name) {
    const auto dot = file_name.rfind('.');
    if (dot == std::string_view::npos) {
        return std::nullopt;
    }
    std::string extension{file_name.substr(dot + 1)};
    for (auto& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension;
}

/// Stores an error message when the caller asked for one.
///
/// @param error Destination of the message; nothing is stored when it is null.
/// @param message The one-line reason.
void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

}  // namespace

std::optional<Track> parse_track(std::string_view file_name, std::string_view content,
                                 std::string* error) {
    const auto name = base_name(file_name);
    const auto extension = lowercase_extension(name);
    if (!extension) {
        set_error(error, "The track file '" + std::string{name} +
                             "' has no extension; expected .gpx or .kml");
        return std::nullopt;
    }
    if (*extension == "gpx") {
        return parse_gpx(content, error);
    }
    if (*extension == "kml") {
        return parse_kml(content, error);
    }
    set_error(error, "Unsupported track file type '." + *extension + "'; expected .gpx or .kml");
    return std::nullopt;
}

std::optional<Track> load_track(const std::string& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        set_error(error, "Cannot read " + path);
        return std::nullopt;
    }
    const std::string content{std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>()};
    std::string reason;
    auto track = parse_track(path, content, &reason);
    if (!track) {
        set_error(error, path + ": " + reason);
        return std::nullopt;
    }
    if (track->name.empty()) {
        track->name = std::string{base_name(path)};
    }
    return track;
}

}  // namespace nmeasim::core::track
