// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the AIS bit packing, six-bit armouring and own-vessel messages.
///
/// Covers nmeasim::core::ais::BitPacker, nmeasim::core::ais::sixbit_code(),
/// nmeasim::core::ais::armor(), nmeasim::core::ais::unarmor(),
/// nmeasim::core::ais::rate_of_turn_code() and nmeasim::core::ais::frame_payload(), and the
/// position and static data reports that nmeasim::core::nmea0183::encode_vdo_position(),
/// encode_vdm_position(), encode_vdo_static() and encode_vdm_static() build from the fixture
/// states of `tests/core/fixtures.hpp`. As ADR 0013 describes, every payload is read back field
/// by field with `BitReader`, a decoder written for these tests independently of the encoder.
///
/// No fixture file is read. The golden VDO lines checked here are also the contents of
/// `tests/fixtures/ais/own_vessel.nmea`, which CI decodes with the third-party pyais library
/// (`tools/check_ais_stream.py`).

#include "core/fixtures.hpp"

#include <nmeasim/core/ais/messages.hpp>
#include <nmeasim/core/ais/sixbit.hpp>
#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/decoder.hpp>
#include <nmeasim/core/nmea0183/encoders.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

using Catch::Approx;
namespace ais = nmeasim::core::ais;
namespace nmea = nmeasim::core::nmea0183;

namespace {

/// Reader of armoured AIS payloads that checks the encoders field by field.
///
/// It is written for the tests and shares no code with the encoder apart from
/// nmeasim::core::ais::unarmor(). The fields are read in message order, most significant bit
/// first, with the widths of ITU-R M.1371-5, Annex 8, which the test supplies; the reader
/// knows nothing of the message layout.
class BitReader {
public:
    /// Unpacks an armoured payload into its bits.
    ///
    /// Fails the running test case (`REQUIRE`) when a character is outside the armouring
    /// alphabet.
    ///
    /// @param payload The payload characters of one message, the fragments of a
    ///        multi-sentence message joined in order (see `joined_payload`). Any fill bits
    ///        stay at the end and count in `remaining`.
    explicit BitReader(std::string_view payload) {
        for (const char c : payload) {
            const int value = ais::unarmor(c);
            REQUIRE(value >= 0);
            for (int bit = 5; bit >= 0; --bit) {
                bits_.push_back(((value >> bit) & 1) != 0);
            }
        }
    }

    /// Reads the next field as an unsigned integer.
    ///
    /// @param count Field width in bits, in [0, 32]; 0 reads nothing and returns 0.
    /// @return The field value.
    /// @throws std::out_of_range when fewer than `count` bits remain, which Catch2 reports as
    ///         a failure of the test case.
    std::uint32_t unsigned_value(int count) {
        std::uint32_t value = 0;
        for (int i = 0; i < count; ++i) {
            value = (value << 1U) | (bits_.at(position_++) ? 1U : 0U);
        }
        return value;
    }

    /// Reads the next field as a two's complement signed integer.
    ///
    /// @param count Field width in bits, in [1, 31].
    /// @return The field value, negative when its most significant bit is set.
    /// @throws std::out_of_range when fewer than `count` bits remain.
    std::int32_t signed_value(int count) {
        const auto raw = unsigned_value(count);
        const auto sign = std::uint32_t{1} << static_cast<unsigned>(count - 1);
        return (raw & sign) != 0U
                   ? static_cast<std::int32_t>(raw) - static_cast<std::int32_t>(sign << 1U)
                   : static_cast<std::int32_t>(raw);
    }

