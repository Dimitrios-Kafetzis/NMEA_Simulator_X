#pragma once

#include "app_settings.hpp"

#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/io/profile/profile.hpp>
#include <nmeasim/io/simulation_runner.hpp>

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QString>

namespace nmeasim::app {

class ConsoleWidget;
class DashboardWidget;
class OutputsWidget;

/// Top-level window: a dashboard in the centre, dockable console and outputs panels, and a
/// toolbar that controls the simulation and the profile.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Loads a profile from disk and makes it current. Returns false and shows the reason
    /// when it cannot be read.
    bool load_profile(const QString& path);
    void set_profile(const io::Profile& profile, const QString& path = {});
    [[nodiscard]] const io::Profile& profile() const noexcept { return profile_; }

    void start();
    void stop();
    [[nodiscard]] bool is_running() const;

    [[nodiscard]] io::SimulationRunner& runner() noexcept { return runner_; }
    [[nodiscard]] DashboardWidget* dashboard() const noexcept { return dashboard_; }
    [[nodiscard]] ConsoleWidget* console() const noexcept { return console_; }
    [[nodiscard]] OutputsWidget* outputs() const noexcept { return outputs_; }

signals:
    /// Raised for every problem the window reports to the operator, so that hosts and tests
    /// can observe errors without a dialog.
    void error_reported(const QString& title, const QString& message);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void build_actions();
    void build_docks();
    void report_error(const QString& title, const QString& message);
    void update_title();
    void update_actions();
    void refresh_status();
    void nudge(core::simulation::Parameter parameter, double delta);
    [[nodiscard]] core::simulation::DeltaSource* delta_source();

    void new_profile();
    void open_profile();
    void edit_settings();
    bool save_profile();
    bool save_profile_as();
    void toggle_run();
    void toggle_pause(bool paused);
    void toggle_steering(bool enabled);

    AppSettings settings_;
    io::Profile profile_{io::Profile::default_profile()};
    QString profile_path_;
    io::SimulationRunner runner_;

    DashboardWidget* dashboard_;
    ConsoleWidget* console_;
    OutputsWidget* outputs_;
    QLabel* status_label_;
    QLabel* counter_label_;

    QAction* new_action_{nullptr};
    QAction* open_action_{nullptr};
    QAction* save_action_{nullptr};
    QAction* save_as_action_{nullptr};
    QAction* settings_action_{nullptr};
    QAction* run_action_{nullptr};
    QAction* pause_action_{nullptr};
    QAction* steering_action_{nullptr};
    QAction* autostart_action_{nullptr};
};

}  // namespace nmeasim::app
