// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::track::parse_gpx`, the GPX 1.0 and 1.1 reader.
///
/// The cases cover timed tracks with several segments, untimed tracks, GPX 1.0 course and
/// speed elements, speed and course in GPX 1.1 extensions, routes, namespace prefixes, times
/// out of order, every reason a file is rejected, the track name taken from under the root
/// when the metadata has none, and optional values that are not numbers.
///
/// Fixture files, all under tests/fixtures/tracks: timestamped.gpx, untimestamped.gpx,
/// gpx10_course_speed.gpx, extensions_speed.gpx, route.gpx, prefixed.gpx,
/// unordered_times.gpx, and the rejected malformed.gpx, no_points.gpx, bad_coordinates.gpx,
/// bad_time.gpx and not_gpx.gpx.
///
/// @see GPX 1.1, https://www.topografix.com/GPX/1/1/

#include "core/fixtures.hpp"

#include <nmeasim/core/geo/geodesic.hpp>
#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/core/track/gpx.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <string>

using Catch::Approx;
using namespace std::chrono_literals;
using nmeasim::core::time::parse_iso8601;
using nmeasim::test::read_fixture;
namespace track = nmeasim::core::track;

TEST_CASE("a timestamped GPX track concatenates its segments", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(read_fixture("tracks/timestamped.gpx"), &error);
    REQUIRE(parsed.has_value());
    CHECK(error.empty());
    CHECK(parsed->name == "Saronic test track");
    CHECK(parsed->kind == track::TrackKind::GpxTrack);
    CHECK(parsed->segment_count == 2);
    REQUIRE(parsed->points.size() == 5);
    CHECK(parsed->has_timestamps());
    CHECK(parsed->duration() == 12min + 500ms);

    const auto& first = parsed->points.front();
    CHECK(first.position.latitude_deg == Approx(37.9));
    CHECK(first.position.longitude_deg == Approx(23.6));
    CHECK(first.elevation_m == Approx(1.0));
    CHECK(first.time == parse_iso8601("2026-09-23T10:00:00Z"));
    CHECK_FALSE(first.course_deg.has_value());
    CHECK_FALSE(first.speed_kn.has_value());

    const auto& fourth = parsed->points[3];
    CHECK_FALSE(fourth.elevation_m.has_value());
    CHECK(fourth.time == parse_iso8601("2026-09-23T10:09:00.5Z"));
    CHECK(parsed->points[4].time == parse_iso8601("2026-09-23T10:12:00.5Z"));
    double expected = 0.0;
    for (std::size_t i = 1; i < parsed->points.size(); ++i) {
        expected +=
            nmeasim::core::geo::inverse(parsed->points[i - 1].position, parsed->points[i].position)
                .distance_m;
    }
    // Two legs of 0.01 degrees north (1109.9 m each) and two of 0.01 degrees east at
    // 37.92 N (879.3 m each), 3978.4 m in all according to GeographicLib; the bound
    // guards against a sum that skipped a leg.
    CHECK(expected > 3900.0);
    CHECK(parsed->length_m() == Approx(expected));
}

