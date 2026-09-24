// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the NMEA 0183 field formatters of `nmeasim/core/nmea0183/fields.hpp`.
///
/// Covers nmeasim::core::nmea0183::format_fixed(), format_padded(), format_latitude(),
/// format_longitude(), format_time(), format_date(), date_parts() and east_west(), including
/// negative zero, leading zeros, rounding up to a whole degree and the last millisecond of a
/// day. No fixture file is read.

#include <nmeasim/core/nmea0183/fields.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace nmea = nmeasim::core::nmea0183;

TEST_CASE("format_fixed renders fixed decimals without negative zero", "[nmea0183][fields]") {
    CHECK(nmea::format_fixed(6.5, 1) == "6.5");
    CHECK(nmea::format_fixed(12.038, 1) == "12.0");
    CHECK(nmea::format_fixed(-2.5, 1) == "-2.5");
    CHECK(nmea::format_fixed(0.0, 1) == "0.0");
    CHECK(nmea::format_fixed(-0.04, 1) == "0.0");
    CHECK(nmea::format_fixed(-0.0, 2) == "0.00");
    CHECK(nmea::format_fixed(3.0, 0) == "3");
    CHECK(nmea::format_fixed(-0.4, 0) == "0");
}

TEST_CASE("format_padded zero-pads integers", "[nmea0183][fields]") {
    CHECK(nmea::format_padded(8, 2) == "08");
    CHECK(nmea::format_padded(74, 3) == "074");
    CHECK(nmea::format_padded(2026, 4) == "2026");
    CHECK(nmea::format_padded(5, 0) == "5");
}

TEST_CASE("coordinates are rendered as degrees and decimal minutes", "[nmea0183][fields]") {
    SECTION("northern and eastern hemispheres") {
        const auto lat = nmea::format_latitude(37.9838, 4);
        CHECK(lat.value == "3759.0280");
        CHECK(lat.hemisphere == 'N');
        const auto lon = nmea::format_longitude(23.7275, 4);
        CHECK(lon.value == "02343.6500");
        CHECK(lon.hemisphere == 'E');
    }
    SECTION("southern and western hemispheres") {
        const auto lat = nmea::format_latitude(-38.9997, 4);
        CHECK(lat.value == "3859.9820");
        CHECK(lat.hemisphere == 'S');
        const auto lon = nmea::format_longitude(-151.5001, 4);
        CHECK(lon.value == "15130.0060");
        CHECK(lon.hemisphere == 'W');
    }
    SECTION("values below one degree keep their leading zeros") {
        CHECK(nmea::format_latitude(0.5, 4).value == "0030.0000");
        CHECK(nmea::format_longitude(0.001, 4).value == "00000.0600");
    }
    SECTION("rounding never produces sixty minutes") {
        CHECK(nmea::format_latitude(37.99999999, 4).value == "3800.0000");
        CHECK(nmea::format_longitude(179.9999999, 2).value == "18000.00");
    }
    SECTION("precision is configurable") {
        CHECK(nmea::format_latitude(37.9838, 2).value == "3759.03");
        CHECK(nmea::format_latitude(37.9838, 6).value == "3759.028000");
    }
}

TEST_CASE("time and date fields use UTC", "[nmea0183][fields]") {
    using namespace std::chrono;
    const auto t = sys_days{2026y / September / 22d} + 12h + 34min + 56s + 780ms;
    CHECK(nmea::format_time(t) == "123456.78");
    CHECK(nmea::format_date(t) == "220926");
    const auto parts = nmea::date_parts(t);
    CHECK(parts.year == 2026);
    CHECK(parts.month == 9);
    CHECK(parts.day == 22);

    const auto midnight = sys_days{2000y / January / 1d};
    CHECK(nmea::format_time(midnight) == "000000.00");
    CHECK(nmea::format_date(midnight) == "010100");

    const auto late = sys_days{2099y / December / 31d} + 23h + 59min + 59s + 999ms;
    CHECK(nmea::format_time(late) == "235959.99");
    CHECK(nmea::format_date(late) == "311299");
}

TEST_CASE("east_west maps the sign of a value", "[nmea0183][fields]") {
    CHECK(nmea::east_west(4.6) == 'E');
    CHECK(nmea::east_west(0.0) == 'E');
    CHECK(nmea::east_west(-1.0) == 'W');
}
