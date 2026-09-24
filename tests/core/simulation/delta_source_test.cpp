// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::simulation::DeltaSource`, the endless source whose values drift
/// around their seeds.
///
/// The cases cover the start from the seed, motion along a held heading, the derived
/// apparent wind, the drift bounds and their wrap across north, reproducible runs, operator
/// overrides and nudges of every `nmeasim::core::simulation::Parameter`, the value an
/// override reports, the return of a released value to its band, rudder steering, values
/// the host sets directly, reset, and the destination and engines that survive it.
/// No fixture file is read; the seed is `nmeasim::test::fixture_state`.

#include "core/fixtures.hpp"

#include <nmeasim/core/geo/geodesic.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>

using Catch::Approx;
using namespace std::chrono_literals;
namespace sim = nmeasim::core::simulation;

namespace {

/// Returns a delta configuration seeded with `nmeasim::test::fixture_state` in which nothing
/// drifts.
///
/// Every `sim::Variation` is `{0.0, 0.0}`, so a test sees only the deterministic parts of
/// the model (motion, derived values, overrides and steering) and turns on the drift it
/// needs by setting one variation.
///
/// @return The configuration, with the default steering gain, rudder limit and random seed.
sim::DeltaConfig frozen_config() {
    sim::DeltaConfig config;
    config.seed = nmeasim::test::fixture_state();
    config.heading = {0.0, 0.0};
    config.speed = {0.0, 0.0};
    config.depth = {0.0, 0.0};
    config.water_temperature = {0.0, 0.0};
    config.wind_direction = {0.0, 0.0};
    config.wind_speed = {0.0, 0.0};
    return config;
}

}  // namespace

TEST_CASE("the delta source starts from its seed", "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    const auto& state = source.current();
    CHECK(state.navigation.heading_true_deg == Approx(45.0));
    CHECK(state.navigation.position.latitude_deg == Approx(37.9838));
    CHECK(state.water.depth_below_transducer_m == Approx(12.4));
}

TEST_CASE("with zero variation the vessel holds heading and speed and moves along it",
          "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    const auto start = source.current();
    for (int i = 0; i < 600; ++i) {
        source.advance(1000ms);
    }
    const auto& state = source.current();
    CHECK(state.navigation.heading_true_deg == Approx(45.0));
    CHECK(state.navigation.course_over_ground_deg == Approx(45.0));
    CHECK(state.navigation.speed_over_ground_kn == Approx(6.5));
    CHECK(state.navigation.speed_through_water_kn == Approx(6.5));
    CHECK(state.time_utc - start.time_utc == 600s);

    // 6.5 knots for ten minutes is 2006.3 metres. A constant heading traces a rhumb line, so
    // the geodesic back-bearing differs from 045 by a few thousandths of a degree.
    const auto travelled =
        nmeasim::core::geo::inverse(start.navigation.position, state.navigation.position);
    CHECK(travelled.distance_m == Approx(6.5 * 1852.0 / 6.0).epsilon(1e-6));
    CHECK(travelled.initial_bearing_deg == Approx(45.0).margin(0.01));
}

TEST_CASE("apparent wind is derived from true wind and motion", "[simulation][delta]") {
    auto config = frozen_config();
    // Dead ahead of heading 045, so the apparent speed is 10 knots plus the 6.5 of the motion.
    config.seed.wind.true_direction_deg = 45.0;
    config.seed.wind.true_speed_kn = 10.0;
    sim::DeltaSource source(config);
    source.advance(1000ms);
    CHECK(source.current().wind.apparent_speed_kn == Approx(16.5));
    CHECK(source.current().wind.apparent_angle_deg == Approx(0.0).margin(1e-9));
}

TEST_CASE("drifting values stay within the configured amplitude of the seed",
          "[simulation][delta]") {
    auto config = frozen_config();
    config.heading = {3.0, 1.0};
    config.speed = {0.5, 0.2};
    config.depth = {2.0, 0.5};
    config.wind_direction = {15.0, 5.0};
    config.wind_speed = {3.0, 1.0};
    sim::DeltaSource source(config);

    // The bounds are the seed plus or minus each amplitude: heading 045 +/- 3, speed
    // 6.5 +/- 0.5 knots, depth 12.4 +/- 2 metres and wind speed 12 +/- 3 knots.
    bool heading_moved = false;
    for (int i = 0; i < 3600; ++i) {
        const auto& state = source.advance(1000ms);
        const double heading_offset =
            std::remainder(state.navigation.heading_true_deg - 45.0, 360.0);
        CHECK(std::fabs(heading_offset) <= 3.0 + 1e-9);
        CHECK(state.navigation.speed_over_ground_kn >= 6.0 - 1e-9);
        CHECK(state.navigation.speed_over_ground_kn <= 7.0 + 1e-9);
        CHECK(state.water.depth_below_transducer_m >= 10.4 - 1e-9);
        CHECK(state.water.depth_below_transducer_m <= 14.4 + 1e-9);
        CHECK(state.wind.true_speed_kn >= 9.0 - 1e-9);
        CHECK(state.wind.true_speed_kn <= 15.0 + 1e-9);
        heading_moved = heading_moved || std::fabs(heading_offset) > 0.01;
    }
    CHECK(heading_moved);
}

