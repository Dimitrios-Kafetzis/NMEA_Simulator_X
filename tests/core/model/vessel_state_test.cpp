#include <nmeasim/core/model/vessel_state.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
namespace model = nmeasim::core::model;

TEST_CASE("magnetic heading removes variation and deviation from true heading", "[model]") {
    model::Navigation navigation;
    navigation.heading_true_deg = 45.0;
    navigation.magnetic_variation_deg = 4.6;
    navigation.magnetic_deviation_deg = 1.4;
    CHECK(navigation.heading_magnetic_deg() == Approx(39.0));

    navigation.heading_true_deg = 2.0;
    navigation.magnetic_variation_deg = 5.0;
    navigation.magnetic_deviation_deg = 0.0;
    CHECK(navigation.heading_magnetic_deg() == Approx(357.0));
}

TEST_CASE("magnetic course over ground removes variation only", "[model]") {
    model::Navigation navigation;
    navigation.course_over_ground_deg = 10.0;
    navigation.magnetic_variation_deg = -3.0;
    navigation.magnetic_deviation_deg = 2.0;
    CHECK(navigation.course_over_ground_magnetic_deg() == Approx(13.0));
}

TEST_CASE("relative true wind angle is measured clockwise from the bow", "[model]") {
    model::Wind wind;
    wind.true_direction_deg = 270.0;
    CHECK(wind.true_angle_relative_deg(45.0) == Approx(225.0));
    CHECK(wind.true_angle_relative_deg(300.0) == Approx(330.0));
    CHECK(wind.true_angle_relative_deg(270.0) == Approx(0.0));
}
