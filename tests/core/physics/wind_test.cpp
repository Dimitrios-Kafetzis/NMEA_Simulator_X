#include <nmeasim/core/physics/wind.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
namespace physics = nmeasim::core::physics;

TEST_CASE("apparent wind on a stationary vessel equals the true wind", "[physics][wind]") {
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
    const auto wind = physics::apparent_wind(180.0, 5.0, 0.0, 10.0);
    CHECK(wind.speed_kn == Approx(5.0));
    CHECK(wind.angle_relative_deg == Approx(0.0).margin(1e-9));
}

TEST_CASE("calm air on a stationary vessel yields zero speed and the true direction",
          "[physics][wind]") {
    const auto wind = physics::apparent_wind(123.0, 0.0, 10.0, 0.0);
    CHECK(wind.speed_kn == Approx(0.0));
    CHECK(wind.direction_true_deg == Approx(123.0));
    CHECK(wind.angle_relative_deg == Approx(113.0));
}
