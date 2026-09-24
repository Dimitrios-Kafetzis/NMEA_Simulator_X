// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Golden tests of the NMEA 0183 sentence encoders of `nmeasim/core/nmea0183/encoders.hpp`.
///
/// Encodes the fixture states of `tests/core/fixtures.hpp` with each encoder (GNSS, heading
/// and speed, depth, wind, steering, autopilot and propulsion sentences) and compares the
/// sentence bodies with golden values; `docs/reference/nmea0183-sentences.md` shows the same
/// values for the full fixture as its examples. Also covers
/// nmeasim::core::nmea0183::mode_indicator() and nmeasim::core::nmea0183::sanitize_waypoint_name().
/// The AIS encoders are tested in `tests/core/ais/ais_test.cpp`. No fixture file is read.

#include "core/fixtures.hpp"

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/encoders.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace nmea = nmeasim::core::nmea0183;
using nmeasim::test::body_of;

namespace {

/// Runs an encoder that must produce exactly one sentence and returns that sentence's body.
///
/// Fails the running test case (`REQUIRE`) unless the encoder returns exactly one sentence,
/// and checks (`CHECK`) its checksum and the NMEA 0183 length limit.
///
/// @param encoder The encoder under test, such as `&nmea::encode_rmc`.
/// @param state The vessel state to encode, usually one of the fixture states.
/// @param talker The two-character talker identifier to encode with.
/// @return The sentence without its start delimiter and `*hh` checksum, as
///         nmeasim::test::body_of() returns it.
std::string single_body(nmea::Encoder encoder, const nmeasim::core::model::VesselState& state,
                        std::string_view talker) {
    const auto sentences = encoder(nmea::EncoderContext{state, talker});
    REQUIRE(sentences.size() == 1);
    CHECK(nmea::verify_checksum(sentences.front()));
    CHECK(nmea::fits_limit(sentences.front()));
    return body_of(sentences.front());
}

}  // namespace

TEST_CASE("GNSS sentences with a valid fix", "[nmea0183][encoders][gnss]") {
    const auto state = nmeasim::test::fixture_state();
    CHECK(single_body(&nmea::encode_rmc, state, "GP") ==
          "GPRMC,123456.78,A,3759.0280,N,02343.6500,E,6.5,47.3,220926,4.6,E,A");
    CHECK(single_body(&nmea::encode_gga, state, "GP") ==
          "GPGGA,123456.78,3759.0280,N,02343.6500,E,1,08,0.9,12.3,M,34.5,M,,");
    CHECK(single_body(&nmea::encode_gll, state, "GP") ==
          "GPGLL,3759.0280,N,02343.6500,E,123456.78,A,A");
    CHECK(single_body(&nmea::encode_gsa, state, "GP") ==
          "GPGSA,A,3,02,05,07,09,12,15,19,21,,,,,1.7,0.9,1.4");
    CHECK(single_body(&nmea::encode_vtg, state, "GP") == "GPVTG,47.3,T,42.7,M,6.5,N,12.0,K,A");
    CHECK(single_body(&nmea::encode_zda, state, "GP") == "GPZDA,123456.78,22,09,2026,00,00");
}

TEST_CASE("GSV lists satellites in view four per sentence", "[nmea0183][encoders][gnss]") {
    const auto state = nmeasim::test::fixture_state();
    const auto sentences = nmea::encode_gsv(nmea::EncoderContext{state, "GP"});
    REQUIRE(sentences.size() == 3);
    for (const auto& sentence : sentences) {
        CHECK(nmea::verify_checksum(sentence));
        CHECK(nmea::fits_limit(sentence));
    }
    CHECK(body_of(sentences[0]) ==
          "GPGSV,3,1,10,02,29,074,32,05,50,185,35,07,64,259,37,09,78,333,39");
    CHECK(body_of(sentences[1]).starts_with("GPGSV,3,2,10,12,"));
    CHECK(body_of(sentences[2]) == "GPGSV,3,3,10,24,43,168,39,25,50,205,40");
}