TEST_CASE("a GPX track without times is untimed", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(read_fixture("tracks/untimestamped.gpx"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->name == "Plain track");
    CHECK(parsed->points.size() == 3);
    CHECK_FALSE(parsed->has_timestamps());
    CHECK_FALSE(parsed->duration().has_value());
    CHECK(parsed->segment_count == 1);
}

TEST_CASE("GPX 1.0 course and speed are read and converted", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(read_fixture("tracks/gpx10_course_speed.gpx"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->name == "GPX 1.0 with course and speed");
    REQUIRE(parsed->points.size() == 3);
    // GPX speeds are in metres per second: 5.144444 m/s is 10 knots.
    CHECK(parsed->points[0].course_deg == Approx(12.5));
    CHECK(parsed->points[0].speed_kn == Approx(10.0).epsilon(1e-4));
    CHECK(parsed->points[1].course_deg == Approx(355.0));
    CHECK(parsed->points[1].speed_kn == Approx(5.0).epsilon(1e-4));
    CHECK_FALSE(parsed->points[2].course_deg.has_value());
    CHECK_FALSE(parsed->points[2].speed_kn.has_value());
}

TEST_CASE("speed and course inside GPX 1.1 extensions are read", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(read_fixture("tracks/extensions_speed.gpx"), &error);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->points.size() == 3);
    // 3 and 4 m/s in knots; 0.514444 m/s is one knot (1852 m per hour).
    CHECK(parsed->points[0].speed_kn == Approx(3.0 / 0.514444).epsilon(1e-4));
    CHECK(parsed->points[0].course_deg == Approx(90.0));
    CHECK(parsed->points[1].speed_kn == Approx(4.0 / 0.514444).epsilon(1e-4));
    CHECK_FALSE(parsed->points[1].course_deg.has_value());
    CHECK_FALSE(parsed->points[2].speed_kn.has_value());
}

TEST_CASE("a GPX route is used when the file has no track", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(read_fixture("tracks/route.gpx"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->kind == track::TrackKind::GpxRoute);
    CHECK(parsed->name == "Piraeus to Aegina");
    CHECK(parsed->segment_count == 1);
    REQUIRE(parsed->points.size() == 3);
    CHECK(parsed->points[2].position.latitude_deg == Approx(37.75));
    CHECK_FALSE(parsed->has_timestamps());
}

TEST_CASE("namespace prefixes on GPX elements are ignored", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(read_fixture("tracks/prefixed.gpx"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->points.size() == 2);
    CHECK(parsed->has_timestamps());
}

TEST_CASE("a GPX track with times out of order is untimed", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(read_fixture("tracks/unordered_times.gpx"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->points.size() == 3);
    CHECK_FALSE(parsed->has_timestamps());
}

TEST_CASE("malformed GPX files are rejected with a reason", "[track][gpx]") {
    std::string error;
    CHECK_FALSE(track::parse_gpx(read_fixture("tracks/malformed.gpx"), &error).has_value());
    CHECK(error.find("Invalid XML") != std::string::npos);

    CHECK_FALSE(track::parse_gpx(read_fixture("tracks/no_points.gpx"), &error).has_value());
    CHECK(error.find("No track points") != std::string::npos);

    CHECK_FALSE(track::parse_gpx(read_fixture("tracks/bad_coordinates.gpx"), &error).has_value());
    CHECK(error.find("point 2") != std::string::npos);
    CHECK(error.find("out of range") != std::string::npos);

    CHECK_FALSE(track::parse_gpx(read_fixture("tracks/bad_time.gpx"), &error).has_value());
    CHECK(error.find("yesterday") != std::string::npos);

    CHECK_FALSE(track::parse_gpx(read_fixture("tracks/not_gpx.gpx"), &error).has_value());
    CHECK(error.find("<gpx>") != std::string::npos);

    CHECK_FALSE(track::parse_gpx("", &error).has_value());
    CHECK_FALSE(
        track::parse_gpx("<gpx><trk><trkseg><trkpt lon=\"1\"/></trkseg></trk></gpx>", &error)
            .has_value());
    CHECK(error.find("lat/lon") != std::string::npos);
    // A null error pointer is accepted.
    CHECK_FALSE(track::parse_gpx("<gpx/>", nullptr).has_value());
}

TEST_CASE("a GPX name under the root is used when the metadata has none", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(
        "<gpx><metadata><author/></metadata><name>Passage</name><trk><name>Leg</name><trkseg>"
        "<trkpt lat=\"37.9\" lon=\"23.6\"/></trkseg></trk></gpx>",
        &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->name == "Passage");

    // A metadata name still wins over the root name.
    const auto both = track::parse_gpx(
        "<gpx><metadata><name>Meta</name></metadata><name>Passage</name><trk><trkseg>"
        "<trkpt lat=\"37.9\" lon=\"23.6\"/></trkseg></trk></gpx>",
        &error);
    REQUIRE(both.has_value());
    CHECK(both->name == "Meta");
}

TEST_CASE("unreadable optional GPX values are left absent", "[track][gpx]") {
    std::string error;
    const auto parsed = track::parse_gpx(
        "<gpx><trk><trkseg><trkpt lat=\"37.9\" lon=\"23.6\"><ele>high</ele><course>north</course>"
        "<speed>fast</speed></trkpt></trkseg></trk></gpx>",
        &error);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->points.size() == 1);
    CHECK_FALSE(parsed->points[0].elevation_m.has_value());
    CHECK_FALSE(parsed->points[0].course_deg.has_value());
    CHECK_FALSE(parsed->points[0].speed_kn.has_value());
}
