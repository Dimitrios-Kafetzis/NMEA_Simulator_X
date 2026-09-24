// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::physics::apparent_wind`, the wind triangle that turns the true
/// wind into the wind felt on board.
///
/// The cases cover a stationary vessel, head, following and beam winds, the relative angle
/// following the heading, a vessel faster than its following wind, and calm air. Each
/// expected value follows from adding the true wind vector and the head wind of the
/// vessel's own motion.

#include <nmeasim/core/physics/wind.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
namespace physics = nmeasim::core::physics;

TEST_CASE("apparent wind on a stationary vessel equals the true wind", "[physics][wind]") {
    // From 270 on a heading of 045 the wind comes from 225 degrees relative to the bow.
    const auto wind = physics::apparent_wind(270.0, 12.0, 45.0, 0.0);
    CHECK(wind.speed_kn == Approx(12.0));
    CHECK(wind.direction_true_deg == Approx(270.0));
    CHECK(wind.angle_relative_deg == Approx(225.0));
}

TEST_CASE("a head wind strengthens with boat speed", "[physics][wind]") {
    const auto wind = physics::apparent_wind(0.0, 10.0, 0.0, 5.0);
    CHECK(wind.speed_kn == Approx(15.0));
    CHECK(wind.angle_relative_deg == Approx(0.0).margin(1e-9));
    CHECK(wind.direction_true_deg == Approx(0.0).margin(1e-9));
}

TEST_CASE("a following wind weakens with boat speed", "[physics][wind]") {
    const auto wind = physics::apparent_wind(180.0, 10.0, 0.0, 5.0);
    CHECK(wind.speed_kn == Approx(5.0));
    CHECK(wind.angle_relative_deg == Approx(180.0));
}

TEST_CASE("a beam wind moves forward as the vessel accelerates", "[physics][wind]") {
    // A 10-knot wind from the beam and the 10-knot head wind of the motion add to
    // 10 * sqrt(2) knots from 45 degrees on the bow.
    const auto wind = physics::apparent_wind(90.0, 10.0, 0.0, 10.0);
    CHECK(wind.speed_kn == Approx(14.142135623730951));
    CHECK(wind.angle_relative_deg == Approx(45.0));
    CHECK(wind.direction_true_deg == Approx(45.0));

    const auto port = physics::apparent_wind(270.0, 10.0, 0.0, 10.0);
    CHECK(port.angle_relative_deg == Approx(315.0));
}

TEST_CASE("relative angle follows the heading", "[physics][wind]") {
    const auto wind = physics::apparent_wind(90.0, 10.0, 90.0, 10.0);
    CHECK(wind.angle_relative_deg == Approx(0.0).margin(1e-9));
    CHECK(wind.speed_kn == Approx(20.0));
}

TEST_CASE("a vessel outrunning its following wind feels wind from ahead", "[physics][wind]") {
    // 10 knots of head wind from the motion minus 5 knots from astern leave 5 knots from
    // ahead.
    const auto wind = physics::apparent_wind(180.0, 5.0, 0.0, 10.0);
    CHECK(wind.speed_kn == Approx(5.0));
    CHECK(wind.angle_relative_deg == Approx(0.0).margin(1e-9));
}

TEST_CASE("calm air on a stationary vessel yields zero speed and the true direction",
          "[physics][wind]") {
    // With no apparent wind the true direction is reported, 113 degrees relative to a
    // heading of 010.
    const auto wind = physics::apparent_wind(123.0, 0.0, 10.0, 0.0);
    CHECK(wind.speed_kn == Approx(0.0));
    CHECK(wind.direction_true_deg == Approx(123.0));
    CHECK(wind.angle_relative_deg == Approx(113.0));
}
