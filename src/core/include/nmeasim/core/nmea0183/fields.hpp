// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Formatting of individual NMEA 0183 field values: numbers, coordinates, times and dates.
///
/// Numbers use a fixed number of decimals and never carry a leading `+` or render negative
/// zero. Latitudes and longitudes are sent as degrees and decimal minutes (`ddmm.mmmm`,
/// `dddmm.mmmm`) followed by a hemisphere field, times as `hhmmss.ss` and dates as `ddmmyy`,
/// all in UTC. The encoders use these functions directly or through `SentenceBuilder`.
///
/// @see NMEA 0183 (IEC 61162-1), field formats.

#pragma once

#include <chrono>
#include <string>
#include <utility>

namespace nmeasim::core::nmea0183 {

/// Formats a number with a fixed number of decimals, rounded to the nearest last digit.
///
/// A result that would read as negative zero, such as `-0.0` for `-0.04` with one decimal, is
/// rendered without the sign.
///
/// @param value The number to format.
/// @param decimals Digits after the decimal point, 0 or more; 0 gives no decimal point.
/// @return The formatted number, for example `"6.5"` for 6.5 with one decimal.
/// @throws std::format_error if `decimals` is negative.
[[nodiscard]] std::string format_fixed(double value, int decimals);

/// Formats an integer, zero-padded to a minimum width.
///
/// @param value The integer to format.
/// @param width Minimum number of characters, reached by adding zeros after the sign of a
///              negative number; 0 or a negative value means no padding. A longer number is
///              not truncated.
/// @return The formatted integer, for example `"08"` for 8 with a width of 2.
[[nodiscard]] std::string format_padded(int value, int width);

/// A latitude or longitude formatted for two consecutive NMEA 0183 fields: the value and the
/// hemisphere letter.
struct CoordinateField {
    /// Whole degrees (two digits for a latitude, three for a longitude) and minutes (two
    /// digits), both zero-padded, then the fractional minutes; no sign.
    std::string value;
    /// Hemisphere letter: `N` or `S` for a latitude, `E` or `W` for a longitude.
    char hemisphere;
};

/// Formats a latitude as `ddmm.mmmm` and its hemisphere letter.
///
/// The minutes are rounded to the last digit with integer arithmetic, so a rounding carry
/// goes into the degrees and the minutes never read `60`.
///
/// @param latitude_deg Latitude in decimal degrees, positive north, in [-90, 90]; values
///                     outside the range are not checked.
/// @param decimals Digits of fractional minutes; the profile allows [2, 8], 4 digits resolve
///                 about 0.19 m. 0 gives whole minutes without a decimal point, and a
///                 negative value is treated as 0.
/// @return The formatted latitude, for example `"3759.0280"` and `N` for 37.9838 with four
///         decimals; `S` for a negative latitude and `N` otherwise.
/// @see NMEA 0183, sentence GLL.
[[nodiscard]] CoordinateField format_latitude(double latitude_deg, int decimals);

/// Formats a longitude as `dddmm.mmmm` and its hemisphere letter.
///
/// The minutes are rounded to the last digit with integer arithmetic, so a rounding carry
/// goes into the degrees and the minutes never read `60`.
///
/// @param longitude_deg Longitude in decimal degrees, positive east, in [-180, 180]; values
///                      outside the range are not checked.
/// @param decimals Digits of fractional minutes; the profile allows [2, 8]. 0 gives whole
///                 minutes without a decimal point, and a negative value is treated as 0.
/// @return The formatted longitude, for example `"02343.6500"` and `E` for 23.7275 with four
///         decimals; `W` for a negative longitude and `E` otherwise.
/// @see NMEA 0183, sentence GLL.
[[nodiscard]] CoordinateField format_longitude(double longitude_deg, int decimals);

/// Formats the UTC time of day as `hhmmss.ss`.
///
/// Fractions of a second are truncated to hundredths, not rounded, so 23:59:59.999 is sent as
/// `235959.99` and never rolls over into the next day.
///
/// @param time_utc The instant to format.
/// @return The time of day, for example `"123456.78"`.
[[nodiscard]] std::string format_time(std::chrono::system_clock::time_point time_utc);

/// Formats the UTC date as `ddmmyy`, with a two-digit year.
///
/// @param time_utc The instant to format.
/// @return The date, for example `"220926"` for 22 September 2026.
/// @see NMEA 0183, sentence RMC.
[[nodiscard]] std::string format_date(std::chrono::system_clock::time_point time_utc);

/// The UTC calendar date split into its components, as sent in ZDA.
struct DateParts {
    /// Year of the Gregorian calendar with all its digits, for example 2026.
    int year;
    /// Month of the year, in [1, 12].
    int month;
    /// Day of the month, in [1, 31].
    int day;
};

/// Splits the UTC date of an instant into year, month and day.
///
/// @param time_utc The instant whose date is wanted.
/// @return The calendar date of `time_utc` in UTC.
/// @see NMEA 0183, sentence ZDA.
[[nodiscard]] DateParts date_parts(std::chrono::system_clock::time_point time_utc);

/// Returns the hemisphere letter for a signed east or west quantity such as magnetic
/// variation or deviation.
///
/// @param value The quantity in degrees, positive east.
/// @return `W` when `value` is negative, `E` otherwise, including for zero.
[[nodiscard]] constexpr char east_west(double value) noexcept {
    return value < 0.0 ? 'W' : 'E';
}

}  // namespace nmeasim::core::nmea0183
