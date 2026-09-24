// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Assembly of one NMEA 0183 sentence field by field, framed with its checksum.
///
/// The encoders write their sentences with `SentenceBuilder`: it starts with the
/// address, appends comma-separated fields formatted by the functions of `fields.hpp` and
/// frames the result with `append_checksum`. `fits_limit` checks a sentence against the
/// NMEA 0183 length limit.
///
/// @see NMEA 0183 (IEC 61162-1), sentence structure.

#pragma once

#include <nmeasim/core/nmea0183/checksum.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace nmeasim::core::nmea0183 {

/// Maximum length of a sentence in bytes without the terminating CR LF: 80, that is
/// `kMaxSentenceLength` less the two terminator bytes.
///
/// Derived from `kMaxSentenceLength`, the single definition of the NMEA 0183 limit; every
/// length check (`fits_limit`, encode_within_limit(), the custom sentence validation) uses
/// this value.
inline constexpr std::size_t kMaxSentenceLengthWithoutTerminator{kMaxSentenceLength - 2};

/// Tells whether a character may appear in a sentence field.
///
/// NMEA 0183 allows the printable ASCII characters, space included, in a field, except those
/// it reserves for framing and encoding: `,` (field delimiter), `*` (checksum delimiter), `$`
/// and `!` (start delimiters), `\` (TAG block delimiter), `^` (code delimiter of the
/// hexadecimal escapes) and `~` (reserved). CR, LF, the other control characters, DEL and
/// bytes outside ASCII are never allowed.
///
/// @param c The character to check.
/// @return `true` for a printable ASCII character that NMEA 0183 does not reserve.
/// @see NMEA 0183 (IEC 61162-1), reserved and valid characters.
[[nodiscard]] bool is_text_field_character(char c) noexcept;

/// Builder for one sentence: the address, then fields appended in order and separated by
/// commas.
///
/// Every `field` and `empty` overload returns the builder, so calls can be chained. Text
/// fields are cleaned of the characters is_text_field_character() rejects, so a field value
/// can never add a field or end the sentence; the address is not validated. The builder does
/// not stop at the length limit; `fits_limit` tells whether the framed sentence will fit.
///
/// @see NMEA 0183, sentence structure.
class SentenceBuilder {
public:
    /// Starts a sentence with its start delimiter and address, such as `$GPRMC`.
    ///
    /// @param talker Talker identifier, two characters by convention, such as `GP`; not
    ///               checked.
    /// @param formatter Sentence formatter, three characters by convention, such as `RMC`;
    ///                  not checked.
    /// @param delimiter Start delimiter: `$` (`kStartDelimiter`) for a parametric sentence,
    ///                  `!` (`kEncapsulationDelimiter`) for an encapsulated one such as AIS
    ///                  VDM and VDO.
    SentenceBuilder(std::string_view talker, std::string_view formatter, char delimiter = '$');

    /// Appends a text field without the characters NMEA 0183 does not allow in it.
    ///
    /// Every character for which is_text_field_character() is false, such as `,`, `*`, CR
    /// or LF, is removed; the others are sent unchanged.
    ///
    /// @param value The field text; empty, or with no allowed character, appends an empty
    ///              field.
    /// @return This builder.
    SentenceBuilder& field(std::string_view value);
    /// Appends a single-character field, such as a status or hemisphere letter.
    ///
    /// @param value The character to send; a character is_text_field_character() rejects
    ///              appends an empty field instead.
    /// @return This builder.
    SentenceBuilder& field(char value);
    /// Appends a number with a fixed number of decimals, formatted by `format_fixed`.
    ///
    /// @param value The number to send.
    /// @param decimals Digits after the decimal point, 0 or more.
    /// @return This builder.
    /// @throws std::format_error if `decimals` is negative.
    SentenceBuilder& field(double value, int decimals);
    /// Appends an integer, formatted by `format_padded`.
    ///
    /// @param value The integer to send.
    /// @param width Minimum number of characters, reached by adding leading zeros; 0 or a
    ///              negative value means no padding.
    /// @return This builder.
    SentenceBuilder& field(int value, int width = 0);
    /// Appends an empty field, which means "no data" in NMEA 0183.
    ///
    /// @return This builder.
    SentenceBuilder& empty();
    /// Appends several empty fields.
    ///
    /// @param count Number of empty fields to append; 0 appends nothing.
    /// @return This builder.
    SentenceBuilder& empty(std::size_t count);

    /// Returns the framed sentence with its checksum.
    ///
    /// The builder is left unchanged, so further fields can still be appended.
    ///
    /// @return The address and fields followed by `*hh`, without line terminator; it may
    ///         exceed the length limit.
    [[nodiscard]] std::string build() const;

    /// Tells whether the framed sentence fits the NMEA 0183 length limit.
    ///
    /// @return `true` when the sentence from `build` has at most
    ///         `kMaxSentenceLengthWithoutTerminator` characters.
    [[nodiscard]] bool fits_limit() const;

private:
    /// The sentence so far: the start delimiter, the address and every appended field with
    /// its leading comma, without the `*hh` checksum.
    std::string body_;
};

/// Tells whether a framed sentence fits the NMEA 0183 length limit.
///
/// @param sentence The sentence from its start delimiter to its checksum, without line
///                 terminator (which the limit already allows for) and without TAG block.
/// @return `true` when `sentence` has at most `kMaxSentenceLengthWithoutTerminator`
///         characters.
[[nodiscard]] bool fits_limit(std::string_view sentence) noexcept;

}  // namespace nmeasim::core::nmea0183
