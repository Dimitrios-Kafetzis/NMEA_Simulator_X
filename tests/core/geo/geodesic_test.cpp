// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the WGS 84 geodesic solutions of `nmeasim/core/geo/geodesic.hpp`.
///
/// Covers nmeasim::core::geo::normalize_bearing(), nmeasim::core::geo::inverse() against
/// distances along the equator and the prime meridian that follow from the WGS 84 ellipsoid,
/// and the agreement of nmeasim::core::geo::direct() with inverse(). No fixture file is read.

#include <nmeasim/core/geo/geodesic.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace geo = nmeasim::core::geo;
using Catch::Approx;

TEST_CASE("normalize_bearing maps any angle into [0, 360)", "[geo]") {
    CHECK(geo::normalize_bearing(0.0) == Approx(0.0));
    CHECK(geo::normalize_bearing(359.9) == Approx(359.9));
    CHECK(geo::normalize_bearing(360.0) == Approx(0.0));
    CHECK(geo::normalize_bearing(725.0) == Approx(5.0));
    CHECK(geo::normalize_bearing(-90.0) == Approx(270.0));
    CHECK(geo::normalize_bearing(-360.0) == Approx(0.0));
}

TEST_CASE("inverse solves known WGS84 distances", "[geo]") {
    SECTION("one degree of latitude along the prime meridian from the equator") {
        const auto s = geo::inverse({0.0, 0.0}, {1.0, 0.0});
        // The length of the WGS 84 meridian arc from the equator to latitude 1 degree.
        CHECK(s.distance_m == Approx(110574.39).margin(0.5));
        CHECK(s.initial_bearing_deg == Approx(0.0).margin(1e-9));
        CHECK(s.final_bearing_deg == Approx(0.0).margin(1e-9));
    }
    SECTION("one degree of longitude along the equator") {
        const auto s = geo::inverse({0.0, 0.0}, {0.0, 1.0});
        // One degree of the equator, a circle: 6378137 m (the WGS 84 semi-major axis) * pi / 180.
        CHECK(s.distance_m == Approx(111319.49).margin(0.5));
        CHECK(s.initial_bearing_deg == Approx(90.0).margin(1e-9));
    }
    SECTION("bearings are normalised, so westward travel reports 270 rather than -90") {
        const auto s = geo::inverse({0.0, 1.0}, {0.0, 0.0});
        CHECK(s.initial_bearing_deg == Approx(270.0).margin(1e-9));
    }
    SECTION("coincident positions have zero distance") {
        const auto s = geo::inverse({-38.9997, 151.5001}, {-38.9997, 151.5001});
        CHECK(s.distance_m == Approx(0.0).margin(1e-9));
    }
}

TEST_CASE("direct and inverse are consistent", "[geo]") {
    const geo::Position start{-38.9997, 151.5001};
    const double bearing = 47.5;
    const double distance = 12345.6;

    const auto end = geo::direct(start, bearing, distance);
    const auto back = geo::inverse(start, end);

    CHECK(back.distance_m == Approx(distance).margin(1e-6));
    CHECK(back.initial_bearing_deg == Approx(bearing).margin(1e-9));
}
