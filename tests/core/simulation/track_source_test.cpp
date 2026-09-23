#include "core/fixtures.hpp"

#include <nmeasim/core/geo/geodesic.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/simulation/simulation.hpp>
#include <nmeasim/core/simulation/track_source.hpp>
#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/core/track/track_file.hpp>
#include <nmeasim/core/units.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>

using Catch::Approx;
using namespace std::chrono_literals;
using nmeasim::core::geo::Position;
using nmeasim::core::time::parse_iso8601;
namespace geo = nmeasim::core::geo;
namespace sim = nmeasim::core::simulation;
namespace track = nmeasim::core::track;
namespace units = nmeasim::core::units;

namespace {

track::Track fixture_track(const char* name) {
    std::string error;
    auto loaded = track::load_track(nmeasim::test::fixture_path(name), &error);
    REQUIRE(loaded.has_value());
    return *loaded;
}

sim::TrackConfig config_for(const char* name, sim::EndBehaviour end = sim::EndBehaviour::Stop,
                            double speed_kn = 6.0, bool use_timestamps = true) {
    sim::TrackConfig config;
    config.track = fixture_track(name);
    config.seed = nmeasim::test::fixture_state();
    config.speed_kn = speed_kn;
    config.use_timestamps = use_timestamps;
    config.end = end;
    return config;
}

double distance_between(Position a, Position b) {
    return geo::inverse(a, b).distance_m;
}

}  // namespace

TEST_CASE("a timed track is followed on its own timestamps", "[simulation][track]") {
    sim::TrackSource source(config_for("tracks/timestamped.gpx"));
    const auto& points = source.config().track.points;
    CHECK(source.timed());
    CHECK(source.duration() == 12min + 500ms);
    CHECK(source.position() == 0ms);
    CHECK_FALSE(source.finished());

    const auto& start = source.current();
    CHECK(start.navigation.position.latitude_deg == Approx(37.9));
    CHECK(start.time_utc == parse_iso8601("2026-09-23T10:00:00Z"));
    const double leg0_m = distance_between(points[0].position, points[1].position);
    CHECK(start.navigation.speed_over_ground_kn == Approx(units::mps_to_knots(leg0_m / 180.0)));
    CHECK(start.navigation.course_over_ground_deg == Approx(0.0).margin(1e-6));
    CHECK(start.navigation.heading_true_deg == Approx(0.0).margin(1e-6));
    CHECK(start.navigation.altitude_m == Approx(1.0));
    // Environment values come from the seed.
    CHECK(start.water.depth_below_transducer_m == Approx(12.4));

    // Half way along the first leg after 90 seconds.
    source.advance(90s);
    const auto& half = source.current();
    CHECK(distance_between(points[0].position, half.navigation.position) ==
          Approx(leg0_m / 2.0).margin(0.5));
    CHECK(half.time_utc == parse_iso8601("2026-09-23T10:01:30Z"));
    CHECK(half.navigation.altitude_m == Approx(2.0));
    CHECK(source.position() == 90000ms);
    CHECK(source.point_index() == 0);

    // Exactly at the second point after 180 seconds, at the third after 360.
    source.advance(90s);
    CHECK(source.current().navigation.position.latitude_deg == Approx(37.91).margin(1e-9));
    source.advance(180s);
    CHECK(source.current().navigation.position.latitude_deg == Approx(37.92).margin(1e-9));
    CHECK(source.point_index() == 2);
    // The next leg turns east across the segment boundary; the rate of turn reflects it.
    CHECK(source.current().navigation.course_over_ground_deg == Approx(90.0).margin(0.01));
    CHECK(source.current().navigation.rate_of_turn_deg_per_min > 0.0);
    CHECK(source.current().navigation.speed_through_water_kn ==
          Approx(source.current().navigation.speed_over_ground_kn));

    // Running past the end stops at the last point.
    source.advance(1h);
    CHECK(source.finished());
    CHECK(source.position() == source.duration());
    CHECK(source.current().navigation.position.longitude_deg == Approx(23.62).margin(1e-9));
    CHECK(source.current().navigation.speed_over_ground_kn == 0.0);
    CHECK(source.current().time_utc == parse_iso8601("2026-09-23T10:12:00.5Z"));
    // Further steps change nothing.
    source.advance(10s);
    CHECK(source.position() == source.duration());

    source.reset();
    CHECK_FALSE(source.finished());
    CHECK(source.position() == 0ms);
    CHECK(source.current().navigation.position.latitude_deg == Approx(37.9));
}

