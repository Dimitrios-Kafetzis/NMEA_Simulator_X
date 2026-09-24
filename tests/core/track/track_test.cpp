// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::track::Track`, `nmeasim::core::track::to_string` and the file
/// loaders `nmeasim::core::track::parse_track` and `nmeasim::core::track::load_track`.
///
/// The cases cover when a track counts as timed, its duration and length, the names of the
/// track kinds, the choice of reader by the extension of the file name (a dotted directory
/// or a missing extension included), the file name as fallback track name, load errors,
/// and the sample tracks shipped with the application.
///
/// Fixture files: tests/fixtures/tracks/route.gpx, linestring.kml, untimestamped.gpx,
/// multi_geometry.kml, prefixed.gpx and malformed.gpx (and the absent missing.gpx); the
/// samples samples/saronic-gulf.gpx, samples/saronic-route.gpx and samples/saronic-gulf.kml,
/// found through the `NMEASIM_SAMPLES_DIR` compile definition.

#include "core/fixtures.hpp"

#include <nmeasim/core/geo/geodesic.hpp>
#include <nmeasim/core/track/track.hpp>
#include <nmeasim/core/track/track_file.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using Catch::Approx;
using namespace std::chrono_literals;
namespace track = nmeasim::core::track;

TEST_CASE("a track knows whether it is timed, its duration and its length", "[track]") {
    track::Track empty;
    CHECK_FALSE(empty.has_timestamps());
    CHECK_FALSE(empty.duration().has_value());
    CHECK(empty.length_m() == 0.0);

    track::Track timed;
    const auto t0 = std::chrono::system_clock::time_point{};
    timed.points.push_back({{37.90, 23.60}, {}, t0, {}, {}});
    timed.points.push_back({{37.91, 23.60}, {}, t0 + 180s, {}, {}});
    CHECK(timed.has_timestamps());
    CHECK(timed.duration() == 180000ms);
    const double leg = nmeasim::core::geo::inverse({37.90, 23.60}, {37.91, 23.60}).distance_m;
    // 0.01 degrees of latitude is 1109.9 m here according to GeographicLib.
    CHECK(leg > 1100.0);
    CHECK(timed.length_m() == Approx(leg));

    // One point without a time, or times going backwards, make the track untimed.
    timed.points.push_back({{37.92, 23.60}, {}, {}, {}, {}});
    CHECK_FALSE(timed.has_timestamps());
    timed.points.back().time = t0 + 60s;
    CHECK_FALSE(timed.has_timestamps());
    timed.points.back().time = t0 + 180s;
    CHECK(timed.has_timestamps());
    CHECK(timed.duration() == 180000ms);
}

TEST_CASE("track kinds have names", "[track]") {
    CHECK(std::string{track::to_string(track::TrackKind::GpxTrack)} == "GPX track");
    CHECK(std::string{track::to_string(track::TrackKind::GpxRoute)} == "GPX route");
    CHECK(std::string{track::to_string(track::TrackKind::KmlTrack)} == "KML track");
    CHECK(std::string{track::to_string(track::TrackKind::KmlLineString)} == "KML line");
}

TEST_CASE("track files are dispatched on their extension", "[track]") {
    std::string error;
    const auto gpx =
        track::parse_track("Sail.GPX", nmeasim::test::read_fixture("tracks/route.gpx"), &error);
    REQUIRE(gpx.has_value());
    CHECK(gpx->kind == track::TrackKind::GpxRoute);
    const auto kml = track::parse_track(
        "track.kml", nmeasim::test::read_fixture("tracks/linestring.kml"), &error);
    REQUIRE(kml.has_value());
    CHECK(kml->kind == track::TrackKind::KmlLineString);

    CHECK_FALSE(track::parse_track("track.csv", "1,2", &error).has_value());
    CHECK(error.find(".csv") != std::string::npos);
    CHECK_FALSE(track::parse_track("noextension", "", &error).has_value());
    CHECK(error == "The track file 'noextension' has no extension; expected .gpx or .kml");

    // Only the file name is looked at, so a dot in a directory name is not an extension.
    CHECK_FALSE(track::parse_track("/home/sail/v1.2/passage", "", &error).has_value());
    CHECK(error == "The track file 'passage' has no extension; expected .gpx or .kml");
    CHECK_FALSE(track::parse_track("C:\\tracks.old\\passage", "", &error).has_value());
    CHECK(error == "The track file 'passage' has no extension; expected .gpx or .kml");
    const auto dotted = track::parse_track("/home/sail/v1.2/passage.gpx",
                                           nmeasim::test::read_fixture("tracks/route.gpx"), &error);
    CHECK(dotted.has_value());
}

TEST_CASE("tracks are loaded from disk with the file name as fallback name", "[track]") {
    std::string error;
    const auto loaded =
        track::load_track(nmeasim::test::fixture_path("tracks/untimestamped.gpx"), &error);
    REQUIRE(loaded.has_value());
    CHECK(loaded->name == "Plain track");
    CHECK(loaded->points.size() == 3);

    const auto unnamed =
        track::load_track(nmeasim::test::fixture_path("tracks/multi_geometry.kml"), &error);
    REQUIRE(unnamed.has_value());
    CHECK(unnamed->name == "Two lines");

    const auto prefixed =
        track::load_track(nmeasim::test::fixture_path("tracks/prefixed.gpx"), &error);
    REQUIRE(prefixed.has_value());
    // prefixed.gpx names neither its metadata nor its track.
    CHECK(prefixed->name == "prefixed.gpx");

    CHECK_FALSE(
        track::load_track(nmeasim::test::fixture_path("tracks/missing.gpx"), &error).has_value());
    CHECK(error.find("Cannot read") != std::string::npos);
    CHECK_FALSE(
        track::load_track(nmeasim::test::fixture_path("tracks/malformed.gpx"), &error).has_value());
    CHECK(error.find("malformed.gpx") != std::string::npos);
    CHECK(error.find("Invalid XML") != std::string::npos);
}

TEST_CASE("the shipped sample tracks load", "[track]") {
    std::string error;
    const std::string samples{NMEASIM_SAMPLES_DIR};
    const auto gpx = track::load_track(samples + "/saronic-gulf.gpx", &error);
    REQUIRE(gpx.has_value());
    CHECK(gpx->kind == track::TrackKind::GpxTrack);
    CHECK(gpx->has_timestamps());
    CHECK(gpx->segment_count == 2);
    CHECK(gpx->points.size() == 12);
    CHECK(gpx->length_m() > 25000.0);

    const auto route = track::load_track(samples + "/saronic-route.gpx", &error);
    REQUIRE(route.has_value());
    CHECK(route->kind == track::TrackKind::GpxRoute);
    CHECK(route->points.size() == 8);

    const auto kml = track::load_track(samples + "/saronic-gulf.kml", &error);
    REQUIRE(kml.has_value());
    CHECK(kml->kind == track::TrackKind::KmlTrack);
    // The KML sample holds the same passage as the GPX track.
    CHECK(kml->points.size() == gpx->points.size());
    CHECK(kml->duration() == gpx->duration());
}
