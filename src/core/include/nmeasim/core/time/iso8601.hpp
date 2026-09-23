#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

/// ISO 8601 date-time parsing and formatting without any locale or time zone database.
///
/// Track files and logs carry timestamps such as `2026-09-23T10:00:00Z` or
/// `2026-09-23T10:00:00.250+02:00`. The parser accepts a calendar date, a `T` or space
/// separator, a time of day with optional seconds and fraction, and an optional `Z` or
/// `±hh[:mm]` offset; a missing offset means UTC.
namespace nmeasim::core::time {

/// Parses an ISO 8601 date-time into a UTC time point with millisecond precision.
/// Returns nullopt for anything that is not a valid date-time.
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> parse_iso8601(
    std::string_view text);

/// Formats a UTC time point as `YYYY-MM-DDThh:mm:ss.mmmZ`.
[[nodiscard]] std::string format_iso8601(std::chrono::system_clock::time_point time_utc);

}  // namespace nmeasim::core::time
