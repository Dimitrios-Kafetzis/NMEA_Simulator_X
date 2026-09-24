// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// XML helpers shared by the GPX and KML readers, a private header of the `core` library.

#pragma once

#include <cstring>
#include <optional>
#include <pugixml.hpp>
#include <string>
#include <string_view>

/// Helpers on top of pugixml shared by the GPX and KML readers of `nmeasim::core::track`;
/// private to the `core` library.
///
/// Elements are matched on their local name, ignoring any namespace prefix, so that files
/// written with or without a prefix (`gpx:trkpt`, `trkpt`) parse the same way. Numbers are
/// parsed independently of the process locale.
namespace nmeasim::core::track::xml {

/// Returns an element name without its namespace prefix.
///
/// @param qualified A qualified name such as `gx:Track`.
/// @return A pointer into `qualified` just after its first `:`, or `qualified` itself when it
///         has none. It is valid as long as `qualified` is.
/// @pre `qualified` is not null.
inline const char* local_name(const char* qualified) noexcept {
    const char* colon = std::strchr(qualified, ':');
    return colon != nullptr ? colon + 1 : qualified;
}

/// Tells whether a node has a given local name, whatever its namespace prefix.
///
/// @param node The node to test; nodes that are not elements have an empty name.
/// @param local The local name to compare with, case-sensitively.
/// @return True when the local name of `node` equals `local`.
/// @pre `local` is not null.
inline bool is_named(const pugi::xml_node& node, const char* local) noexcept {
    return std::strcmp(local_name(node.name()), local) == 0;
}

/// Finds the first child element with a given local name.
///
/// Only direct children are searched.
///
/// @param parent The element whose children are searched.
/// @param local The local name to look for.
/// @return The first child element of `parent` whose local name is `local`, or an empty
///         (null) node when there is none. It refers into the document that owns `parent`.
/// @pre `local` is not null.
inline pugi::xml_node child(const pugi::xml_node& parent, const char* local) noexcept {
    for (const auto& node : parent.children()) {
        if (node.type() == pugi::node_element && is_named(node, local)) {
            return node;
        }
    }
    return {};
}

/// Returns the text of the first child element with a given local name.
///
/// @param parent The element whose children are searched.
/// @param local The local name of the child.
/// @return The child's first text or CDATA node with surrounding whitespace removed, as
///         pugixml's `child_value` returns it; empty when the child is absent or has no text.
/// @pre `local` is not null.
[[nodiscard]] std::string child_text(const pugi::xml_node& parent, const char* local);

/// Removes leading and trailing whitespace.
///
/// @param text The text to trim.
/// @return A view into `text` without the whitespace, as classified by `std::isspace` in the
///         current C locale, at either end; valid as long as the viewed characters are.
[[nodiscard]] std::string_view trim(std::string_view text) noexcept;

/// Parses a decimal number independently of the process locale.
///
/// The number is read with the classic locale, so the decimal separator is always `.`; a
/// sign and an exponent (`1.5e3`) are accepted.
///
/// @param text The text to parse; surrounding whitespace is ignored.
/// @return The value, or `std::nullopt` when `text` is empty, is not a number, has anything
///         after the number, or is out of the range of `double`.
[[nodiscard]] std::optional<double> parse_number(std::string_view text);

/// Parses XML text into a document and formats a one-line reason when it is not well formed.
///
/// @param[out] document Receives the parsed tree; its previous content is discarded.
/// @param xml The XML text. pugixml copies it, so it need not outlive the call; the encoding
///        is detected automatically.
/// @param error Receives `Invalid XML at offset N: DESCRIPTION` on failure, where `N` is the
///        byte offset of the error in `xml` and `DESCRIPTION` pugixml's text; left unchanged
///        on success. May be null.
/// @return True when the document was parsed, false when it is not well-formed XML,
///         including empty input, which has no document element.
[[nodiscard]] bool load_document(pugi::xml_document& document, std::string_view xml,
                                 std::string* error);

}  // namespace nmeasim::core::track::xml
