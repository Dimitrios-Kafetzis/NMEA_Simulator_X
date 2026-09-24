// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// ISO 8601 date-time parsing and formatting without any locale or time zone database.
///
/// Track files and logs carry timestamps such as `2026-09-23T10:00:00Z` or
/// `2026-09-23T10:00:00.250+02:00`; `parse_iso8601` reads them and `format_iso8601` writes
/// the form every output of the simulator uses.

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

/// Date and time text handling, part of the `nmeasim::core` library.
///
/// It converts between ISO 8601 date-time text and `std::chrono::system_clock` time points
/// in UTC, using only the calendar types of `std::chrono`, so the result never depends on
/// the locale or the time zone of the host.
namespace nmeasim::core::time {

/// Parses an ISO 8601 date-time into a UTC time point with millisecond precision.
///
/// Accepted, after leading and trailing whitespace is trimmed:
/// - a calendar date `YYYY-MM-DD`, which must exist (2024-02-29 does, 2026-02-30 does not);
/// - optionally a `T` or space separator and a time `hh:mm`, optionally followed by `:ss`
///   and a fraction introduced by `.` or `,`; digits beyond milliseconds are truncated. Hour
///   24 is accepted only as `24:00` or `24:00:00`, with a fraction of zeros at most, meaning
///   the end of the day, and second 60 for a leap second, which lands on the next minute;
/// - after a time, optionally `Z` or an offset `+hh`, `+hhmm` or `+hh:mm` (or with `-`), in
///   hours up to 23 and minutes up to 59, which is subtracted to give UTC.
///
/// A date-time without an offset is taken as UTC, not local time. Basic-format dates
/// without hyphens, week dates and ordinal dates are rejected.
///
/// @param text Text to parse.
/// @return The instant in UTC, or `std::nullopt` when `text` is not a valid date-time of the
///         accepted forms.
/// @see ISO 8601-1, extended format of calendar dates and times of day.
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> parse_iso8601(
    std::string_view text);

/// Formats a UTC time point as `YYYY-MM-DDThh:mm:ss.sssZ`.
///
/// The time is rounded down to whole milliseconds and the result always has three
/// decimals, so a time point of whole milliseconds round-trips through `parse_iso8601`.
///
/// @param time_utc Instant to format; years outside 0000 to 9999 do not give valid
///        ISO 8601 text.
/// @return The date-time text, for example `2026-09-23T10:34:56.780Z`.
[[nodiscard]] std::string format_iso8601(std::chrono::system_clock::time_point time_utc);

}  // namespace nmeasim::core::time