    /// Reads the next field as six-bit AIS text.
    ///
    /// Codes 0 to 31 map to `@` to `_` and codes 32 to 63 to space to `?`, the text alphabet
    /// of ITU-R M.1371-5, Annex 8. Trailing `@` and spaces, the padding of a short text, are
    /// removed.
    ///
    /// @param characters Field width in characters; the field is `6 * characters` bits wide.
    /// @return The text without its padding; empty when the field is all padding.
    /// @throws std::out_of_range when fewer than `6 * characters` bits remain.
    std::string text(int characters) {
        std::string result;
        for (int i = 0; i < characters; ++i) {
            const auto code = unsigned_value(6);
            result += static_cast<char>(code < 32 ? code + 64 : code);
        }
        while (!result.empty() && (result.back() == '@' || result.back() == ' ')) {
            result.pop_back();
        }
        return result;
    }

    /// Returns the number of bits not read yet.
    ///
    /// @return The unread bits, fill bits included; zero once a whole message without fill
    ///         bits has been read.
    [[nodiscard]] std::size_t remaining() const { return bits_.size() - position_; }

private:
    /// Every bit of the payload in transmission order, fill bits included.
    std::vector<bool> bits_;
    /// Index in `bits_` of the next bit to read.
    std::size_t position_{0};
};

/// Joins the payload fragments of a multi-sentence message.
///
/// Fails the running test case (`REQUIRE`) when a sentence does not parse. The fill bits
/// field is not read: only the last fragment has fill bits, and they stay at the end.
///
/// @param sentences The VDO or VDM sentences of one message, in order.
/// @return The payload fields (data field 4) of the sentences, concatenated.
std::string joined_payload(const std::vector<std::string>& sentences) {
    std::string payload;
    for (const auto& sentence : sentences) {
        const auto parsed = nmea::parse_sentence(sentence);
        REQUIRE(parsed.has_value());
        payload += parsed->field(4);
    }
    return payload;
}

}  // namespace

TEST_CASE("the bit packer writes fields most significant bit first", "[ais]") {
    ais::BitPacker packer;
    packer.append_unsigned(5, 6);
    packer.append_signed(-1, 8);
    packer.append_signed(200, 8);   // clamped to 127
    packer.append_signed(-200, 8);  // clamped to -128
    packer.append_text("Ab", 3);
    packer.append_bool(true);
    CHECK(packer.size() == 6 + 8 + 8 + 8 + 18 + 1);
    const std::vector<bool> expected{0, 0, 0, 1, 0, 1,        // 5
                                     1, 1, 1, 1, 1, 1, 1, 1,  // -1
                                     0, 1, 1, 1, 1, 1, 1, 1,  // 127
                                     1, 0, 0, 0, 0, 0, 0, 0,  // -128
                                     0, 0, 0, 0, 0, 1, 0, 0, 0,
                                     0, 1, 0, 0, 0, 0, 0, 0, 0,  // A, B, @
                                     1};
    CHECK(packer.bits() == expected);
}

TEST_CASE("six-bit text codes cover the AIS alphabet", "[ais]") {
    CHECK(ais::sixbit_code('@') == 0);
    CHECK(ais::sixbit_code('A') == 1);
    CHECK(ais::sixbit_code('z') == 26);
    CHECK(ais::sixbit_code('_') == 31);
    CHECK(ais::sixbit_code(' ') == 32);
    CHECK(ais::sixbit_code('9') == 57);
    CHECK(ais::sixbit_code('?') == 63);
    // Characters outside the alphabet, here '~' and the Latin-1 byte 0xE9, are sent as '?'.
    CHECK(ais::sixbit_code('~') == 63);
    CHECK(ais::sixbit_code('\xe9') == 63);
}

TEST_CASE("armoring maps six bits to the payload alphabet with fill bits", "[ais]") {
    // The ends of the two ranges of the armouring alphabet: 0 -> '0', 39 -> 'W', 40 -> '`',
    // 63 -> 'w'.
    ais::BitPacker packer;
    packer.append_unsigned(0, 6);
    packer.append_unsigned(39, 6);
    packer.append_unsigned(40, 6);
    packer.append_unsigned(63, 6);
    auto payload = ais::armor(packer.bits());
    CHECK(payload.text == "0W`w");
    CHECK(payload.fill_bits == 0);
    packer.append_unsigned(1, 2);  // two bits left over: padded with four zero bits
    // 01 and the four fill bits make 010000, 16, which armours as '0' + 16 = '@'.
    payload = ais::armor(packer.bits());
    CHECK(payload.text == "0W`w@");
    CHECK(payload.fill_bits == 4);
    for (const char c : payload.text) {
        CHECK(ais::unarmor(c) >= 0);
    }
    CHECK(ais::unarmor('X') == -1);
    CHECK(ais::unarmor('x') == -1);
    CHECK(ais::unarmor(' ') == -1);
}

