#include <nmeasim/core/nmea0183/fields.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>

namespace nmeasim::core::nmea0183 {

namespace {

std::int64_t pow10(int exponent) {
    std::int64_t result = 1;
    for (int i = 0; i < exponent; ++i) {
        result *= 10;
    }
    return result;
}

/// Splits |degrees| into whole degrees and fractional minutes using integer arithmetic so that
/// rounding never produces "60.0000" minutes.
std::string format_coordinate(double degrees, int degree_digits, int decimals) {
    const std::int64_t scale = pow10(decimals);
    const std::int64_t minutes_per_degree = 60 * scale;
    const auto total = static_cast<std::int64_t>(
        std::llround(std::fabs(degrees) * 60.0 * static_cast<double>(scale)));
    const std::int64_t whole_degrees = total / minutes_per_degree;
    const std::int64_t scaled_minutes = total % minutes_per_degree;
    const std::int64_t whole_minutes = scaled_minutes / scale;
    const std::int64_t fraction = scaled_minutes % scale;

    std::string result = std::format("{:0{}}{:02}", whole_degrees, degree_digits, whole_minutes);
    if (decimals > 0) {
        result += std::format(".{:0{}}", fraction, decimals);
    }
    return result;
}

}  // namespace

std::string format_fixed(double value, int decimals) {
    std::string text = std::format("{:.{}f}", value, decimals);
    if (text.front() == '-' &&
        std::all_of(text.begin() + 1, text.end(), [](char c) { return c == '0' || c == '.'; })) {
        text.erase(0, 1);
    }
    return text;
}

std::string format_padded(int value, int width) {
    if (width <= 0) {
        return std::format("{}", value);
    }
    return std::format("{:0{}}", value, width);
}

CoordinateField format_latitude(double latitude_deg, int decimals) {
    return {format_coordinate(latitude_deg, 2, decimals), latitude_deg < 0.0 ? 'S' : 'N'};
}

CoordinateField format_longitude(double longitude_deg, int decimals) {
    return {format_coordinate(longitude_deg, 3, decimals), longitude_deg < 0.0 ? 'W' : 'E'};
}

std::string format_time(std::chrono::system_clock::time_point time_utc) {
    using namespace std::chrono;
    const auto since_midnight = time_utc - floor<days>(time_utc);
    const hh_mm_ss time_of_day{floor<milliseconds>(since_midnight)};
    const auto centiseconds = time_of_day.subseconds().count() / 10;
    return std::format("{:02}{:02}{:02}.{:02}", time_of_day.hours().count(),
                       time_of_day.minutes().count(), time_of_day.seconds().count(), centiseconds);
}

DateParts date_parts(std::chrono::system_clock::time_point time_utc) {
    using namespace std::chrono;
    const year_month_day ymd{floor<days>(time_utc)};
    return {static_cast<int>(ymd.year()), static_cast<int>(static_cast<unsigned>(ymd.month())),
            static_cast<int>(static_cast<unsigned>(ymd.day()))};
}

std::string format_date(std::chrono::system_clock::time_point time_utc) {
    const auto parts = date_parts(time_utc);
    return std::format("{:02}{:02}{:02}", parts.day, parts.month, parts.year % 100);
}

}  // namespace nmeasim::core::nmea0183
