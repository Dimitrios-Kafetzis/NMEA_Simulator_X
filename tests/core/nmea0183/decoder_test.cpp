// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the NMEA 0183 decoder of `nmeasim/core/nmea0183/decoder.hpp`.
///
/// Covers nmeasim::core::nmea0183::parse_sentence(), the field parsers parse_coordinate(),
/// parse_time_of_day(), parse_date() and parse_number_field(), sentence_time(), and
/// apply_sentence(): every sentence of the registry, encoded from the fixture states of
/// `tests/core/fixtures.hpp`, must decode back to the state it came from, and individual
/// sentences must update only the values they carry. No fixture file is read.

#include "core/fixtures.hpp"

#include <nmeasim/core/nmea0183/decoder.hpp>
#include <nmeasim/core/nmea0183/registry.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using Catch::Approx;
using namespace std::chrono_literals;
namespace nmea = nmeasim::core::nmea0183;

TEST_CASE("sentences are split into talker, formatter and fields", "[nmea0183][decoder]") {
    const auto rmc = nmea::parse_sentence(
        "$GPRMC,123456.78,A,3759.0280,N,02343.6500,E,6.5,47.3,220926,4.6,E,A*06\r\n");
    REQUIRE(rmc.has_value());
    CHECK(rmc->delimiter == '$');
    CHECK(rmc->talker == "GP");
    CHECK(rmc->formatter == "RMC");
    CHECK(rmc->has_checksum);
    REQUIRE(rmc->fields.size() == 12);
    CHECK(rmc->field(0) == "123456.78");
    CHECK(rmc->field(11) == "A");
    CHECK(rmc->field(12).empty());

    // A missing checksum is accepted, a wrong one is not.
    const auto bare = nmea::parse_sentence("  $HEHDT,45.0,T  ");
    REQUIRE(bare.has_value());
    CHECK_FALSE(bare->has_checksum);
    CHECK(bare->fields == std::vector<std::string>{"45.0", "T"});
    CHECK_FALSE(nmea::parse_sentence("$HEHDT,45.0,T*00").has_value());

    const auto proprietary = nmea::parse_sentence("$PGRME,15.0,M,45.0,M,25.0,M*1C");
    REQUIRE(proprietary.has_value());
    CHECK(proprietary->talker == "P");
    CHECK(proprietary->formatter == "GRME");

    const auto ais = nmea::parse_sentence("!AIVDM,1,1,,A,13aEOK?P00PD2wVMdLDRhgvL289?,0*26");
    REQUIRE(ais.has_value());
    CHECK(ais->delimiter == '!');
    CHECK(ais->talker == "AI");
    CHECK(ais->formatter == "VDM");

    const auto empty_fields = nmea::parse_sentence("$SDDPT,,,");
    REQUIRE(empty_fields.has_value());
    CHECK(empty_fields->fields.size() == 3);
    CHECK(empty_fields->field(0).empty());

    CHECK_FALSE(nmea::parse_sentence("").has_value());
    CHECK_FALSE(nmea::parse_sentence("GPRMC,1,2").has_value());
    CHECK_FALSE(nmea::parse_sentence("$").has_value());
    CHECK_FALSE(nmea::parse_sentence("$GP").has_value());
    CHECK_FALSE(nmea::parse_sentence("$P,1").has_value());
}

