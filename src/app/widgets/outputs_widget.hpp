#pragma once

#include <nmeasim/io/simulation_runner.hpp>

#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace nmeasim::app {

/// Table of the configured outputs with their live status.
class OutputsWidget : public QWidget {
    Q_OBJECT

public:
    explicit OutputsWidget(QWidget* parent = nullptr);

    /// Observes a runner. Pass nullptr to detach.
    void set_runner(io::SimulationRunner* runner);
    void refresh();

    [[nodiscard]] int row_count() const;
    [[nodiscard]] QString status_text(int row) const;

private:
    QTableWidget* table_;
    QTimer refresh_timer_;
    io::SimulationRunner* runner_{nullptr};
};

}  // namespace nmeasim::app