TEST_CASE("rate of turn is coded on the AIS square-root scale", "[ais]") {
    // The code is 4.733 * sqrt(|rate|) with the rate's sign: 4.733 * sqrt(2.5) = 7.48 and
    // 4.733 * sqrt(10) = 14.97; above about 714 deg/min the code rounds to more than 126 and
    // is clamped.
    CHECK(ais::rate_of_turn_code(0.0) == 0);
    CHECK(ais::rate_of_turn_code(-2.5) == -7);
    CHECK(ais::rate_of_turn_code(10.0) == 15);
    CHECK(ais::rate_of_turn_code(-720.0) == -126);
    CHECK(ais::rate_of_turn_code(9999.0) == 126);
}

TEST_CASE("the position report carries the fixture vessel", "[ais][nmea0183][encoders]") {
    const auto state = nmeasim::test::fixture_state();
    const auto sentences = nmea::encode_vdo_position(nmea::EncoderContext{state, "AI"});
    REQUIRE(sentences.size() == 1);
    CHECK(nmea::verify_checksum(sentences.front()));
    CHECK(nmea::fits_limit(sentences.front()));
    // Golden line, decoded independently with pyais in the CI cross-check.
    CHECK(sentences.front() == "!AIVDO,1,1,,A,13SsIh@vA11dWJ`Eg0R1nAKh0000,0*34");

    BitReader reader(joined_payload(sentences));
    CHECK(reader.unsigned_value(6) == 1);           // message type
    CHECK(reader.unsigned_value(2) == 0);           // repeat
    CHECK(reader.unsigned_value(30) == 239000001);  // MMSI, the AisStatic default
    CHECK(reader.unsigned_value(4) == 0);           // under way using engine
    CHECK(reader.signed_value(8) == -7);            // rate of turn code for -2.5 deg/min
    CHECK(reader.unsigned_value(10) == 65);         // speed over ground 6.5 kn
    CHECK(reader.unsigned_value(1) == 0);           // position accuracy: no differential fix
    // Longitude, then latitude, in 1/10000 minute.
    CHECK(reader.signed_value(28) / 600000.0 == Approx(23.7275).margin(1e-6));
    CHECK(reader.signed_value(27) / 600000.0 == Approx(37.9838).margin(1e-6));
    CHECK(reader.unsigned_value(12) == 473);  // course over ground 47.3 degrees
    CHECK(reader.unsigned_value(9) == 45);    // true heading
    CHECK(reader.unsigned_value(6) == 56);    // UTC second of 12:34:56.78
    CHECK(reader.unsigned_value(2) == 0);     // manoeuvre indicator
    CHECK(reader.unsigned_value(3) == 0);     // spare
    CHECK(reader.unsigned_value(1) == 0);     // RAIM flag
    CHECK(reader.unsigned_value(19) == 0);    // radio status
    CHECK(reader.remaining() == 0);
}

