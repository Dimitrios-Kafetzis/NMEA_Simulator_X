// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `ConsoleWidget`: filtering, buffering and the timed flush into the view.

#include "console_widget.hpp"

#include "sentence_highlighter.hpp"
#include "theme/theme.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace nmeasim::app {

namespace {

/// Lines kept in the view and in the buffer. A user-interface choice that bounds the memory
/// of the view and the cost of re-colouring it after a theme change.
constexpr int kMaxLines{2000};
/// Interval between flushes into the view, in milliseconds. Batching lines this way keeps the
/// number of repaints near seven per second whatever the output rate.
constexpr int kFlushIntervalMs{150};

}  // namespace

ConsoleWidget::ConsoleWidget(QWidget* parent)
    : QWidget(parent),
      view_(new QPlainTextEdit(this)),
      pause_check_(new QCheckBox(tr("Pause"), this)),
      filter_edit_(new QLineEdit(this)) {
    view_->setObjectName(QStringLiteral("console_view"));
    view_->setReadOnly(true);
    view_->setMaximumBlockCount(kMaxLines);
    view_->setFont(theme::Theme::mono_font());
    view_->setLineWrapMode(QPlainTextEdit::NoWrap);
    highlighter_ = new SentenceHighlighter(view_->document());

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
    // The scroll bar of a QPlainTextEdit counts lines: within two lines of the bottom still
    // counts as at the bottom, so the view keeps following after a small scroll movement.
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
