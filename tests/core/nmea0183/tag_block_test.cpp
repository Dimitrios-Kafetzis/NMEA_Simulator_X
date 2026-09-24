// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the IEC 61162-450 TAG blocks of `nmeasim/core/nmea0183/tag_block.hpp`.
///
/// Covers nmeasim::core::nmea0183::format_tag_block() with and without the time parameter in
/// seconds and milliseconds, nmeasim::core::nmea0183::prepend_tag_block() and
/// nmeasim::core::nmea0183::sanitize_tag_source(). No fixture file is read; the time comes
/// from the fixture state of `tests/core/fixtures.hpp`.

#include "core/fixtures.hpp"

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/tag_block.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace nmea = nmeasim::core::nmea0183;

TEST_CASE("a TAG block names the source and the time with its own checksum", "[nmea0183][tag]") {
    const auto time = nmeasim::test::fixture_state()
                          .time_utc;  // 2026-09-22T12:34:56.78Z is 1790080496 Unix seconds
    nmea::TagBlockOptions options;
    options.source = "GP0001";
    CHECK(nmea::format_tag_block(options, time) == "\\s:GP0001,c:1790080496*26\\");
    options.milliseconds = true;
    CHECK(nmea::format_tag_block(options, time) == "\\s:GP0001,c:1790080496780*19\\");
    options.include_time = false;
    CHECK(nmea::format_tag_block(options, time) == "\\s:GP0001*5F\\");
    CHECK(nmea::prepend_tag_block("$HEHDT,45.0,T*1E", options, time) ==
          "\\s:GP0001*5F\\$HEHDT,45.0,T*1E");
    // The checksum covers the text between the backslashes, as for a sentence.
    CHECK(nmea::compute_checksum("s:GP0001,c:1790080496") == 0x26);
}

TEST_CASE("TAG block sources are sanitised", "[nmea0183][tag]") {
    CHECK(nmea::sanitize_tag_source("GP0001") == "GP0001");
    // Spaces and reserved characters are dropped and at most 15 characters are kept.
    CHECK(nmea::sanitize_tag_source("my source, with*stars\\and$") == "mysourcewithsta");
    CHECK(nmea::sanitize_tag_source("") == "SIM");
    CHECK(nmea::sanitize_tag_source(",,*") == "SIM");
}
