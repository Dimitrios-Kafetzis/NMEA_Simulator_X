// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `SentenceHighlighter` that colours the console, and `sentence_spans`, the pure
/// function that splits a console line into coloured runs.
///
/// @see docs/reference/desktop-app.md, section "Console".

#pragma once

#include <QString>
#include <QStringView>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <vector>

class QTextDocument;

namespace nmeasim::app {

/// The parts of a console line that are coloured differently.
///
/// Each role has its own colour in `theme::Colors`.
enum class SentenceRole {
    /// An IEC 61162-450 TAG block, from its opening backslash to its closing backslash
    /// inclusive. Coloured in `console_tag`.
    Tag,
    /// The `$` or `!` start delimiter and the talker identifier, for example `$GP` or `!AI`;
    /// only `$P` for a proprietary sentence. Coloured in `console_talker`.
    Talker,
    /// The sentence formatter, for example `RMC`, or the rest of a proprietary address after
    /// `$P`, for example `GRMZ`. Coloured in `console_formatter`, in bold.
    Formatter,
    /// A comma between fields. Coloured in `console_separator`.
    Separator,
    /// The `*` and the two checksum digits. Coloured in `console_checksum`.
    Checksum,
    /// A JSON message such as a Signal K delta, coloured as a whole in `console_json`.
    Json,
};

/// A run of characters in one role.
struct SentenceSpan {
    /// Index of the first character in the line, in UTF-16 code units, from 0.
    int start{0};
    /// Number of characters, in UTF-16 code units, at least 1 in a span that
    /// `sentence_spans` returns.
    int length{0};
    /// What the characters are.
    SentenceRole role{SentenceRole::Separator};

    /// Compares two spans member by member.
    ///
    /// @return True when start, length and role are all equal.
    bool operator==(const SentenceSpan&) const = default;
};

/// Splits a console line into coloured runs.
///
/// A line that starts with `{` is one `SentenceRole::Json` span; its JSON is not checked.
/// Otherwise an optional TAG block (from a leading backslash to the next one) comes first,
/// then an NMEA 0183 or AIS sentence starting with `$` or `!`. Its address runs to the first
/// comma, or to the first `*` when there is none, or to the end of the line; the first three
/// characters are the talker (two for `$P`, proprietary) and the rest the formatter. Every
/// comma after the address and before the last `*` is a separator. The last `*` followed by
/// two hexadecimal digits, of either case, is the checksum; its value is not verified, and a
/// missing or malformed checksum is simply not coloured. Field contents are not listed; they
/// keep the normal text colour.
///
/// The function is pure: it reads only `line`, in one pass, and allocates only the result,
/// so it is cheap enough for every line the console shows and can be tested without a
/// widget.
///
/// @param line One console line, without line terminator.
/// @return The spans in ascending order of `start`, never overlapping. Empty for an empty
///   line, a line with an unterminated TAG block and any line that is neither a sentence
///   nor JSON; a TAG block followed by something other than a sentence gives only the
///   `SentenceRole::Tag` span.
/// @see NMEA 0183 (IEC 61162-1), sentence structure; IEC 61162-450, TAG blocks.
[[nodiscard]] std::vector<SentenceSpan> sentence_spans(QStringView line);

/// Colours the console like a protocol analyser, using `sentence_spans` and the colours of
/// the current theme; it re-colours everything when the theme changes.
///
/// Each block of the document is one console line. `ConsoleWidget` creates one for its view.
class SentenceHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /// Attaches a highlighter to a document and takes the colours of the current theme.
    ///
    /// On every `theme::Theme::changed` it takes the new colours and re-colours the whole
    /// document, which costs one `sentence_spans` call per line.
    ///
    /// @param document Document to colour; it becomes the Qt parent and owner of the
    ///   highlighter.
    explicit SentenceHighlighter(QTextDocument* document);

protected:
    /// Colours one console line with the formats of its spans.
    ///
    /// Called by Qt for every block that is added or changed. Characters outside every span
    /// keep the normal text format.
    ///
    /// @param text Text of the block, one console line.
    void highlightBlock(const QString& text) override;

private:
    /// Sets the foreground of every format from the colours of the current theme, and bold
    /// for the formatter.
    void update_formats();

    /// Format of `SentenceRole::Tag` spans.
    QTextCharFormat tag_;
    /// Format of `SentenceRole::Talker` spans.
    QTextCharFormat talker_;
    /// Format of `SentenceRole::Formatter` spans, in bold.
    QTextCharFormat formatter_;
    /// Format of `SentenceRole::Separator` spans.
    QTextCharFormat separator_;
    /// Format of `SentenceRole::Checksum` spans.
    QTextCharFormat checksum_;
    /// Format of `SentenceRole::Json` spans.
    QTextCharFormat json_;
};

}  // namespace nmeasim::app
