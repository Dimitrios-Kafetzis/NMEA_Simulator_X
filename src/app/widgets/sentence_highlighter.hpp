#pragma once

#include <QString>
#include <QStringView>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <vector>

class QTextDocument;

namespace nmeasim::app {

/// The parts of a console line that are coloured differently.
enum class SentenceRole {
    /// An IEC 61162-450 TAG block, backslash to backslash.
    Tag,
    /// The `$` or `!` start delimiter and the talker identifier, e.g. `$GP`.
    Talker,
    /// The sentence formatter, e.g. `RMC`, or a proprietary address after `$P`.
    Formatter,
    /// A comma between fields.
    Separator,
    /// The `*` and the two checksum digits.
    Checksum,
    /// A JSON message such as a Signal K delta, coloured as a whole.
    Json,
};

/// A run of characters in one role.
struct SentenceSpan {
    int start{0};
    int length{0};
    SentenceRole role{SentenceRole::Separator};

    bool operator==(const SentenceSpan&) const = default;
};

/// Splits a console line into coloured runs. Field contents are not listed; they keep the
/// normal text colour. Lines that are neither sentences nor JSON give no spans.
[[nodiscard]] std::vector<SentenceSpan> sentence_spans(QStringView line);

/// Colours the console like a protocol analyser, using `sentence_spans` and the colours of
/// the current theme; it re-colours everything when the theme changes.
class SentenceHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SentenceHighlighter(QTextDocument* document);

protected:
    void highlightBlock(const QString& text) override;

private:
    void update_formats();

    QTextCharFormat tag_;
    QTextCharFormat talker_;
    QTextCharFormat formatter_;
    QTextCharFormat separator_;
    QTextCharFormat checksum_;
    QTextCharFormat json_;
};

}  // namespace nmeasim::app
