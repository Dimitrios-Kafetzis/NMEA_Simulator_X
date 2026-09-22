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

/// Encodes with the fixture and checks framing, then returns the single sentence body.
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
    CHECK(single_body(&nmea::encode_hdg, state, "HC") == "HCHDG,40.4,0.0,E,4.6,E");
    CHECK(single_body(&nmea::encode_hdm, state, "HC") == "HCHDM,40.4,M");
    CHECK(single_body(&nmea::encode_hdt, state, "HE") == "HEHDT,45.0,T");
    CHECK(single_body(&nmea::encode_vhw, state, "VW") == "VWVHW,45.0,T,40.4,M,6.2,N,11.5,K");
    CHECK(single_body(&nmea::encode_vbw, state, "VW") == "VWVBW,6.2,0.0,A,6.5,0.3,A,0.0,A,0.0,A");
    CHECK(single_body(&nmea::encode_rot, state, "TI") == "TIROT,-2.5,A");
}

TEST_CASE("depth and water sentences", "[nmea0183][encoders][depth]") {
    const auto state = nmeasim::test::fixture_state();
    CHECK(single_body(&nmea::encode_dpt, state, "SD") == "SDDPT,12.4,0.5,");
    CHECK(single_body(&nmea::encode_dbt, state, "SD") == "SDDBT,40.7,f,12.4,M,6.8,F");
    CHECK(single_body(&nmea::encode_mtw, state, "YC") == "YCMTW,21.5,C");
}

TEST_CASE("wind sentences", "[nmea0183][encoders][wind]") {
    const auto state = nmeasim::test::fixture_state();
    CHECK(single_body(&nmea::encode_mwv_apparent, state, "WI") == "WIMWV,300.0,R,14.2,N,A");
    CHECK(single_body(&nmea::encode_mwv_true, state, "WI") == "WIMWV,225.0,T,12.0,N,A");
    CHECK(single_body(&nmea::encode_mwd, state, "WI") == "WIMWD,270.0,T,265.4,M,12.0,N,6.2,M");
}

TEST_CASE("rudder sentence", "[nmea0183][encoders][steering]") {
    const auto state = nmeasim::test::fixture_state();
    CHECK(single_body(&nmea::encode_rsa, state, "II") == "IIRSA,-3.5,A,,V");
}

TEST_CASE("the talker identifier is taken from the context", "[nmea0183][encoders]") {
    const auto state = nmeasim::test::fixture_state();
    CHECK(single_body(&nmea::encode_rmc, state, "GN").starts_with("GNRMC,"));
    CHECK(single_body(&nmea::encode_hdt, state, "IN").starts_with("INHDT,"));
}
