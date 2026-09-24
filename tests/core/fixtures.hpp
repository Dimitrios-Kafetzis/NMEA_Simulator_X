// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Shared fixtures of the core test suite: fixture file access, reference vessel states and
/// a sentence helper.
///
/// The test files of the `nmeasim_core_tests` executable include it: the AIS, NMEA 0183, log,
/// Signal K, simulation, track and ViewSync tests. `fixture_state` is the reference vessel whose
/// encoded sentences are the golden values of tests/core/nmea0183/encoders_test.cpp and the
/// examples in docs/reference/nmea0183-sentences.md, so a change to one of its values
/// changes expected output across the suite. The file paths rely on the
/// `NMEASIM_FIXTURES_DIR` compile definition that tests/CMakeLists.txt sets to tests/fixtures.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

/// Shared helpers of the test suites: fixtures and utilities that several test files use.
///
/// It holds the core suite's fixture files and reference vessel states
/// (tests/core/fixtures.hpp) and the Qt event-loop helper of the `nmeasim::io` suite
/// (tests/io/event_loop.hpp). It is compiled only into the test executables.
namespace nmeasim::test {

/// Returns the absolute path of a file under tests/fixtures.
///
/// @param relative Path relative to tests/fixtures with `/` separators, such as
///   `tracks/timestamped.gpx`.
/// @return `NMEASIM_FIXTURES_DIR`, a `/` and `relative`; whether the file exists is not
///   checked, so tests can also name missing files.
inline std::string fixture_path(std::string_view relative) {
    return std::string{NMEASIM_FIXTURES_DIR} + "/" + std::string{relative};
}

/// Returns the contents of a file under tests/fixtures.
///
/// The file is read in binary mode, so line terminators reach the parser byte for byte.
///
/// @param relative Path relative to tests/fixtures, as for `fixture_path`.
/// @return The whole file, or an empty string when it does not exist or cannot be read.
inline std::string read_fixture(std::string_view relative) {
    std::ifstream file(fixture_path(relative), std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// Returns the reference vessel state, off Athens at 12:34:56.780 UTC on 22 September 2026.
///
/// Every navigation, GNSS, steering, water, wind, engine and destination value is set, and
/// the values are chosen so that every encoder has something distinctive to show: a
/// non-zero millisecond part for the time fields; course 047.3 and heading 045.0 that
/// differ, and an easterly variation of 4.6 degrees that makes every magnetic value
/// different from its true one (heading 040.4 and course 042.7 magnetic); a negative rate of
/// turn and rudder angle, which exercise the signs of a turn to port; ten satellites in view, which
/// need three GSV sentences; a positive transducer offset, which gives the Signal K
/// `surfaceToTransducer` and `belowSurface` depths; one running and one stopped engine; and a
/// destination off the vessel's track. The AIS static data keeps the `model::AisStatic` defaults
/// (MMSI 239000001, which also forms the default Signal K context).
///
/// @return A new copy of the state; tests modify their copy freely.
inline core::model::VesselState fixture_state() {
    using namespace std::chrono;
    core::model::VesselState state;
    state.time_utc = sys_days{2026y / September / 22d} + 12h + 34min + 56s + 780ms;
    state.navigation.position = {37.9838, 23.7275};
    state.navigation.altitude_m = 12.3;
    state.navigation.course_over_ground_deg = 47.3;
    state.navigation.speed_over_ground_kn = 6.5;
    state.navigation.heading_true_deg = 45.0;
    state.navigation.magnetic_variation_deg = 4.6;
    state.navigation.magnetic_deviation_deg = 0.0;
    state.navigation.speed_through_water_kn = 6.2;
    state.navigation.rate_of_turn_deg_per_min = -2.5;
    state.gnss.has_fix = true;
    state.gnss.quality = core::model::FixQuality::Gps;
    state.gnss.satellites_in_use = 8;
    state.gnss.satellites_in_view = 10;
    state.gnss.hdop = 0.9;
    state.gnss.pdop = 1.7;
    state.gnss.vdop = 1.4;
    state.gnss.geoid_separation_m = 34.5;
    state.steering.rudder_angle_deg = -3.5;
    state.water.depth_below_transducer_m = 12.4;
    state.water.transducer_offset_m = 0.5;
    state.water.temperature_c = 21.5;
    state.wind.true_direction_deg = 270.0;
    state.wind.true_speed_kn = 12.0;
    state.wind.apparent_angle_deg = 300.0;
    state.wind.apparent_speed_kn = 14.2;
    state.engines = {{"Port engine", true, 1800.0, 82.0}, {"Starboard engine", false, 0.0, 65.5}};
    // A leg from off Piraeus towards Aegina; the vessel is about 1.6 nm left of it.
    state.destination = core::model::Destination{"AEGINA", {37.7466, 23.4275}, {38.0, 23.7}, 100.0};
    return state;
}

/// Returns `fixture_state` with the GNSS receiver reporting no fix.
///
/// Every other value, the satellite counts included, is unchanged, so a test sees only
/// what losing the fix changes: empty positions, `V` status fields, and the Signal K paths
/// that need a fix left out.
///
/// @return `fixture_state()` with `gnss.has_fix` false.
inline core::model::VesselState fixture_state_without_fix() {
    auto state = fixture_state();
    state.gnss.has_fix = false;
    return state;
}

/// Returns `fixture_state` with extreme values that produce the longest possible fields.
///
/// Positions within 0.00001 degrees of the poles and the antimeridian, the last
/// centisecond of 2099, four-digit speeds, negative values with the most digits, twelve
/// satellites, a differential fix, engine revolutions of 99999.9 and a waypoint name longer
/// than `model::kMaxWaypointNameLength` with NMEA reserved characters. The registry test
/// encodes every sentence from it to prove that each one still fits the 82-character limit,
/// and the AIS test to prove that the fields are clamped.
///
/// @return A new copy of the extreme state.
inline core::model::VesselState fixture_state_extreme() {
    using namespace std::chrono;
    auto state = fixture_state();
    state.time_utc = sys_days{2099y / December / 31d} + 23h + 59min + 59s + 990ms;
    state.navigation.position = {-89.99999, -179.99999};
    state.navigation.altitude_m = -9999.9;
    state.navigation.course_over_ground_deg = 359.9;
    state.navigation.speed_over_ground_kn = 999.9;
    state.navigation.heading_true_deg = 359.9;
    state.navigation.magnetic_variation_deg = -179.9;
    state.navigation.magnetic_deviation_deg = -179.9;
    state.navigation.speed_through_water_kn = 999.9;
    state.navigation.rate_of_turn_deg_per_min = -999.9;
    state.gnss.quality = core::model::FixQuality::Differential;
    state.gnss.satellites_in_use = 12;
    state.gnss.satellites_in_view = 12;
    state.gnss.hdop = 99.9;
    state.gnss.pdop = 99.9;
    state.gnss.vdop = 99.9;
    state.gnss.geoid_separation_m = -999.9;
    state.steering.rudder_angle_deg = -45.0;
    state.water.depth_below_transducer_m = 99999.9;
    state.water.transducer_offset_m = -99.9;
    state.water.temperature_c = -99.9;
    state.wind.true_direction_deg = 359.9;
    state.wind.true_speed_kn = 999.9;
    state.wind.apparent_angle_deg = 359.9;
    state.wind.apparent_speed_kn = 999.9;
    state.engines = {{"Engine one", true, 99999.9, -99.9}, {"Two", true, 99999.9, 999.9}};
    state.destination =
        core::model::Destination{"A very long waypoint name, with $reserved* characters!",
                                 {89.99999, 179.99999},
                                 {-89.99999, -179.99999},
                                 0.0};
    return state;
}

/// Strips the start delimiter and the checksum from a framed sentence.
///
/// Tests compare bodies so that the expected strings need no checksum.
///
/// @param sentence A sentence such as `$GPHDT,45.0,T*0C`, without line terminator.
/// @return The text between the first character and the last `*`, such as `GPHDT,45.0,T`;
///   everything after the first character when there is no `*`; empty when `sentence` is
///   empty, so that a test comparing the body of a missing sentence fails instead of throwing.
inline std::string body_of(std::string_view sentence) {
    if (sentence.empty()) {
        return {};
    }
    const auto star = sentence.rfind('*');
    return std::string{
        sentence.substr(1, star == std::string_view::npos ? sentence.npos : star - 1)};
}

}  // namespace nmeasim::test