TEST_CASE("the position report reports missing data without a fix and honours the options",
          "[ais][nmea0183][encoders]") {
    auto state = nmeasim::test::fixture_state_without_fix();
    state.ais.navigation_status = 8;
    state.ais.position_report_type = 3;
    state.navigation.rate_of_turn_deg_per_min = 0.0;
    const auto sentences = nmea::encode_vdm_position(nmea::EncoderContext{state, "AI"});
    REQUIRE(sentences.size() == 1);
    CHECK(sentences.front().starts_with("!AIVDM,1,1,,A,"));
    BitReader reader(joined_payload(sentences));
    CHECK(reader.unsigned_value(6) == 3);
    reader.unsigned_value(2);
    reader.unsigned_value(30);
    CHECK(reader.unsigned_value(4) == 8);  // under way sailing
    CHECK(reader.signed_value(8) == 0);
    // Without a fix the speed (1023), longitude (181 degrees), latitude (91 degrees) and
    // course (3600) are sent as "not available"; the heading comes from the compass and stays.
    CHECK(reader.unsigned_value(10) == 1023);
    reader.unsigned_value(1);
    CHECK(reader.signed_value(28) == 181 * 600000);
    CHECK(reader.signed_value(27) == 91 * 600000);
    CHECK(reader.unsigned_value(12) == 3600);
    CHECK(reader.unsigned_value(9) == 45);

    // An unknown type falls back to 1; a differential fix sets the accuracy flag; the speed
    // saturates at 102.2 knots.
    auto fast = nmeasim::test::fixture_state();
    fast.ais.position_report_type = 9;
    fast.gnss.quality = nmeasim::core::model::FixQuality::Differential;
    fast.navigation.speed_over_ground_kn = 150.0;
    BitReader fast_reader(
        joined_payload(nmea::encode_vdo_position(nmea::EncoderContext{fast, "AI"})));
    CHECK(fast_reader.unsigned_value(6) == 1);
    fast_reader.unsigned_value(2);
    fast_reader.unsigned_value(30);
    fast_reader.unsigned_value(4);
    fast_reader.signed_value(8);
    CHECK(fast_reader.unsigned_value(10) == 1022);
    CHECK(fast_reader.unsigned_value(1) == 1);
}

TEST_CASE("the static data report spans two sentences and carries the vessel particulars",
          "[ais][nmea0183][encoders]") {
    auto state = nmeasim::test::fixture_state();
    state.ais.destination = "Aegina";
    const auto sentences = nmea::encode_vdo_static(nmea::EncoderContext{state, "AI"});
    REQUIRE(sentences.size() == 2);
    for (const auto& sentence : sentences) {
        INFO(sentence);
        CHECK(nmea::verify_checksum(sentence));
        CHECK(nmea::fits_limit(sentence));
    }
    // Golden lines, decoded independently with pyais in the CI cross-check. The sequential
    // message id is the UTC second of the fixture clock modulo 10 (12:34:56 -> 6).
    CHECK(sentences[0] ==
          "!AIVDO,2,1,6,A,53SsIh@00001<TmP000plD61<TmDh5@u:1P0000U1P43340Ht4PAAjCP@000,0*7A");
    CHECK(sentences[1] == "!AIVDO,2,2,6,A,00000000000,2*20");
    const auto first = nmea::parse_sentence(sentences[0]);
    const auto second = nmea::parse_sentence(sentences[1]);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first->delimiter == '!');
    CHECK(first->field(0) == "2");
    CHECK(first->field(1) == "1");
    CHECK(first->field(2) == "6");
    CHECK(first->field(5) == "0");
    CHECK(second->field(1) == "2");
    CHECK(second->field(2) == "6");
    CHECK(second->field(5) == "2");

    BitReader reader(joined_payload(sentences));
    CHECK(reader.unsigned_value(6) == 5);
    CHECK(reader.unsigned_value(2) == 0);
    CHECK(reader.unsigned_value(30) == 239000001);
    CHECK(reader.unsigned_value(2) == 0);   // AIS version indicator
    CHECK(reader.unsigned_value(30) == 0);  // IMO
    // Call sign, name, ship type and dimensions are the AisStatic defaults.
    CHECK(reader.text(7) == "SIMX");
    CHECK(reader.text(20) == "NMEA SIMULATOR X");
    CHECK(reader.unsigned_value(8) == 37);  // pleasure craft
    CHECK(reader.unsigned_value(9) == 12);  // to bow, metres
    CHECK(reader.unsigned_value(9) == 4);   // to stern
    CHECK(reader.unsigned_value(6) == 3);   // to port
    CHECK(reader.unsigned_value(6) == 3);   // to starboard
    CHECK(reader.unsigned_value(4) == 1);   // GPS
    CHECK(reader.unsigned_value(4) == 0);   // ETA month
    CHECK(reader.unsigned_value(5) == 0);   // ETA day
    CHECK(reader.unsigned_value(5) == 24);  // ETA hour
    CHECK(reader.unsigned_value(6) == 60);  // ETA minute
    CHECK(reader.unsigned_value(8) == 18);  // draught 1.8 m
    CHECK(reader.text(20) == "AEGINA");     // destination, upper-cased
    CHECK(reader.unsigned_value(1) == 0);   // DTE
    CHECK(reader.unsigned_value(1) == 0);   // spare
    CHECK(reader.remaining() == 2);         // fill bits: 424 bits take 71 characters, 426 bits
}