TEST_CASE("runs with the same random seed are reproducible", "[simulation][delta]") {
    auto config = frozen_config();
    config.heading = {3.0, 1.0};
    sim::DeltaSource a(config);
    sim::DeltaSource b(config);
    for (int i = 0; i < 100; ++i) {
        a.advance(500ms);
        b.advance(500ms);
    }
    CHECK(a.current().navigation.heading_true_deg ==
          Approx(b.current().navigation.heading_true_deg));
    CHECK(a.current().navigation.position.latitude_deg ==
          Approx(b.current().navigation.position.latitude_deg));
}

TEST_CASE("heading drift wraps correctly across north", "[simulation][delta]") {
    auto config = frozen_config();
    config.seed.navigation.heading_true_deg = 1.0;
    config.heading = {5.0, 2.0};
    sim::DeltaSource source(config);
    for (int i = 0; i < 1000; ++i) {
        const auto& state = source.advance(1000ms);
        CHECK(state.navigation.heading_true_deg >= 0.0);
        CHECK(state.navigation.heading_true_deg < 360.0);
        const double offset = std::remainder(state.navigation.heading_true_deg - 1.0, 360.0);
        CHECK(std::fabs(offset) <= 5.0 + 1e-9);
    }
}

TEST_CASE("overrides pin a value and nudges move it", "[simulation][delta]") {
    auto config = frozen_config();
    config.speed = {1.0, 0.5};
    sim::DeltaSource source(config);

    source.set_override(sim::Parameter::SpeedOverGround, 3.0);
    for (int i = 0; i < 50; ++i) {
        source.advance(1000ms);
    }
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(3.0));
    CHECK(source.override_value(sim::Parameter::SpeedOverGround) == 3.0);

    source.nudge(sim::Parameter::SpeedOverGround, 0.1);
    source.advance(1000ms);
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(3.1));

    // 3.1 - 10 is negative and is raised to zero.
    source.nudge(sim::Parameter::SpeedOverGround, -10.0);
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(0.0));

    source.clear_override(sim::Parameter::SpeedOverGround);
    CHECK_FALSE(source.override_value(sim::Parameter::SpeedOverGround).has_value());
}

TEST_CASE("every parameter can be overridden and nudged within its range", "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    const auto& state = source.current();

    source.set_override(sim::Parameter::HeadingTrue, -30.0);
    CHECK(state.navigation.heading_true_deg == Approx(330.0));
    source.nudge(sim::Parameter::HeadingTrue, 45.0);
    CHECK(state.navigation.heading_true_deg == Approx(15.0));

    source.set_override(sim::Parameter::SpeedOverGround, -2.0);
    CHECK(state.navigation.speed_over_ground_kn == Approx(0.0));
    source.set_override(sim::Parameter::SpeedThroughWater, -1.0);
    CHECK(state.navigation.speed_through_water_kn == Approx(0.0));
    source.nudge(sim::Parameter::SpeedThroughWater, 4.5);
    CHECK(state.navigation.speed_through_water_kn == Approx(4.5));

    source.set_override(sim::Parameter::Altitude, 150.5);
    source.nudge(sim::Parameter::Altitude, 10.0);
    CHECK(state.navigation.altitude_m == Approx(160.5));

    source.set_override(sim::Parameter::Depth, -5.0);
    CHECK(state.water.depth_below_transducer_m == Approx(0.0));
    source.nudge(sim::Parameter::Depth, 3.2);
    CHECK(state.water.depth_below_transducer_m == Approx(3.2));

    source.set_override(sim::Parameter::WaterTemperature, 4.0);
    source.nudge(sim::Parameter::WaterTemperature, -1.5);
    CHECK(state.water.temperature_c == Approx(2.5));

    source.set_override(sim::Parameter::WindDirectionTrue, 725.0);
    CHECK(state.wind.true_direction_deg == Approx(5.0));
    source.nudge(sim::Parameter::WindDirectionTrue, -10.0);
    CHECK(state.wind.true_direction_deg == Approx(355.0));

    source.set_override(sim::Parameter::WindSpeedTrue, -1.0);
    CHECK(state.wind.true_speed_kn == Approx(0.0));
    source.nudge(sim::Parameter::WindSpeedTrue, 7.0);
    CHECK(state.wind.true_speed_kn == Approx(7.0));

    // The rudder is clamped to the default 35 degrees either side. The nudge starts from the
    // clamped 35 and -45 is clamped to -35, which is also the value the override reports.
    source.set_override(sim::Parameter::RudderAngle, 50.0);
    CHECK(state.steering.rudder_angle_deg == Approx(35.0));
    CHECK(source.override_value(sim::Parameter::RudderAngle) == Approx(35.0));
    source.nudge(sim::Parameter::RudderAngle, -80.0);
    CHECK(state.steering.rudder_angle_deg == Approx(-35.0));
    CHECK(source.override_value(sim::Parameter::RudderAngle) == Approx(-35.0));

    // Overridden values stay put while the simulation advances.
    source.advance(1000ms);
    CHECK(state.navigation.altitude_m == Approx(160.5));
    CHECK(state.water.temperature_c == Approx(2.5));
    CHECK(state.wind.true_direction_deg == Approx(355.0));
}

