#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/fields.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>

namespace nmeasim::core::nmea0183 {

SentenceBuilder::SentenceBuilder(std::string_view talker, std::string_view formatter,
                                 char delimiter) {
    body_.reserve(kMaxSentenceLengthWithoutTerminator);
    body_ += delimiter;
    body_ += talker;
    body_ += formatter;
}

SentenceBuilder& SentenceBuilder::field(std::string_view value) {
    body_ += ',';
    body_ += value;
    return *this;
}

SentenceBuilder& SentenceBuilder::field(char value) {
    body_ += ',';
    body_ += value;
    return *this;
}

SentenceBuilder& SentenceBuilder::field(double value, int decimals) {
    return field(format_fixed(value, decimals));
}

SentenceBuilder& SentenceBuilder::field(int value, int width) {
    return field(format_padded(value, width));
}

SentenceBuilder& SentenceBuilder::empty() {
    body_ += ',';
    return *this;
}

SentenceBuilder& SentenceBuilder::empty(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        empty();
    }
    return *this;
}

std::string SentenceBuilder::build() const {
    return append_checksum(body_);
}

bool SentenceBuilder::fits_limit() const {
    return body_.size() + 3 <= kMaxSentenceLengthWithoutTerminator;
}

bool fits_limit(std::string_view sentence) noexcept {
    return sentence.size() <= kMaxSentenceLengthWithoutTerminator;
}

}  // namespace nmeasim::core::nmea0183
