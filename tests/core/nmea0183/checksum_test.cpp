// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the NMEA 0183 checksum functions of `nmeasim/core/nmea0183/checksum.hpp`.
///
/// Covers nmeasim::core::nmea0183::compute_checksum(),
/// nmeasim::core::nmea0183::format_checksum(), nmeasim::core::nmea0183::append_checksum() and
/// nmeasim::core::nmea0183::verify_checksum(), including the framing errors that
/// verify_checksum() must reject. No fixture file is read.

#include <nmeasim/core/nmea0183/checksum.hpp>

#include <catch2/catch_test_macros.hpp>

namespace nmea = nmeasim::core::nmea0183;

TEST_CASE("compute_checksum XORs every byte of the body", "[nmea0183][checksum]") {
    CHECK(nmea::compute_checksum("") == 0x00);
    CHECK(nmea::compute_checksum("A") == 0x41);
    CHECK(nmea::compute_checksum("AA") == 0x00);
    CHECK(nmea::compute_checksum("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,") ==
          0x47);
}

TEST_CASE("format_checksum produces two upper-case hex digits", "[nmea0183][checksum]") {
    CHECK(nmea::format_checksum(0x00) == "00");
    CHECK(nmea::format_checksum(0x0A) == "0A");
    CHECK(nmea::format_checksum(0x47) == "47");
    CHECK(nmea::format_checksum(0xFF) == "FF");
}

TEST_CASE("append_checksum matches sentences from real receivers", "[nmea0183][checksum]") {
    CHECK(nmea::append_checksum("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,") ==
          "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
    CHECK(nmea::append_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,") ==
          "$GPGLL,4916.45,N,12311.12,W,225444,A,*1D");
    CHECK(nmea::append_checksum("$WIMWD,299.3,T,299.3,M,9.2,N,4.7,M") ==
          "$WIMWD,299.3,T,299.3,M,9.2,N,4.7,M*52");
    CHECK(nmea::append_checksum("$WIMWV,309.6,R,11.5,N,A") == "$WIMWV,309.6,R,11.5,N,A*1A");
    CHECK(nmea::append_checksum("$SDDPT,4.1,") == "$SDDPT,4.1,*7C");
    CHECK(nmea::append_checksum("!AIVDM,1,1,,A,402E3Miv0r<BCPDAjjMdjuW000S:,0") ==
          "!AIVDM,1,1,,A,402E3Miv0r<BCPDAjjMdjuW000S:,0*26");
}

TEST_CASE("verify_checksum accepts valid sentences", "[nmea0183][checksum]") {
    CHECK(
        nmea::verify_checksum("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"));
    CHECK(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,*1D\r\n"));
    CHECK(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,*1d\n"));
    CHECK(nmea::verify_checksum("!AIVDM,1,1,,A,402E3Miv0r<BCPDAjjMdjuW000S:,0*26"));
    CHECK(nmea::verify_checksum(
        "$GPRMC,121824.81,A,5149.480,N,00407.224,E,0.1,344.0,200316,001.0,W,D*20"));
}

TEST_CASE("verify_checksum rejects malformed or corrupted sentences", "[nmea0183][checksum]") {
    // In order: nothing, an empty body, no start delimiter, no checksum, one checksum digit,
    // text after the checksum, non-hexadecimal digits, a wrong checksum, and a changed field
    // (A to V) under the checksum of the original sentence.
    CHECK_FALSE(nmea::verify_checksum(""));
    CHECK_FALSE(nmea::verify_checksum("$*00"));
    CHECK_FALSE(nmea::verify_checksum("GPGLL,4916.45,N,12311.12,W,225444,A,*1D"));
    CHECK_FALSE(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,"));
    CHECK_FALSE(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,*1"));
    CHECK_FALSE(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,*1DX"));
    CHECK_FALSE(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,*ZZ"));
    CHECK_FALSE(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,A,*1E"));
    CHECK_FALSE(nmea::verify_checksum("$GPGLL,4916.45,N,12311.12,W,225444,V,*1D"));
}
