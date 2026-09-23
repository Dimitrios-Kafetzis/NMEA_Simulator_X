#pragma once

#include <cstddef>
#include <string>
#include <string_view>

/// Assembles one NMEA 0183 sentence field by field and frames it with a checksum.
namespace nmeasim::core::nmea0183 {

/// Maximum length of a sentence excluding the terminating CR LF.
inline constexpr std::size_t kMaxSentenceLengthWithoutTerminator{80};

/// Builder for one sentence: fields are appended in order, separated by commas.
class SentenceBuilder {
public:
    /// Starts a sentence such as `$GPRMC`. `talker` is two characters, `formatter` three.
    SentenceBuilder(std::string_view talker, std::string_view formatter, char delimiter = '$');

    /// Appends a text field verbatim.
    SentenceBuilder& field(std::string_view value);
    /// Appends a single-character field.
    SentenceBuilder& field(char value);
    /// Appends a number with a fixed number of decimals.
    SentenceBuilder& field(double value, int decimals);
    /// Appends an integer, optionally zero-padded to `width`.
    SentenceBuilder& field(int value, int width = 0);
    /// Appends an empty field, meaning "no data".
    SentenceBuilder& empty();
    /// Appends `count` empty fields.
    SentenceBuilder& empty(std::size_t count);

    /// Returns the framed sentence with checksum and without line terminator.
    [[nodiscard]] std::string build() const;

    /// True when the framed sentence fits the NMEA 0183 length limit.
    [[nodiscard]] bool fits_limit() const;

private:
    std::string body_;
};

/// True when `sentence` (without terminator) fits the NMEA 0183 length limit.
[[nodiscard]] bool fits_limit(std::string_view sentence) noexcept;

}  // namespace nmeasim::core::nmea0183