TEST_CASE("long names, call signs and dimensions are truncated and clamped", "[ais]") {
    auto state = nmeasim::test::fixture_state_extreme();
    state.ais.name = "a name longer than twenty characters";
    state.ais.call_sign = "CALLSIGN9";
    state.ais.ship_type = 999;
    state.ais.dimension_to_bow_m = 9999.0;
    state.ais.dimension_to_port_m = 99.0;
    state.ais.draught_m = 99.0;
    state.ais.imo_number = 9074729;
    const auto sentences = nmea::encode_vdm_static(nmea::EncoderContext{state, "AI"});
    REQUIRE(sentences.size() == 2);
    BitReader reader(joined_payload(sentences));
    reader.unsigned_value(6);
    reader.unsigned_value(2);
    reader.unsigned_value(30);
    reader.unsigned_value(2);
    CHECK(reader.unsigned_value(30) == 9074729);
    CHECK(reader.text(7) == "CALLSIG");
    CHECK(reader.text(20) == "A NAME LONGER THAN T");
    // Each value is clamped to the largest its field can carry.
    CHECK(reader.unsigned_value(8) == 255);  // ship type
    CHECK(reader.unsigned_value(9) == 511);  // to bow
    reader.unsigned_value(9);
    CHECK(reader.unsigned_value(6) == 63);  // to port
    reader.unsigned_value(6);
    reader.unsigned_value(4);
    reader.unsigned_value(4);
    reader.unsigned_value(5);
    reader.unsigned_value(5);
    reader.unsigned_value(6);
    CHECK(reader.unsigned_value(8) == 255);  // draught 25.5 m
}

TEST_CASE("payloads are split into fragments that fit the length limit", "[ais]") {
    ais::Payload payload;
    payload.text = std::string(130, '0');
    payload.fill_bits = 3;
    // 130 characters take fragments of 60, 60 and 10; the sequence id 12 is clamped to 9.
    const auto sentences = ais::frame_payload("AI", "VDM", payload, 12);
    REQUIRE(sentences.size() == 3);
    CHECK(sentences[0].starts_with("!AIVDM,3,1,9,A,"));
    CHECK(sentences[1].starts_with("!AIVDM,3,2,9,A,"));
    CHECK(sentences[2].starts_with("!AIVDM,3,3,9,A,"));
    CHECK(nmea::parse_sentence(sentences[0])->field(5) == "0");
    CHECK(nmea::parse_sentence(sentences[2])->field(5) == "3");
    CHECK(nmea::parse_sentence(sentences[2])->field(4).size() == 10);
    for (const auto& sentence : sentences) {
        CHECK(nmea::fits_limit(sentence));
    }
    const auto single = ais::frame_payload("AI", "VDO", {"0", 0}, 5);
    REQUIRE(single.size() == 1);
    CHECK(single.front().starts_with("!AIVDO,1,1,,A,0,0*"));
}