TEST_CASE("an untimed track is sailed at the configured speed", "[simulation][track]") {
    auto config = config_for("tracks/untimestamped.gpx", sim::EndBehaviour::Stop, 6.0);
    sim::TrackSource source(config);
    const auto& points = source.config().track.points;
    CHECK_FALSE(source.timed());
    const double length_m = config.track.length_m();
    const double expected_s = length_m / units::knots_to_mps(6.0);
    REQUIRE(source.duration().has_value());
    CHECK(static_cast<double>(source.duration()->count()) ==
          Approx(expected_s * 1000.0).margin(1.0));
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(6.0));
    CHECK(source.current().time_utc == config.seed.time_utc);

    source.advance(60s);
    const double sailed = units::knots_to_mps(6.0) * 60.0;
    CHECK(distance_between(points[0].position, source.current().navigation.position) ==
          Approx(sailed).margin(0.5));
    CHECK(source.current().time_utc == config.seed.time_utc + 60s);
    // The course turns east at the second point.
    source.advance(10min);
    CHECK(source.current().navigation.course_over_ground_deg == Approx(90.0).margin(0.01));
}

TEST_CASE("recorded point speeds and courses are used when the track has no timestamps",
          "[simulation][track]") {
    sim::TrackSource source(
        config_for("tracks/extensions_speed.gpx", sim::EndBehaviour::Stop, 6.0));
    CHECK(source.current().navigation.speed_over_ground_kn ==
          Approx(units::mps_to_knots(3.0)).epsilon(1e-6));
    CHECK(source.current().navigation.course_over_ground_deg == Approx(90.0));
    source.jump_to_point(1);
    CHECK(source.point_index() == 1);
    CHECK(source.current().navigation.speed_over_ground_kn ==
          Approx(units::mps_to_knots(4.0)).epsilon(1e-6));
    // No recorded course on the second point: the leg bearing is used.
    CHECK(source.current().navigation.course_over_ground_deg == Approx(90.0).margin(0.01));
    source.jump_to_point(2);
    CHECK(source.finished());
    CHECK(source.current().navigation.speed_over_ground_kn == 0.0);
}

TEST_CASE("timestamps can be ignored to sail a timed track at a set speed", "[simulation][track]") {
    sim::TrackSource timed(config_for("tracks/gpx10_course_speed.gpx"));
    CHECK(timed.timed());
    CHECK(timed.current().navigation.speed_over_ground_kn == Approx(10.0).epsilon(1e-4));
    CHECK(timed.current().navigation.course_over_ground_deg == Approx(12.5));
    CHECK(timed.duration() == 6min);

    sim::TrackSource untimed(
        config_for("tracks/gpx10_course_speed.gpx", sim::EndBehaviour::Stop, 4.0, false));
    CHECK_FALSE(untimed.timed());
    // Recorded speeds still win over the configured one; the last leg has none.
    CHECK(untimed.current().navigation.speed_over_ground_kn == Approx(10.0).epsilon(1e-4));
    untimed.jump_to_point(1);
    CHECK(untimed.current().navigation.speed_over_ground_kn == Approx(5.0).epsilon(1e-4));
    CHECK(untimed.current().time_utc == untimed.config().seed.time_utc);
}

TEST_CASE("a looping track starts again and its clock keeps running", "[simulation][track]") {
    auto config = config_for("tracks/untimestamped.gpx", sim::EndBehaviour::Loop, 6.0);
    sim::TrackSource source(config);
    const auto total = *source.duration();
    source.advance(total + 10s);
    CHECK_FALSE(source.finished());
    CHECK(static_cast<double>(source.position().count()) == Approx(10000.0).margin(1.0));
    CHECK(source.point_index() == 0);
    CHECK(source.current().time_utc == config.seed.time_utc + total + 10s);

    // A timed loop rewinds the track clock as well.
    sim::TrackSource timed(config_for("tracks/timestamped.gpx", sim::EndBehaviour::Loop));
    timed.advance(*timed.duration() + 30s);
    CHECK_FALSE(timed.finished());
    CHECK(timed.current().time_utc == parse_iso8601("2026-09-23T10:00:30Z"));
}