TEST_CASE("field helpers parse coordinates, times, dates and numbers", "[nmea0183][decoder]") {
    CHECK(nmea::parse_coordinate("3759.0280", "N") == Approx(37.9838).epsilon(1e-6));
    CHECK(nmea::parse_coordinate("02343.6500", "E") == Approx(23.7275).epsilon(1e-6));
    CHECK(nmea::parse_coordinate("3859.982", "S") == Approx(-38.9997).epsilon(1e-6));
    CHECK(nmea::parse_coordinate("15130.006", "W") == Approx(-151.5001).epsilon(1e-6));
    CHECK_FALSE(nmea::parse_coordinate("", "N").has_value());
    CHECK_FALSE(nmea::parse_coordinate("3759.0280", "").has_value());
    CHECK_FALSE(nmea::parse_coordinate("3759.0280", "X").has_value());
    // Sixty minutes, 91 degrees north and 181 degrees east are out of range.
    CHECK_FALSE(nmea::parse_coordinate("3760.0000", "N").has_value());
    CHECK_FALSE(nmea::parse_coordinate("9100.0000", "N").has_value());
    CHECK_FALSE(nmea::parse_coordinate("18100.0000", "E").has_value());

    CHECK(nmea::parse_time_of_day("123456.78") == 12h + 34min + 56s + 780ms);
    CHECK(nmea::parse_time_of_day("000000") == 0ms);
    CHECK(nmea::parse_time_of_day("235959.999") == 23h + 59min + 59s + 999ms);
    CHECK(nmea::parse_time_of_day("120000.5") == 12h + 500ms);
    CHECK_FALSE(nmea::parse_time_of_day("").has_value());
    CHECK_FALSE(nmea::parse_time_of_day("1234").has_value());
    CHECK_FALSE(nmea::parse_time_of_day("250000").has_value());
    CHECK_FALSE(nmea::parse_time_of_day("12345x").has_value());
    CHECK_FALSE(nmea::parse_time_of_day("123456.x").has_value());

    const auto date = nmea::parse_date("220926");
    REQUIRE(date.has_value());
    CHECK(date->year == 2026);
    CHECK(date->month == 9);
    CHECK(date->day == 22);
    CHECK(nmea::parse_date("311299")->year == 1999);  // two-digit years from 80 are 19xx
    CHECK_FALSE(nmea::parse_date("").has_value());
    CHECK_FALSE(nmea::parse_date("321226").has_value());
    CHECK_FALSE(nmea::parse_date("011326").has_value());

    CHECK(nmea::parse_number_field("6.5") == Approx(6.5));
    CHECK(nmea::parse_number_field("-12.25") == Approx(-12.25));
    CHECK(nmea::parse_number_field("+3") == Approx(3.0));
    CHECK(nmea::parse_number_field("08") == Approx(8.0));
    CHECK(nmea::parse_number_field(".5") == Approx(0.5));
    CHECK_FALSE(nmea::parse_number_field("").has_value());
    CHECK_FALSE(nmea::parse_number_field("abc").has_value());
    CHECK_FALSE(nmea::parse_number_field("1,5").has_value());
    CHECK_FALSE(nmea::parse_number_field("-").has_value());
}

TEST_CASE("sentence times are read from the sentences that carry one", "[nmea0183][decoder]") {
    const auto rmc = nmea::parse_sentence(
        "$GPRMC,123456.78,A,3759.0280,N,02343.6500,E,6.5,47.3,220926,4.6,E,A*06");
    const auto time = nmea::sentence_time(*rmc);
    REQUIRE(time.has_value());
    CHECK(time->since_midnight == 12h + 34min + 56s + 780ms);
    REQUIRE(time->date.has_value());
    CHECK(time->date->day == 22);

    const auto gga =
        nmea::parse_sentence("$GPGGA,100000.10,3759.0281,N,02343.6502,E,1,08,0.9,0.0,M,0.0,M,,*59");
    const auto gga_time = nmea::sentence_time(*gga);
    REQUIRE(gga_time.has_value());
    CHECK(gga_time->since_midnight == 10h + 100ms);
    CHECK_FALSE(gga_time->date.has_value());

    const auto gll = nmea::parse_sentence("$GPGLL,3759.0280,N,02343.6500,E,123456.78,A,A");
    CHECK(nmea::sentence_time(*gll)->since_midnight == 12h + 34min + 56s + 780ms);

    const auto zda = nmea::parse_sentence("$GPZDA,123456.78,22,09,2026,00,00");
    const auto zda_time = nmea::sentence_time(*zda);
    REQUIRE(zda_time.has_value());
    REQUIRE(zda_time->date.has_value());
    CHECK(zda_time->date->year == 2026);
    CHECK(zda_time->date->month == 9);

    CHECK_FALSE(nmea::sentence_time(*nmea::parse_sentence("$HEHDT,45.0,T")).has_value());
    CHECK_FALSE(nmea::sentence_time(*nmea::parse_sentence("$GPRMC,,V,,,,,,,,,,N")).has_value());
}

