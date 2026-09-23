#pragma once

#include <cstring>
#include <optional>
#include <pugixml.hpp>
#include <string>
#include <string_view>

/// Small helpers shared by the GPX and KML readers. Namespace prefixes are ignored so that
/// files written with or without a prefix parse the same way.
namespace nmeasim::core::track::xml {

/// The element name without its namespace prefix.
inline const char* local_name(const char* qualified) noexcept {
    const char* colon = std::strchr(qualified, ':');
    return colon != nullptr ? colon + 1 : qualified;
}

inline bool is_named(const pugi::xml_node& node, const char* local) noexcept {
    return std::strcmp(local_name(node.name()), local) == 0;
}

/// First child element whose local name is `local`, or an empty node.
inline pugi::xml_node child(const pugi::xml_node& parent, const char* local) noexcept {
    for (const auto& node : parent.children()) {
        if (node.type() == pugi::node_element && is_named(node, local)) {
            return node;
        }
    }
    return {};
}

/// Text of the first child element named `local`, trimmed; empty when absent.
[[nodiscard]] std::string child_text(const pugi::xml_node& parent, const char* local);

/// Trims ASCII whitespace from both ends.
[[nodiscard]] std::string_view trim(std::string_view text) noexcept;

/// Parses a decimal number independently of the process locale. Rejects trailing garbage.
[[nodiscard]] std::optional<double> parse_number(std::string_view text);

/// Loads a document and formats a one-line reason when the XML is not well formed.
[[nodiscard]] bool load_document(pugi::xml_document& document, std::string_view xml,
                                 std::string* error);

}  // namespace nmeasim::core::track::xml
