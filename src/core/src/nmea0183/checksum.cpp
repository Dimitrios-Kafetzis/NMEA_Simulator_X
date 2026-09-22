#include <nmeasim/core/nmea0183/checksum.hpp>

#include <array>
#include <optional>

namespace nmeasim::core::nmea0183 {

namespace {

constexpr std::array<char, 16> kHexDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

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
    result += format_checksum(compute_checksum(sentence.substr(1)));
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