TEST_CASE("a time of day that wraps past midnight advances the date", "[nmea0183][decoder]") {
    using std::chrono::sys_days;
    using std::chrono::year;
    nmeasim::core::model::VesselState state;
    CHECK(nmea::apply_sentence("$GPZDA,235959.50,31,12,2026,00,00", state));
    CHECK(state.time_utc == sys_days{year{2026} / 12 / 31} + 23h + 59min + 59s + 500ms);
    // GGA and GLL just after midnight, before the next date: more than 12 hours back on the
    // same date is the next day.
    CHECK(nmea::apply_sentence("$GPGGA,000000.50,,,,,0,00,,,M,,M,,", state));
    CHECK(state.time_utc == sys_days{year{2027} / 1 / 1} + 500ms);
    CHECK(nmea::apply_sentence("$GPGLL,,,,,000001.00,V,N", state));
    CHECK(state.time_utc == sys_days{year{2027} / 1 / 1} + 1s);
    // A smaller step back is taken as sent, on the same date.
    CHECK(nmea::apply_sentence("$GPGGA,000000.00,,,,,0,00,,,M,,M,,", state));
    CHECK(state.time_utc == sys_days{year{2027} / 1 / 1});
    state.time_utc = sys_days{year{2027} / 1 / 1} + 13h;
    CHECK(nmea::apply_sentence("$GPGGA,010000.00,,,,,0,00,,,M,,M,,", state));
    CHECK(state.time_utc == sys_days{year{2027} / 1 / 1} + 1h);
    // An RMC whose date does not exist counts as time-only and advances the date too.
    state.time_utc = sys_days{year{2027} / 1 / 1} + 23h;
    CHECK(nmea::apply_sentence("$GPRMC,000010.00,V,,,,,,,320127,,,N", state));
    CHECK(state.time_utc == sys_days{year{2027} / 1 / 2} + 10s);
    // A sentence with a valid date is always taken as sent, even far back.
    CHECK(nmea::apply_sentence("$GPZDA,120000.00,01,01,2027,00,00", state));
    CHECK(state.time_utc == sys_days{year{2027} / 1 / 1} + 12h);
}

