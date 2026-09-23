#include "core/fixtures.hpp"

#include <nmeasim/core/signalk/delta.hpp>
#include <nmeasim/core/version.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <string>

using Catch::Approx;
namespace signalk = nmeasim::core::signalk;

namespace {

std::map<std::string, std::string> as_map(const nmeasim::core::model::VesselState& state) {
    std::map<std::string, std::string> result;
    for (const auto& value : signalk::path_values(state)) {
        REQUIRE(result.insert({value.path, value.json_value}).second);
    }
    return result;
}

double number(const std::map<std::string, std::string>& values, const std::string& path) {
    REQUIRE(values.contains(path));
    return std::stod(values.at(path));
}

}  // namespace

TEST_CASE("JSON helpers escape strings and format numbers", "[signalk]") {
    CHECK(signalk::json_string("plain") == "\"plain\"");
    CHECK(signalk::json_string("say \"hi\"\\\n") == "\"say \\\"hi\\\"\\\\\\n\"");
    CHECK(signalk::json_string(std::string{"\x01"}) == "\"\\u0001\"");
    CHECK(signalk::json_number(37.9838) == "37.9838");
    CHECK(signalk::json_number(12.0) == "12");
    CHECK(signalk::json_number(0.0) == "0");
    CHECK(signalk::json_number(-0.0) == "0");
    CHECK(signalk::json_number(-0.00000001) == "0");
    CHECK(signalk::json_number(3.14159265358979) == "3.1415927");
    CHECK(signalk::json_number(std::nan("")) == "null");
    CHECK(signalk::json_number(1e300) != "null");
}

TEST_CASE("engine identifiers come from the labels", "[signalk]") {
    CHECK(signalk::engine_id("Port engine", 0) == "port");
    CHECK(signalk::engine_id("Starboard Engine", 1) == "starboard");
    CHECK(signalk::engine_id("Engine", 0) == "engine1");
    CHECK(signalk::engine_id("", 2) == "engine3");
    CHECK(signalk::engine_id("Main-Diesel #2", 0) == "maindiesel2");
}

TEST_CASE("the delta carries the standard paths in SI units", "[signalk]") {
    const auto state = nmeasim::test::fixture_state();
    const auto values = as_map(state);
    constexpr double pi = std::numbers::pi;
    CHECK(values.at("navigation.datetime") == "\"2026-09-22T12:34:56.780Z\"");
    CHECK(values.at("navigation.position") ==
          "{\"longitude\":23.7275,\"latitude\":37.9838,\"altitude\":12.3}");
    CHECK(number(values, "navigation.courseOverGroundTrue") == Approx(47.3 * pi / 180.0));
    CHECK(number(values, "navigation.courseOverGroundMagnetic") == Approx(42.7 * pi / 180.0));
    CHECK(number(values, "navigation.speedOverGround") == Approx(6.5 * 1852.0 / 3600.0));
    CHECK(number(values, "navigation.headingTrue") == Approx(45.0 * pi / 180.0));
    CHECK(number(values, "navigation.headingMagnetic") == Approx(40.4 * pi / 180.0));
    CHECK(number(values, "navigation.magneticVariation") == Approx(4.6 * pi / 180.0));
    CHECK(number(values, "navigation.speedThroughWater") == Approx(6.2 * 1852.0 / 3600.0));
    CHECK(number(values, "navigation.rateOfTurn") == Approx(-2.5 * pi / 180.0 / 60.0).margin(1e-7));
    CHECK(values.at("navigation.gnss.methodQuality") == "\"GNSS Fix\"");
    CHECK(values.at("navigation.gnss.satellites") == "8");
    CHECK(number(values, "navigation.gnss.horizontalDilution") == Approx(0.9));
    CHECK(number(values, "navigation.gnss.antennaAltitude") == Approx(12.3));
    CHECK(values.at("navigation.courseRhumbline.nextPoint.position") ==
          "{\"longitude\":23.4275,\"latitude\":37.7466}");
    CHECK(number(values, "navigation.courseRhumbline.nextPoint.bearingTrue") ==
          Approx(225.168 * pi / 180.0).epsilon(1e-4));
    CHECK(number(values, "navigation.courseRhumbline.nextPoint.distance") ==
          Approx(37282.7).margin(0.1));
    CHECK(number(values, "navigation.courseRhumbline.crossTrackError") ==
          Approx(-3004.5).margin(0.5));
    CHECK(number(values, "navigation.courseRhumbline.nextPoint.velocityMadeGood") < 0.0);
    CHECK(values.at("navigation.courseRhumbline.previousPoint.position") ==
          "{\"longitude\":23.7,\"latitude\":38}");
    CHECK(number(values, "environment.depth.belowTransducer") == Approx(12.4));
    CHECK(number(values, "environment.depth.surfaceToTransducer") == Approx(0.5));
    CHECK(number(values, "environment.depth.belowSurface") == Approx(12.9));
    CHECK_FALSE(values.contains("environment.depth.belowKeel"));
    CHECK(number(values, "environment.water.temperature") == Approx(294.65));
    CHECK(number(values, "environment.wind.speedTrue") == Approx(12.0 * 1852.0 / 3600.0));
    CHECK(number(values, "environment.wind.directionTrue") == Approx(270.0 * pi / 180.0));
    CHECK(number(values, "environment.wind.angleTrueWater") == Approx(-135.0 * pi / 180.0));
    CHECK(number(values, "environment.wind.angleApparent") == Approx(-60.0 * pi / 180.0));
    CHECK(number(values, "environment.wind.speedApparent") == Approx(14.2 * 1852.0 / 3600.0));
    CHECK(number(values, "steering.rudderAngle") == Approx(-3.5 * pi / 180.0));
    CHECK(number(values, "propulsion.port.revolutions") == Approx(30.0));
    CHECK(number(values, "propulsion.port.temperature") == Approx(355.15));
    CHECK(values.at("propulsion.port.state") == "\"started\"");
    CHECK(number(values, "propulsion.starboard.revolutions") == 0.0);
    CHECK(values.at("propulsion.starboard.state") == "\"stopped\"");
}

