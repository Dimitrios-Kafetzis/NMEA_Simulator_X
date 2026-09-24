// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Display names of track kinds and the timing and length queries of `Track`.

#include <nmeasim/core/track/track.hpp>

#include <cstddef>

namespace nmeasim::core::track {

const char* to_string(TrackKind kind) noexcept {
    switch (kind) {
        case TrackKind::GpxTrack:
            return "GPX track";
        case TrackKind::GpxRoute:
            return "GPX route";
        case TrackKind::KmlTrack:
            return "KML track";
        case TrackKind::KmlLineString:
            return "KML line";
    }
    // Reached only for a value outside the enumeration, which a cast can produce.
    return "track";
}

bool Track::has_timestamps() const noexcept {
    if (points.empty()) {
        return false;
    }
    std::optional<std::chrono::system_clock::time_point> previous;
    for (const auto& point : points) {
        if (!point.time) {
            return false;
        }
        if (previous && *point.time < *previous) {
            return false;
        }
        previous = point.time;
    }
    return true;
}

std::optional<std::chrono::milliseconds> Track::duration() const {
    if (!has_timestamps()) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(*points.back().time -
                                                                 *points.front().time);
}

double Track::length_m() const {
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        total += geo::inverse(points[i - 1].position, points[i].position).distance_m;
    }
    return total;
}

}  // namespace nmeasim::core::track
