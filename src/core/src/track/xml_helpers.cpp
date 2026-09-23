#include "xml_helpers.hpp"

#include <cctype>
#include <format>
#include <locale>
#include <sstream>

namespace nmeasim::core::track::xml {

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

std::string child_text(const pugi::xml_node& parent, const char* local) {
    const auto node = child(parent, local);
    if (!node) {
        return {};
    }
    return std::string{trim(node.child_value())};
}

std::optional<double> parse_number(std::string_view text) {
    text = trim(text);
    if (text.empty()) {
        return std::nullopt;
    }
    std::istringstream stream{std::string{text}};
    stream.imbue(std::locale::classic());
    double value = 0.0;
    stream >> value;
    if (stream.fail() || !stream.eof()) {
        return std::nullopt;
    }
    return value;
}

bool load_document(pugi::xml_document& document, std::string_view xml, std::string* error) {
    const auto result = document.load_buffer(xml.data(), xml.size());
    if (!result) {
        if (error != nullptr) {
            *error =
                std::format("Invalid XML at offset {}: {}", result.offset, result.description());
        }
        return false;
    }
    return true;
}

}  // namespace nmeasim::core::track::xml
