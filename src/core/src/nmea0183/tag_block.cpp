// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Formatting of IEC 61162-450 TAG blocks and sanitising of their source identifier.
///
/// Implements the functions declared in `tag_block.hpp`.

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/tag_block.hpp>

#include <cstddef>
#include <format>

namespace nmeasim::core::nmea0183 {

namespace {

/// Maximum number of characters of the `s:` source identifier that are sent; longer
/// identifiers are truncated.
///
/// The limit is part of the documented output format (sentence reference, section "TAG
/// blocks").
constexpr std::size_t kMaxSourceLength{15};

}  // namespace

std::string sanitize_tag_source(std::string_view source) {
    std::string result;
    for (const char c : source) {
        // Keep the identifier to printable ASCII without spaces. The reserved characters would
        // be read as a parameter separator (`,`), the checksum or block delimiter (`*` and the
        // backslash) or the start of a sentence (`!`, `$`).
        const bool printable = c > ' ' && c <= '~';
        const bool reserved = c == ',' || c == '*' || c == '\\' || c == '!' || c == '$';
        if (printable && !reserved) {
            result += c;
        }
        if (result.size() == kMaxSourceLength) {
            break;
        }
    }
    return result.empty() ? "SIM" : result;
}

std::string format_tag_block(const TagBlockOptions& options,
                             std::chrono::system_clock::time_point time) {
    std::string body = "s:" + sanitize_tag_source(options.source);
    if (options.include_time) {
        const auto since_epoch = time.time_since_epoch();
        const long long value =
            options.milliseconds
                ? std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch).count()
                : std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
        body += std::format(",c:{}", value);
    }
    // The checksum covers the text between the backslashes, as for a sentence.
    return "\\" + body + "*" + format_checksum(compute_checksum(body)) + "\\";
}

std::string prepend_tag_block(std::string_view sentence, const TagBlockOptions& options,
                              std::chrono::system_clock::time_point time) {
    return format_tag_block(options, time) + std::string{sentence};
}

}  // namespace nmeasim::core::nmea0183
