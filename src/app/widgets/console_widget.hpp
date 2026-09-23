#pragma once

#include <QCheckBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTimer>
#include <QWidget>

namespace nmeasim::app {

class SentenceHighlighter;

/// Scrolling view of the sentences being sent, coloured like a protocol analyser. Lines are
/// buffered and flushed a few times per second so that high output rates never stall the
/// interface.
class ConsoleWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConsoleWidget(QWidget* parent = nullptr);

    void append_sentence(const QString& id, const QString& text);
    void clear();
    [[nodiscard]] int line_count() const;
    [[nodiscard]] bool paused() const;

private:
    void flush();

    QPlainTextEdit* view_;
    SentenceHighlighter* highlighter_{nullptr};
    QCheckBox* pause_check_;
    QLineEdit* filter_edit_;
    QTimer flush_timer_;
    QStringList pending_;
};

}  // namespace nmeasim::app
