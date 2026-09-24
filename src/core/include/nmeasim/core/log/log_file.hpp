// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Reading and writing sentence logs: `parse_log`, `load_log`, `format_log_line` and
/// `format_header_line`.
///
/// When reading, any of these line shapes is accepted, mixed freely in one file:
///
/// - `2026-09-23T10:00:00.250Z $GPRMC,...*hh` (this simulator, and many loggers)
/// - `1790416800.250 $GPRMC,...*hh` (Unix seconds or milliseconds)
/// - `\s:GP0001,c:1790416800*hh\$GPRMC,...*hh` (an IEC 61162-450 TAG block; its `c:` time
///   is used when present)
/// - `$GPRMC,...*hh` (a plain sentence; timing comes from the UTC time fields of the
///   sentences, or from a fixed interval when the log has none)

#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Recorded sentence logs, part of the Qt-free `nmeasim::core` library: the format the
/// simulator writes when recording and the third-party variants it reads back for replay.
///
/// A log is a text file with one sentence per line. The recorder (`nmeasim::io::LogTransport`)
/// writes `kHeaderLine`, `# key: value` header lines made by `format_header_line`, and one
/// `ISO-8601-UTC-time sentence` line per sentence made by `format_log_line`. `parse_log` and
/// `load_log` read that format and also plain NMEA logs, Unix-time prefixes and
/// IEC 61162-450 TAG block times, mixed freely, for `simulation::ReplaySource`. The format
/// is described in docs/reference/log-format.md and chosen in ADR 0012.
namespace nmeasim::core::log {

/// First line written by the recorder; the trailing number is the format version.
///
/// `parse_log` stores the text after `NMEA Simulator X log` as the `format` header entry.
inline constexpr std::string_view kHeaderLine{"# NMEA Simulator X log 1"};

/// One sentence of a log, with the time at which a replay sends it.
struct LogEntry {
    /// Time from the start of the replay; the first entry of a log is at zero, and offsets
    /// never decrease along a log (see `parse_log`).
    std::chrono::milliseconds offset{0};
    /// The sentence from its `$` or `!` to the end of the line, surrounding whitespace and
    /// line terminator removed, otherwise byte for byte as in the file. It excludes any time
    /// prefix and TAG block.
    std::string sentence;
    /// The absolute UTC time the line carried (time prefix or TAG block `c:` parameter), not
    /// clamped like `offset`; empty when the line carried none.
    std::optional<std::chrono::system_clock::time_point> recorded_at;
};

/// How the entry offsets of a log were derived, in `parse_log`'s order of preference.
enum class TimingSource {
    /// At least one line carried an absolute time (prefix or TAG block); offsets come from
    /// those times. Display name `timestamps`.
    Timestamps,
    /// No line carried an absolute time, but at least one sentence carried a UTC time field;
    /// offsets come from those fields. Display name `sentence times`.
    SentenceTimes,
    /// The log had no time information; entries are spaced by
    /// `LogParseOptions::fixed_interval`. Display name `fixed interval`.
    FixedInterval,
};

/// Returns the lower-case display name of a timing source, such as `sentence times`.
///
/// @param source The timing source to name.
/// @return A static string, valid for the lifetime of the program: `timestamps`,
///         `sentence times` or `fixed interval`, and `unknown` for a value outside the
///         enumeration.
[[nodiscard]] const char* to_string(TimingSource source) noexcept;

/// A parsed log: its sentences in file order with their offsets, and the header metadata.
///
/// A log returned by `parse_log` has at least one entry; a default-constructed one has none.
struct Log {
    /// The valid sentences in file order; the first has offset zero and offsets never
    /// decrease.
    std::vector<LogEntry> entries;
    /// Metadata from `# key: value` comment lines anywhere in the file, keyed by `key`, plus
    /// `format` from `kHeaderLine`. A later line with the same key replaces an earlier one.
    std::map<std::string, std::string> header;
    /// Where the entry offsets came from.
    TimingSource timing{TimingSource::FixedInterval};
    /// Number of lines that are neither blank nor comments and yield no valid sentence: no
    /// `$` or `!`, an unterminated TAG block or one whose checksum does not match, or a
    /// sentence that fails `nmea0183::parse_sentence` (such as a checksum mismatch).
    std::size_t skipped_lines{0};

