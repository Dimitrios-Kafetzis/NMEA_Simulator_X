// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The *Outputs* panel of the main window: one table row per output of the runner, with a
/// coloured status light and live counters.

#pragma once

#include <nmeasim/io/simulation_runner.hpp>

#include <QColor>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace nmeasim::app {

/// Returns the colour of an output state in the current theme: green open, amber opening, red
/// failed, grey closed.
///
/// @param state The transport state.
/// @return The theme's `ok`, `warning`, `danger` or `inactive` colour; `inactive` also for a
///   value outside the enumeration.
[[nodiscard]] QColor state_color(io::Transport::State state);

/// Table of the configured outputs with their live status.
///
/// Each row is one `io::OutputChannel` of the observed runner, in profile order, with the
/// columns *Output* (the transport description), *Status* (the state as `io::to_string`
/// names it, with a light in `state_color`), *Clients*, *Sentences* (lines handed to the
/// open transport), *Bytes* and *Last error*. The table is read-only and refreshes every
/// 500 ms while a runner is attached, when the runner starts or stops or reports an output
/// error, and when the theme changes.
class OutputsWidget : public QWidget {
    Q_OBJECT

public:
    /// Creates an empty table with no runner attached.
    ///
    /// @param parent Parent widget, which owns the panel; may be null, in which case the
    ///   caller owns it.
    explicit OutputsWidget(QWidget* parent = nullptr);

    /// Observes a runner. Pass nullptr to detach.
    ///
    /// Disconnects from the previous runner, connects to the new one's `started`, `stopped`
    /// and `output_error` signals, starts or stops the periodic refresh and refreshes at once.
    ///
    /// @param runner The runner to show; not owned. It must outlive the widget or be detached
    ///   before it is destroyed. Null empties the table.
    void set_runner(io::SimulationRunner* runner);
    /// Rebuilds the rows from the runner's outputs, or empties the table without a runner.
    ///
    /// Called by the timer and the connected signals; the main window also calls it after
    /// applying a profile, which replaces the outputs.
    void refresh();

    /// Returns the number of rows.
    ///
    /// @return One row per output as of the last `refresh`.
    [[nodiscard]] int row_count() const;
    /// Returns the text of the status column.
    ///
    /// @param row Row index, from 0.
    /// @return The state name, such as `open` or `closed`; empty when the row does not exist.
    [[nodiscard]] QString status_text(int row) const;
    /// Returns the colour of the status column.
    ///
    /// @param row Row index, from 0.
    /// @return The `state_color` of the state shown; an invalid `QColor` when the row does not
    ///   exist.
    [[nodiscard]] QColor status_color(int row) const;

private:
    /// The table, owned by this widget.
    QTableWidget* table_;
    /// Drives the periodic refresh while a runner is attached.
    QTimer refresh_timer_;
    /// The observed runner, not owned; null when detached.
    io::SimulationRunner* runner_{nullptr};
};

}  // namespace nmeasim::app
