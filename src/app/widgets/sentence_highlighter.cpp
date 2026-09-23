#include "sentence_highlighter.hpp"

#include "theme/theme.hpp"

#include <QFont>
#include <QTextDocument>

#include <algorithm>

namespace nmeasim::app {

namespace {

bool is_hex(QChar c) {
    return c.isDigit() || (c >= QLatin1Char('A') && c <= QLatin1Char('F')) ||
           (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
}

}  // namespace

std::vector<SentenceSpan> sentence_spans(QStringView line) {
    std::vector<SentenceSpan> spans;
    const auto size = static_cast<int>(line.size());
    if (size == 0) {
        return spans;
    }
    if (line.front() == QLatin1Char('{')) {
        spans.push_back({0, size, SentenceRole::Json});
        return spans;
    }
    int position = 0;
    if (line.front() == QLatin1Char('\\')) {
        const auto end = line.indexOf(QLatin1Char('\\'), 1);
        if (end < 0) {
            return spans;
        }
        spans.push_back({0, static_cast<int>(end) + 1, SentenceRole::Tag});
        position = static_cast<int>(end) + 1;
    }
    if (position >= size ||
        (line[position] != QLatin1Char('$') && line[position] != QLatin1Char('!'))) {
        return spans;
    }
    // The address runs from the delimiter to the first comma (or the checksum).
    auto address_end = line.indexOf(QLatin1Char(','), position);
    if (address_end < 0) {
        address_end = line.indexOf(QLatin1Char('*'), position);
    }
    const int address_stop = address_end < 0 ? size : static_cast<int>(address_end);
    const bool proprietary = position + 1 < size && line[position + 1] == QLatin1Char('P');
    const int talker_length = std::min(proprietary ? 2 : 3, address_stop - position);
    spans.push_back({position, talker_length, SentenceRole::Talker});
    if (address_stop > position + talker_length) {
        spans.push_back({position + talker_length, address_stop - position - talker_length,
                         SentenceRole::Formatter});
    }
    const auto star = line.lastIndexOf(QLatin1Char('*'));
    const int fields_stop = star > address_stop ? static_cast<int>(star) : size;
    for (int index = address_stop; index < fields_stop; ++index) {
        if (line[index] == QLatin1Char(',')) {
            spans.push_back({index, 1, SentenceRole::Separator});
        }
    }
    if (star >= 0 && star + 2 < size && is_hex(line[star + 1]) && is_hex(line[star + 2])) {
        spans.push_back({static_cast<int>(star), 3, SentenceRole::Checksum});
    }
    return spans;
}

SentenceHighlighter::SentenceHighlighter(QTextDocument* document) : QSyntaxHighlighter(document) {
    update_formats();
    connect(&theme::Theme::instance(), &theme::Theme::changed, this, [this] {
        update_formats();
        rehighlight();
    });
}

void SentenceHighlighter::update_formats() {
    const auto& colors = theme::Theme::instance().colors();
    tag_.setForeground(colors.console_tag);
    talker_.setForeground(colors.console_talker);
    formatter_.setForeground(colors.console_formatter);
    formatter_.setFontWeight(QFont::Bold);
    separator_.setForeground(colors.console_separator);
    checksum_.setForeground(colors.console_checksum);
    json_.setForeground(colors.console_json);
}

void SentenceHighlighter::highlightBlock(const QString& text) {
    for (const auto& span : sentence_spans(text)) {
        const QTextCharFormat* format = &separator_;
        switch (span.role) {
            case SentenceRole::Tag:
                format = &tag_;
                break;
            case SentenceRole::Talker:
                format = &talker_;
                break;
            case SentenceRole::Formatter:
                format = &formatter_;
                break;
            case SentenceRole::Separator:
                format = &separator_;
                break;
            case SentenceRole::Checksum:
                format = &checksum_;
                break;
            case SentenceRole::Json:
                format = &json_;
                break;
        }
        setFormat(span.start, span.length, *format);
    }
}

}  // namespace nmeasim::app
