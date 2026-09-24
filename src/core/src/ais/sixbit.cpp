// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Bit packing, six-bit text codes and payload armouring for AIS messages.

#include <nmeasim/core/ais/sixbit.hpp>

#include <algorithm>
#include <cctype>

namespace nmeasim::core::ais {

void BitPacker::append_unsigned(std::uint32_t value, int bits) {
    for (int bit = bits - 1; bit >= 0; --bit) {
        bits_.push_back(((value >> static_cast<unsigned>(bit)) & 1U) != 0U);
    }
}

void BitPacker::append_signed(std::int32_t value, int bits) {
    const std::int32_t maximum = (std::int32_t{1} << (bits - 1)) - 1;
    const std::int32_t minimum = -maximum - 1;
    const std::int32_t clamped = std::clamp(value, minimum, maximum);
    const auto mask = (std::uint32_t{1} << static_cast<unsigned>(bits)) - 1U;
    append_unsigned(static_cast<std::uint32_t>(clamped) & mask, bits);
}

void BitPacker::append_text(std::string_view text, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        const char c = i < text.size() ? text[i] : '@';
        append_unsigned(sixbit_code(c), 6);
    }
}

std::uint8_t sixbit_code(char c) noexcept {
    const auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper >= '@' && upper <= '_') {
        return static_cast<std::uint8_t>(upper - '@');
    }
    if (upper >= ' ' && upper <= '?') {
        return static_cast<std::uint8_t>(upper - ' ' + 32);
    }
    return 63;  // '?'
}

Payload armor(const std::vector<bool>& bits) {
    Payload payload;
    payload.text.reserve((bits.size() + 5) / 6);
    std::size_t index = 0;
    while (index < bits.size()) {
        std::uint8_t value = 0;
        for (int bit = 0; bit < 6; ++bit, ++index) {
            value = static_cast<std::uint8_t>(value << 1U);
            if (index < bits.size() && bits[index]) {
                value = static_cast<std::uint8_t>(value | 1U);
            }
        }
        // The armouring alphabet skips the eight characters `X` to `_` between `W` and
        // the backquote, hence the two offsets.
        payload.text += static_cast<char>(value < 40 ? value + 48 : value + 56);
    }
    payload.fill_bits = static_cast<int>((6 - bits.size() % 6) % 6);
    return payload;
}

int unarmor(char c) noexcept {
    if (c >= '0' && c <= 'W') {
        return c - 48;
    }
    if (c >= '`' && c <= 'w') {
        return c - 56;
    }
    return -1;
}

}  // namespace nmeasim::core::ais
