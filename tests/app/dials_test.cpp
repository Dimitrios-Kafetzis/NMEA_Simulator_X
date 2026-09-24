// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the dashboard dials `nmeasim::app::CompassDial` and `nmeasim::app::WindDial`.
///
/// Covers the relative-angle helpers `nmeasim::app::signed_relative_angle` and
/// `nmeasim::app::format_wind_angle`, how `nmeasim::app::DashboardWidget` passes a vessel
/// state to both dials and writes the apparent wind on its tile in the same form, the
/// override controls of the rudder (its limit, and staying disabled outside steering mode),
/// and that the dials paint their face in the night and the daylight
/// theme on the offscreen platform. The file reads no fixtures.

#include "widgets/dials.hpp"

#include "theme/theme.hpp"
#include "widgets/dashboard_widget.hpp"
#include "widgets/instrument_tile.hpp"

#include <nmeasim/core/model/vessel_state.hpp>

#include <QDoubleSpinBox>
#include <QImage>
#include <QPoint>
#include <QRegion>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using nmeasim::app::format_wind_angle;
using nmeasim::app::signed_relative_angle;

TEST_CASE("relative angles fold to port and starboard", "[app][dials]") {
    CHECK(signed_relative_angle(0.0) == Approx(0.0));
    CHECK(signed_relative_angle(30.0) == Approx(30.0));
    CHECK(signed_relative_angle(180.0) == Approx(180.0));
    // 255.8 degrees clockwise is 360 - 255.8 = 104.2 degrees to port.
    CHECK(signed_relative_angle(255.8) == Approx(-104.2));
    CHECK(signed_relative_angle(359.0) == Approx(-1.0));
    CHECK(signed_relative_angle(-190.0) == Approx(170.0));
    CHECK(signed_relative_angle(540.0) == Approx(180.0));

    CHECK(format_wind_angle(255.8) == QStringLiteral("104°P"));
    CHECK(format_wind_angle(30.0) == QStringLiteral("30°S"));
    CHECK(format_wind_angle(0.0) == QStringLiteral("0°"));
    // 359.6 folds to -0.4, which rounds to 0: dead ahead has no side letter.
    CHECK(format_wind_angle(359.6) == QStringLiteral("0°"));
    CHECK(format_wind_angle(180.0) == QStringLiteral("180°"));
}

TEST_CASE("the dashboard feeds the compass and the wind dial", "[app][dials]") {
    nmeasim::app::DashboardWidget dashboard;
    nmeasim::core::model::VesselState state;
    state.navigation.position = {37.9838, 23.7275};
    state.navigation.heading_true_deg = 45.0;
    state.navigation.course_over_ground_deg = 47.0;
    state.wind.true_direction_deg = 270.0;
    state.wind.true_speed_kn = 12.0;
    state.wind.apparent_angle_deg = 255.8;
    state.wind.apparent_speed_kn = 8.7;
    dashboard.update_state(state);

    auto* compass = dashboard.compass_dial();
    REQUIRE(compass != nullptr);
    CHECK(compass->heading() == Approx(45.0));
    CHECK(compass->course() == Approx(47.0));
    CHECK_FALSE(compass->bearing().has_value());
    CHECK(dashboard.wind_dial()->apparent_angle() == Approx(255.8));
    // The tile writes the angle as the dial does: 255.8 clockwise is 104 degrees to port.
    CHECK(dashboard.apparent_wind_text() == QStringLiteral("104°P at 8.7 kn"));
    // True wind from 270 with the bow at 045 is 225 degrees clockwise from the bow.
    CHECK(dashboard.wind_dial()->true_angle() == Approx(225.0));

    nmeasim::core::model::Destination destination;
    destination.position = {37.7466, 23.4275};
    destination.origin = state.navigation.position;
    state.destination = destination;
    dashboard.update_state(state);
    REQUIRE(compass->bearing().has_value());
    // The destination, Aegina, lies south-west of the vessel at Athens.
    CHECK(*compass->bearing() > 180.0);
    CHECK(*compass->bearing() < 270.0);
}

TEST_CASE("the dials paint in both themes", "[app][dials]") {
    for (const auto mode : {nmeasim::app::theme::Mode::Night, nmeasim::app::theme::Mode::Day}) {
        nmeasim::app::theme::Theme::instance().apply(mode);
        const auto background = nmeasim::app::theme::Theme::instance().colors().inset;
        nmeasim::app::CompassDial compass;
        compass.resize(240, 240);
        compass.set_values(45.0, 47.0, 225.0);
        nmeasim::app::WindDial wind;
        wind.resize(240, 240);
        wind.set_values(255.8, 8.7, 225.0, 12.0);
        CHECK(compass.hasHeightForWidth());
        CHECK(compass.heightForWidth(300) == 300);
        for (QWidget* dial : {static_cast<QWidget*>(&compass), static_cast<QWidget*>(&wind)}) {
            QImage image(dial->size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            // Without the window background, so that only the dial's own painting shows.
            dial->render(&image, QPoint(), QRegion(), QWidget::DrawChildren);
            // The face is drawn: a point inside the ring has the inset colour or a marking.
            CHECK(image.pixelColor(120, 200).alpha() == 255);
            // The corner outside the bezel stays untouched.
            CHECK(image.pixelColor(1, 1).alpha() == 0);
            int face = 0;
            for (int x = 60; x < 180; ++x) {
                face += image.pixelColor(x, 170) == background ? 1 : 0;
            }
            CHECK(face > 20);
        }
    }
}

TEST_CASE("the rudder control keeps to its limit and stays disabled outside steering mode",
          "[app][dials]") {
    using nmeasim::core::simulation::Parameter;
    nmeasim::app::DashboardWidget dashboard;
    auto* rudder = dashboard.control(Parameter::RudderAngle);
    auto* spin = rudder->findChild<QDoubleSpinBox*>();
    REQUIRE(spin != nullptr);
    // The default limit of a delta simulation, 35 degrees either side.
    CHECK(spin->maximum() == 35.0);
    dashboard.set_rudder_limit(20.0);
    CHECK(spin->minimum() == -20.0);
    CHECK(spin->maximum() == 20.0);

    // Outside steering mode the rudder control is disabled; an override set from elsewhere
    // ticks it without enabling the spin box.
    dashboard.set_steering_mode(false);
    rudder->set_override(true, 40.0);
    CHECK(rudder->override_active());
    CHECK_FALSE(spin->isEnabled());
    CHECK(rudder->override_value() == 20.0);

    dashboard.set_steering_mode(true);
    CHECK(spin->isEnabled());
}
