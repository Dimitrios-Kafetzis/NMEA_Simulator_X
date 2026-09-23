#include "xml_helpers.hpp"

#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/core/track/gpx.hpp>
#include <nmeasim/core/units.hpp>

#include <cstddef>
#include <format>
#include <optional>
#include <string>

namespace nmeasim::core::track {

namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

/// Reads an optional numeric child, searching `<extensions>` as well because GPX 1.1 moved
/// `<course>` and `<speed>` there.
std::optional<double> optional_number(const pugi::xml_node& point, const char* local) {
    auto node = xml::child(point, local);
    if (!node) {
        const auto extensions = xml::child(point, "extensions");
        if (extensions) {
            for (const auto& candidate : extensions.children()) {
                if (xml::is_named(candidate, local)) {
                    node = candidate;
                    break;
                }
                for (const auto& nested : candidate.children()) {
                    if (xml::is_named(nested, local)) {
                        node = nested;
                        break;
                    }
                }
                if (node) {
                    break;
                }
            }
        }
    }
    if (!node) {
        return std::nullopt;
    }
    return xml::parse_number(node.child_value());
}

bool read_point(const pugi::xml_node& node, std::size_t index, Track& track, std::string* error) {
    const auto lat = xml::parse_number(node.attribute("lat").value());
    const auto lon = xml::parse_number(node.attribute("lon").value());
    if (!lat || !lon) {
        set_error(error, std::format("point {}: missing or invalid lat/lon attribute", index + 1));
        return false;
    }
    if (*lat < -90.0 || *lat > 90.0 || *lon < -180.0 || *lon > 180.0) {
        set_error(error, std::format("point {}: coordinates {}, {} are out of range", index + 1,
                                     *lat, *lon));
        return false;
    }
    TrackPoint point;
    point.position = {*lat, *lon};
    point.elevation_m = optional_number(node, "ele");
    const auto time_text = xml::child_text(node, "time");
    if (!time_text.empty()) {
        point.time = time::parse_iso8601(time_text);
        if (!point.time) {
            set_error(error,
                      std::format("point {}: '{}' is not an ISO 8601 time", index + 1, time_text));
            return false;
        }
    }
    point.course_deg = optional_number(node, "course");
    if (const auto speed_mps = optional_number(node, "speed")) {
        point.speed_kn = units::mps_to_knots(*speed_mps);
    }
    track.points.push_back(point);
    return true;
}

}  // namespace

std::optional<Track> parse_gpx(std::string_view xml_text, std::string* error) {
    pugi::xml_document document;
    if (!xml::load_document(document, xml_text, error)) {
        return std::nullopt;
    }
    const auto root = xml::child(document, "gpx");
    if (!root) {
        set_error(error, "Not a GPX document: the root element is not <gpx>");
        return std::nullopt;
    }

    Track track;
    track.kind = TrackKind::GpxTrack;
    const auto metadata = xml::child(root, "metadata");
    track.name = metadata ? xml::child_text(metadata, "name") : xml::child_text(root, "name");

    for (const auto& trk : root.children()) {
        if (!xml::is_named(trk, "trk")) {
            continue;
        }
        if (track.name.empty()) {
            track.name = xml::child_text(trk, "name");
        }
        for (const auto& segment : trk.children()) {
            if (!xml::is_named(segment, "trkseg")) {
                continue;
            }
            ++track.segment_count;
            for (const auto& point : segment.children()) {
                if (xml::is_named(point, "trkpt") &&
                    !read_point(point, track.points.size(), track, error)) {
                    return std::nullopt;
                }
            }
        }
    }

    if (track.points.empty()) {
        track.kind = TrackKind::GpxRoute;
        track.segment_count = 0;
        for (const auto& route : root.children()) {
            if (!xml::is_named(route, "rte")) {
                continue;
            }
            if (track.name.empty()) {
                track.name = xml::child_text(route, "name");
            }
            ++track.segment_count;
            for (const auto& point : route.children()) {
                if (xml::is_named(point, "rtept") &&
                    !read_point(point, track.points.size(), track, error)) {
                    return std::nullopt;
                }
            }
        }
    }

    if (track.points.empty()) {
        set_error(error, "No track points (<trkpt>) or route points (<rtept>) found");
        return std::nullopt;
    }
    return track;
}

}  // namespace nmeasim::core::track