TEST_CASE("an override reports the value the simulation uses", "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());

    // Normalised into [0, 360) and raised to zero, as the state holds them.
    source.set_override(sim::Parameter::HeadingTrue, -30.0);
    CHECK(source.override_value(sim::Parameter::HeadingTrue) == Approx(330.0));
    source.set_override(sim::Parameter::SpeedOverGround, -2.0);
    CHECK(source.override_value(sim::Parameter::SpeedOverGround) == Approx(0.0));
    source.set_override(sim::Parameter::WindDirectionTrue, 725.0);
    CHECK(source.override_value(sim::Parameter::WindDirectionTrue) == Approx(5.0));
    source.nudge(sim::Parameter::WindDirectionTrue, -10.0);
    CHECK(source.override_value(sim::Parameter::WindDirectionTrue) == Approx(355.0));
}

TEST_CASE("in steering mode a heading override reports the heading the rudder produces",
          "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    source.set_override(sim::Parameter::HeadingTrue, 90.0);
    source.set_steering_mode(true);
    // 10 degrees of rudder turn 6 degrees per minute: 090 to 096 in a minute.
    source.set_override(sim::Parameter::RudderAngle, 10.0);
    source.advance(60s);
    CHECK(source.current().navigation.heading_true_deg == Approx(96.0));
    CHECK(source.override_value(sim::Parameter::HeadingTrue) == Approx(96.0));

    // Leaving steering mode, the override holds the heading where the rudder left it.
    source.set_steering_mode(false);
    source.advance(60s);
    CHECK(source.current().navigation.heading_true_deg == Approx(96.0));
    CHECK(source.override_value(sim::Parameter::HeadingTrue) == Approx(96.0));
}

TEST_CASE("a released value pinned outside its band drifts back at its step rate",
          "[simulation][delta]") {
    auto config = frozen_config();
    // Seed 6.5 knots, so the band is [5.5, 7.5] and the value moves at most 0.5 knots a second.
    config.speed = {1.0, 0.5};
    // Seed 045, band [040, 050], at most 2 degrees a second.
    config.heading = {5.0, 2.0};
    sim::DeltaSource source(config);
    source.set_override(sim::Parameter::SpeedOverGround, 20.0);
    source.set_override(sim::Parameter::HeadingTrue, 90.0);
    source.clear_override(sim::Parameter::SpeedOverGround);
    source.clear_override(sim::Parameter::HeadingTrue);

    // Outside the band every step moves the value a full step towards it.
    source.advance(1000ms);
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(19.5));
    CHECK(source.current().navigation.heading_true_deg == Approx(88.0));
    CHECK(source.current().navigation.rate_of_turn_deg_per_min == Approx(-120.0));
    for (int i = 0; i < 9; ++i) {
        source.advance(1000ms);
    }
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(15.0));
    CHECK(source.current().navigation.heading_true_deg == Approx(70.0));

    // Once back inside the band the value walks at random and stays there.
    for (int i = 0; i < 100; ++i) {
        source.advance(1000ms);
    }
    for (int i = 0; i < 100; ++i) {
        const auto& state = source.advance(1000ms);
        CHECK(state.navigation.speed_over_ground_kn >= 5.5 - 1e-9);
        CHECK(state.navigation.speed_over_ground_kn <= 7.5 + 1e-9);
        CHECK(std::fabs(std::remainder(state.navigation.heading_true_deg - 45.0, 360.0)) <=
              5.0 + 1e-9);
    }
}

