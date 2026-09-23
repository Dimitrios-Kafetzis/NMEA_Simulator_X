#pragma once

#include <chrono>
#include <string>
#include <string_view>

/// IEC 61162-450 TAG blocks: an optional prefix `\s:<source>,c:<time>*hh\` in front of a
/// sentence that names the source and the time of the sentence.
namespace nmeasim::core::nmea0183 {

/// What the TAG block in front of each sentence of an output contains.
struct TagBlockOptions {
    /// Source identifier sent as `s:`; usually a talker plus four digits such as `GP0001`.
    /// Restricted to printable characters without `,`, `*`, `\` or `!` when formatted.
    std::string source{"SIM0001"};
    /// Whether the `c:` time parameter is sent.
    bool include_time{true};
    /// Whether `c:` carries milliseconds (second edition) instead of seconds.
    bool milliseconds{false};
};

/// Formats the TAG block for a sentence sent at `time`, including both backslashes.
[[nodiscard]] std::string format_tag_block(const TagBlockOptions& options,
                                           std::chrono::system_clock::time_point time);

/// Returns `sentence` with a TAG block in front.
[[nodiscard]] std::string prepend_tag_block(std::string_view sentence,
                                            const TagBlockOptions& options,
                                            std::chrono::system_clock::time_point time);

/// The source identifier as it will be sent: reserved characters removed, at most 15
/// characters, "SIM" when nothing is left.
[[nodiscard]] std::string sanitize_tag_source(std::string_view source);

}  // namespace nmeasim::core::nmea0183
