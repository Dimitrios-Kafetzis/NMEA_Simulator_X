// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Log parsing with its timing rules, log loading, and formatting of recorder lines.
///
/// The time prefix and TAG block helpers here decide which lines carry an absolute time.

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/decoder.hpp>
#include <nmeasim/core/time/iso8601.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>

namespace nmeasim::core::log {

namespace {

using std::chrono::milliseconds;
using std::chrono::system_clock;

/// Removes leading and trailing whitespace.
///
/// @param text The text to trim.
/// @return A view into `text` without the whitespace, as classified by `std::isspace`, at
///         either end.
std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

/// Parses a Unix time in seconds (with optional fraction) or in milliseconds.
///
/// @param text The number; surrounding whitespace is ignored. Only digits and `.` are
///        allowed, so signs, exponents and any other character reject it.
/// @return The time point, rounded to the millisecond, or `std::nullopt` when `text` is not
///         such a number or is not positive. A value above 1e11 is taken as milliseconds.
std::optional<system_clock::time_point> parse_unix_time(std::string_view text) {
    text = trim(text);
    if (text.empty() || !std::all_of(text.begin(), text.end(),
                                     [](char c) { return (c >= '0' && c <= '9') || c == '.'; })) {
        return std::nullopt;
    }
    const auto value = nmea0183::parse_number_field(text);
    if (!value || *value <= 0.0) {
        return std::nullopt;
    }
    // 1e11 seconds is past the year 5000, while 1e11 milliseconds is in 1973, so larger
    // values are millisecond counts.
    const double seconds = *value > 1e11 ? *value / 1000.0 : *value;
    return system_clock::time_point{
        milliseconds{static_cast<milliseconds::rep>(std::llround(seconds * 1000.0))}};
}

/// Reads an absolute time from the text before a sentence.
///
/// Trailing `,`, `;`, `:`, tab and space separators are removed first. The remaining text is
/// tried as a whole, as ISO 8601 and then as Unix time, and then its last
/// whitespace-separated word the same way.
///
/// @param prefix The text of the line before the `$` or `!`, after any TAG block.
/// @return The time, or `std::nullopt` when the prefix is empty or holds no recognised time.
std::optional<system_clock::time_point> parse_prefix_time(std::string_view prefix) {
    prefix = trim(prefix);
    while (!prefix.empty() &&
           (prefix.back() == ',' || prefix.back() == ';' || prefix.back() == ':' ||
            prefix.back() == '\t' || prefix.back() == ' ')) {
        prefix.remove_suffix(1);
    }
    if (prefix.empty()) {
        return std::nullopt;
    }
    if (const auto iso = time::parse_iso8601(prefix)) {
        return iso;
    }
    // The time may be the last word of a longer prefix. The whole prefix is tried first
    // because an ISO 8601 time with a space separator ("2026-09-23 10:00:00") is two words.
    if (const auto unix = parse_unix_time(prefix)) {
        return unix;
    }
    const auto space = prefix.find_last_of(" \t");
    if (space != std::string_view::npos) {
        const auto tail = prefix.substr(space + 1);
        if (const auto iso = time::parse_iso8601(tail)) {
            return iso;
        }
        return parse_unix_time(tail);
    }
    return std::nullopt;
}

/// Reads the time from the `c:` parameter of a TAG block.
///
/// @param block The TAG block between its two backslashes; the `*hh` checksum, when present,
///        is ignored and not verified.
/// @return The time of the first `c:` parameter, read by `parse_unix_time` as seconds or
///         milliseconds since the Unix epoch, or `std::nullopt` when there is no `c:`
///         parameter or its value is not a positive number.
/// @see IEC 61162-450, TAG block parameter "c".
std::optional<system_clock::time_point> tag_block_time(std::string_view block) {
    const auto star = block.find('*');
    block = block.substr(0, star);
    std::size_t start = 0;
    while (start < block.size()) {
        auto end = block.find(',', start);
        if (end == std::string_view::npos) {
            end = block.size();
        }
        const auto parameter = block.substr(start, end - start);
        if (parameter.size() > 2 && parameter[0] == 'c' && parameter[1] == ':') {
            return parse_unix_time(parameter.substr(2));
        }
        start = end + 1;
    }
    return std::nullopt;
}

/// A valid sentence line as read by the first pass of `parse_log`, before offsets are
/// assigned.
struct RawEntry {
    /// The sentence text, from its `$` or `!`, trimmed.
    std::string sentence;
    /// The absolute time of the line, from its TAG block or prefix; empty when it had none.
    std::optional<system_clock::time_point> recorded_at;
    /// The UTC time field of the sentence, when its formatter carries one and it is valid.
    std::optional<nmea0183::SentenceTime> sentence_time;
};

/// Stores an error message when the caller asked for one.
///
/// @param error Destination of the message; nothing is stored when it is null.
/// @param message The one-line reason.
void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

}  // namespace

const char* to_string(TimingSource source) noexcept {
    switch (source) {
        case TimingSource::Timestamps:
            return "timestamps";
        case TimingSource::SentenceTimes:
            return "sentence times";
        case TimingSource::FixedInterval:
            return "fixed interval";
    }
    // Reached only for a value outside the enumeration, which a cast can produce.
    return "unknown";
}

milliseconds Log::duration() const noexcept {
    return entries.empty() ? milliseconds{0} : entries.back().offset;
}

std::optional<Log> parse_log(std::string_view text, const LogParseOptions& options,
                             std::string* error) {
    Log log;
    std::vector<RawEntry> raw;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty()) {
            continue;
        }
        if (line.front() == '#') {
            const auto body = trim(line.substr(1));
            const auto colon = body.find(':');
            if (colon != std::string_view::npos && colon > 0) {
                const auto key = trim(body.substr(0, colon));
                const auto value = trim(body.substr(colon + 1));
                if (!key.empty() && key.find(' ') == std::string_view::npos) {
                    log.header[std::string{key}] = std::string{value};
                    continue;
                }
            }
            // The version follows the 20 characters of "NMEA Simulator X log".
            if (body.starts_with("NMEA Simulator X log")) {
                log.header["format"] = std::string{trim(body.substr(20))};
            }
            continue;
        }

