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

/// A small independent reader of armored payloads, used to check the encoders field by field.
class BitReader {
public:
    explicit BitReader(std::string_view payload) {
        for (const char c : payload) {
            const int value = ais::unarmor(c);
            REQUIRE(value >= 0);
            for (int bit = 5; bit >= 0; --bit) {
                bits_.push_back(((value >> bit) & 1) != 0);
            }
        }
    }

    std::uint32_t unsigned_value(int count) {
        std::uint32_t value = 0;
        for (int i = 0; i < count; ++i) {
            value = (value << 1U) | (bits_.at(position_++) ? 1U : 0U);
        }
        return value;
    }

    std::int32_t signed_value(int count) {
        const auto raw = unsigned_value(count);
        const auto sign = std::uint32_t{1} << static_cast<unsigned>(count - 1);
        return (raw & sign) != 0U
                   ? static_cast<std::int32_t>(raw) - static_cast<std::int32_t>(sign << 1U)
                   : static_cast<std::int32_t>(raw);
    }

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

    [[nodiscard]] std::size_t remaining() const { return bits_.size() - position_; }

private:
    std::vector<bool> bits_;
    std::size_t position_{0};
};

/// Joins the payload fragments of a multi-sentence message.
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
    CHECK(ais::sixbit_code('~') == 63);
    CHECK(ais::sixbit_code('\xe9') == 63);
}

TEST_CASE("armoring maps six bits to the payload alphabet with fill bits", "[ais]") {
    // 0 -> '0', 39 -> 'W', 40 -> '`', 63 -> 'w'.
    ais::BitPacker packer;
    packer.append_unsigned(0, 6);
    packer.append_unsigned(39, 6);
    packer.append_unsigned(40, 6);
    packer.append_unsigned(63, 6);
    auto payload = ais::armor(packer.bits());
    CHECK(payload.text == "0W`w");
    CHECK(payload.fill_bits == 0);
    packer.append_unsigned(1, 2);  // two bits left over: padded with four zero bits
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
    CHECK(reader.unsigned_value(6) == 1);  // message type
    CHECK(reader.unsigned_value(2) == 0);  // repeat
    CHECK(reader.unsigned_value(30) == 239000001);
    CHECK(reader.unsigned_value(4) == 0);  // under way using engine
    CHECK(reader.signed_value(8) == -7);   // rate of turn code for -2.5 deg/min
    CHECK(reader.unsigned_value(10) == 65);
    CHECK(reader.unsigned_value(1) == 0);
    CHECK(reader.signed_value(28) / 600000.0 == Approx(23.7275).margin(1e-6));
    CHECK(reader.signed_value(27) / 600000.0 == Approx(37.9838).margin(1e-6));
    CHECK(reader.unsigned_value(12) == 473);
    CHECK(reader.unsigned_value(9) == 45);
    CHECK(reader.unsigned_value(6) == 56);
    CHECK(reader.unsigned_value(2) == 0);
    CHECK(reader.unsigned_value(3) == 0);
    CHECK(reader.unsigned_value(1) == 0);
    CHECK(reader.unsigned_value(19) == 0);
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
    CHECK(reader.unsigned_value(4) == 8);
    CHECK(reader.signed_value(8) == 0);
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
    // message id comes from the fixture clock (12:34:56 -> 6).
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
    CHECK(reader.unsigned_value(2) == 0);
    CHECK(reader.unsigned_value(30) == 0);  // IMO
    CHECK(reader.text(7) == "SIMX");
    CHECK(reader.text(20) == "NMEA SIMULATOR X");
    CHECK(reader.unsigned_value(8) == 37);
    CHECK(reader.unsigned_value(9) == 12);
    CHECK(reader.unsigned_value(9) == 4);
    CHECK(reader.unsigned_value(6) == 3);
    CHECK(reader.unsigned_value(6) == 3);
    CHECK(reader.unsigned_value(4) == 1);   // GPS
    CHECK(reader.unsigned_value(4) == 0);   // ETA month
    CHECK(reader.unsigned_value(5) == 0);   // ETA day
    CHECK(reader.unsigned_value(5) == 24);  // ETA hour
    CHECK(reader.unsigned_value(6) == 60);  // ETA minute
    CHECK(reader.unsigned_value(8) == 18);  // draught 1.8 m
    CHECK(reader.text(20) == "AEGINA");
    CHECK(reader.unsigned_value(1) == 0);
    CHECK(reader.unsigned_value(1) == 0);
    CHECK(reader.remaining() == 2);  // fill bits
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
    CHECK(reader.unsigned_value(8) == 255);
    CHECK(reader.unsigned_value(9) == 511);
    reader.unsigned_value(9);
    CHECK(reader.unsigned_value(6) == 63);
    reader.unsigned_value(6);
    reader.unsigned_value(4);
    reader.unsigned_value(4);
    reader.unsigned_value(5);
    reader.unsigned_value(5);
    reader.unsigned_value(6);
    CHECK(reader.unsigned_value(8) == 255);
}

TEST_CASE("payloads are split into fragments that fit the length limit", "[ais]") {
    ais::Payload payload;
    payload.text = std::string(130, '0');
    payload.fill_bits = 3;
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
