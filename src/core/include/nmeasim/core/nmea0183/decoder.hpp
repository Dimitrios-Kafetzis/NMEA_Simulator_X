// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Decoder that reads NMEA 0183 sentences back into a vessel state.
///
/// The decoder is the counterpart of the encoders declared in `encoders.hpp`. Log replay uses
/// it to rebuild the vessel state, and with it the dashboard and the map, from recorded
/// sentences (see simulation::ReplaySource); the log reader uses sentence_time() to time
/// recordings that carry no timestamps; the tests use it to cross-check the encoders.
///
/// parse_sentence() splits a line into its address and fields, and apply_sentence() writes
/// the values of a parsed sentence into a model::VesselState. The field parsers
/// parse_coordinate(), parse_time_of_day(), parse_date() and parse_number_field() are public
/// so that other readers can share them. The decoder accepts any talker, ignores unknown
/// formatters and never throws; a missing or malformed field leaves the corresponding value
/// unchanged.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/nmea0183/fields.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nmeasim::core::nmea0183 {

/// A sentence split into its start delimiter, address and data fields.
///
/// parse_sentence() produces it; the checksum has already been verified and is not kept.
struct ParsedSentence {
    /// Start delimiter: `$` for parametric sentences, `!` for encapsulated ones such as AIS.
    char delimiter{'$'};
    /// Talker identifier: `P` when the address starts with `P` (a proprietary sentence),
    /// otherwise the first two characters of the address.
    std::string talker;
    /// The rest of the address after the talker: three characters for a standard sentence
    /// such as `RMC`, the manufacturer code and sentence type for a proprietary one such as
    /// `GRME`. Never empty.
    std::string formatter;
    /// Data fields after the address, in order and untrimmed; empty strings for empty fields.
    ///
    /// The vector is empty when the address is not followed by a comma. A trailing comma
    /// yields a trailing empty field, so `$SDDPT,,,` has three fields.
    std::vector<std::string> fields;
    /// True when the sentence carried a `*hh` checksum, which then matched.
    bool has_checksum{false};

    /// Returns the data field at a position, tolerating short sentences.
    ///
    /// @param index Zero-based position among the data fields; field 0 is the first field
    ///     after the address.
    /// @return A view of the field, or an empty view when the sentence has `index` fields or
    ///     fewer. The view refers to `fields` and stays valid while this object lives and
    ///     `fields` is not modified.
    [[nodiscard]] std::string_view field(std::size_t index) const noexcept;
};

/// Splits a sentence into its start delimiter, address and data fields.
///
/// Surrounding whitespace, including the CR LF line terminator, is ignored. A checksum is
/// optional; when a `*` is present the checksum must match. The talker is `P` for an address
/// starting with `P` and two characters otherwise; the formatter is the rest of the address.
/// Field contents are not checked.
///
/// @param line One sentence, with or without checksum and line terminator.
/// @return The parsed sentence, or `std::nullopt` when the line does not start with `$` or
///     `!`, when its address is shorter than three characters or has no formatter after the
///     talker, or when it carries a checksum that does not match.
/// @see verify_checksum
[[nodiscard]] std::optional<ParsedSentence> parse_sentence(std::string_view line);

/// A UTC time carried by a sentence, with the date when the sentence carries one.
struct SentenceTime {
    /// Time of day, UTC, since midnight: below 24 h, except that a leap second sent as
    /// `235960` reaches 24 h.
    std::chrono::milliseconds since_midnight{0};
    /// The date, for sentences that carry one (RMC and ZDA) and only when it parsed;
    /// `std::nullopt` otherwise.
    ///
    /// The date always exists in the calendar: parse_date() checks the RMC date, and the ZDA
    /// day, month and year must be whole numbers of one or two, one or two and four digits
    /// that form a date that exists.
    std::optional<DateParts> date;
};

/// Returns the UTC time a sentence carries, when it carries a valid one.
///
/// The time of day is read from field 0 of RMC, GGA, ZDA, GNS, GST, GBS and GRS, and from
/// field 4 of GLL. The date comes from the `ddmmyy` field 8 of RMC, or from the day, month
/// and four-digit year in fields 1 to 3 of ZDA when they form a date that exists; otherwise
/// the time is returned without a date.
///
/// @param sentence A parsed sentence of any formatter; the talker is ignored.
/// @return The time, or `std::nullopt` for other formatters and when the time field is empty
///     or malformed.
/// @see NMEA 0183, sentences RMC, GGA, GLL, ZDA, GNS, GST, GBS and GRS.
[[nodiscard]] std::optional<SentenceTime> sentence_time(const ParsedSentence& sentence);