        RawEntry entry;
        if (line.front() == '\\') {
            const auto close = line.find('\\', 1);
            if (close == std::string_view::npos) {
                ++log.skipped_lines;
                continue;
            }
            entry.recorded_at = tag_block_time(line.substr(1, close - 1));
            line = trim(line.substr(close + 1));
        }
        const auto delimiter = line.find_first_of("$!");
        if (delimiter == std::string_view::npos) {
            ++log.skipped_lines;
            continue;
        }
        const std::string_view sentence_text = trim(line.substr(delimiter));
        const auto parsed = nmea0183::parse_sentence(sentence_text);
        if (!parsed) {
            ++log.skipped_lines;
            continue;
        }
        if (!entry.recorded_at) {
            entry.recorded_at = parse_prefix_time(line.substr(0, delimiter));
        }
        entry.sentence = std::string{sentence_text};
        entry.sentence_time = nmea0183::sentence_time(*parsed);
        raw.push_back(std::move(entry));
    }

    if (raw.empty()) {
        set_error(error, log.skipped_lines > 0 ? "No valid NMEA sentence found in the log"
                                               : "The log is empty");
        return std::nullopt;
    }

    const bool any_timestamp = std::any_of(
        raw.begin(), raw.end(), [](const RawEntry& e) { return e.recorded_at.has_value(); });
    const bool any_sentence_time = std::any_of(
        raw.begin(), raw.end(), [](const RawEntry& e) { return e.sentence_time.has_value(); });

    log.entries.reserve(raw.size());
    milliseconds offset{0};
    if (any_timestamp) {
        log.timing = TimingSource::Timestamps;
        std::optional<system_clock::time_point> origin;
        for (auto& entry : raw) {
            if (entry.recorded_at) {
                if (!origin) {
                    origin = entry.recorded_at;
                }
                const auto candidate =
                    std::chrono::duration_cast<milliseconds>(*entry.recorded_at - *origin);
                // Time never runs backwards in a replay.
                offset = std::max(offset, candidate);
            }
            log.entries.push_back({offset, std::move(entry.sentence), entry.recorded_at});
        }
    } else if (any_sentence_time) {
        log.timing = TimingSource::SentenceTimes;
        std::optional<milliseconds> previous_time_of_day;
        for (auto& entry : raw) {
            if (entry.sentence_time) {
                const auto now = entry.sentence_time->since_midnight;
                if (previous_time_of_day) {
                    auto delta = now - *previous_time_of_day;
                    // A large step back is the time of day wrapping at midnight.
                    if (delta < -std::chrono::hours{12}) {
                        delta += std::chrono::hours{24};
                    }
                    if (delta > milliseconds{0}) {
                        offset += delta;
                    }
                }
                previous_time_of_day = now;
            }
            log.entries.push_back({offset, std::move(entry.sentence), std::nullopt});
        }
    } else {
        log.timing = TimingSource::FixedInterval;
        for (auto& entry : raw) {
            log.entries.push_back({offset, std::move(entry.sentence), std::nullopt});
            offset += options.fixed_interval;
        }
    }
    return log;
}

std::optional<Log> load_log(const std::string& path, const LogParseOptions& options,
                            std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        set_error(error, "Cannot read " + path);
        return std::nullopt;
    }
    const std::string content{std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>()};
    std::string reason;
    auto log = parse_log(content, options, &reason);
    if (!log) {
        set_error(error, path + ": " + reason);
        return std::nullopt;
    }
    return log;
}

// Doxygen cannot match this definition to its declaration through the local
// `system_clock` alias and would list it as a second, undocumented function, so it is
// hidden from Doxygen; the documentation is in log_file.hpp.
/// @cond
std::string format_log_line(system_clock::time_point recorded_at, std::string_view sentence) {
    std::string line = time::format_iso8601(recorded_at);
    line += ' ';
    line += sentence;
    return line;
}
/// @endcond

std::string format_header_line(std::string_view key, std::string_view value) {
    std::string line{"# "};
    line += key;
    line += ": ";
    line += value;
    return line;
}

}  // namespace nmeasim::core::log
