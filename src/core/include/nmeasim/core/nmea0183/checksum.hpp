// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// NMEA 0183 sentence framing: delimiters, the length limit and the XOR checksum.
///
/// A sentence has the form `$TTSSS,field,field,...*hh` followed by CR LF, where `TT` is the
/// talker and `SSS` the sentence formatter; encapsulated sentences such as the AIS sentences
/// VDM and VDO start with `!` instead of `$`. The checksum `hh` is the bitwise XOR of every
/// byte strictly between the start delimiter and the `*`, written as two upper-case
/// hexadecimal digits. `append_checksum` frames a sentence and `verify_checksum` checks one.
///
/// @see NMEA 0183 (IEC 61162-1), sentence structure and checksum.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nmeasim::core::nmea0183 {

/// Maximum length of a sentence in bytes, from NMEA 0183: 82, counting the start delimiter
/// and the terminating CR LF.
///
/// The only definition of the limit: the checks derive their bound from it.
///
/// @see kMaxSentenceLengthWithoutTerminator for the same limit without CR LF.
inline constexpr std::size_t kMaxSentenceLength{82};

/// Start delimiter of parametric sentences, `$`.
inline constexpr char kStartDelimiter{'$'};

/// Start delimiter of encapsulated sentences, `!`, used by the AIS sentences VDM and VDO.
inline constexpr char kEncapsulationDelimiter{'!'};

/// Separator between the sentence body and its checksum, `*`.
inline constexpr char kChecksumDelimiter{'*'};

/// Computes the NMEA 0183 checksum of `body`: the XOR of all its bytes.
///
/// @param body The text the checksum covers: everything after the start delimiter and before
///             the `*`, without either. The same rule covers the text of a TAG block between
///             its backslashes.
/// @return The XOR of every byte of `body`; `0` for an empty `body`.
[[nodiscard]] std::uint8_t compute_checksum(std::string_view body) noexcept;

/// Formats a checksum as exactly two upper-case hexadecimal digits.
///
/// @param checksum The checksum, as returned by `compute_checksum`.
/// @return Two characters, for example `"0A"` for 10 and `"FF"` for 255.
[[nodiscard]] std::string format_checksum(std::uint8_t checksum);

/// Appends `*hh` to a sentence that has no checksum yet.
///
/// The checksum covers every character after the first, so the first character must be the
/// start delimiter (`$` or `!`); the function does not check it. The result has no line
/// terminator; the caller adds CR LF when transmitting.
///
/// @param sentence The sentence from its start delimiter to its last field, without `*hh`
///                 and without CR LF.
/// @return `sentence` followed by `*` and the two checksum digits.
/// @throws std::out_of_range if `sentence` is empty.
/// @note A `sentence` that already contains a `*` gets a second checksum computed over the
///       first one, which is not a valid sentence.
[[nodiscard]] std::string append_checksum(std::string_view sentence);

/// Checks the framing and the checksum of a received sentence.
///
/// Any trailing CR and LF characters are ignored. The sentence must then start with `$` or
/// `!`, have a non-empty body, and end with `*` followed by exactly two hexadecimal digits,
/// which may be upper or lower case. A sentence without checksum, or with a TAG block in
/// front, is rejected.
///
/// @param sentence The sentence to check, with or without its line terminator.
/// @return `true` when the framing is valid and the digits after the first `*` match the
///         checksum of the body; `false` otherwise.
[[nodiscard]] bool verify_checksum(std::string_view sentence) noexcept;

}  // namespace nmeasim::core::nmea0183
