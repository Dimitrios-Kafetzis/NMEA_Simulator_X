#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace nmeasim::test {

/// Absolute path of a file under `tests/fixtures/`.
inline std::string fixture_path(std::string_view relative) {
    return std::string{NMEASIM_FIXTURES_DIR} + "/" + std::string{relative};
}

/// Contents of a file under `tests/fixtures/`; empty when it does not exist.
inline std::string read_fixture(std::string_view relative) {
    std::ifstream file(fixture_path(relative), std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// A fully populated vessel state near Athens, used as the golden-file fixture.
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

/// The fixture with the GNSS receiver reporting no fix.
inline core::model::VesselState fixture_state_without_fix() {
    auto state = fixture_state();
    state.gnss.has_fix = false;
    return state;
}

/// Extreme values that produce the longest possible fields.
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
inline std::string body_of(std::string_view sentence) {
    const auto star = sentence.rfind('*');
    return std::string{
        sentence.substr(1, star == std::string_view::npos ? sentence.npos : star - 1)};
}

}  // namespace nmeasim::test