/// Applies the values carried by a parametric sentence to a vessel state.
///
/// Recognised formatters, whatever the talker: RMC, GGA, GLL, GSA, GSV, VTG, ZDA, HDT, HDG,
/// HDM, ROT, VHW, VBW, DPT, DBT, MTW, MWV, MWD, RSA, RMB, RPM and XDR, plus APB and XTE,
/// which are recognised but change nothing because RMB carries the same destination with its
/// position. Only the quantities a sentence carries are updated, and a field that is empty or
/// malformed leaves its value unchanged, so a state can be built up from a mixed stream.
///
/// A time field updates `state.time_utc`: RMC and ZDA with a valid date set date and time;
/// GGA, GLL, and RMC or ZDA without a valid date, set the time of day and keep the date of
/// the current `state.time_utc`.
///
/// @param sentence The parsed sentence to apply.
/// @param[in,out] state The state to update; values the sentence does not carry are kept.
/// @return True when the formatter is recognised, even if no field could be used; false for
///     an unknown formatter or an encapsulated (`!`) sentence, in which case `state` is
///     unchanged.
/// @note The date is not advanced when a time of day wraps past midnight: until a sentence
///     with a date arrives, a time just after midnight is placed on the previous date.
bool apply_sentence(const ParsedSentence& sentence, model::VesselState& state);

/// Parses a sentence and applies it to a vessel state in one call.
///
/// @param line One sentence as accepted by parse_sentence().
/// @param[in,out] state The state to update, as for the overload taking a ParsedSentence.
/// @return True when the line parsed and its formatter is recognised; false for a malformed
///     line, an unknown formatter or an encapsulated sentence, in which case `state` is
///     unchanged.
bool apply_sentence(std::string_view line, model::VesselState& state);

/// Parses a latitude or longitude field with its hemisphere letter into decimal degrees.
///
/// The value is `ddmm.mmmm` for a latitude or `dddmm.mmmm` for a longitude: the digits before
/// the last two integer digits are degrees, the rest are minutes. Any number of fractional
/// digits is accepted, and leading zeros of the degrees may be left out. The hemisphere
/// letter decides the sign, the valid range and the largest number of degree digits: two
/// for `N` and `S`, three for `E` and `W`.
///
/// @param value The coordinate field, unsigned digits with at most one full stop; surrounding
///     whitespace is ignored.
/// @param hemisphere Exactly one of `N`, `S`, `E` or `W`.
/// @return Decimal degrees, positive north or east and negative south or west; or
///     `std::nullopt` when either field is empty or malformed, when the value has a sign or
///     more degree digits than the hemisphere allows, when the minutes are 60 or more, or
///     when the result exceeds 90 degrees for `N` and `S` or 180 degrees for `E` and `W`.
[[nodiscard]] std::optional<double> parse_coordinate(std::string_view value,
                                                     std::string_view hemisphere);

/// Parses a UTC time of day in the form `hhmmss` or `hhmmss.s...` into milliseconds since
/// midnight.
///
/// Fractional seconds may have any number of digits; digits beyond the third are ignored
/// (truncated, not rounded).
///
/// @param value The time field; surrounding whitespace is ignored.
/// @return The time since midnight, or `std::nullopt` when the field is not six digits
///     optionally followed by a full stop and at least one digit, or when the hours exceed
///     23, the minutes 59 or the seconds 60 (60 is accepted for a leap second).
[[nodiscard]] std::optional<std::chrono::milliseconds> parse_time_of_day(std::string_view value);

/// Parses a `ddmmyy` date field, as sent in RMC.
///
/// Two-digit years below 80 belong to the 21st century (`25` is 2025), years from 80 to 99
/// to the 20th (`99` is 1999), which covers every date an NMEA 0183 recording can carry.
///
/// @param value The date field; surrounding whitespace is ignored.
/// @return The date with a four-digit year, or `std::nullopt` when the field is not exactly
///     six digits or the date does not exist: the month outside [1, 12], or the day outside
///     the days of that month, with 29 February only in a leap year.
[[nodiscard]] std::optional<DateParts> parse_date(std::string_view value);

/// Parses a decimal field independently of the process locale.
///
/// Accepts an optional `+` or `-` sign, digits and at most one full stop as the decimal
/// separator, with at least one digit in total (`.5` and `5.` are valid, `-` and `.` are
/// not). No exponent, thousands separator or decimal comma is accepted.
///
/// @param value The field; surrounding whitespace is ignored.
/// @return The value, or `std::nullopt` when the field is empty or not a plain decimal
///     number.
[[nodiscard]] std::optional<double> parse_number_field(std::string_view value);

}  // namespace nmeasim::core::nmea0183
