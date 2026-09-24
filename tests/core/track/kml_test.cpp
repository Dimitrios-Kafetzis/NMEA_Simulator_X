// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::track::parse_kml`, the KML 2.2 track reader.
///
/// The cases cover a `LineString` as an untimed track, `gx:Track` elements concatenated
/// with their timestamps, a `MultiGeometry` whose lines are kept and whose polygon is
/// skipped, and every reason a file is rejected.
///
/// Fixture files, all under tests/fixtures/tracks: linestring.kml, gx_track.kml,
/// multi_geometry.kml, and the rejected malformed.kml, no_geometry.kml, bad_coordinates.kml
/// and timestamped.gpx (a GPX file, not KML).
///
/// @see OGC KML 2.2, https://www.ogc.org/standard/kml/

#include "core/fixtures.hpp"

#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/core/track/kml.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using Catch::Approx;
using namespace std::chrono_literals;
using nmeasim::core::time::parse_iso8601;
using nmeasim::test::read_fixture;
namespace track = nmeasim::core::track;

TEST_CASE("a KML LineString becomes an untimed track", "[track][kml]") {
    std::string error;
    const auto parsed = track::parse_kml(read_fixture("tracks/linestring.kml"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->kind == track::TrackKind::KmlLineString);
    CHECK(parsed->name == "Leg one");
    CHECK(parsed->segment_count == 1);
    REQUIRE(parsed->points.size() == 3);
    CHECK(parsed->points[0].position.latitude_deg == Approx(37.9));
    CHECK(parsed->points[0].position.longitude_deg == Approx(23.6));
    CHECK(parsed->points[0].elevation_m == Approx(0.0));
    CHECK(parsed->points[1].elevation_m == Approx(2.5));
    CHECK_FALSE(parsed->points[2].elevation_m.has_value());
    CHECK(parsed->points[2].position.longitude_deg == Approx(23.61));
    CHECK_FALSE(parsed->has_timestamps());
}

TEST_CASE("gx:Track elements are concatenated with their timestamps", "[track][kml]") {
    std::string error;
    const auto parsed = track::parse_kml(read_fixture("tracks/gx_track.kml"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->kind == track::TrackKind::KmlTrack);
    CHECK(parsed->name == "Morning sail");
    CHECK(parsed->segment_count == 2);
    REQUIRE(parsed->points.size() == 4);
    CHECK(parsed->has_timestamps());
    CHECK(parsed->duration() == 9min);
    CHECK(parsed->points[0].time == parse_iso8601("2026-09-23T10:00:00Z"));
    CHECK(parsed->points[1].elevation_m == Approx(3.0));
    CHECK(parsed->points[3].position.longitude_deg == Approx(23.61));
    CHECK_FALSE(parsed->points[3].elevation_m.has_value());
}

TEST_CASE("a MultiGeometry keeps its lines in document order and skips polygons", "[track][kml]") {
    std::string error;
    const auto parsed = track::parse_kml(read_fixture("tracks/multi_geometry.kml"), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->segment_count == 2);
    REQUIRE(parsed->points.size() == 4);
    CHECK(parsed->points[1].position.longitude_deg == Approx(23.61));
    CHECK(parsed->points[2].position.longitude_deg == Approx(23.62));
}

TEST_CASE("malformed KML files are rejected with a reason", "[track][kml]") {
    std::string error;
    CHECK_FALSE(track::parse_kml(read_fixture("tracks/malformed.kml"), &error).has_value());
    CHECK(error.find("Invalid XML") != std::string::npos);

    CHECK_FALSE(track::parse_kml(read_fixture("tracks/no_geometry.kml"), &error).has_value());
    CHECK(error.find("LineString") != std::string::npos);

    CHECK_FALSE(track::parse_kml(read_fixture("tracks/bad_coordinates.kml"), &error).has_value());
    CHECK(error.find("coordinate 2") != std::string::npos);

    CHECK_FALSE(track::parse_kml(read_fixture("tracks/timestamped.gpx"), &error).has_value());
    CHECK(error.find("<kml>") != std::string::npos);

    CHECK_FALSE(track::parse_kml("<kml><Placemark><LineString><coordinates>200,0 1,1"
                                 "</coordinates></LineString></Placemark></kml>",
                                 &error)
                    .has_value());
    CHECK(error.find("out of range") != std::string::npos);
    CHECK_FALSE(track::parse_kml("<kml><Placemark><gx:Track><when>noon</when><gx:coord>1 2"
                                 "</gx:coord></gx:Track></Placemark></kml>",
                                 &error)
                    .has_value());
    CHECK(error.find("noon") != std::string::npos);
}