TEST_CASE("seeking and jumping move the vessel immediately", "[simulation][track]") {
    sim::TrackSource source(config_for("tracks/timestamped.gpx"));
    source.seek(3min);
    CHECK(source.position() == 180000ms);
    CHECK(source.current().navigation.position.latitude_deg == Approx(37.91).margin(1e-9));
    CHECK(source.current().navigation.rate_of_turn_deg_per_min == 0.0);
    source.seek(-5s);
    CHECK(source.position() == 0ms);
    source.seek(1h);
    CHECK(source.position() == source.duration());
    CHECK(source.finished());
    source.seek(4min);
    CHECK_FALSE(source.finished());
    CHECK(source.point_index() == 1);

    source.jump_to_point(3);
    CHECK(source.point_index() == 3);
    CHECK(source.current().navigation.position.longitude_deg == Approx(23.61).margin(1e-9));
    CHECK(source.current().time_utc == parse_iso8601("2026-09-23T10:09:00.5Z"));
    source.jump_to_point(99);
    CHECK(source.finished());
    CHECK(source.point_index() == 4);
}

TEST_CASE("degenerate tracks are handled", "[simulation][track]") {
    sim::TrackConfig single;
    single.seed = nmeasim::test::fixture_state();
    single.track.points.push_back({{10.0, 20.0}, 5.0, {}, 33.0, {}});
    sim::TrackSource source(single);
    CHECK(source.duration() == 0ms);
    CHECK(source.current().navigation.position.latitude_deg == Approx(10.0));
    CHECK(source.current().navigation.course_over_ground_deg == Approx(33.0));
    CHECK(source.current().navigation.altitude_m == Approx(5.0));
    CHECK_FALSE(source.finished());
    source.advance(1s);
    CHECK(source.finished());
    CHECK(source.point_index() == 0);
    source.jump_to_point(5);
    CHECK(source.position() == 0ms);

    // Repeated points with equal timestamps produce a zero-length leg and no NaN.
    sim::TrackConfig repeated;
    repeated.seed = nmeasim::test::fixture_state();
    const auto t0 = *parse_iso8601("2026-09-23T10:00:00Z");
    repeated.track.points.push_back({{37.90, 23.60}, {}, t0, {}, {}});
    repeated.track.points.push_back({{37.90, 23.60}, {}, t0, {}, {}});
    repeated.track.points.push_back({{37.91, 23.60}, {}, t0 + 60s, {}, {}});
    sim::TrackSource stalled(repeated);
    CHECK(stalled.timed());
    stalled.advance(30s);
    CHECK_FALSE(std::isnan(stalled.current().navigation.position.latitude_deg));
    CHECK(stalled.current().navigation.position.latitude_deg > 37.90);
    CHECK(stalled.current().navigation.course_over_ground_deg == Approx(0.0).margin(1e-6));
    CHECK(stalled.duration() == 60000ms);

    // A zero configured speed is raised to the minimum instead of making legs endless.
    sim::TrackConfig crawl = config_for("tracks/untimestamped.gpx", sim::EndBehaviour::Stop, 0.0);
    sim::TrackSource slow(crawl);
    CHECK(slow.current().navigation.speed_over_ground_kn == Approx(0.1));
    CHECK(slow.duration()->count() > 0);
}

TEST_CASE("a track source drives a simulation to completion", "[simulation][track]") {
    auto config = config_for("tracks/timestamped.gpx");
    config.seed.gnss.satellites_in_use = 7;
    sim::Simulation simulation(std::make_unique<sim::TrackSource>(std::move(config)),
                               sim::SentenceScheduler{});
    CHECK(simulation.source().duration().has_value());
    int rounds = 0;
    while (!simulation.finished() && rounds < 10000) {
        const auto sentences = simulation.step(1s);
        if (rounds == 0) {
            REQUIRE_FALSE(sentences.empty());
            CHECK(sentences.front().id == "RMC");
            // The first step advances one second before the first round is encoded.
            CHECK(sentences.front().text.find("100001.00") != std::string::npos);
        }
        ++rounds;
    }
    CHECK(simulation.finished());
    CHECK(rounds == 721);
    CHECK(simulation.state().gnss.satellites_in_use == 7);
    simulation.source().seek(1min);
    CHECK(simulation.source().position() == 60000ms);
    CHECK_FALSE(simulation.finished());
    simulation.reset();
    CHECK(simulation.source().position() == 0ms);
}

TEST_CASE("the delta source is endless and ignores seeks", "[simulation][track]") {
    sim::DeltaConfig config;
    config.seed = nmeasim::test::fixture_state();
    sim::DeltaSource source(config);
    sim::Source& base = source;
    CHECK_FALSE(base.duration().has_value());
    CHECK(base.position() == 0ms);
    base.seek(5min);
    CHECK(base.position() == 0ms);
    CHECK_FALSE(base.finished());
}
