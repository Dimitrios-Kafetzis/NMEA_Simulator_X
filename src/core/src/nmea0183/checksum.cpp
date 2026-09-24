// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Computation, formatting and verification of NMEA 0183 checksums.
///
/// Implements the functions declared in `checksum.hpp`, with the hexadecimal and
/// line-terminator helpers they share.

#include <nmeasim/core/nmea0183/checksum.hpp>

#include <array>
#include <optional>

namespace nmeasim::core::nmea0183 {

namespace {

/// Upper-case hexadecimal digits indexed by their value, used to write checksums.
constexpr std::array<char, 16> kHexDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/// Returns the value of one hexadecimal digit.
///
/// Lower-case digits are accepted as well, so that `verify_checksum` accepts `*1d` as
/// readily as `*1D`.
///
/// @param c The character to convert.
/// @return The value in [0, 15], or `std::nullopt` when `c` is not a hexadecimal digit.
std::optional<std::uint8_t> hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return static_cast<std::uint8_t>(c - '0');
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<std::uint8_t>(c - 'A' + 10);
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<std::uint8_t>(c - 'a' + 10);
    }
    return std::nullopt;
}

/// Removes every trailing CR and LF character from a sentence.
///
/// @param sentence The sentence, with or without its line terminator.
/// @return A view of `sentence` without the trailing CR and LF characters; it refers to the
///         same characters as `sentence`.
std::string_view strip_line_terminator(std::string_view sentence) noexcept {
    while (!sentence.empty() && (sentence.back() == '\r' || sentence.back() == '\n')) {
        sentence.remove_suffix(1);
    }
    return sentence;
}

}  // namespace

std::uint8_t compute_checksum(std::string_view body) noexcept {
    std::uint8_t checksum{0};
    for (const char c : body) {
        checksum = static_cast<std::uint8_t>(checksum ^ static_cast<unsigned char>(c));
    }
    return checksum;
}

std::string format_checksum(std::uint8_t checksum) {
    return {kHexDigits[static_cast<std::size_t>(checksum >> 4U)],
            kHexDigits[static_cast<std::size_t>(checksum & 0x0FU)]};
}

std::string append_checksum(std::string_view sentence) {
    std::string result{sentence};
    result += kChecksumDelimiter;
    // The checksum does not cover the start delimiter; an empty sentence has neither
    // delimiter nor body.
    result += format_checksum(compute_checksum(sentence.empty() ? sentence : sentence.substr(1)));
    return result;
}

bool verify_checksum(std::string_view sentence) noexcept {
    sentence = strip_line_terminator(sentence);
    if (sentence.size() < 4) {
        return false;
    }
    if (sentence.front() != kStartDelimiter && sentence.front() != kEncapsulationDelimiter) {
        return false;
    }
    // The body between the start delimiter and '*' must be non-empty, and exactly two
    // checksum digits must follow the '*'.
    const auto star = sentence.find(kChecksumDelimiter);
    if (star == std::string_view::npos || star < 2 || star + 3 != sentence.size()) {
        return false;
    }
    const auto high = hex_value(sentence[star + 1]);
    const auto low = hex_value(sentence[star + 2]);
    if (!high || !low) {
        return false;
    }
    const auto expected = static_cast<std::uint8_t>((*high << 4U) | *low);
    return compute_checksum(sentence.substr(1, star - 1)) == expected;
}

}  // namespace nmeasim::core::nmea0183
