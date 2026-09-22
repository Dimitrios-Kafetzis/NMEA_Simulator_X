#include "console_widget.hpp"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace nmeasim::app {

namespace {

constexpr int kMaxLines{2000};
constexpr int kFlushIntervalMs{150};

}  // namespace

ConsoleWidget::ConsoleWidget(QWidget* parent)
    : QWidget(parent),
      view_(new QPlainTextEdit(this)),
      pause_check_(new QCheckBox(tr("Pause"), this)),
      filter_edit_(new QLineEdit(this)) {
    view_->setReadOnly(true);
    view_->setMaximumBlockCount(kMaxLines);
    view_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    view_->setLineWrapMode(QPlainTextEdit::NoWrap);

    filter_edit_->setPlaceholderText(tr("Filter, e.g. RMC or $HC"));
    filter_edit_->setClearButtonEnabled(true);
    auto* clear_button = new QPushButton(tr("Clear"), this);
    connect(clear_button, &QPushButton::clicked, this, &ConsoleWidget::clear);

    auto* controls = new QHBoxLayout;
    controls->addWidget(pause_check_);
    controls->addWidget(filter_edit_, 1);
    controls->addWidget(clear_button);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(controls);
    layout->addWidget(view_, 1);

    flush_timer_.setInterval(kFlushIntervalMs);
    connect(&flush_timer_, &QTimer::timeout, this, &ConsoleWidget::flush);
}

void ConsoleWidget::append_sentence(const QString& id, const QString& text) {
    if (pause_check_->isChecked()) {
        return;
    }
    const QString filter = filter_edit_->text().trimmed();
    if (!filter.isEmpty() && !id.contains(filter, Qt::CaseInsensitive) &&
        !text.contains(filter, Qt::CaseInsensitive)) {
        return;
    }
    pending_.append(text);
    if (pending_.size() > kMaxLines) {
        pending_.remove(0, pending_.size() - kMaxLines);
    }
    if (!flush_timer_.isActive()) {
        flush_timer_.start();
    }
}

void ConsoleWidget::flush() {
    if (pending_.isEmpty()) {
        flush_timer_.stop();
        return;
    }
    const bool at_bottom =
        view_->verticalScrollBar()->value() >= view_->verticalScrollBar()->maximum() - 2;
    view_->appendPlainText(pending_.join(QLatin1Char('\n')));
    pending_.clear();
    if (at_bottom) {
        view_->verticalScrollBar()->setValue(view_->verticalScrollBar()->maximum());
    }
}

void ConsoleWidget::clear() {
    pending_.clear();
    view_->clear();
}

int ConsoleWidget::line_count() const {
    return view_->document()->isEmpty() ? 0 : view_->document()->blockCount();
}

bool ConsoleWidget::paused() const {
    return pause_check_->isChecked();
}

}  // namespace nmeasim::app
