#pragma once

#include "app_settings.hpp"

#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/io/profile/profile.hpp>
#include <nmeasim/io/simulation_runner.hpp>

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QSlider>
#include <QString>

#include <chrono>

namespace nmeasim::app {

class ConsoleWidget;
class DashboardWidget;
class OutputsWidget;
namespace map {
class MapWidget;
class TileCache;
}  // namespace map

/// Top-level window: a dashboard in the centre, dockable console, outputs and map panels, and
/// a toolbar that controls the profile, the simulation and the transport (pause, step, seek).
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Loads a profile from disk and makes it current. Returns false and shows the reason
    /// when it cannot be read.
    bool load_profile(const QString& path);
    /// Makes `profile` current and applies it to the runner. Returns false and shows the
    /// reason when it cannot be applied; the previous profile then stays in place.
    bool set_profile(const io::Profile& profile, const QString& path = {});
    [[nodiscard]] const io::Profile& profile() const noexcept { return profile_; }

    /// Switches the current profile to track mode with this GPX or KML file.
    bool load_track(const QString& path);
    /// Switches the current profile to replay mode with this log file.
    bool load_log(const QString& path);
    /// Records every emitted sentence to a log file; an empty path stops recording.
    bool set_recording(const QString& path);

    void start();
    void stop();
    [[nodiscard]] bool is_running() const;

    [[nodiscard]] io::SimulationRunner& runner() noexcept { return runner_; }
    [[nodiscard]] DashboardWidget* dashboard() const noexcept { return dashboard_; }
    [[nodiscard]] ConsoleWidget* console() const noexcept { return console_; }
    [[nodiscard]] OutputsWidget* outputs() const noexcept { return outputs_; }
    [[nodiscard]] map::MapWidget* map_view() const noexcept { return map_; }
    [[nodiscard]] QAction* step_action() const noexcept { return step_action_; }
    [[nodiscard]] QAction* record_action() const noexcept { return record_action_; }
    [[nodiscard]] QSlider* seek_slider() const noexcept { return seek_slider_; }
    [[nodiscard]] QLabel* position_label() const noexcept { return position_label_; }

    /// Formats a duration as `mm:ss` or `h:mm:ss`.
    [[nodiscard]] static QString format_duration(std::chrono::milliseconds duration);

    /// Moves the vessel to a position, both in the running simulation and in the profile
    /// seed so that saving keeps it.
    void move_vessel(core::geo::Position position);

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
    /// Refreshes the dashboard and the map from the current vessel state.
    void refresh_view();
    /// Refreshes the seek slider and the position label from the runner.
    void refresh_transport();
    void nudge(core::simulation::Parameter parameter, double delta);
    [[nodiscard]] core::simulation::DeltaSource* delta_source();

    void new_profile();
    void open_profile();
    void open_track();
    void open_log();
    void edit_settings();
    bool save_profile();
    bool save_profile_as();
    void toggle_run();
    void toggle_pause(bool paused);
    void toggle_steering(bool enabled);
    void toggle_recording(bool checked);
    void seek_from_slider(int value);

    AppSettings settings_;
    io::Profile profile_{io::Profile::default_profile()};
    QString profile_path_;
    io::SimulationRunner runner_;

    map::TileCache* tile_cache_;
    DashboardWidget* dashboard_;
    ConsoleWidget* console_;
    OutputsWidget* outputs_;
    map::MapWidget* map_;
    QLabel* status_label_;
    QLabel* counter_label_;
    QSlider* seek_slider_;
    QLabel* position_label_;
    bool updating_slider_{false};

    QAction* new_action_{nullptr};
    QAction* open_action_{nullptr};
    QAction* open_track_action_{nullptr};
    QAction* open_log_action_{nullptr};
    QAction* record_action_{nullptr};
    QAction* step_action_{nullptr};
    QAction* save_action_{nullptr};
    QAction* save_as_action_{nullptr};
    QAction* settings_action_{nullptr};
    QAction* run_action_{nullptr};
    QAction* pause_action_{nullptr};
    QAction* steering_action_{nullptr};
    QAction* autostart_action_{nullptr};
    QAction* follow_action_{nullptr};
    QAction* online_tiles_action_{nullptr};
};

}  // namespace nmeasim::app
