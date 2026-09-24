// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `ConsoleWidget` dock panel that shows the sentences and messages being sent.
///
/// @see docs/reference/desktop-app.md, section "Console".

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
///
/// Above the read-only view sit a *Pause* check box, a filter field and a *Clear* button.
/// The view keeps the most recent 2000 lines, drops older ones, does not wrap lines and uses
/// `theme::Theme::mono_font`; a `SentenceHighlighter` colours it. New lines are added every
/// 150 ms while sentences arrive. The view follows new lines only while it is scrolled to the
/// bottom, so the operator can scroll back and read without being pulled down.
class ConsoleWidget : public QWidget {
    Q_OBJECT

public:
    /// Creates an empty, unpaused console with an empty filter.
    ///
    /// @param parent Qt parent that owns the widget; null leaves ownership to the caller.
    explicit ConsoleWidget(QWidget* parent = nullptr);

    /// Queues one sentence or message for display, unless it is paused or filtered out.
    ///
    /// Does nothing while *Pause* is ticked: those sentences are dropped, not kept for later.
    /// When the filter field (trimmed) is not empty, the line is kept only if `id` or `text`
    /// contains it, ignoring case; changing the filter affects new lines only. A kept line
    /// waits in a buffer that holds at most 2000 lines, dropping the oldest, and appears at
    /// the next flush, within 150 ms. Connected to `io::SimulationRunner::sentence_emitted`.
    ///
    /// @param id Registry or custom sentence id such as `RMC`, or `SIGNALK` or `VIEWSYNC` for
    ///   a state message; used only by the filter.
    /// @param text The line to show, without line terminator.
    void append_sentence(const QString& id, const QString& text);
    /// Removes every line shown and every line still waiting to be shown.
    ///
    /// Leaves *Pause* and the filter as they are. Connected to the *Clear* button.
    void clear();
    /// Returns the number of lines shown in the view.
    ///
    /// @return From 0 to 2000; lines still waiting for the next flush are not counted.
    [[nodiscard]] int line_count() const;
    /// Returns whether the operator ticked *Pause*.
    ///
    /// @return True while new sentences are dropped instead of shown.
    [[nodiscard]] bool paused() const;

private:
    /// Appends the waiting lines to the view in one edit, then empties the buffer.
    ///
    /// Scrolls to the new bottom only when the view was at the bottom before. Stops the
    /// timer when nothing was waiting, so an idle console costs nothing. Called by
    /// `flush_timer_`.
    void flush();

    /// The read-only text view; owned by this widget through the Qt parent.
    QPlainTextEdit* view_;
    /// Highlighter of the document of `view_`, which owns it.
    SentenceHighlighter* highlighter_{nullptr};
    /// The *Pause* check box; owned by this widget.
    QCheckBox* pause_check_;
    /// The filter field; owned by this widget.
    QLineEdit* filter_edit_;
    /// Fires every 150 ms while lines are waiting; stopped when the console is idle.
    QTimer flush_timer_;
    /// Lines waiting for the next flush, oldest first; at most 2000.
    QStringList pending_;
};

}  // namespace nmeasim::app