TEST_CASE("GNSS sentences without a fix omit position and mark data invalid",
          "[nmea0183][encoders][gnss]") {
    const auto state = nmeasim::test::fixture_state_without_fix();
    CHECK(single_body(&nmea::encode_rmc, state, "GP") == "GPRMC,123456.78,V,,,,,,,220926,4.6,E,N");
    CHECK(single_body(&nmea::encode_gga, state, "GP") == "GPGGA,123456.78,,,,,0,00,,,M,,M,,");
    CHECK(single_body(&nmea::encode_gll, state, "GP") == "GPGLL,,,,,123456.78,V,N");
    CHECK(single_body(&nmea::encode_gsa, state, "GP") == "GPGSA,A,1,,,,,,,,,,,,,,,");
    CHECK(single_body(&nmea::encode_vtg, state, "GP") == "GPVTG,,T,,M,,N,,K,N");
    const auto gsv = nmea::encode_gsv(nmea::EncoderContext{state, "GP"});
    REQUIRE(gsv.size() == 1);
    CHECK(body_of(gsv.front()) == "GPGSV,1,1,00");
}

TEST_CASE("differential fixes use the D mode indicator", "[nmea0183][encoders][gnss]") {
    auto state = nmeasim::test::fixture_state();
    state.gnss.quality = nmeasim::core::model::FixQuality::Differential;
    CHECK(nmea::mode_indicator(state.gnss) == 'D');
    CHECK(single_body(&nmea::encode_gga, state, "GP")
              .starts_with("GPGGA,123456.78,3759.0280,N,02343.6500,E,2,"));
    CHECK(single_body(&nmea::encode_rmc, state, "GP").ends_with(",D"));
}

TEST_CASE("heading and speed sentences", "[nmea0183][encoders][heading]") {
    const auto state = nmeasim::test::fixture_state();
    // Magnetic heading 40.4 = 45.0 true - 4.6 variation; 11.5 km/h = 6.2 kn * 1.852. VBW
    // resolves the ground speed (6.5 kn on 47.3 degrees) onto the bow at 45.0 degrees:
    // 6.5 cos 2.3 = 6.5 along and 6.5 sin 2.3 = 0.3 across.
    CHECK(single_body(&nmea::encode_hdg, state, "HC") == "HCHDG,40.4,0.0,E,4.6,E");
    CHECK(single_body(&nmea::encode_hdm, state, "HC") == "HCHDM,40.4,M");
    CHECK(single_body(&nmea::encode_hdt, state, "HE") == "HEHDT,45.0,T");
    CHECK(single_body(&nmea::encode_vhw, state, "VW") == "VWVHW,45.0,T,40.4,M,6.2,N,11.5,K");
    CHECK(single_body(&nmea::encode_vbw, state, "VW") == "VWVBW,6.2,0.0,A,6.5,0.3,A,0.0,A,0.0,A");
    CHECK(single_body(&nmea::encode_rot, state, "TI") == "TIROT,-2.5,A");
}

TEST_CASE("depth and water sentences", "[nmea0183][encoders][depth]") {
    const auto state = nmeasim::test::fixture_state();
    // 12.4 m is 40.7 feet (0.3048 m) and 6.8 fathoms (1.8288 m).
    CHECK(single_body(&nmea::encode_dpt, state, "SD") == "SDDPT,12.4,0.5,");
    CHECK(single_body(&nmea::encode_dbt, state, "SD") == "SDDBT,40.7,f,12.4,M,6.8,F");
    CHECK(single_body(&nmea::encode_mtw, state, "YC") == "YCMTW,21.5,C");
}

TEST_CASE("wind sentences", "[nmea0183][encoders][wind]") {
    const auto state = nmeasim::test::fixture_state();
    // True MWV is relative to the bow: 270.0 - 45.0 = 225.0. MWD adds the magnetic direction
    // 270.0 - 4.6 = 265.4 and the speed in metres per second, 12.0 kn = 6.2 m/s.
    CHECK(single_body(&nmea::encode_mwv_apparent, state, "WI") == "WIMWV,300.0,R,14.2,N,A");
    CHECK(single_body(&nmea::encode_mwv_true, state, "WI") == "WIMWV,225.0,T,12.0,N,A");
    CHECK(single_body(&nmea::encode_mwd, state, "WI") == "WIMWD,270.0,T,265.4,M,12.0,N,6.2,M");
}

TEST_CASE("rudder sentence", "[nmea0183][encoders][steering]") {
    const auto state = nmeasim::test::fixture_state();
    CHECK(single_body(&nmea::encode_rsa, state, "II") == "IIRSA,-3.5,A,,V");
}