TEST_CASE("every encoded sentence decodes back to the state it came from", "[nmea0183][decoder]") {
    const auto original = nmeasim::test::fixture_state();
    nmeasim::core::model::VesselState decoded;
    // A date far from the fixture's, so that the time check below proves the date decoded.
    decoded.time_utc = std::chrono::sys_days{std::chrono::year{2000} / 1 / 1};
    const auto& registry = nmea::SentenceRegistry::standard();
    int applied = 0;
    for (const auto& descriptor : registry.descriptors()) {
        if (descriptor.group == nmea::SentenceGroup::Ais) {
            continue;  // encapsulated sentences are not decoded
        }
        for (const auto& sentence :
             nmea::encode_within_limit(descriptor, original, descriptor.default_talker, {})) {
            INFO(sentence);
            CHECK(nmea::apply_sentence(sentence, decoded));
            ++applied;
        }
    }
    CHECK(applied >= 27);
    CHECK(decoded.time_utc == original.time_utc);
    CHECK(decoded.navigation.position.latitude_deg ==
          Approx(original.navigation.position.latitude_deg).margin(2e-6));
    CHECK(decoded.navigation.position.longitude_deg ==
          Approx(original.navigation.position.longitude_deg).margin(2e-6));
    CHECK(decoded.navigation.speed_over_ground_kn == Approx(6.5));
    CHECK(decoded.navigation.course_over_ground_deg == Approx(47.3));
    CHECK(decoded.navigation.heading_true_deg == Approx(45.0));
    CHECK(decoded.navigation.magnetic_variation_deg == Approx(4.6));
    CHECK(decoded.navigation.speed_through_water_kn == Approx(6.2));
    CHECK(decoded.navigation.rate_of_turn_deg_per_min == Approx(-2.5));
    CHECK(decoded.navigation.altitude_m == Approx(12.3));
    CHECK(decoded.gnss.has_fix);
    CHECK(decoded.gnss.quality == nmeasim::core::model::FixQuality::Gps);
    CHECK(decoded.gnss.satellites_in_use == 8);
    CHECK(decoded.gnss.satellites_in_view == 10);
    CHECK(decoded.gnss.hdop == Approx(0.9));
    CHECK(decoded.gnss.pdop == Approx(1.7));
    CHECK(decoded.gnss.vdop == Approx(1.4));
    CHECK(decoded.gnss.geoid_separation_m == Approx(34.5));
    CHECK(decoded.steering.rudder_angle_deg == Approx(-3.5));
    CHECK(decoded.water.depth_below_transducer_m == Approx(12.4));
    CHECK(decoded.water.transducer_offset_m == Approx(0.5));
    CHECK(decoded.water.temperature_c == Approx(21.5));
    CHECK(decoded.wind.true_direction_deg == Approx(270.0));
    CHECK(decoded.wind.true_speed_kn == Approx(12.0));
    CHECK(decoded.wind.apparent_angle_deg == Approx(300.0));
    CHECK(decoded.wind.apparent_speed_kn == Approx(14.2));
    REQUIRE(decoded.engines.size() == 2);
    CHECK(decoded.engines[0].running);
    CHECK(decoded.engines[0].revolutions_rpm == Approx(1800.0));
    CHECK(decoded.engines[0].coolant_temperature_c == Approx(82.0));
    CHECK_FALSE(decoded.engines[1].running);
    CHECK(decoded.engines[1].coolant_temperature_c == Approx(65.5));
    REQUIRE(decoded.destination.has_value());
    CHECK(decoded.destination->name == "AEGINA");
    CHECK(decoded.destination->position.latitude_deg == Approx(37.7466).margin(2e-6));
    CHECK(decoded.destination->position.longitude_deg == Approx(23.4275).margin(2e-6));
}

