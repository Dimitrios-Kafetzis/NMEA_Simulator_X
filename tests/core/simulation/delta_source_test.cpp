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

    // 6.5 knots for ten minutes is 2005.7 metres. A constant heading traces a rhumb line, so
    // the geodesic back-bearing differs from 045 by a few thousandths of a degree.
    const auto travelled =
        nmeasim::core::geo::inverse(start.navigation.position, state.navigation.position);
    CHECK(travelled.distance_m == Approx(6.5 * 1852.0 / 6.0).epsilon(1e-6));
    CHECK(travelled.initial_bearing_deg == Approx(45.0).margin(0.01));
}

TEST_CASE("apparent wind is derived from true wind and motion", "[simulation][delta]") {
    auto config = frozen_config();
    config.seed.wind.true_direction_deg = 45.0;  // dead ahead of heading 045
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

    source.nudge(sim::Parameter::SpeedOverGround, -10.0);
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(0.0));

    source.clear_override(sim::Parameter::SpeedOverGround);
    CHECK_FALSE(source.override_value(sim::Parameter::SpeedOverGround).has_value());
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
    source.set_override(sim::Parameter::RudderAngle, 10.0);  // 6 degrees per minute

    for (int i = 0; i < 60; ++i) {
        source.advance(1000ms);
    }
    CHECK(source.current().navigation.rate_of_turn_deg_per_min == Approx(6.0));
    CHECK(source.current().navigation.heading_true_deg == Approx(51.0));

    source.set_override(sim::Parameter::RudderAngle, -50.0);  // clamped to -35
    CHECK(source.current().steering.rudder_angle_deg == Approx(-35.0));
    source.advance(60s);
    CHECK(source.current().navigation.heading_true_deg == Approx(30.0));
}

TEST_CASE("position, fix and satellites can be set directly", "[simulation][delta]") {
    sim::DeltaSource source(frozen_config());
    source.set_position({-38.9997, 151.5001});
    source.set_fix(false);
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
