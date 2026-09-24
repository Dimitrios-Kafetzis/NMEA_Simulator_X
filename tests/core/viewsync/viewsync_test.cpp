// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::viewsync::encode_packet`, the ViewSync camera packet for Google
/// Earth.
///
/// The cases check the field layout and number formats with the default camera settings and
/// with custom altitude, tilt, roll and planet, the largest packet counter, the epoch offset
/// of ViewSync times, and the removal of separators and control characters from the planet. No
/// fixture file is read; the packet is encoded from `nmeasim::test::fixture_state`.
///
/// @see https://github.com/LiquidGalaxy/liquid-galaxy/wiki/GoogleEarth_ViewSync

#include "core/fixtures.hpp"

#include <nmeasim/core/viewsync/viewsync.hpp>

#include <catch2/catch_test_macros.hpp>

namespace viewsync = nmeasim::core::viewsync;

TEST_CASE("a ViewSync packet follows the Liquid Galaxy layout", "[viewsync]") {
    const auto state = nmeasim::test::fixture_state();
    viewsync::ViewSyncOptions options;
    // 2026-09-22T12:34:56Z is 1790080496 Unix seconds; ViewSync counts from year 1, which
    // adds `kSecondsBeforeUnixEpoch`. The altitude 512.30 is the fixture's 12.3 m plus the
    // default camera height of 500 m.
    CHECK(viewsync::encode_packet(state, options, 7) ==
          "7,37.9838000,23.7275000,512.30,45.00,60.00,0.00,63925677296,63925677296,");
    options.camera_altitude_m = 1000.0;
    options.tilt_deg = 0.0;
    options.roll_deg = -5.5;
    options.planet = "mars";
    CHECK(viewsync::encode_packet(state, options, 4294967295U) ==
          "4294967295,37.9838000,23.7275000,1012.30,45.00,0.00,-5.50,63925677296,63925677296,"
          "mars");
    CHECK(viewsync::kSecondsBeforeUnixEpoch == 62135596800);
}

TEST_CASE("the planet name cannot add a field or a line to a ViewSync packet", "[viewsync]") {
    const auto state = nmeasim::test::fixture_state();
    viewsync::ViewSyncOptions options;
    // Commas, CR, LF, other control characters and bytes outside ASCII are removed; the
    // packet keeps its ten fields and one line.
    options.planet = "mars,1,2\r\n\tmoon\x7f\xe9";
    const auto packet = viewsync::encode_packet(state, options, 1);
    CHECK(packet ==
          "1,37.9838000,23.7275000,512.30,45.00,60.00,0.00,63925677296,63925677296,"
          "mars12moon");
    CHECK(viewsync::sanitize_planet("moon") == "moon");
    CHECK(viewsync::sanitize_planet("sky") == "sky");
    CHECK(viewsync::sanitize_planet("").empty());
    CHECK(viewsync::sanitize_planet(",,,").empty());
}
