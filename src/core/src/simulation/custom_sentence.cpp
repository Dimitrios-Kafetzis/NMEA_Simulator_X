// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Validation and framing of operator-defined sentence bodies.
///
/// Both public functions share one normalisation pass, so that the reason
/// `validate_custom_sentence` gives and the refusal of `frame_custom_sentence` always agree.

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>
#include <nmeasim/core/simulation/custom_sentence.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace nmeasim::core::simulation {

namespace {

/// Removes leading and trailing white space, as classified by `std::isspace`.
///
/// This also removes a CR LF terminator pasted together with a sentence.
///
/// @param text The text to trim.
/// @return A view into `text` without the surrounding white space; empty when `text` is all
///   white space.
std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

/// Outcome of `normalise`: the parts of an acceptable body, or the reason it was refused.
struct Normalised {
    /// Start delimiter to frame with: the one typed, or `$` when none was typed.
    char delimiter{'$'};
    /// The address and fields without delimiter, checksum and terminator; empty when `error`
    /// is set, except for the length error, where it holds the rejected text.
    std::string body;
    /// Why the body is refused, as shown to the operator; `std::nullopt` when it is
    /// acceptable.
    std::optional<std::string> error;
};

/// Splits a typed body into delimiter and payload and checks it against the rules listed at
/// `validate_custom_sentence`.
///
/// @param text The body as typed by the operator.
/// @return The delimiter and payload, or an error message for the first rule `text` breaks.
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
    // The remaining characters NMEA 0183 reserves may not appear in a field; `*` and CR LF are
    // already gone, and the comma is the field separator.
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
    // Framing adds four characters: the start delimiter, the `*` and the two checksum digits.
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
