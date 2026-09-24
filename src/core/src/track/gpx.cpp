// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// GPX 1.0 and 1.1 reader: `parse_gpx` and the reading of single track and route points.
///
/// @see GPX 1.1, https://www.topografix.com/GPX/1/1/

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

/// Stores an error message when the caller asked for one.
///
/// @param error Destination of the message; nothing is stored when it is null.
/// @param message The one-line reason.
void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

/// Reads an optional numeric child of a point, looking in `<extensions>` as well.
///
/// GPX 1.0 has `<course>` and `<speed>` as direct children of a point; GPX 1.1 dropped them,
/// and writers put them in `<extensions>`, either directly or inside an extension element
/// such as `gpxtpx:TrackPointExtension`. A direct child wins; otherwise the first match in
/// document order among the children and grandchildren of `<extensions>` is used.
///
/// @param point The `<trkpt>` or `<rtept>` element.
/// @param local Local name of the value, such as `speed`; the prefix is ignored.
/// @return The number, or `std::nullopt` when no such element exists or its text is not a
///         number.
/// @pre `local` is not null.
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

/// Reads one `<trkpt>` or `<rtept>` and appends it to a track.
///
/// @param node The point element.
/// @param index 0-based number of the point in the file; messages show it 1-based.
/// @param[in,out] track The track the point is appended to; unchanged on failure.
/// @param error Receives the reason when the point is rejected (invalid or out-of-range
///        `lat`/`lon`, unparsable `<time>`); see `parse_gpx`. May be null.
/// @return True when the point was appended, false when it is rejected.
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
    // GPX 1.1 keeps the document name in <metadata>, GPX 1.0 directly under <gpx>; a file
    // may have <metadata> without a name and still name itself under <gpx>.
    if (const auto metadata = xml::child(root, "metadata")) {
        track.name = xml::child_text(metadata, "name");
    }
    if (track.name.empty()) {
        track.name = xml::child_text(root, "name");
    }

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

    // Routes are a fallback only: a file with both keeps its recorded track.
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