TEST_CASE("autopilot and propulsion sentences update the destination and the engines",
          "[nmea0183][decoder]") {
    nmeasim::core::model::VesselState state;
    state.navigation.position = {38.0, 23.7};
    // A new destination starts its leg at the vessel; the same name keeps the leg.
    CHECK(nmea::apply_sentence(
        "$GPRMB,A,0.10,L,,AEGINA,3744.7960,N,02325.6500,E,20.1,225.2,6.5,V,A", state));
    REQUIRE(state.destination.has_value());
    CHECK(state.destination->name == "AEGINA");
    CHECK(state.destination->origin.latitude_deg == Approx(38.0));
    state.navigation.position = {37.9, 23.6};
    CHECK(nmea::apply_sentence(
        "$GPRMB,A,0.10,L,,AEGINA,3744.7960,N,02325.6500,E,20.1,225.2,6.5,V,A", state));
    CHECK(state.destination->origin.latitude_deg == Approx(38.0));
    CHECK(nmea::apply_sentence("$GPRMB,A,0.10,L,,,3744.7960,N,02325.6500,E,20.1,225.2,6.5,V,A",
                               state));
    CHECK(state.destination->name == "WPT");
    CHECK(state.destination->origin.latitude_deg == Approx(37.9));
    // An invalid RMB or one without a position changes nothing.
    CHECK(nmea::apply_sentence("$GPRMB,V,,,,,,,,,,,,V,N", state));
    CHECK(state.destination->name == "WPT");
    CHECK(nmea::apply_sentence("$GPRMB,A,0.10,L,,NOWHERE,,,,,20.1,225.2,6.5,V,A", state));
    CHECK(state.destination->name == "WPT");

    // RPM numbers engines from 1 and XDR from 0; a sentence for a missing engine adds it.
    CHECK(nmea::apply_sentence("$ERRPM,E,2,1500.0,,A", state));
    REQUIRE(state.engines.size() == 2);
    CHECK(state.engines[0].label == "Engine 1");
    CHECK_FALSE(state.engines[0].running);
    CHECK(state.engines[1].running);
    CHECK(state.engines[1].revolutions_rpm == Approx(1500.0));
    CHECK(nmea::apply_sentence("$ERRPM,S,1,900.0,,A", state));  // shafts are not engines
    CHECK_FALSE(state.engines[0].running);
    CHECK(nmea::apply_sentence("$ERRPM,E,1,900.0,,V", state));  // invalid status
    CHECK_FALSE(state.engines[0].running);
    CHECK(nmea::apply_sentence("$ERRPM,E,0,900.0,,A", state));  // no engine 0 in RPM
    CHECK(state.engines.size() == 2);
    CHECK(nmea::apply_sentence("$ERXDR,C,79.5,C,ENGINE#0,T,0.0,R,ENGINE#1,P,1.0,B,BARO", state));
    CHECK(state.engines[0].coolant_temperature_c == Approx(79.5));
    CHECK_FALSE(state.engines[1].running);
    // A malformed engine number and a temperature in Fahrenheit are both ignored.
    CHECK(nmea::apply_sentence("$ERXDR,C,30.0,C,ENGINE#x,C,31.0,F,ENGINE#0", state));
    CHECK(state.engines[0].coolant_temperature_c == Approx(79.5));
    CHECK(state.engines.size() == 2);
}

TEST_CASE("a receiver without a fix decodes as such", "[nmea0183][decoder]") {
    const auto original = nmeasim::test::fixture_state_without_fix();
    auto decoded = nmeasim::test::fixture_state();
    const auto& registry = nmea::SentenceRegistry::standard();
    for (const auto& id : {"RMC", "GGA", "GLL", "GSA", "VTG"}) {
        const auto* descriptor = registry.find(id);
        REQUIRE(descriptor != nullptr);
        for (const auto& sentence :
             nmea::encode_within_limit(*descriptor, original, descriptor->default_talker, {})) {
            CHECK(nmea::apply_sentence(sentence, decoded));
        }
    }
    CHECK_FALSE(decoded.gnss.has_fix);
    CHECK(decoded.gnss.quality == nmeasim::core::model::FixQuality::Invalid);
    // The position is left as it was rather than zeroed.
    CHECK(decoded.navigation.position.latitude_deg == Approx(37.9838));
}

