#include "xml_helpers.hpp"

#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/core/track/kml.hpp>

#include <cctype>
#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace nmeasim::core::track {

namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

std::vector<std::string_view> split_whitespace(std::string_view text) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start < text.size()) {
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
            ++start;
        }
        std::size_t end = start;
        while (end < text.size() && std::isspace(static_cast<unsigned char>(text[end])) == 0) {
            ++end;
        }
        if (end > start) {
            parts.push_back(text.substr(start, end - start));
        }
        start = end;
    }
    return parts;
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const auto end = text.find(separator, start);
        parts.push_back(text.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

/// Reads a `lon,lat[,alt]` (comma) or `lon lat [alt]` (space) tuple.
bool read_tuple(const std::vector<std::string_view>& parts, std::size_t index, TrackPoint& point,
                std::string* error) {
    if (parts.size() < 2) {
        set_error(error, std::format("coordinate {}: expected longitude and latitude", index + 1));
        return false;
    }
    const auto lon = xml::parse_number(parts[0]);
    const auto lat = xml::parse_number(parts[1]);
    if (!lon || !lat) {
        set_error(error, std::format("coordinate {}: '{},{}' is not numeric", index + 1, parts[0],
                                     parts[1]));
        return false;
    }
    if (*lat < -90.0 || *lat > 90.0 || *lon < -180.0 || *lon > 180.0) {
        set_error(error,
                  std::format("coordinate {}: {}, {} is out of range", index + 1, *lat, *lon));
        return false;
    }
    point.position = {*lat, *lon};
    if (parts.size() >= 3) {
        point.elevation_m = xml::parse_number(parts[2]);
    }
    return true;
}

class Reader {
public:
    explicit Reader(std::string* error) : error_(error) {}

    bool visit(const pugi::xml_node& node) {
        if (node.type() != pugi::node_element) {
            return true;
        }
        if (xml::is_named(node, "Placemark")) {
            const auto name = xml::child_text(node, "name");
            if (!name.empty()) {
                placemark_name_ = name;
            }
        } else if (xml::is_named(node, "Document") || xml::is_named(node, "Folder")) {
            if (document_name_.empty()) {
                document_name_ = xml::child_text(node, "name");
            }
        }
        if (xml::is_named(node, "Track")) {
            return read_track(node);
        }
        if (xml::is_named(node, "LineString")) {
            return read_line_string(node);
        }
        for (const auto& element : node.children()) {
            if (!visit(element)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] Track& track() noexcept { return track_; }
    [[nodiscard]] bool saw_track() const noexcept { return saw_track_; }

private:
    void take_name() {
        if (track_.name.empty()) {
            track_.name = !placemark_name_.empty() ? placemark_name_ : document_name_;
        }
    }

    bool read_track(const pugi::xml_node& node) {
        saw_track_ = true;
        ++track_.segment_count;
        take_name();
        std::vector<std::optional<std::chrono::system_clock::time_point>> whens;
        std::vector<TrackPoint> points;
        for (const auto& element : node.children()) {
            if (xml::is_named(element, "when")) {
                const auto text = xml::trim(element.child_value());
                const auto parsed = time::parse_iso8601(text);
                if (!parsed) {
                    set_error(error_, std::format("<when> {}: '{}' is not an ISO 8601 time",
                                                  whens.size() + 1, text));
                    return false;
                }
                whens.push_back(parsed);
            } else if (xml::is_named(element, "coord")) {
                TrackPoint point;
                if (!read_tuple(split_whitespace(element.child_value()), points.size(), point,
                                error_)) {
                    return false;
                }
                points.push_back(point);
            }
        }
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i < whens.size()) {
                points[i].time = whens[i];
            }
            track_.points.push_back(points[i]);
        }
        return true;
    }

    bool read_line_string(const pugi::xml_node& node) {
        ++track_.segment_count;
        take_name();
        const auto tuples = split_whitespace(xml::child(node, "coordinates").child_value());
        for (std::size_t i = 0; i < tuples.size(); ++i) {
            TrackPoint point;
            if (!read_tuple(split(tuples[i], ','), i, point, error_)) {
                return false;
            }
            track_.points.push_back(point);
        }
        return true;
    }

    std::string* error_;
    Track track_;
    std::string placemark_name_;
    std::string document_name_;
    bool saw_track_{false};
};

}  // namespace

std::optional<Track> parse_kml(std::string_view xml_text, std::string* error) {
    pugi::xml_document document;
    if (!xml::load_document(document, xml_text, error)) {
        return std::nullopt;
    }
    const auto root = xml::child(document, "kml");
    if (!root) {
        set_error(error, "Not a KML document: the root element is not <kml>");
        return std::nullopt;
    }
    Reader reader(error);
    if (!reader.visit(root)) {
        return std::nullopt;
    }
    Track track = std::move(reader.track());
    if (track.points.empty()) {
        set_error(error, "No <gx:Track> or <LineString> geometry found");
        return std::nullopt;
    }
    track.kind = reader.saw_track() ? TrackKind::KmlTrack : TrackKind::KmlLineString;
    return track;
}

}  // namespace nmeasim::core::track
