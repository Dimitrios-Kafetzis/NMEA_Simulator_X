#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>
#include <nmeasim/core/simulation/custom_sentence.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace nmeasim::core::simulation {

namespace {

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

/// The body without delimiter, checksum and terminator, or an error.
struct Normalised {
    char delimiter{'$'};
    std::string body;
    std::optional<std::string> error;
};

Normalised normalise(std::string_view text) {
    Normalised result;
    text = trim(text);
    if (text.empty()) {
        result.error = "The sentence is empty";
        return result;
    }
    if (text.front() == nmea0183::kStartDelimiter ||
        text.front() == nmea0183::kEncapsulationDelimiter) {
        result.delimiter = text.front();
        text.remove_prefix(1);
    }
    const auto star = text.find(nmea0183::kChecksumDelimiter);
    if (star != std::string_view::npos) {
        const auto suffix = text.substr(star + 1);
        const bool looks_like_checksum =
            suffix.size() == 2 && std::all_of(suffix.begin(), suffix.end(), [](char c) {
                return std::isxdigit(static_cast<unsigned char>(c)) != 0;
            });
        if (!looks_like_checksum) {
            result.error = "'*' may only introduce a two-digit checksum at the end";
            return result;
        }
        text = text.substr(0, star);
    }
    for (const char c : text) {
        const bool printable = c >= ' ' && c <= '~';
        if (!printable || c == '$' || c == '!' || c == '\\' || c == '^' || c == '~') {
            result.error = "Only printable ASCII without $ ! \\ ^ ~ is allowed";
            return result;
        }
    }
    const auto comma = text.find(',');
    const auto address = text.substr(0, comma);
    if (address.size() < 3 || !std::all_of(address.begin(), address.end(), [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0;
        })) {
        result.error =
            "The sentence must start with an address of at least three letters "
            "or digits, such as PXYZ";
        return result;
    }
    result.body = std::string{text};
    if (result.body.size() + 4 > nmea0183::kMaxSentenceLengthWithoutTerminator) {
        result.error = "The sentence exceeds 82 characters with its checksum";
        return result;
    }
    return result;
}

}  // namespace

std::optional<std::string> validate_custom_sentence(std::string_view body) {
    return normalise(body).error;
}

std::optional<std::string> frame_custom_sentence(std::string_view body) {
    const auto normalised = normalise(body);
    if (normalised.error) {
        return std::nullopt;
    }
    return nmea0183::append_checksum(std::string{normalised.delimiter} + normalised.body);
}

}  // namespace nmeasim::core::simulation