TEST_CASE("autopilot sentences describe the leg to the destination",
          "[nmea0183][encoders][autopilot]") {
    const auto state = nmeasim::test::fixture_state();
    // The leg geometry is the one route_test.cpp checks: the vessel is 3004.5 m (1.62 nm)
    // left of the leg, so the direction to steer is R; the leg bears 220.5 and the
    // destination 225.2 degrees at 37282.7 m (20.1 nm). Heading away from it on 47.3 degrees,
    // the vessel closes at 6.5 cos(225.2 - 47.3) = -6.5 kn.
    CHECK(single_body(&nmea::encode_apb, state, "GP") ==
          "GPAPB,A,A,1.62,R,N,V,V,220.5,T,AEGINA,225.2,T,225.2,T,A");
    CHECK(single_body(&nmea::encode_rmb, state, "GP") ==
          "GPRMB,A,1.62,R,,AEGINA,3744.7960,N,02325.6500,E,20.1,225.2,-6.5,V,A");
    CHECK(single_body(&nmea::encode_xte, state, "GP") == "GPXTE,A,A,1.62,R,N,A");

    // Arrival and perpendicular flags, and the side to steer, follow the geometry.
    auto arriving = state;
    arriving.navigation.position = {37.7470, 23.4280};
    CHECK(single_body(&nmea::encode_apb, arriving, "GP").starts_with("GPAPB,A,A,0.0"));
    CHECK(single_body(&nmea::encode_apb, arriving, "GP").find(",A,V,220.5,T,") !=
          std::string::npos);
    CHECK(single_body(&nmea::encode_rmb, arriving, "GP").ends_with(",A,A"));
    auto right_of_leg = state;
    right_of_leg.navigation.position = {37.85, 23.50};
    CHECK(single_body(&nmea::encode_xte, right_of_leg, "GP").find(",L,N,") != std::string::npos);

    // Without a fix the mode indicator changes; without a destination nothing is sent.
    CHECK(single_body(&nmea::encode_xte, nmeasim::test::fixture_state_without_fix(), "GP")
              .ends_with(",N"));
    auto none = state;
    none.destination.reset();
    CHECK(nmea::encode_apb(nmea::EncoderContext{none, "GP"}).empty());
    CHECK(nmea::encode_rmb(nmea::EncoderContext{none, "GP"}).empty());
    CHECK(nmea::encode_xte(nmea::EncoderContext{none, "GP"}).empty());
}

TEST_CASE("waypoint names are restricted to what a field may carry", "[nmea0183][encoders]") {
    CHECK(nmea::sanitize_waypoint_name("AEGINA") == "AEGINA");
    CHECK(nmea::sanitize_waypoint_name("Piraeus East, ferry $dock*!") == "PiraeusEastferry");
    CHECK(nmea::sanitize_waypoint_name("a\\b^c~d") == "abcd");
    CHECK(nmea::sanitize_waypoint_name("") == "WPT");
    CHECK(nmea::sanitize_waypoint_name(" , ") == "WPT");
}

TEST_CASE("propulsion sentences list every engine", "[nmea0183][encoders][propulsion]") {
    const auto state = nmeasim::test::fixture_state();
    const auto rpm = nmea::encode_rpm(nmea::EncoderContext{state, "ER"});
    REQUIRE(rpm.size() == 2);
    CHECK(body_of(rpm[0]) == "ERRPM,E,1,1800.0,,A");
    CHECK(body_of(rpm[1]) == "ERRPM,E,2,0.0,,A");
    const auto xdr = nmea::encode_xdr(nmea::EncoderContext{state, "ER"});
    REQUIRE(xdr.size() == 2);
    CHECK(body_of(xdr[0]) == "ERXDR,C,82.0,C,ENGINE#0,T,1800.0,R,ENGINE#0");
    CHECK(body_of(xdr[1]) == "ERXDR,C,65.5,C,ENGINE#1,T,0.0,R,ENGINE#1");
    for (const auto& sentence : rpm) {
        CHECK(nmea::verify_checksum(sentence));
    }
    for (const auto& sentence : xdr) {
        CHECK(nmea::verify_checksum(sentence));
    }

    auto none = state;
    none.engines.clear();
    CHECK(nmea::encode_rpm(nmea::EncoderContext{none, "ER"}).empty());
    CHECK(nmea::encode_xdr(nmea::EncoderContext{none, "ER"}).empty());
}

TEST_CASE("the talker identifier is taken from the context", "[nmea0183][encoders]") {
    const auto state = nmeasim::test::fixture_state();
    CHECK(single_body(&nmea::encode_rmc, state, "GN").starts_with("GNRMC,"));
    CHECK(single_body(&nmea::encode_hdt, state, "IN").starts_with("INHDT,"));
}
