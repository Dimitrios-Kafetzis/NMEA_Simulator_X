// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// KML 2.2 track reader: `parse_kml` and the traversal that collects `<gx:Track>` and
/// `<LineString>` geometries.
///
/// @see OGC KML 2.2, https://www.ogc.org/standard/kml/

#include "xml_helpers.hpp"

#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/core/track/kml.hpp>

#include <cctype>
#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

/// Splits text into its whitespace-separated words.
///
/// @param text The text to split.
/// @return The non-empty words in order, as views into `text`; empty when `text` holds only
///         whitespace. Whitespace is what `std::isspace` classifies as such.
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

/// Splits text at every occurrence of a separator.
///
/// @param text The text to split.
/// @param separator The separator character.
/// @return The parts in order, as views into `text`, including empty parts between adjacent
///         separators or at either end; one part (all of `text`) when there is no separator.
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

/// Reads a coordinate tuple into a point: longitude, latitude and optional altitude.
///
/// The tuple has already been split, at commas for a `<coordinates>` tuple
/// (`lon,lat[,alt]`) or at whitespace for a `<gx:coord>` (`lon lat [alt]`). Parts after the
/// third are ignored, and an altitude that is not a number is left absent.
///
/// @param parts The values of the tuple, in KML order: longitude first.
/// @param number 1-based number of the point in the whole file, counted across geometries,
///        as the messages show it.
/// @param[out] point Receives the position and elevation; its time is not touched.
/// @param error Receives the reason when the tuple is rejected (fewer than two values, a
///        longitude or latitude that is not a number, or one out of range); see
///        `parse_kml`. May be null.
/// @return True when the tuple was read, false when it is rejected.
bool read_tuple(const std::vector<std::string_view>& parts, std::size_t number, TrackPoint& point,
                std::string* error) {
    if (parts.size() < 2) {
        set_error(error, std::format("point {}: expected longitude and latitude", number));
        return false;
    }
    const auto lon = xml::parse_number(parts[0]);
    const auto lat = xml::parse_number(parts[1]);
    if (!lon || !lat) {
        set_error(error,
                  std::format("point {}: '{},{}' is not numeric", number, parts[0], parts[1]));
        return false;
    }
    if (*lat < -90.0 || *lat > 90.0 || *lon < -180.0 || *lon > 180.0) {
        set_error(error,
                  std::format("point {}: coordinates {}, {} are out of range", number, *lat, *lon));
        return false;
    }
    point.position = {*lat, *lon};
    if (parts.size() >= 3) {
        point.elevation_m = xml::parse_number(parts[2]);
    }
    return true;
}

/// Depth-first traversal of a KML document that concatenates its track geometries.
///
/// It keeps the name of the placemark it is inside and of the first named container so
/// that the first geometry can name the track. The error pointer it is given must outlive
/// it.
class Reader {
public:
    /// Creates a reader with an empty track.
    ///
    /// @param error Receives the reason when a geometry is rejected. May be null; not
    ///        owned, and must outlive the reader.
    explicit Reader(std::string* error) : error_(error) {}

    /// Visits an element and everything below it, reading every `<gx:Track>` and
    /// `<LineString>` into the track.
    ///
    /// A `<gx:Track>` or `<LineString>` is read and not descended into; nodes that are not
    /// elements are skipped. Visiting stops at the first rejected geometry.
    ///
    /// @param node The element to visit, normally the `<kml>` root.
    /// @return True when every geometry below `node` was read, false when one was rejected
    ///         and the error was set.
    bool visit(const pugi::xml_node& node) {
        if (node.type() != pugi::node_element) {
            return true;
        }
        if (xml::is_named(node, "Placemark")) {
            // The name, even an empty one, belongs to the geometries of this placemark only.
            auto outer = std::exchange(placemark_name_, xml::child_text(node, "name"));
            const bool read = visit_children(node);
            placemark_name_ = std::move(outer);
            return read;
        }
        if ((xml::is_named(node, "Document") || xml::is_named(node, "Folder")) &&
            document_name_.empty()) {
            document_name_ = xml::child_text(node, "name");
        }
        if (xml::is_named(node, "Track")) {
            return read_track(node);
        }
        if (xml::is_named(node, "LineString")) {
            return read_line_string(node);
        }
        return visit_children(node);
    }

