// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Hand-written ISO 8601 date-time parser and formatter on `std::chrono` calendar types.

#include <nmeasim/core/time/iso8601.hpp>

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>

namespace nmeasim::core::time {

namespace {

/// Reads a fixed number of decimal digits.
///
/// @param text Text being parsed.
/// @param[in,out] offset Position of the first digit; advanced past the digits on success
///        and left unchanged on failure.
/// @param digits Number of digits to read, at most 9 so that the value fits an `int`.
/// @return The value of the digits, or `std::nullopt` when `text` has fewer than `digits`
///         characters left or one of them is not a digit.
std::optional<int> read_digits(std::string_view text, std::size_t& offset, std::size_t digits) {
    if (offset + digits > text.size()) {
        return std::nullopt;
    }
    int value = 0;
    for (std::size_t i = 0; i < digits; ++i) {
        const char c = text[offset + i];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }
    offset += digits;
    return value;
}

/// Consumes one expected character.
///
/// @param text Text being parsed.
/// @param[in,out] offset Position to look at; advanced by one when the character matches.
/// @param expected Character to accept.
/// @return Whether the character at `offset` was `expected`.
bool consume(std::string_view text, std::size_t& offset, char expected) {
    if (offset < text.size() && text[offset] == expected) {
        ++offset;
        return true;
    }
    return false;
}

}  // namespace

std::optional<std::chrono::system_clock::time_point> parse_iso8601(std::string_view text) {
    using namespace std::chrono;

    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }

    std::size_t offset = 0;
    const auto year = read_digits(text, offset, 4);
    if (!year || !consume(text, offset, '-')) {
        return std::nullopt;
    }
    const auto month = read_digits(text, offset, 2);
    if (!month || !consume(text, offset, '-')) {
        return std::nullopt;
    }
    const auto day = read_digits(text, offset, 2);
    if (!day) {
        return std::nullopt;
    }
    const year_month_day date{std::chrono::year{*year},
                              std::chrono::month{static_cast<unsigned>(*month)},
                              std::chrono::day{static_cast<unsigned>(*day)}};
    if (!date.ok()) {
        return std::nullopt;
    }

    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;
    // True when the fraction has a non-zero digit, including one beyond the millisecond.
    bool fraction_nonzero = false;
    if (offset < text.size()) {
        if (!consume(text, offset, 'T') && !consume(text, offset, ' ')) {
            return std::nullopt;
        }
        const auto h = read_digits(text, offset, 2);
        if (!h || !consume(text, offset, ':')) {
            return std::nullopt;
        }
        const auto m = read_digits(text, offset, 2);
        if (!m) {
            return std::nullopt;
        }
        hour = *h;
        minute = *m;
        if (consume(text, offset, ':')) {
            const auto s = read_digits(text, offset, 2);
            if (!s) {
                return std::nullopt;
            }
            second = *s;
            if (consume(text, offset, '.') || consume(text, offset, ',')) {
                // Only milliseconds are kept: further digits are skipped, which truncates
                // rather than rounds.
                std::size_t digits = 0;
                int fraction = 0;
                while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9') {
                    if (digits < 3) {
                        fraction = fraction * 10 + (text[offset] - '0');
                    }
                    fraction_nonzero = fraction_nonzero || text[offset] != '0';
                    ++digits;
                    ++offset;
                }
                if (digits == 0) {
                    return std::nullopt;
                }
                for (std::size_t i = digits; i < 3; ++i) {
                    fraction *= 10;
                }
                millisecond = fraction;
            }
        }
    }
    // Hour 24 is only the instant that ends the day: nothing may follow it.
    if (hour > 24 || minute > 59 || second > 60 ||
        (hour == 24 && (minute != 0 || second != 0 || fraction_nonzero))) {
        return std::nullopt;
    }

    minutes offset_from_utc{0};
    if (offset < text.size()) {
        if (consume(text, offset, 'Z')) {
            // Z designates UTC: the offset stays zero.
        } else if (text[offset] == '+' || text[offset] == '-') {
            const int sign = text[offset] == '-' ? -1 : 1;
            ++offset;
            const auto oh = read_digits(text, offset, 2);
            if (!oh) {
                return std::nullopt;
            }
            int om = 0;
            if (offset < text.size()) {
                // The colon is optional: both +hh:mm and +hhmm are accepted.
                consume(text, offset, ':');
                const auto parsed = read_digits(text, offset, 2);
                if (!parsed) {
                    return std::nullopt;
                }
                om = *parsed;
            }
            if (*oh > 23 || om > 59) {
                return std::nullopt;
            }
            offset_from_utc = minutes{sign * (*oh * 60 + om)};
        } else {
            return std::nullopt;
        }
    }
    if (offset != text.size()) {
        return std::nullopt;
    }

    const auto local = sys_days{date} + hours{hour} + minutes{minute} + seconds{second} +
                       milliseconds{millisecond};
    return system_clock::time_point{local - offset_from_utc};
}

std::string format_iso8601(std::chrono::system_clock::time_point time_utc) {
    using namespace std::chrono;
    const auto day_start = floor<days>(time_utc);
    const year_month_day date{day_start};
    const hh_mm_ss time_of_day{floor<milliseconds>(time_utc - day_start)};
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z", static_cast<int>(date.year()),
                       static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                       time_of_day.hours().count(), time_of_day.minutes().count(),
                       time_of_day.seconds().count(), time_of_day.subseconds().count());
}

}  // namespace nmeasim::core::time
