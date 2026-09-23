#include <nmeasim/core/track/gpx.hpp>
#include <nmeasim/core/track/kml.hpp>
#include <nmeasim/core/track/track_file.hpp>

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

namespace nmeasim::core::track {

namespace {

std::string lowercase_extension(std::string_view file_name) {
    const auto dot = file_name.rfind('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    std::string extension{file_name.substr(dot + 1)};
    for (auto& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension;
}

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

}  // namespace

std::optional<Track> parse_track(std::string_view file_name, std::string_view content,
                                 std::string* error) {
    const auto extension = lowercase_extension(file_name);
    if (extension == "gpx") {
        return parse_gpx(content, error);
    }
    if (extension == "kml") {
        return parse_kml(content, error);
    }
    set_error(error, "Unsupported track file type '." + extension + "'; expected .gpx or .kml");
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
        const auto slash = path.find_last_of("/\\");
        track->name = slash == std::string::npos ? path : path.substr(slash + 1);
    }
    return track;
}

}  // namespace nmeasim::core::track
