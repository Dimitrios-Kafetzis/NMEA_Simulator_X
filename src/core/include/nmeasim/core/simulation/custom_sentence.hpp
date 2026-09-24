// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Operator-defined sentences: their settings and the validation and framing of their bodies.
///
/// The operator types a sentence body, for example a proprietary `$PXYZ,1,2,3`, and the
/// simulator sends it on its own period with a checksum it computes itself.
/// `validate_custom_sentence` says why a body is refused, `frame_custom_sentence` turns an
/// acceptable body into a complete sentence, and `SentenceScheduler::set_custom_sentences`
/// schedules a list of `CustomSentence` values.
///
/// @see NMEA 0183 (IEC 61162-1), sentence structure and checksum.

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace nmeasim::core::simulation {

/// One operator-defined sentence and its schedule.
///
/// The values are taken as typed; `SentenceScheduler::set_custom_sentences` fills in an empty
/// id, replaces a non-positive period and drops the sentence when its body cannot be framed
/// or its id clashes with a registry id.
struct CustomSentence {
    /// Identifier used by the output filters and the console, and as `EmittedSentence::id`.
    ///
    /// Must not equal a registry id such as `RMC`, or the scheduler drops the sentence. An
    /// empty id is replaced by `CUSTOM-n`, where `n` is the 1-based position of the sentence
    /// in the list given to the scheduler.
    std::string id;
    /// The sentence body as typed: an optional `$` or `!`, the address, then the fields.
    ///
    /// A trailing `*hh` checksum, surrounding white space and a line terminator are allowed
    /// and ignored, because the checksum is always recomputed. Examples: `$PXYZ,1,2,3`,
    /// `PXYZ,1,2,3` and `$PXYZ,1,2,3*00`. `validate_custom_sentence` lists what is refused.
    std::string body;
    /// Interval between emissions in simulated time; the scheduler replaces zero or a
    /// negative value by one second.
    std::chrono::milliseconds period{1000};
    /// Whether the sentence is sent; a disabled sentence is kept in the list but not sent.
    bool enabled{true};
};

/// Frames a custom sentence body into a complete sentence with a freshly computed checksum.
///
/// Surrounding white space, the line terminator and any existing `*hh` checksum are removed;
/// the start delimiter is kept when present and `$` is added otherwise; the checksum is then
/// appended.
///
/// @param body The body as typed by the operator, in the format described at
///   `CustomSentence::body`.
/// @return The framed sentence without line terminator, for example `$PXYZ,1,2,3*17` for
///   `PXYZ,1,2,3`, or `std::nullopt` when `validate_custom_sentence` refuses `body`.
/// @see NMEA 0183 (IEC 61162-1), sentence structure and checksum.
[[nodiscard]] std::optional<std::string> frame_custom_sentence(std::string_view body);

/// Checks whether `frame_custom_sentence` accepts a body and explains why not.
///
/// After surrounding white space is trimmed, a body is refused when:
/// - it is empty;
/// - it contains a `*` that is not followed by exactly two hexadecimal digits at the end;
/// - apart from the leading delimiter, it contains a character outside printable ASCII
///   (0x20 to 0x7E) or one of the reserved characters `$`, `!`, `\`, `^` and `~`;
/// - its address, the text before the first comma, is shorter than three characters or
///   contains anything but letters and digits;
/// - the framed sentence would be longer than 80 characters without its line terminator,
///   the NMEA 0183 limit of 82 characters including CR LF.
///
/// @param body The body as typed by the operator.
/// @return `std::nullopt` when the body is acceptable, otherwise an English sentence for the
///   operator saying what is wrong.
[[nodiscard]] std::optional<std::string> validate_custom_sentence(std::string_view body);

}  // namespace nmeasim::core::simulation
