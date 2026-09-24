// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `MainWindow`, the top-level window of the desktop application.
///
/// The window hosts a `io::SimulationRunner` and shows its state in a dashboard, a map, a
/// sentence console and an outputs table; its menus, toolbar and keyboard shortcuts are the
/// only entry points that change the profile or the run, as ADR 0009 decides.
///
/// @see docs/adr/0009-desktop-shell.md
/// @see docs/reference/desktop-app.md

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
class StatusLed;
namespace map {
class MapWidget;
class TileCache;
}  // namespace map

/// Top-level window: a dashboard in the centre, dockable console, outputs and map panels, and
/// a toolbar that controls the profile, the simulation and the transport (pause, step, seek).
///
/// Layout: the `DashboardWidget` is the central widget; the `map::MapWidget` docks on the
/// left, the `OutputsWidget` on the right and the `ConsoleWidget` at the bottom, each in a
/// `QDockWidget` the operator can move, float or hide. The status bar holds the run and
/// recording lights, the profile name and mode, the outputs light and the sentence counter.
/// The geometry and dock layout are restored from `AppSettings` at construction and saved in
/// `closeEvent`.
///
/// Menus, in the order they appear in the menu bar (the Ctrl shortcuts use Cmd on macOS):
///
/// - *File*: New profile (Ctrl+N), Open profile... (Ctrl+O), Save profile (Ctrl+S), Save
///   profile as... (Ctrl+Shift+S), Open track... (Ctrl+T), Open log for replay... (Ctrl+L),
///   Record log... (Ctrl+R), Settings... (Ctrl+comma) and Quit (Ctrl+Q).
/// - *Simulation*: Start or Stop (F5), Pause (F6), Step (F7), Steering mode, Clear
///   destination and Start automatically on launch.
/// - *Help*: About.
/// - *View*: the Map, Console and Outputs panels, Follow vessel on the map (Home), Download
///   map tiles, Clear map tile cache and the Theme submenu.
///
/// The toolbar repeats Open, Save, Settings, Start, Pause, Step, Steering mode and Record, and
/// ends with the seek slider and the position label. The arrow keys nudge the vessel; see
/// `keyPressEvent`.
///
/// Profile: the window keeps its own copy of the current profile, which `move_vessel` and
/// `set_destination` edit so that saving keeps those changes, and applies it to the runner
/// whenever it is replaced. A profile that the runner rejects leaves the previous one in
/// place.
///
/// Ownership: the window owns the runner and the settings by value. The tile cache, the
/// dashboard, the console, the outputs table, the map, the status bar widgets, the docks and
/// every action have the window as Qt parent and are deleted with it; the pointers returned by
/// the accessors stay valid for the window's lifetime.
///
/// Threads: the window, its widgets and its runner all live on the GUI thread; every signal
/// connected here is delivered there directly.
///
/// Errors meant for the operator are shown in the status bar for ten seconds, emitted as
/// `error_reported` and, while the window is visible, shown in a message box, so that tests
/// running offscreen never block on a dialog.
///
/// @see docs/reference/desktop-app.md
/// @see docs/adr/0009-desktop-shell.md
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// Builds the window, restores its layout and applies the built-in default profile.
    ///
    /// Reads the map preferences (tiles online, tile URL, attribution, zoom) and the autostart flag
    /// from `AppSettings`, builds the actions, menus, toolbar, docks and status bar, and connects
    /// the map, dashboard, theme and runner signals. On a first start, when no dock layout was
    /// saved, it gives the map about 400 pixels, the outputs about 260 and the console about 170.
    /// The simulation is not started.
    ///
    /// @param parent Qt parent that owns the window; null for a top-level window, as `main`
    ///   creates it.
    explicit MainWindow(QWidget* parent = nullptr);
    /// Destroys the window, stopping the simulation and closing its outputs first.
    ~MainWindow() override;

    /// Loads a profile file and makes it current.
    ///
    /// Stops a running simulation, applies the profile and starts it again, as `set_profile`
    /// does, and remembers `path` as the profile to reopen at the next start.
    ///
    /// @param path Profile file in the JSON format of docs/reference/profile.md.
    /// @return False when the file cannot be read or parsed, or when `set_profile` rejects
    ///   the profile (for example a track file that is missing); the reason is reported (see
    ///   `error_reported`) and the current profile stays. True when the profile is current.
    bool load_profile(const QString& path);
    /// Opens the profile to start with; called once by `main` after the window is shown.
    ///
    /// Loads `requested` with `load_profile` when it is not empty. When it is empty, or it
    /// cannot be loaded (which `load_profile` reports), the profile stored under
    /// `profile/last_path` is loaded instead, provided that file exists. Otherwise the
    /// built-in default profile stays.
    ///
    /// @param requested Profile path given on the command line; empty when none was given.
    void open_initial_profile(const QString& requested);
    /// Writes the current profile to a file and makes that file the profile's file.
    ///
    /// Only after a successful write does `path` become the file *Save profile* writes to,
    /// the one shown in the title and the profile to reopen at the next start; the saved path
    /// is shown in the status bar for three seconds. A failed write is reported (see
    /// `error_reported`) and changes nothing.
    ///
    /// @param path File to write, in the JSON format of docs/reference/profile.md.
    /// @return True when the profile was written; false when the write failed.
    bool save_profile_to(const QString& path);
    /// Returns the file the current profile was loaded from or last saved to.
    ///
    /// @return The path *Save profile* writes to; empty when the profile has no file yet.
    [[nodiscard]] const QString& profile_path() const noexcept { return profile_path_; }
    /// Makes a profile current and applies it to the runner.
    ///
    /// A running simulation is stopped, the profile applied and the run started again. On
    /// success the map's sailed track is cleared, the route of a track is drawn, the map is
    /// centred on the vessel, the dashboard overrides and *Steering mode* are enabled only in
    /// delta mode (in track and replay mode *Steering mode* is also unticked), and the title, the
    /// transport controls and the outputs table are refreshed.
    ///
    /// @param profile The profile to use; copied.
    /// @param path File the profile came from and *Save profile* writes to. Empty means the
    ///   profile has no file yet: the title shows the profile name and *Save profile* asks for
    ///   a file name. A non-empty path is also stored as the profile to reopen at the next
    ///   start.
    /// @return False when the runner cannot apply the profile (for example a track or log file
    ///   that cannot be read); the reason is reported (see `error_reported`), the previous
    ///   profile stays current and a run that was going is restarted with it. True otherwise.
    bool set_profile(const io::Profile& profile, const QString& path = {});
    /// Returns the current profile, including changes made on the map.
    ///
    /// @return The window's copy, valid until the next change of profile; it can differ from
    ///   `io::SimulationRunner::profile` by the start position and destination that
    ///   `move_vessel`, `set_destination` and `clear_destination` write into its seed.
    [[nodiscard]] const io::Profile& profile() const noexcept { return profile_; }

    /// Switches the current profile to track mode with a track file.
    ///
    /// Copies the current profile, sets its mode to `io::SimulationMode::Track` and its track
    /// path, and applies it with `set_profile`, keeping the current profile file path.
    ///
    /// @param path GPX 1.1 or KML 2.2 file, as docs/reference/track-files.md describes.
    /// @return The result of `set_profile`: false, with the reason reported and the previous
    ///   profile kept, when the file cannot be read or parsed.
    bool load_track(const QString& path);
    /// Switches the current profile to replay mode with a log file.
    ///
    /// Copies the current profile, sets its mode to `io::SimulationMode::Replay` and its replay
    /// path, and applies it with `set_profile`, keeping the current profile file path.
    ///
    /// @param path Recorded or plain NMEA log, as docs/reference/log-format.md describes.
    /// @return The result of `set_profile`: false, with the reason reported and the previous
    ///   profile kept, when the file cannot be read.
    bool load_log(const QString& path);
    /// Records every emitted sentence to a log file.
    ///
    /// Passes the path to `io::SimulationRunner::set_recording`; the runner's
    /// `recording_changed` signal then ticks or unticks *Record log...* and updates the
    /// status bar. While stopped the file opens only at the next `start`, so a bad path is
    /// reported by the runner then, in the status bar.
    ///
    /// @param path Log file, truncated when the recording opens; empty stops recording.
    /// @return False when the file was opened at once and that failed: the error is reported
    ///   (see `error_reported`) and the recording is cleared. True otherwise.
    bool set_recording(const QString& path);

    /// Opens the outputs and starts ticking, unless the simulation is already running.
    ///
    /// Clears the console, starts the runner, carries the *Steering mode* state into a delta
    /// source, updates the dashboard's override controls and gives the window keyboard focus
    /// so that the arrow keys steer at once. The toolbar and status bar follow through the
    /// runner's `started` signal.
    void start();
    /// Stops ticking and closes every output; the simulation keeps its state.
    ///
    /// Also unticks *Pause*. Safe to call when not running.
    void stop();
    /// Returns whether the simulation is running.
    ///
    /// @return True between `start` and `stop`, paused or not.
    [[nodiscard]] bool is_running() const;

    /// Returns the runner the window drives, for tests and for the outputs table.
    ///
    /// @return The runner owned by the window, valid for the window's lifetime.
    [[nodiscard]] io::SimulationRunner& runner() noexcept { return runner_; }
    /// Returns the central dashboard.
    ///
    /// @return The dashboard, owned by the window; never null.
    [[nodiscard]] DashboardWidget* dashboard() const noexcept { return dashboard_; }
    /// Returns the sentence console of the *Console* dock.
    ///
    /// @return The console, owned by the window; never null.
    [[nodiscard]] ConsoleWidget* console() const noexcept { return console_; }
    /// Returns the outputs table of the *Outputs* dock.
    ///
    /// @return The outputs table, owned by the window; never null.
    [[nodiscard]] OutputsWidget* outputs() const noexcept { return outputs_; }
    /// Returns the map of the *Map* dock.
    ///
    /// @return The map, owned by the window; never null.
    [[nodiscard]] map::MapWidget* map_view() const noexcept { return map_; }
    /// Returns the *Step* action (F7).
    ///
    /// @return The action, owned by the window; never null after construction.
    [[nodiscard]] QAction* step_action() const noexcept { return step_action_; }
    /// Returns the checkable *Record log...* action (Ctrl+R).
    ///
    /// @return The action, owned by the window; never null after construction.
    [[nodiscard]] QAction* record_action() const noexcept { return record_action_; }
    /// Returns the checkable *Steering mode* action.
    ///
    /// @return The action, owned by the window; never null after construction. It is enabled
    ///   in delta mode only.
    [[nodiscard]] QAction* steering_action() const noexcept { return steering_action_; }
    /// Returns the seek slider at the end of the toolbar.
    ///
    /// @return The slider, owned by the window; never null. Its range is the length of the
    ///   track or log in milliseconds; it is disabled with an empty range for the delta
    ///   simulation.
    [[nodiscard]] QSlider* seek_slider() const noexcept { return seek_slider_; }
    /// Returns the label next to the seek slider.
    ///
    /// @return The label, owned by the window; never null. It shows the elapsed and total time
    ///   as `03:00 / 12:00`, and is empty for the delta simulation.
    [[nodiscard]] QLabel* position_label() const noexcept { return position_label_; }
    /// Returns the run state light at the left of the status bar.
    ///
    /// @return The light, owned by the window; green *RUNNING*, amber *PAUSED* or unlit
    ///   *STOPPED*. Null only during construction.
    [[nodiscard]] StatusLed* run_led() const noexcept { return run_led_; }
    /// Returns the recording light of the status bar.
    ///
    /// @return The light, owned by the window: a red *REC*, visible only while recording and
    ///   blinking while the run is going and not paused. Null only during construction.
    [[nodiscard]] StatusLed* recording_led() const noexcept { return recording_led_; }
    /// Returns the outputs light at the right of the status bar.
    ///
    /// @return The light, owned by the window, captioned *n/m OUTPUTS*: while running, green
    ///   when every output is open, red when one failed and amber otherwise; unlit when
    ///   stopped or without outputs. Null only during construction.
    [[nodiscard]] StatusLed* outputs_led() const noexcept { return outputs_led_; }
    /// Returns the *View → Theme* entries.
    ///
    /// @return Three checkable actions in one exclusive group, in the order *Follow the
    ///   system*, *Night bridge (dark)* and *Daylight (light)*; each carries its stored name
    ///   (`system`, `night`, `day`) as data. Owned by the window.
    [[nodiscard]] QList<QAction*> theme_actions() const { return theme_actions_; }

    /// Returns the rich text of the About dialog.
    ///
    /// @return The product name with `core::version_description`, which for a development
    ///   build includes the `git describe` string of the commit, a one-line description and a
    ///   link to the project repository.
    [[nodiscard]] static QString about_text();

    /// Formats a duration for the position label.
    ///
    /// @param duration The duration; negative values are shown as zero, and the fraction of a
    ///   second is dropped.
    /// @return `mm:ss` below one hour (`01:05`), `h:mm:ss` from one hour (`3:07:09`).
    [[nodiscard]] static QString format_duration(std::chrono::milliseconds duration);

    /// Moves the vessel to a position, both in the running simulation and in the profile seed
    /// so that saving keeps it.
    ///
    /// A destination's leg restarts at the new position, the sailed track on the map starts a
    /// new segment instead of drawing a line from the old place, and the map recentres. Shows
    /// the new position in the status bar for three seconds. Connected to
    /// `map::MapWidget::position_picked`.
    ///
    /// @param position The new position, latitude in [-90, 90] and longitude in [-180, 180].
    void move_vessel(core::geo::Position position);
    /// Steers for a waypoint at a position; the leg starts where the vessel is now.
    ///
    /// The destination is applied to the running source and kept in the profile seed. Its
    /// arrival radius is taken from the seed's previous destination, or the default of
    /// `core::model::Destination` when there was none. *Clear destination* becomes available
    /// and the status bar confirms for three seconds. Connected to
    /// `map::MapWidget::destination_picked`.
    ///
    /// @param position The waypoint, latitude in [-90, 90] and longitude in [-180, 180].
    /// @param name Waypoint id sent in APB and RMB; empty gives `WPT`.
    void set_destination(core::geo::Position position, const QString& name = {});
    /// Stops steering for the waypoint, in the running simulation and in the profile seed.
    ///
    /// APB, RMB and XTE are no longer sent, and the status bar confirms for three seconds.
    /// Connected to the *Clear destination* action and to
    /// `map::MapWidget::destination_cleared`.
    void clear_destination();
    /// Returns the *Simulation → Clear destination* action.
    ///
    /// @return The action, owned by the window; enabled only while the vessel state has a
    ///   destination. Never null after construction.
    [[nodiscard]] QAction* clear_destination_action() const noexcept {
        return clear_destination_action_;
    }