    /// Returns the track collected so far; its `kind` is not set by the reader.
    ///
    /// @return A reference to the reader's track, valid while the reader lives; the caller
    ///         may move from it.
    [[nodiscard]] Track& track() noexcept { return track_; }
    /// Tells whether a `<gx:Track>` element provided points.
    ///
    /// @return True when at least one `<gx:Track>` added a point to the track; false when
    ///         the points all come from `<LineString>` elements, even when an empty
    ///         `<gx:Track>` was met.
    [[nodiscard]] bool track_points() const noexcept { return track_points_; }

private:
    /// Visits every child of an element in document order.
    ///
    /// @param node The element whose children to visit.
    /// @return True when every geometry below `node` was read, false when one was rejected
    ///         and the error was set.
    bool visit_children(const pugi::xml_node& node) {
        for (const auto& element : node.children()) {
            if (!visit(element)) {
                return false;
            }
        }
        return true;
    }

    /// Names the track, unless it already has a name: the `<name>` of the `<Placemark>`
    /// holding the geometry being read, else the first named `<Document>` or `<Folder>` met
    /// so far.
    void take_name() {
        if (track_.name.empty()) {
            track_.name = !placemark_name_.empty() ? placemark_name_ : document_name_;
        }
    }

    /// Reads a `<gx:Track>`, pairing each `<gx:coord>` with the `<when>` at the same index.
    ///
    /// Points without a matching `<when>` get no time; surplus `<when>` elements are
    /// ignored. The track's points are appended only when the whole element is valid.
    ///
    /// @param node The `<gx:Track>` element.
    /// @return True when the element was read, false when a `<when>` or a `<gx:coord>` is
    ///         rejected and the error was set.
    bool read_track(const pugi::xml_node& node) {
        ++track_.segment_count;
        take_name();
        std::vector<std::optional<std::chrono::system_clock::time_point>> whens;
        std::vector<TrackPoint> points;
        for (const auto& element : node.children()) {
            if (xml::is_named(element, "when")) {
                const auto text = xml::trim(element.child_value());
                const auto parsed = time::parse_iso8601(text);
                if (!parsed) {
                    // The `<when>` belongs to the point at the same index of this track.
                    set_error(error_, std::format("point {}: '{}' is not an ISO 8601 time",
                                                  track_.points.size() + whens.size() + 1, text));
                    return false;
                }
                whens.push_back(parsed);
            } else if (xml::is_named(element, "coord")) {
                TrackPoint point;
                if (!read_tuple(split_whitespace(element.child_value()),
                                track_.points.size() + points.size() + 1, point, error_)) {
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
        track_points_ = track_points_ || !points.empty();
        return true;
    }

    /// Reads the `<coordinates>` of a `<LineString>` as `lon,lat[,alt]` tuples separated by
    /// whitespace.
    ///
    /// A missing or empty `<coordinates>` yields no point but still counts as a segment.
    /// Points read before a rejected tuple stay in the track, which is discarded anyway.
    ///
    /// @param node The `<LineString>` element.
    /// @return True when every tuple was read, false when one is rejected and the error was
    ///         set.
    bool read_line_string(const pugi::xml_node& node) {
        ++track_.segment_count;
        take_name();
        const auto tuples = split_whitespace(xml::child(node, "coordinates").child_value());
        for (const auto tuple : tuples) {
            TrackPoint point;
            if (!read_tuple(split(tuple, ','), track_.points.size() + 1, point, error_)) {
                return false;
            }
            track_.points.push_back(point);
        }
        return true;
    }

    /// Destination of the rejection reason; may be null, not owned.
    std::string* error_;
    /// The points and name collected so far.
    Track track_;
    /// `<name>` of the `<Placemark>` being visited; empty outside a placemark or when it has
    /// none.
    std::string placemark_name_;
    /// First non-empty `<name>` of a `<Document>` or `<Folder>`; empty before one.
    std::string document_name_;
    /// Whether a `<gx:Track>` added points, which makes the result a `TrackKind::KmlTrack`.
    bool track_points_{false};
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
    track.kind = reader.track_points() ? TrackKind::KmlTrack : TrackKind::KmlLineString;
    return track;
}

}  // namespace nmeasim::core::track
