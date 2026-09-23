#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Recorded sentence logs: the format the simulator writes and the third-party variants it
/// reads back for replay.
///
/// A log is a text file with one sentence per line. The simulator writes
/// `<ISO 8601 UTC timestamp> <sentence>` lines after a short `#` header. When reading, any
/// of these line shapes is accepted, mixed freely:
///
/// - `2026-09-23T10:00:00.250Z $GPRMC,...*hh` (this simulator, and many loggers)
/// - `1790416800.250 $GPRMC,...*hh` (Unix seconds or milliseconds)
/// - `\s:GP0001,c:1790416800*hh\$GPRMC,...*hh` (an IEC 61162-450 TAG block; its `c:` time
///   is used when present)
/// - `$GPRMC,...*hh` (a plain sentence; timing comes from the RMC/GGA/GLL/ZDA time fields,
///   or from a fixed interval when the log has none)
///
/// Blank lines and lines without a `$` or `!` sentence are skipped, as are sentences whose
/// checksum does not match. Lines starting with `#` are comments; `# key: value` comments
/// are kept as header metadata.
namespace nmeasim::core::log {

/// First line written by the recorder; the trailing number is the format version.
inline constexpr std::string_view kHeaderLine{"# NMEA Simulator X log 1"};

/// One recorded sentence.
struct LogEntry {
    /// Time since the first entry.
    std::chrono::milliseconds offset{0};
    /// The sentence without line terminator.
    std::string sentence;
    /// The absolute time on the line, when the line carried one.
    std::optional<std::chrono::system_clock::time_point> recorded_at;
};

/// How the entry offsets were derived.
enum class TimingSource {
    /// Every line, or at least the first, carried an absolute timestamp.
    Timestamps,
    /// Offsets come from the UTC time fields inside the sentences.
    SentenceTimes,
    /// The log had no time information; entries are spaced by a fixed interval.
    FixedInterval,
};

[[nodiscard]] const char* to_string(TimingSource source) noexcept;

struct Log {
    std::vector<LogEntry> entries;
    /// `# key: value` header lines, keyed by `key`.
    std::map<std::string, std::string> header;
    TimingSource timing{TimingSource::FixedInterval};
    /// Lines that carried no usable sentence.
    std::size_t skipped_lines{0};

    /// Offset of the last entry.
    [[nodiscard]] std::chrono::milliseconds duration() const noexcept;
};

struct LogParseOptions {
    /// Spacing used when the log carries no time information at all.
    std::chrono::milliseconds fixed_interval{100};
};

/// Parses log text. Fails only when no sentence at all could be read.
[[nodiscard]] std::optional<Log> parse_log(std::string_view text, const LogParseOptions& options,
                                           std::string* error);

/// Reads and parses a log file from disk.
[[nodiscard]] std::optional<Log> load_log(const std::string& path, const LogParseOptions& options,
                                          std::string* error);

/// Formats one line as the recorder writes it, without line terminator.
[[nodiscard]] std::string format_log_line(std::chrono::system_clock::time_point recorded_at,
                                          std::string_view sentence);

/// Formats a `# key: value` header line, without line terminator.
[[nodiscard]] std::string format_header_line(std::string_view key, std::string_view value);

}  // namespace nmeasim::core::log
