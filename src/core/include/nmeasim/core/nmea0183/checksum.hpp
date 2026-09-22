#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/// NMEA 0183 sentence framing helpers.
///
/// A sentence has the form `$TTSSS,field,field,...*hh<CR><LF>` (or `!` instead of `$` for
/// encapsulated sentences such as AIS VDM/VDO). The checksum `hh` is the bitwise XOR of every
/// byte strictly between the start delimiter and the `*`, written as two upper-case hex digits.
namespace nmeasim::core::nmea0183 {

/// Maximum length of a sentence in bytes, including `$`/`!` and the terminating <CR><LF>.
inline constexpr std::size_t kMaxSentenceLength{82};

/// Sentence start delimiter for parametric sentences.
inline constexpr char kStartDelimiter{'$'};

/// Sentence start delimiter for encapsulated sentences (AIS).
inline constexpr char kEncapsulationDelimiter{'!'};

/// Separates the sentence body from its checksum.
inline constexpr char kChecksumDelimiter{'*'};

/// Computes the XOR checksum of `body`, which must exclude the start delimiter and the
/// checksum delimiter.
[[nodiscard]] std::uint8_t compute_checksum(std::string_view body) noexcept;

/// Formats a checksum as exactly two upper-case hexadecimal digits.
[[nodiscard]] std::string format_checksum(std::uint8_t checksum);

/// Appends `*hh` to a sentence that starts with `$` or `!` and has no checksum yet.
///
/// The result has no line terminator; callers add <CR><LF> when transmitting.
/// The behaviour is undefined if `sentence` is empty or already contains a `*`.
[[nodiscard]] std::string append_checksum(std::string_view sentence);

/// Returns true when `sentence` starts with `$` or `!`, ends with `*hh` (optionally followed
/// by <CR>, <LF> or <CR><LF>) and `hh` matches the computed checksum.
[[nodiscard]] bool verify_checksum(std::string_view sentence) noexcept;

}  // namespace nmeasim::core::nmea0183