TEST_CASE("individual sentences update only what they carry", "[nmea0183][decoder]") {
    nmeasim::core::model::VesselState state;
    state.navigation.magnetic_variation_deg = 4.0;
    // The true heading adds variation and deviation to the magnetic one: 41.0 + 4.0 = 45.0,
    // then 40.0 - 1.0 (west) + 5.0 (east) = 44.0.
    CHECK(nmea::apply_sentence("$HCHDM,41.0,M", state));
    CHECK(state.navigation.heading_true_deg == Approx(45.0));
    CHECK(nmea::apply_sentence("$HCHDG,40.0,1.0,W,5.0,E", state));
    CHECK(state.navigation.heading_true_deg == Approx(44.0));
    CHECK(state.navigation.magnetic_deviation_deg == Approx(-1.0));
    CHECK(state.navigation.magnetic_variation_deg == Approx(5.0));

    // Wind speed units are converted; true MWV is relative to the heading, so 90.0 on 44.0
    // comes from 134.0. 10 m/s = 19.438 kn, 18.52 km/h = 10 kn and 5 m/s = 9.719 kn.
    CHECK(nmea::apply_sentence("$WIMWV,90.0,T,10.0,M,A", state));
    CHECK(state.wind.true_direction_deg == Approx(134.0));
    CHECK(state.wind.true_speed_kn == Approx(19.438).epsilon(1e-3));
    CHECK(nmea::apply_sentence("$WIMWV,30.0,R,18.52,K,A", state));
    CHECK(state.wind.apparent_angle_deg == Approx(30.0));
    CHECK(state.wind.apparent_speed_kn == Approx(10.0).epsilon(1e-3));
    // An invalid MWV is ignored.
    CHECK(nmea::apply_sentence("$WIMWV,99.0,R,99.0,N,V", state));
    CHECK(state.wind.apparent_angle_deg == Approx(30.0));
    CHECK(nmea::apply_sentence("$WIMWD,,T,,M,,N,5.0,M", state));
    CHECK(state.wind.true_speed_kn == Approx(9.719).epsilon(1e-3));

    // Without the metres field the depth comes from the feet: 32.8 ft = 9.997 m.
    CHECK(nmea::apply_sentence("$SDDBT,32.8,f,,M,5.5,F", state));
    CHECK(state.water.depth_below_transducer_m == Approx(10.0).epsilon(1e-3));
    // VBW sets the water speed; the ground speed comes from RMC and VTG.
    CHECK(nmea::apply_sentence("$VWVBW,5.0,0.0,A,3.0,4.0,A,,V,,V", state));
    CHECK(state.navigation.speed_through_water_kn == Approx(5.0));
    CHECK(state.navigation.speed_over_ground_kn == Approx(0.0));
    // A water speed with status V is ignored.
    CHECK(nmea::apply_sentence("$VWVBW,6.0,0.0,V,3.0,4.0,A,,V,,V", state));
    CHECK(state.navigation.speed_through_water_kn == Approx(5.0));
    CHECK(nmea::apply_sentence("$IIRSA,7.5,A,,V", state));
    CHECK(state.steering.rudder_angle_deg == Approx(7.5));
    CHECK(nmea::apply_sentence("$IIRSA,9.5,V,,V", state));
    CHECK(state.steering.rudder_angle_deg == Approx(7.5));
    CHECK(nmea::apply_sentence("$GPGSV,3,1,11,02,45,120,40", state));
    CHECK(state.gnss.satellites_in_view == 11);
    // Satellites in use are counted from the non-empty PRN fields.
    CHECK(nmea::apply_sentence("$GPGSA,A,3,02,05,07,,,,,,,,,,2.0,1.1,1.6", state));
    CHECK(state.gnss.satellites_in_use == 3);
    CHECK(state.gnss.pdop == Approx(2.0));

    // A time of day alone keeps the current date; a full date replaces it.
    state.time_utc = std::chrono::sys_days{std::chrono::year{2026} / 9 / 23} + 8h;
    CHECK(nmea::apply_sentence("$GPGGA,101500.00,,,,,0,00,,,M,,M,,", state));
    CHECK(state.time_utc == std::chrono::sys_days{std::chrono::year{2026} / 9 / 23} + 10h + 15min);
    CHECK_FALSE(state.gnss.has_fix);
    CHECK(nmea::apply_sentence("$GPZDA,120000.00,01,01,2027,00,00", state));
    CHECK(state.time_utc == std::chrono::sys_days{std::chrono::year{2027} / 1 / 1} + 12h);
    CHECK(nmea::apply_sentence("$GPGLL,3759.0280,N,02343.6500,E,123456.78,A,A", state));
    CHECK(state.gnss.has_fix);
    CHECK(state.navigation.position.longitude_deg == Approx(23.7275).epsilon(1e-6));

    // Unknown formatters, encapsulated sentences and malformed lines change nothing.
    CHECK_FALSE(nmea::apply_sentence("$GPXYZ,1,2,3", state));
    CHECK_FALSE(nmea::apply_sentence("!AIVDM,1,1,,A,13aEOK?P00PD2wVMdLDRhgvL289?,0*26", state));
    CHECK_FALSE(nmea::apply_sentence("not a sentence", state));
    CHECK(state.gnss.has_fix);
}