signals:
    /// Emitted for every error the window itself reports to the operator, so that hosts and
    /// tests can observe errors without a dialog.
    ///
    /// Emitted synchronously from `load_profile`, `set_profile` (and so `load_track`,
    /// `load_log` and the settings dialog), `set_recording` and *Save profile*, before any
    /// message box opens. Errors of the outputs, which the runner reports through
    /// `io::SimulationRunner::output_error`, only reach the status bar and do not emit it.
    ///
    /// @param title Short summary, such as `Cannot open profile`.
    /// @param message The reason, as the profile loader, the runner or the file system gave
    ///   it.
    void error_reported(const QString& title, const QString& message);

protected:
    /// Nudges the vessel with the arrow keys; other keys go to `QMainWindow`.
    ///
    /// Up and Down change the speed over ground by 0.1 kn (1 kn with Shift). Left and Right
    /// change the heading by 1° (10° with Shift), or the rudder angle when *Steering mode* is
    /// on; Left is to port. A nudge pins the parameter as an override of the delta source.
    /// In track and replay mode, which have nothing to nudge, the arrow keys go to
    /// `QMainWindow` as well. The window receives them only while it or a child that ignores
    /// them has focus.
    ///
    /// @param event The key press.
    void keyPressEvent(QKeyEvent* event) override;
    /// Saves the window geometry and dock layout and stops the simulation before the window
    /// closes.
    ///
    /// Reached from *File → Quit* and from the window manager's close button.
    ///
    /// @param event The close request, accepted by the base class.
    void closeEvent(QCloseEvent* event) override;