TEST_CASE("the delta source ignores the transport controls of replay sources",
          "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    const auto before = source.current().navigation.position;
    CHECK_FALSE(source.provides_sentences());
    source.seek(60s);
    source.step_once();
    CHECK(source.take_sentences().empty());
    CHECK(source.current().navigation.position.latitude_deg == Approx(before.latitude_deg));
    CHECK(source.current().navigation.position.longitude_deg == Approx(before.longitude_deg));
}

TEST_CASE("heading override normalises and stops rate of turn", "[simulation][delta]") {
    auto config = frozen_config();
    config.heading = {5.0, 2.0};
    sim::DeltaSource source(config);
    source.set_override(sim::Parameter::HeadingTrue, 370.0);
    source.advance(1000ms);
    CHECK(source.current().navigation.heading_true_deg == Approx(10.0));
    CHECK(source.current().navigation.rate_of_turn_deg_per_min == Approx(0.0));
}

TEST_CASE("steering mode turns the vessel according to the rudder", "[simulation][delta]") {
    auto config = frozen_config();
    config.turn_rate_per_rudder_deg = 0.6;
    sim::DeltaSource source(config);
    source.set_steering_mode(true);
    // 10 degrees of rudder at 0.6 degrees per minute per degree turn 6 degrees per minute, so
    // a minute takes the heading from 045 to 051.
    source.set_override(sim::Parameter::RudderAngle, 10.0);

    for (int i = 0; i < 60; ++i) {
        source.advance(1000ms);
    }
    CHECK(source.current().navigation.rate_of_turn_deg_per_min == Approx(6.0));
    CHECK(source.current().navigation.heading_true_deg == Approx(51.0));

    // Clamped to -35 degrees, which turns 21 degrees to port in a minute: 051 to 030.
    source.set_override(sim::Parameter::RudderAngle, -50.0);
    CHECK(source.current().steering.rudder_angle_deg == Approx(-35.0));
    source.advance(60s);
    CHECK(source.current().navigation.heading_true_deg == Approx(30.0));
}

TEST_CASE("position, fix and satellites can be set directly", "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    source.set_position({-38.9997, 151.5001});
    source.set_fix(false);
    // Fewer satellites in view than in use are raised to the number in use.
    source.set_satellites(5, 3);
    const auto& state = source.current();
    CHECK(state.navigation.position.latitude_deg == Approx(-38.9997));
    CHECK_FALSE(state.gnss.has_fix);
    CHECK(state.gnss.satellites_in_use == 5);
    CHECK(state.gnss.satellites_in_view == 5);
}

TEST_CASE("reset returns to the seed and clears overrides", "[simulation][delta]") {
    auto config = frozen_config();
    config.heading = {5.0, 2.0};
    sim::DeltaSource source(config);
    source.set_override(sim::Parameter::Depth, 99.0);
    source.advance(30s);
    source.reset();
    CHECK(source.current().water.depth_below_transducer_m == Approx(12.4));
    CHECK(source.current().navigation.position.latitude_deg == Approx(37.9838));
    CHECK_FALSE(source.override_value(sim::Parameter::Depth).has_value());
}

TEST_CASE("the destination and the engines can be changed and survive a reset",
          "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    REQUIRE(source.current().destination.has_value());
    CHECK(source.current().destination->name == "AEGINA");
    source.set_destination(nmeasim::core::model::Destination{
        "POROS", {37.5, 23.45}, source.current().navigation.position, 50.0});
    source.advance(10s);
    source.reset();
    REQUIRE(source.current().destination.has_value());
    CHECK(source.current().destination->name == "POROS");
    CHECK(source.current().destination->arrival_radius_m == Approx(50.0));
    source.set_destination(std::nullopt);
    CHECK_FALSE(source.current().destination.has_value());
    source.reset();
    CHECK_FALSE(source.current().destination.has_value());

    REQUIRE(source.current().engines.size() == 2);
    source.set_engine(1, {"Starboard engine", true, 2200.0, 85.0});
    // An index past the end appends; removing index 0 drops the port engine and index 9 is
    // ignored.
    source.set_engine(5, {"Generator", true, 1500.0, 70.0});
    REQUIRE(source.current().engines.size() == 3);
    CHECK(source.current().engines[1].running);
    CHECK(source.current().engines[1].revolutions_rpm == Approx(2200.0));
    CHECK(source.current().engines[2].label == "Generator");
    source.remove_engine(0);
    source.remove_engine(9);
    source.reset();
    REQUIRE(source.current().engines.size() == 2);
    CHECK(source.current().engines[0].label == "Starboard engine");
}
