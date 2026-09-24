// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Bit-level packing of AIS messages and the six-bit ASCII armouring of their payload.
///
/// `BitPacker` collects the fields of one message, `armor` turns the bits into the characters
/// of the payload field of VDO and VDM sentences, and `sixbit_code` and `unarmor` expose the
/// two six-bit alphabets involved: the text alphabet of string fields and the payload
/// armouring alphabet.
///
/// @see ITU-R M.1371-5, Annex 8, for the six-bit text alphabet, and NMEA 0183 (IEC 61162-1),
/// sentences VDM and VDO, for the payload armouring.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nmeasim::core::ais {

/// Accumulates the bit fields of one AIS message, most significant bit first.
///
/// Fields are appended in message order; the packer knows nothing of the message layout, so
/// the caller is responsible for the field widths. A default-constructed packer is empty.
///
/// @see armor to turn the bits into a payload.
class BitPacker {
public:
    /// Appends the low `bits` bits of an unsigned value, most significant bit first.
    ///
    /// Bits of `value` above the field width are dropped, so a value that does not fit is
    /// silently truncated; callers clamp first.
    ///
    /// @param value Field value.
    /// @param bits Field width in bits; 0 or a negative width appends nothing, and a field
    ///        wider than 32 bits gets zeros above the value's 32 bits.
    void append_unsigned(std::uint32_t value, int bits);

    /// Appends a signed value in two's complement on `bits` bits.
    ///
    /// A value outside the range of the field, [-2^(bits-1), 2^(bits-1) - 1], is clamped to
    /// its nearest end rather than wrapped. A field of 32 bits or more holds every value; a
    /// field wider than 32 bits repeats the sign bit above the value's 32 bits.
    ///
    /// @param value Field value.
    /// @param bits Field width in bits; 0 or a negative width appends nothing.
    void append_signed(std::int32_t value, int bits);

    /// Appends a text field of exactly `length` six-bit characters.
    ///
    /// Lower-case letters are sent upper-case, characters outside the six-bit text alphabet
    /// become `?`, a shorter text is padded with `@` (the AIS "not available" character) and
    /// a longer one is truncated. The field is `6 * length` bits wide.
    ///
    /// @param text Text to send, one character per byte.
    /// @param length Number of characters in the field, for example 20 for the vessel name.
    /// @see sixbit_code for the character mapping.
    void append_text(std::string_view text, std::size_t length);

    /// Appends one bit: 1 for `true`, 0 for `false`.
    ///
    /// @param value Flag to append.
    void append_bool(bool value) { append_unsigned(value ? 1U : 0U, 1); }

    /// Returns the number of bits appended so far.
    ///
    /// @return Message length in bits.
    [[nodiscard]] std::size_t size() const noexcept { return bits_.size(); }

    /// Returns the appended bits, most significant bit of the first field first.
    ///
    /// @return The bit string, ready for `armor`. The reference stays valid until the next
    ///         append or the packer's destruction.
    [[nodiscard]] const std::vector<bool>& bits() const noexcept { return bits_; }

private:
    /// The message bits in transmission order.
    std::vector<bool> bits_;
};

/// A packed payload ready for a sentence: the armoured characters and the fill bits.
///
/// `text` holds `ceil(n / 6)` characters for a message of `n` bits and `fill_bits` is
/// `6 * text.size() - n`.
///
/// @see frame_payload to split it into sentences.
struct Payload {
    /// Armoured six-bit characters for the payload field of VDO and VDM, each in `0`-`W` or
    /// `` ` ``-`w`; empty for a message of no bits.
    std::string text;
    /// Zero bits appended to complete the last character, in [0, 5]; sent in the last field
    /// of the last sentence.
    int fill_bits{0};
};

/// Armours a bit string into six-bit ASCII payload characters.
///
/// Each group of six bits, most significant first, becomes one character: values 0 to 39
/// map to `0`-`W` (value plus 48) and 40 to 63 to `` ` ``-`w` (value plus 56). A last group
/// shorter than six bits is padded with zero bits on the right.
///
/// @param bits Message bits in transmission order, as `BitPacker::bits` returns them.
/// @return The payload characters and the number of fill bits.
/// @see NMEA 0183 (IEC 61162-1), sentence VDM.
[[nodiscard]] Payload armor(const std::vector<bool>& bits);

/// Returns the six-bit code of a character of the AIS text alphabet.
///
/// The alphabet is `@`, `A`-`Z`, `[`, `\`, `]`, `^`, `_` (codes 0 to 31) followed by space
/// and `!` to `?`, digits included (codes 32 to 63). Lower-case letters are upper-cased
/// first.
///
/// @param c Character to encode.
/// @return The code in [0, 63]; 63, the code of `?`, for any character outside the alphabet.
/// @see ITU-R M.1371-5, Annex 8, six-bit ASCII table.
[[nodiscard]] std::uint8_t sixbit_code(char c) noexcept;

/// Returns the six-bit value an armoured payload character stands for.
///
/// This is the inverse of the mapping `armor` applies.
///
/// @param c Payload character.
/// @return The value in [0, 63], or -1 when `c` is not in `0`-`W` or `` ` ``-`w`.
[[nodiscard]] int unarmor(char c) noexcept;

}  // namespace nmeasim::core::ais
