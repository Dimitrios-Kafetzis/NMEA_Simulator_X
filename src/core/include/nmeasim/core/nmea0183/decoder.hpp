#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/nmea0183/fields.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Reading NMEA 0183 sentences back into a vessel state.
///
/// The decoder is the counterpart of the encoders: it is used to rebuild the dashboard state
/// while a log is replayed and to cross-check what the encoders produce. It accepts any
/// talker, ignores unknown formatters and never throws; a malformed field simply leaves the
/// corresponding value unchanged.
namespace nmeasim::core::nmea0183 {

/// A sentence split into its address and fields.
struct ParsedSentence {
    /// `$` for parametric sentences, `!` for encapsulated ones.
    char delimiter{'$'};
    /// Two characters for standard talkers, `P` for proprietary sentences.
    std::string talker;
    /// Three characters for standard sentences, the rest of the address otherwise.
    std::string formatter;
    /// Data fields after the address, in order, possibly empty.
    std::vector<std::string> fields;
    /// True when the sentence carried a checksum.
    bool has_checksum{false};

    /// The field at `index`, or an empty view when the sentence has fewer fields.
    [[nodiscard]] std::string_view field(std::size_t index) const noexcept;
};

/// Splits a sentence into its parts. Returns nullopt when the line does not start with `$`
/// or `!`, has no formatter, or carries a checksum that does not match. A missing checksum
/// is accepted. Line terminators and surrounding whitespace are ignored.
[[nodiscard]] std::optional<ParsedSentence> parse_sentence(std::string_view line);

/// A time carried by a sentence.
struct SentenceTime {
    /// Time of day, UTC.
    std::chrono::milliseconds since_midnight{0};
    /// The date when the sentence carries one (RMC, ZDA).
    std::optional<DateParts> date;
};

/// The UTC time a sentence carries (RMC, GGA, GLL, ZDA, GNS, GST, GBS, GRS), when valid.
[[nodiscard]] std::optional<SentenceTime> sentence_time(const ParsedSentence& sentence);

/// Applies the values carried by a sentence to `state`. Returns true when the formatter is
/// known and the sentence was applied, false for unknown formatters. Time fields update
/// `state.time_utc`, keeping the current date when the sentence only carries a time of day.
bool apply_sentence(const ParsedSentence& sentence, model::VesselState& state);

/// Parses and applies a sentence in one call. Malformed lines return false.
bool apply_sentence(std::string_view line, model::VesselState& state);

/// Parses `ddmm.mmmm` / `dddmm.mmmm` with its hemisphere letter into decimal degrees.
[[nodiscard]] std::optional<double> parse_coordinate(std::string_view value,
                                                     std::string_view hemisphere);

/// Parses `hhmmss[.ss]` into milliseconds since midnight.
[[nodiscard]] std::optional<std::chrono::milliseconds> parse_time_of_day(std::string_view value);

/// Parses `ddmmyy` into a date; two-digit years below 80 belong to the 21st century.
[[nodiscard]] std::optional<DateParts> parse_date(std::string_view value);

/// Parses a decimal field independently of the process locale. Empty fields yield nullopt.
[[nodiscard]] std::optional<double> parse_number_field(std::string_view value);

}  // namespace nmeasim::core::nmea0183