TEST_CASE("paths that need a fix or a destination disappear without them", "[signalk]") {
    auto state = nmeasim::test::fixture_state_without_fix();
    state.destination.reset();
    state.engines.clear();
    state.water.transducer_offset_m = -1.5;
    const auto values = as_map(state);
    CHECK_FALSE(values.contains("navigation.position"));
    CHECK_FALSE(values.contains("navigation.speedOverGround"));
    CHECK_FALSE(values.contains("navigation.gnss.horizontalDilution"));
    CHECK(values.at("navigation.gnss.methodQuality") == "\"no GPS\"");
    CHECK(values.at("navigation.gnss.satellites") == "0");
    CHECK(values.contains("navigation.headingTrue"));
    CHECK_FALSE(values.contains("navigation.courseRhumbline.nextPoint.distance"));
    CHECK(std::none_of(values.begin(), values.end(),
                       [](const auto& entry) { return entry.first.starts_with("propulsion."); }));
    CHECK(number(values, "environment.depth.transducerToKeel") == Approx(1.5));
    CHECK(number(values, "environment.depth.belowKeel") == Approx(10.9));
    auto differential = nmeasim::test::fixture_state();
    differential.gnss.quality = nmeasim::core::model::FixQuality::Differential;
    CHECK(as_map(differential).at("navigation.gnss.methodQuality") == "\"DGNSS fix\"");
}

TEST_CASE("the delta and hello messages are well-formed JSON documents", "[signalk]") {
    const auto state = nmeasim::test::fixture_state();
    signalk::SignalKOptions options;
    CHECK(signalk::default_context(state) == "vessels.urn:mrn:imo:mmsi:239000001");
    CHECK(signalk::effective_context(options, state) == "vessels.urn:mrn:imo:mmsi:239000001");
    options.context = "aircraft.urn:mrn:signalk:uuid:c0d79334-4e25-4245-8892-54e8ccc8021d";
    CHECK(signalk::effective_context(options, state) == options.context);

    const auto delta = signalk::encode_delta(state, options);
    CHECK(delta.starts_with(
        "{\"context\":\"aircraft.urn:mrn:signalk:uuid:c0d79334-4e25-4245-8892-54e8ccc8021d\","
        "\"updates\":[{\"source\":{\"label\":\"nmeasim\",\"type\":\"simulator\"},"
        "\"timestamp\":\"2026-09-22T12:34:56.780Z\",\"values\":[{\"path\":\"navigation.datetime\","
        "\"value\":\"2026-09-22T12:34:56.780Z\"},{\"path\":\"navigation.position\",\"value\":"
        "{\"longitude\":23.7275,\"latitude\":37.9838,\"altitude\":12.3}}"));
    CHECK(delta.ends_with("\"value\":\"stopped\"}]}]}"));
    CHECK(delta.find('\n') == std::string::npos);

    // A filter keeps the paths it admits, in order.
    const auto filtered = signalk::encode_delta(
        state, options, [](std::string_view path) { return path.starts_with("environment.wind"); });
    CHECK(filtered.find("navigation.") == std::string::npos);
    CHECK(filtered.find("\"path\":\"environment.wind.speedTrue\"") != std::string::npos);
    const auto nothing =
        signalk::encode_delta(state, options, [](std::string_view) { return false; });
    CHECK(nothing.ends_with("\"values\":[]}]}"));

    const auto hello = signalk::encode_hello(signalk::SignalKOptions{}, state, state.time_utc);
    CHECK(hello == "{\"name\":\"NMEASimulatorX\",\"version\":\"" +
                       std::string{nmeasim::core::kVersion} +
                       "\",\"self\":\"vessels.urn:mrn:imo:mmsi:239000001\",\"roles\":[\"master\","
                       "\"main\"],\"timestamp\":\"2026-09-22T12:34:56.780Z\"}");
}
