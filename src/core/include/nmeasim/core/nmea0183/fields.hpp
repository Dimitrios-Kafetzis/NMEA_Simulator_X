#pragma once

#include <chrono>
#include <string>
#include <utility>

/// Formatting of individual NMEA 0183 field values.
namespace nmeasim::core::nmea0183 {

/// Formats a number with a fixed number of decimals. Negative zero is rendered as zero.
[[nodiscard]] std::string format_fixed(double value, int decimals);

/// Formats an integer zero-padded to `width` characters.
[[nodiscard]] std::string format_padded(int value, int width);

/// A coordinate formatted as `ddmm.mmmm` (latitude) or `dddmm.mmmm` (longitude), with the
/// hemisphere letter to send in the following field.
struct CoordinateField {
    std::string value;
    char hemisphere;
};

/// Formats a latitude in decimal degrees as `ddmm.mmmm` with `decimals` fractional minutes.
[[nodiscard]] CoordinateField format_latitude(double latitude_deg, int decimals);

/// Formats a longitude in decimal degrees as `dddmm.mmmm` with `decimals` fractional minutes.
[[nodiscard]] CoordinateField format_longitude(double longitude_deg, int decimals);

/// Formats a UTC time of day as `hhmmss.ss`.
[[nodiscard]] std::string format_time(std::chrono::system_clock::time_point time_utc);

/// Formats a UTC date as `ddmmyy`.
[[nodiscard]] std::string format_date(std::chrono::system_clock::time_point time_utc);

/// The UTC calendar date split into its components.
struct DateParts {
    int year;
    int month;
    int day;
};

[[nodiscard]] DateParts date_parts(std::chrono::system_clock::time_point time_utc);

/// The letter for a signed east/west quantity such as variation or deviation.
[[nodiscard]] constexpr char east_west(double value) noexcept {
    return value < 0.0 ? 'W' : 'E';
}

}  // namespace nmeasim::core::nmea0183