private:
    /// Creates the *File* and *Simulation* menus, their actions and shortcuts, and the toolbar
    /// with the seek slider and position label, and connects each action to its slot.
    void build_actions();
    /// Puts the console, outputs and map into docks and creates the *View* menu, after
    /// *Simulation*, with the dock toggles, the map actions and the *Theme* submenu.
    void build_docks();
    /// Creates the *Help* menu with *About*, last in the menu bar.
    void build_help_menu();
    /// Paints the action icons in the colours of the current theme.
    ///
    /// Connected to `theme::Theme::changed`, since the icons are drawn rather than loaded.
    void apply_icons();
    /// Reports an error to the operator: ten seconds in the status bar, `error_reported`, and
    /// a warning box while the window is visible.
    ///
    /// Emits `error_reported`.
    ///
    /// @param title Short summary, used as the box title.
    /// @param message The reason.
    void report_error(const QString& title, const QString& message);
    /// Sets the title to `name - NMEA Simulator X`, where the name is the profile file name or,
    /// for a profile without a file, the profile's own name.
    void update_title();
    /// Brings the run controls in line with the run state.
    ///
    /// *Start* becomes *Stop* (text and icon) while running, *Pause* is enabled only while
    /// running and unticked otherwise; then refreshes the transport controls and the status bar.
    /// Connected to the runner's `started` and `stopped` signals.
    void update_actions();
    /// Refreshes the status bar: the three lights, the profile name with its mode (and the
    /// track or log file name), and the sentence counter.
    ///
    /// Called after every tick, on pause, recording and theme changes and from
    /// `update_actions`.
    void refresh_status();
    /// Refreshes the dashboard and the map from the current vessel state.
    ///
    /// Also enables *Clear destination* only while the state has a destination. Does nothing
    /// before a profile has been applied.
    void refresh_view();
    /// Refreshes the seek slider and the position label from the runner.
    ///
    /// The slider is enabled for a source of known, non-zero length and does not jump while
    /// the operator holds it. *Step* is enabled once a simulation exists.
    void refresh_transport();
    /// Nudges a parameter of the delta source and updates the dashboard.
    ///
    /// Does nothing when the current source is not a delta source.
    ///
    /// @param parameter The value to change.
    /// @param delta The change, in the parameter's unit (knots or degrees); negative to
    ///   decrease.
    void nudge(core::simulation::Parameter parameter, double delta);
    /// Returns the current source when it is the delta simulation.
    ///
    /// @return The runner's source, valid until the next successful `set_profile`; null in
    ///   track and replay mode and before a profile has been applied.
    [[nodiscard]] core::simulation::DeltaSource* delta_source();

    /// Replaces the current profile with the built-in default; slot of *New profile*.
    ///
    /// The new profile has no file, so *Save profile* asks for a name.
    void new_profile();
    /// Asks for a profile file in the profiles directory and loads it; slot of *Open
    /// profile...*.
    void open_profile();
    /// Asks for a GPX or KML file and calls `load_track`; slot of *Open track...*.
    ///
    /// The dialog starts in the folder of the current track, or in the documents folder.
    void open_track();
    /// Asks for a log file and calls `load_log`; slot of *Open log for replay...*.
    ///
    /// The dialog starts in the folder of the current log, or in the documents folder.
    void open_log();
    /// Opens the settings dialog on a copy of the current profile; slot of *Settings...*.
    ///
    /// *OK* applies the edited profile with `set_profile`, keeping its file path; *Cancel*
    /// discards it. The file on disk is not written.
    void edit_settings();
    /// Writes the current profile to its file; slot of *Save profile*.
    ///
    /// Asks for a file name through `save_profile_as` when the profile has none, and writes
    /// through `save_profile_to` otherwise.
    ///
    /// @return True when the profile was written; false when it failed or the operator
    ///   cancelled the file dialog.
    bool save_profile();
    /// Asks for a file name in the profiles directory and writes the profile there with
    /// `save_profile_to`; slot of *Save profile as...*.
    ///
    /// @return True when the profile was written; false when the operator cancelled or the
    ///   write failed.
    bool save_profile_as();
    /// Stops a running simulation or starts a stopped one; slot of *Start* / *Stop* (F5).
    void toggle_run();
    /// Pauses or resumes the run; slot of the checkable *Pause* action (F6).
    ///
    /// @param paused True to freeze the simulated clock, false to continue.
    void toggle_pause(bool paused);
    /// Switches between steering with the rudder and setting the heading; slot of the
    /// checkable *Steering mode* action.
    ///
    /// Leaving steering mode centres the rudder and releases its override, so the vessel stops
    /// turning. The dashboard enables the rudder or the heading control accordingly.
    ///
    /// @param enabled True for the arrow keys to move the rudder.
    void toggle_steering(bool enabled);
    /// Starts or stops recording; slot of the checkable *Record log...* action.
    ///
    /// Ticking asks for a file, suggesting `nmeasim-yyyyMMdd-HHmmss.log` (UTC) in the documents
    /// folder, and calls `set_recording`; cancelling the dialog unticks the action again.
    /// Unticking stops the recording.
    ///
    /// @param checked The new state of the action.
    void toggle_recording(bool checked);
    /// Seeks the runner to the slider position; connected to `QSlider::valueChanged`.
    ///
    /// Ignores changes made by `refresh_transport` and changes while the slider is disabled.
    /// Starts a new segment of the sailed track on the map.
    ///
    /// @param value Position in the track or log, in milliseconds.
    void seek_from_slider(int value);

    /// Preferences read at construction and written when the operator changes them.
    AppSettings settings_;
    /// The current profile, as last applied or changed on the map; see `profile`.
    io::Profile profile_{io::Profile::default_profile()};
    /// File the current profile was loaded from or saved to; empty for a profile without a
    /// file.
    QString profile_path_;
    /// The runner of the current profile; the outputs table observes it.
    io::SimulationRunner runner_;

    // tile_cache_ is declared before map_ because the map is constructed with it.
    /// Map tile store shared with the map; Qt child of the window.
    map::TileCache* tile_cache_;
    /// Central widget with the instruments and override controls.
    DashboardWidget* dashboard_;
    /// Sentence console, in the *Console* dock.
    ConsoleWidget* console_;
    /// Outputs table, in the *Outputs* dock.
    OutputsWidget* outputs_;
    /// Slippy map, in the *Map* dock.
    map::MapWidget* map_;
    /// Status bar text: the profile name and, in track or replay mode, the file name.
    QLabel* status_label_;
    /// Status bar counter of sentences emitted since the profile was applied, in the monospaced
    /// font.
    QLabel* counter_label_;
    /// Toolbar slider over the length of a track or log; see `seek_slider`.
    QSlider* seek_slider_;
    /// Toolbar label with the elapsed and total time; see `position_label`.
    QLabel* position_label_;
    /// Run state light; see `run_led`.
    StatusLed* run_led_{nullptr};
    /// Recording light; see `recording_led`.
    StatusLed* recording_led_{nullptr};
    /// Outputs light; see `outputs_led`.
    StatusLed* outputs_led_{nullptr};
    /// The *View → Theme* entries; see `theme_actions`.
    QList<QAction*> theme_actions_;
    /// True while `refresh_transport` sets the slider, so that `seek_from_slider` does not
    /// turn that update into a seek.
    bool updating_slider_{false};

    /// *File → New profile* (Ctrl+N); calls `new_profile`.
    QAction* new_action_{nullptr};
    /// *File → Open profile...* (Ctrl+O); calls `open_profile`.
    QAction* open_action_{nullptr};
    /// *File → Open track...* (Ctrl+T); calls `open_track`.
    QAction* open_track_action_{nullptr};
    /// *File → Open log for replay...* (Ctrl+L); calls `open_log`.
    QAction* open_log_action_{nullptr};
    /// Checkable *File → Record log...* (Ctrl+R); calls `toggle_recording`.
    QAction* record_action_{nullptr};
    /// *Simulation → Step* (F7); steps the runner, which starts the run paused when stopped.
    QAction* step_action_{nullptr};
    /// *File → Save profile* (Ctrl+S); calls `save_profile`.
    QAction* save_action_{nullptr};
    /// *File → Save profile as...* (Ctrl+Shift+S); calls `save_profile_as`.
    QAction* save_as_action_{nullptr};
    /// *File → Settings...* (Ctrl+comma); calls `edit_settings`.
    QAction* settings_action_{nullptr};
    /// *Simulation → Start* / *Stop* (F5); calls `toggle_run`.
    QAction* run_action_{nullptr};
    /// Checkable *Simulation → Pause* (F6); calls `toggle_pause`; enabled only while running.
    QAction* pause_action_{nullptr};
    /// Checkable *Simulation → Steering mode*; calls `toggle_steering`; enabled in delta mode.
    QAction* steering_action_{nullptr};
    /// *Simulation → Clear destination*; calls `clear_destination`.
    QAction* clear_destination_action_{nullptr};
    /// Checkable *Simulation → Start automatically on launch*; stores `simulation/autostart`.
    QAction* autostart_action_{nullptr};
    /// Checkable *View → Follow vessel on the map* (Home); kept in step with the map's own
    /// follow state in both directions.
    QAction* follow_action_{nullptr};
    /// Checkable *View → Download map tiles*; switches the tile cache and stores `map/online`.
    QAction* online_tiles_action_{nullptr};
};

}  // namespace nmeasim::app
