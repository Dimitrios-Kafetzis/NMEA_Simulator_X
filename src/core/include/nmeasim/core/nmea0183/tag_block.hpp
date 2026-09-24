// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// IEC 61162-450 TAG blocks that name the source and the time of a sentence.
///
/// A TAG block is an optional prefix of the form `\s:<source>,c:<time>*hh\` in front of a
/// sentence, enabled per output. The `s:` parameter identifies the sender, the optional `c:`
/// parameter carries the Unix time of the sentence, and `hh` is the checksum of the text
/// between the backslashes, computed like a sentence checksum. The sentence after the block is
/// unchanged. `format_tag_block` builds a block and `prepend_tag_block` puts one in front of a
/// sentence.
///
/// @see IEC 61162-450, TAG block parameters "s" and "c".

#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace nmeasim::core::nmea0183 {

/// What the TAG block in front of each sentence of an output contains.
///
/// The defaults match the `tag_block` object of an output in the profile.
///
/// @see IEC 61162-450, TAG block parameters "s" and "c".
struct TagBlockOptions {
    /// Source identifier sent as the `s:` parameter, usually a talker followed by four digits
    /// such as `GP0001`.
    ///
    /// Any text is accepted; `sanitize_tag_source` reduces it to what may be sent when the
    /// block is formatted.
    std::string source{"SIM0001"};
    /// Whether the `c:` time parameter is sent.
    bool include_time{true};
    /// Whether `c:` carries Unix milliseconds instead of Unix seconds.
    bool milliseconds{false};
};

/// Formats the TAG block for a sentence sent at a given time.
///
/// The block reads `\s:<source>,c:<time>*hh\`, without the `c:` parameter when
/// `options.include_time` is false. The time is the number of whole seconds, or whole
/// milliseconds, since the Unix epoch, truncated rather than rounded.
///
/// @param options The source and time parameters to send; the source is passed through
///                `sanitize_tag_source`.
/// @param time The time of the sentence, normally the simulated UTC time.
/// @return The block, including both backslashes, for example `\s:GP0001,c:1790080496*26\`.
/// @see IEC 61162-450, TAG block parameter "c".
[[nodiscard]] std::string format_tag_block(const TagBlockOptions& options,
                                           std::chrono::system_clock::time_point time);

/// Returns a sentence with a TAG block in front.
///
/// @param sentence The framed sentence, with or without line terminator; it is copied
///                 unchanged after the block.
/// @param options The source and time parameters to send.
/// @param time The time of the sentence, normally the simulated UTC time.
/// @return The block from `format_tag_block` followed by `sentence`.
[[nodiscard]] std::string prepend_tag_block(std::string_view sentence,
                                            const TagBlockOptions& options,
                                            std::chrono::system_clock::time_point time);

/// Returns a source identifier as it is sent in the `s:` parameter.
///
/// Only printable ASCII characters other than the space and the reserved characters `,`,
/// `*`, `\`, `!` and `$` are kept, and at most the first 15 of them.
///
/// @param source The configured source identifier.
/// @return The kept characters, or `"SIM"` when none are left.
[[nodiscard]] std::string sanitize_tag_source(std::string_view source);

}  // namespace nmeasim::core::nmea0183
