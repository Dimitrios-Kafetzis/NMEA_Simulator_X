// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the leg geometry computed by nmeasim::core::geo::solve_leg().
///
/// Covers the bearings and distances of a leg, the sign of the cross-track error on either
/// side of the leg and the along-track distance behind the origin and beyond the destination.
/// No fixture file is read.

#include <nmeasim/core/geo/route.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using nmeasim::core::geo::Position;
using nmeasim::core::geo::solve_leg;

TEST_CASE("a leg is solved for a vessel beside it", "[geo][route]") {
    // Values checked independently with GeographicLib's Python bindings and the spherical
    // cross-track formula.
    const Position origin{38.0, 23.7};
    const Position destination{37.7466, 23.4275};
    const auto leg = solve_leg(origin, destination, {37.9838, 23.7275});
    CHECK(leg.leg_bearing_deg == Approx(220.529).margin(1e-3));
    CHECK(leg.leg_length_m == Approx(36957.9).margin(0.1));
    CHECK(leg.bearing_deg == Approx(225.168).margin(1e-3));
    CHECK(leg.distance_m == Approx(37282.7).margin(0.1));
    CHECK(leg.cross_track_m == Approx(-3004.5).margin(0.5));
    // The vessel projects slightly behind the origin along the leg.
    CHECK(leg.along_track_m == Approx(-203.5).margin(0.5));
}

TEST_CASE("the cross-track sign follows the side of the leg", "[geo][route]") {
    const Position origin{38.0, 23.7};
    const Position destination{38.0, 23.9};                                    // due east
    CHECK(solve_leg(origin, destination, {38.01, 23.8}).cross_track_m < 0.0);  // north: left
    CHECK(solve_leg(origin, destination, {37.99, 23.8}).cross_track_m > 0.0);  // south: right
    // The geodesic bulges a few metres north of the parallel, so a vessel on the parallel is
    // a little to the right of the leg.
    const auto on_leg = solve_leg(origin, destination, {38.0, 23.8});
    CHECK(on_leg.cross_track_m == Approx(0.0).margin(10.0));
    CHECK(on_leg.along_track_m == Approx(on_leg.leg_length_m / 2.0).margin(5.0));
}

TEST_CASE("the along-track distance is negative behind the origin and past the leg beyond it",
          "[geo][route]") {
    const Position origin{38.0, 23.7};
    const Position destination{38.0, 23.9};
    CHECK(solve_leg(origin, destination, {38.0, 23.6}).along_track_m < 0.0);
    const auto beyond = solve_leg(origin, destination, {38.0, 24.0});
    CHECK(beyond.along_track_m > beyond.leg_length_m);
    const auto at_origin = solve_leg(origin, destination, origin);
    CHECK(at_origin.cross_track_m == 0.0);
    CHECK(at_origin.along_track_m == 0.0);
    CHECK(at_origin.distance_m == Approx(at_origin.leg_length_m));
}
