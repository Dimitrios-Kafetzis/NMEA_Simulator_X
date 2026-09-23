#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/// Bit-level packing of AIS messages and their six-bit ASCII "armoring" into the payload
/// field of VDO and VDM sentences (ITU-R M.1371).
namespace nmeasim::core::ais {

/// Accumulates the bit fields of one AIS message, most significant bit first.
class BitPacker {
public:
    /// Appends the low `bits` bits of an unsigned value.
    void append_unsigned(std::uint32_t value, int bits);
    /// Appends a signed value in two's complement on `bits` bits, clamped to the range.
    void append_signed(std::int32_t value, int bits);
    /// Appends `length` characters of six-bit ASCII, upper-cased, padded with `@` and
    /// truncated as needed; characters outside the six-bit alphabet become `?`.
    void append_text(std::string_view text, std::size_t length);
    /// Appends one bit: 1 for true, 0 for false.
    void append_bool(bool value) { append_unsigned(value ? 1U : 0U, 1); }

    /// Number of bits appended so far.
    [[nodiscard]] std::size_t size() const noexcept { return bits_.size(); }
    /// The appended bits, most significant first, ready for armor().
    [[nodiscard]] const std::vector<bool>& bits() const noexcept { return bits_; }

private:
    std::vector<bool> bits_;
};

/// A packed payload ready for a sentence: the armored characters and the number of padding
/// bits added to the last one.
struct Payload {
    /// Armored six-bit characters for the payload field of VDO and VDM.
    std::string text;
    /// Zero bits padding the last character to six bits, 0-5; sent in the last field.
    int fill_bits{0};
};

/// Armors a bit string into six-bit ASCII characters (`0`-`W` and `` ` ``-`w`).
[[nodiscard]] Payload armor(const std::vector<bool>& bits);

/// The six-bit code of a character of the AIS text alphabet, or nullopt-like `63` (`?`) for
/// characters outside it.
[[nodiscard]] std::uint8_t sixbit_code(char c) noexcept;

/// The character an armored payload character stands for, or -1 when it is not valid.
[[nodiscard]] int unarmor(char c) noexcept;

}  // namespace nmeasim::core::ais
