// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of nmeasim::core::nmea0183::SentenceBuilder and nmeasim::core::nmea0183::fits_limit().
///
/// Covers field formatting and framing for parametric and encapsulated sentences, and the
/// length limit checked before and after framing. No fixture file is read.

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace nmea = nmeasim::core::nmea0183;

TEST_CASE("SentenceBuilder frames fields with a checksum", "[nmea0183][builder]") {
    nmea::SentenceBuilder builder("GP", "GLL");
    builder.field("4916.45")
        .field('N')
        .field("12311.12")
        .field('W')
        .field("225444")
        .field('A')
        .empty();
    CHECK(builder.build() == "$GPGLL,4916.45,N,12311.12,W,225444,A,*1D");
}

TEST_CASE("SentenceBuilder formats numbers and padded integers", "[nmea0183][builder]") {
    nmea::SentenceBuilder builder("SD", "DPT");
    builder.field(4.1, 1).field(1.0, 1).empty();
    const auto sentence = builder.build();
    CHECK(sentence.starts_with("$SDDPT,4.1,1.0,*"));
    CHECK(nmea::verify_checksum(sentence));

    nmea::SentenceBuilder gsa("GP", "GSA");
    gsa.field('A').field(3).field(2, 2).empty(3);
    CHECK(gsa.build().starts_with("$GPGSA,A,3,02,,,*"));
}

TEST_CASE("SentenceBuilder supports encapsulated sentences", "[nmea0183][builder]") {
    nmea::SentenceBuilder builder("AI", "VDM", '!');
    builder.field(1).field(1).empty().field('A').field("402E3Miv0r<BCPDAjjMdjuW000S:").field(0);
    CHECK(builder.build() == "!AIVDM,1,1,,A,402E3Miv0r<BCPDAjjMdjuW000S:,0*26");
}

TEST_CASE("length limit is checked before and after framing", "[nmea0183][builder]") {
    nmea::SentenceBuilder builder("GP", "TXT");
    CHECK(builder.fits_limit());
    // `$GPTXT,`, 75 characters and `*hh` make 85 characters, over the limit of 80.
    builder.field(std::string(75, 'x'));
    CHECK_FALSE(builder.fits_limit());
    CHECK_FALSE(nmea::fits_limit(builder.build()));
    CHECK(nmea::fits_limit(std::string(80, 'x')));
    CHECK_FALSE(nmea::fits_limit(std::string(81, 'x')));
}
