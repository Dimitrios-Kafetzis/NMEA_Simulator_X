#pragma once

#include <QCheckBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTimer>
#include <QWidget>

namespace nmeasim::app {

/// Scrolling view of the sentences being sent. Lines are buffered and flushed a few times per
/// second so that high output rates never stall the interface.
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
    QCheckBox* pause_check_;
    QLineEdit* filter_edit_;
    QTimer flush_timer_;
    QStringList pending_;
};

}  // namespace nmeasim::app
