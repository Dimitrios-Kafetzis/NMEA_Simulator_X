#include "core/fixtures.hpp"

#include <nmeasim/core/viewsync/viewsync.hpp>

#include <catch2/catch_test_macros.hpp>

namespace viewsync = nmeasim::core::viewsync;

TEST_CASE("a ViewSync packet follows the Liquid Galaxy layout", "[viewsync]") {
    const auto state = nmeasim::test::fixture_state();
    viewsync::ViewSyncOptions options;
    // 2026-09-22T12:34:56Z is 1790080496 Unix seconds; ViewSync counts from year 1.
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