    /// Returns the length of a replay of the log.
    ///
    /// @return The offset of the last entry, or zero when there is none.
    [[nodiscard]] std::chrono::milliseconds duration() const noexcept;
};

/// Options for `parse_log` and `load_log`.
struct LogParseOptions {
    /// Spacing between consecutive entries, used only when the log carries no time
    /// information at all (`TimingSource::FixedInterval`).
    ///
    /// The 100 ms default is the one ADR 0012 specifies. Zero puts every entry at offset
    /// zero; `parse_log` rejects a negative value.
    std::chrono::milliseconds fixed_interval{100};
};

/// Parses the text of a log into its sentences and their replay offsets.
///
/// The text is split into lines at line feeds and each line is trimmed, so `<CR><LF>` and
/// `<LF>` endings both work. Each line is then handled on its own:
/// - a blank line is ignored;
/// - a line starting with `#` is a comment. `# key: value`, with a non-empty key without
///   spaces, becomes a `Log::header` entry, and `kHeaderLine` sets `format`; other comments
///   are ignored. Comments are not counted as skipped;
/// - a line starting with `\` has an IEC 61162-450 TAG block up to the next `\`; its first
///   `c:` parameter, a positive Unix time in seconds (with optional fraction) or in
///   milliseconds, gives the line's time. A `*hh` checksum at the end of the block must
///   match, as for a sentence, and a block without one is accepted. A line whose block is
///   not closed or has a checksum that does not match is skipped and counted in
///   `Log::skipped_lines`;
/// - the sentence starts at the first `$` or `!` and must pass
///   `nmea0183::parse_sentence`: a checksum, when present, must match, and a sentence
///   without one is accepted. A line without `$` or `!`, or with a rejected sentence, is
///   skipped and counted in `Log::skipped_lines`;
/// - when the TAG block gave no time, the text before the sentence is read as a time: an
///   ISO 8601 time (`T` or space separator, optional fraction and offset, UTC when there is
///   none) or a Unix time in seconds or milliseconds (a number above 1e11 is taken as
///   milliseconds), optionally followed by `,`, `;`, `:`, tabs or spaces, either as the
///   whole prefix or as its last whitespace-separated word. Any other prefix, such as
///   `[10:00:00]`, is ignored and the line has no time.
///
/// The offsets are then derived in this order of preference, recorded in `Log::timing`:
/// 1. `TimingSource::Timestamps`, when any line has an absolute time. The first time in the
///    file is the origin; each timed entry is at its time minus the origin, and an untimed
///    entry shares the previous entry's offset (zero before the first timed line). An
///    offset is never lower than the previous one, so a clock stepping back holds the
///    replay instead of reversing it.
/// 2. `TimingSource::SentenceTimes`, when any sentence carries a valid UTC time of day
///    (RMC, GGA, GLL, ZDA, GNS, GST, GBS, GRS; see `nmea0183::sentence_time`). Each such
///    sentence advances the offset by its time minus the latest sentence time seen so far;
///    a step back of more than 12 hours is taken as crossing midnight and gets 24 hours
///    added. Any other step back leaves the offset unchanged and does not lower the latest
///    time, so the replay holds until the times have caught up and no time is counted
///    twice. Dates are ignored. Sentences without a time share the offset of the previous
///    entry.
/// 3. `TimingSource::FixedInterval` otherwise: entry `i` is at `i` times
///    `options.fixed_interval`.
///
/// @param text The complete log text.
/// @param options Parsing options; only the fixed interval, which must not be negative.
/// @param error Receives the reason when the log is rejected; left unchanged on success.
///        May be null. It is `The fixed interval must not be negative` for a negative
///        `options.fixed_interval`, `The log is empty` when no line was counted as skipped
///        (the text holds only blank and comment lines, or nothing), and
///        `No valid NMEA sentence found in the log` when lines were skipped.
/// @return The log, with at least one entry, or `std::nullopt` when no sentence could be
///         read or the options are invalid. Malformed lines never reject the log on their
///         own.
/// @see IEC 61162-450, TAG block parameter "c" and checksum.
/// @see docs/reference/log-format.md
[[nodiscard]] std::optional<Log> parse_log(std::string_view text, const LogParseOptions& options,
                                           std::string* error);

/// Reads a log file from disk and parses it with `parse_log`.
///
/// The whole file is read into memory first (ADR 0012 puts multi-gigabyte logs out of
/// scope).
///
/// @param path Path of the file, in the encoding `std::ifstream` expects.
/// @param options Parsing options passed on to `parse_log`.
/// @param error Receives the reason on failure; left unchanged on success. May be null. It
///        is `Cannot read PATH` when the file cannot be opened, and `PATH: REASON` with the
///        reason from `parse_log` when no sentence could be read.
/// @return The log, or `std::nullopt` on failure.
[[nodiscard]] std::optional<Log> load_log(const std::string& path, const LogParseOptions& options,
                                          std::string* error);

/// Formats one log line as the recorder writes it, without line terminator.
///
/// @param recorded_at The UTC wall-clock time at which the sentence was written; it is
///        written with millisecond resolution as `YYYY-MM-DDThh:mm:ss.mmmZ`.
/// @param sentence The sentence as sent, without line terminator; copied unchanged.
/// @return The time, one space and the sentence, a line that `parse_log` reads back with
///         `recorded_at` as its time.
[[nodiscard]] std::string format_log_line(std::chrono::system_clock::time_point recorded_at,
                                          std::string_view sentence);

/// Formats a `# key: value` header line, without line terminator.
///
/// @param key The metadata key, such as `profile`.
/// @param value The metadata value.
/// @return `# `, the key, `: ` and the value. `parse_log` reads the pair back into
///         `Log::header` when `key` is non-empty and contains no space or `:`; it trims
///         whitespace from both ends of `value`.
[[nodiscard]] std::string format_header_line(std::string_view key, std::string_view value);

}  // namespace nmeasim::core::log
