// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `MainWindow`: building the actions, menus, toolbar, docks and status bar,
/// wiring them to the `io::SimulationRunner`, and the slots behind every action.
///
/// The signal connections made in the constructor:
///
/// - `map::MapWidget`: `position_picked`, `destination_picked` and `destination_cleared` edit
///   the vessel and the destination; `view_changed` stores the zoom level.
/// - `DashboardWidget`: `override_changed`, `fix_changed`, `satellites_changed` and
///   `engine_changed` go to the delta source, and are ignored in track and replay mode.
/// - `theme::Theme::changed` repaints the icons, the lights and the map.
/// - `io::SimulationRunner`: `ticked` refreshes the dashboard, map, transport controls and
///   status bar; `sentence_emitted` feeds the console; `started` and `stopped` update the run
///   controls; `paused_changed` ticks *Pause*; `recording_changed` ticks *Record log...*;
///   `finished` and `output_error` show a message in the status bar.
///
/// Status bar messages last three seconds for confirmations, five for the end of a track or
/// log and the recording state, and ten for errors.

#include "main_window.hpp"

#include "dialogs/settings_dialog.hpp"
#include "map/map_widget.hpp"
#include "map/tile_cache.hpp"
#include "theme/icons.hpp"
#include "theme/theme.hpp"
#include "widgets/console_widget.hpp"
#include "widgets/dashboard_widget.hpp"
#include "widgets/outputs_widget.hpp"
#include "widgets/status_led.hpp"

#include <nmeasim/core/simulation/track_source.hpp>
#include <nmeasim/core/version.hpp>

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointF>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nmeasim::app {

using core::simulation::Parameter;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      tile_cache_(new map::TileCache(AppSettings::tile_cache_directory(), this)),
      dashboard_(new DashboardWidget(this)),
      console_(new ConsoleWidget(this)),
      outputs_(new OutputsWidget(this)),
      map_(new map::MapWidget(tile_cache_, this)),
      status_label_(new QLabel(this)),
      counter_label_(new QLabel(this)),
      seek_slider_(new QSlider(Qt::Horizontal, this)),
      position_label_(new QLabel(this)) {
    setWindowTitle(QStringLiteral("NMEA Simulator X"));
    setMinimumSize(900, 600);
    // The window itself can hold focus, so the arrow keys reach keyPressEvent when no child
    // widget has taken it.
    setFocusPolicy(Qt::StrongFocus);
    setCentralWidget(dashboard_);

    tile_cache_->set_online(settings_.map_online());
    tile_cache_->set_url_template(settings_.map_tile_url());
    map_->set_attribution(settings_.map_tile_attribution());
    // Through zoom_to, which keeps the fractional level stored by earlier sessions; its anchor
    // is the widget centre, so the centre stays where it is.
    map_->zoom_to(settings_.map_zoom(), QPointF(map_->width(), map_->height()) / 2.0);
    connect(map_, &map::MapWidget::position_picked, this, &MainWindow::move_vessel);
    // Through a lambda: a pointer to set_destination cannot supply its default name.
    connect(map_, &map::MapWidget::destination_picked, this,
            [this](core::geo::Position position) { set_destination(position); });
    connect(map_, &map::MapWidget::destination_cleared, this, &MainWindow::clear_destination);
    // Follow mode emits view_changed on every tick; storing only a changed zoom level keeps
    // QSettings from rewriting its file several times a second.
    connect(map_, &map::MapWidget::view_changed, this, [this] {
        if (settings_.map_zoom() != map_->zoom_level()) {
            settings_.set_map_zoom(map_->zoom_level());
        }
    });

    build_actions();
    build_docks();
    build_help_menu();

    run_led_ = new StatusLed(this);
    run_led_->setObjectName(QStringLiteral("run_led"));
    recording_led_ = new StatusLed(this);
    recording_led_->setObjectName(QStringLiteral("recording_led"));
    recording_led_->setVisible(false);
    outputs_led_ = new StatusLed(this);
    outputs_led_->setObjectName(QStringLiteral("outputs_led"));
    statusBar()->addWidget(run_led_);
    statusBar()->addWidget(recording_led_);
    statusBar()->addWidget(status_label_, 1);
    statusBar()->addPermanentWidget(outputs_led_);
    statusBar()->addPermanentWidget(counter_label_);
    counter_label_->setFont(theme::Theme::mono_font());
    apply_icons();
    connect(&theme::Theme::instance(), &theme::Theme::changed, this, [this] {
        apply_icons();
        refresh_status();
        map_->update();
    });

    connect(&runner_, &io::SimulationRunner::ticked, this, [this] {
        refresh_view();
        refresh_transport();
        refresh_status();
    });
    connect(&runner_, &io::SimulationRunner::sentence_emitted, console_,
            &ConsoleWidget::append_sentence);
    connect(&runner_, &io::SimulationRunner::started, this, &MainWindow::update_actions);
    connect(&runner_, &io::SimulationRunner::stopped, this, &MainWindow::update_actions);
    connect(&runner_, &io::SimulationRunner::paused_changed, this, [this](bool paused) {
        pause_action_->setChecked(paused);
        refresh_status();
    });
    connect(&runner_, &io::SimulationRunner::finished, this,
            [this] { statusBar()->showMessage(tr("End of the track or log reached"), 5000); });
    connect(&runner_, &io::SimulationRunner::recording_changed, this, [this](const QString& path) {
        // Blocked, or ticking the action here would open the file dialog of toggle_recording.
        const QSignalBlocker blocker(record_action_);
        record_action_->setChecked(!path.isEmpty());
        refresh_status();
        record_action_->setToolTip(path.isEmpty() ? tr("Record every sentence to a log file")
                                                  : tr("Recording to %1").arg(path));
        statusBar()->showMessage(
            path.isEmpty() ? tr("Recording stopped") : tr("Recording to %1").arg(path), 5000);
    });
    connect(&runner_, &io::SimulationRunner::output_error, this,
            [this](const QString& description, const QString& message) {
                statusBar()->showMessage(QStringLiteral("%1: %2").arg(description, message), 10000);
            });
    outputs_->set_runner(&runner_);

    connect(dashboard_, &DashboardWidget::override_changed, this,
            [this](Parameter parameter, bool active, double value) {
                if (auto* source = delta_source()) {
                    if (active) {
                        source->set_override(parameter, value);
                    } else {
                        source->clear_override(parameter);
                    }
                }
            });
    connect(dashboard_, &DashboardWidget::fix_changed, this, [this](bool has_fix) {
        if (auto* source = delta_source()) {
            source->set_fix(has_fix);
        }
    });
    connect(dashboard_, &DashboardWidget::satellites_changed, this, [this](int in_use) {
        if (auto* source = delta_source()) {
            source->set_satellites(in_use, source->current().gnss.satellites_in_view);
        }
    });
    connect(dashboard_, &DashboardWidget::engine_changed, this,
            [this](int index, const core::model::Engine& engine) {
                if (auto* source = delta_source()) {
                    source->set_engine(static_cast<std::size_t>(index), engine);
                    refresh_view();
                }
            });

    restoreGeometry(settings_.window_geometry());
    if (!restoreState(settings_.window_state())) {
        // First start: room for the map and the instruments, a short console.
        auto* map_dock = findChild<QDockWidget*>(QStringLiteral("map_dock"));
        auto* outputs_dock = findChild<QDockWidget*>(QStringLiteral("outputs_dock"));
        auto* console_dock = findChild<QDockWidget*>(QStringLiteral("console_dock"));
        resizeDocks({map_dock, outputs_dock}, {400, 260}, Qt::Horizontal);
        resizeDocks({console_dock}, {170}, Qt::Vertical);
    }
    autostart_action_->setChecked(settings_.autostart());

    // A simulation exists from the start, so the dashboard, the map and Step have a state even
    // before main loads a profile.
    set_profile(profile_);
    update_actions();
    refresh_status();
}

MainWindow::~MainWindow() {
    // Stop while the window is complete: the runner's own destructor would otherwise emit
    // stopped into update_actions of a window whose members are being destroyed.
    runner_.stop();
}

void MainWindow::build_actions() {
    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName(QStringLiteral("main_toolbar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    new_action_ = new QAction(tr("&New profile"), this);
    new_action_->setShortcut(QKeySequence::New);
    connect(new_action_, &QAction::triggered, this, &MainWindow::new_profile);
    open_action_ = new QAction(tr("&Open profile..."), this);
    open_action_->setShortcut(QKeySequence::Open);
    connect(open_action_, &QAction::triggered, this, &MainWindow::open_profile);
    open_track_action_ = new QAction(tr("Open &track..."), this);
    open_track_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    open_track_action_->setToolTip(tr("Follow a GPX or KML track with the current profile"));
    connect(open_track_action_, &QAction::triggered, this, &MainWindow::open_track);
    open_log_action_ = new QAction(tr("Open &log for replay..."), this);
    open_log_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    open_log_action_->setToolTip(tr("Replay a recorded or plain NMEA log"));
    connect(open_log_action_, &QAction::triggered, this, &MainWindow::open_log);
    record_action_ = new QAction(tr("&Record log..."), this);
    record_action_->setCheckable(true);
    record_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    record_action_->setToolTip(tr("Record every sentence to a log file"));
    connect(record_action_, &QAction::toggled, this, &MainWindow::toggle_recording);
    save_action_ = new QAction(tr("&Save profile"), this);
    save_action_->setShortcut(QKeySequence::Save);
    connect(save_action_, &QAction::triggered, this, &MainWindow::save_profile);
    save_as_action_ = new QAction(tr("Save profile &as..."), this);
    save_as_action_->setShortcut(QKeySequence::SaveAs);
    connect(save_as_action_, &QAction::triggered, this, &MainWindow::save_profile_as);
    settings_action_ = new QAction(tr("Se&ttings..."), this);
    settings_action_->setShortcut(QKeySequence::Preferences);
    settings_action_->setToolTip(tr("Edit the simulation, sentences and outputs of this profile"));
    connect(settings_action_, &QAction::triggered, this, &MainWindow::edit_settings);

    run_action_ = new QAction(tr("Start"), this);
    run_action_->setShortcut(Qt::Key_F5);
    connect(run_action_, &QAction::triggered, this, &MainWindow::toggle_run);
    pause_action_ = new QAction(tr("Pause"), this);
    pause_action_->setCheckable(true);
    pause_action_->setShortcut(Qt::Key_F6);
    connect(pause_action_, &QAction::toggled, this, &MainWindow::toggle_pause);
    step_action_ = new QAction(tr("Step"), this);
    step_action_->setShortcut(Qt::Key_F7);
    step_action_->setToolTip(
        tr("Pause and advance by one tick, or by one recorded sentence during a replay"));
    connect(step_action_, &QAction::triggered, this, [this] {
        runner_.step();
        update_actions();
    });
    seek_slider_->setObjectName(QStringLiteral("seek_slider"));
    seek_slider_->setMinimumWidth(180);
    // Seek only when the slider is released; sliderMoved previews the position in the label.
    seek_slider_->setTracking(false);
    seek_slider_->setToolTip(tr("Position within the track or log"));
    connect(seek_slider_, &QSlider::valueChanged, this, &MainWindow::seek_from_slider);
    connect(seek_slider_, &QSlider::sliderMoved, this, [this](int value) {
        if (const auto duration = runner_.duration()) {
            position_label_->setText(QStringLiteral("%1 / %2").arg(
                format_duration(std::chrono::milliseconds{value}), format_duration(*duration)));
        }
    });
    position_label_->setMinimumWidth(110);
    position_label_->setAlignment(Qt::AlignCenter);
    steering_action_ = new QAction(tr("Steering mode"), this);
    steering_action_->setCheckable(true);
    steering_action_->setToolTip(
        tr("Steer with the rudder (left and right arrows) instead of setting the heading"));
    connect(steering_action_, &QAction::toggled, this, &MainWindow::toggle_steering);
    clear_destination_action_ = new QAction(tr("Clear destination"), this);
    clear_destination_action_->setToolTip(
        tr("Stop steering for the waypoint; APB, RMB and XTE are no longer sent"));
    connect(clear_destination_action_, &QAction::triggered, this, &MainWindow::clear_destination);
    autostart_action_ = new QAction(tr("Start automatically on launch"), this);
    autostart_action_->setCheckable(true);
    connect(autostart_action_, &QAction::toggled, this,
            [this](bool checked) { settings_.set_autostart(checked); });

    auto* quit_action = new QAction(tr("&Quit"), this);
    quit_action->setShortcut(QKeySequence::Quit);
    connect(quit_action, &QAction::triggered, this, &QWidget::close);

    auto* file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addActions({new_action_, open_action_, save_action_, save_as_action_});
    file_menu->addSeparator();
    file_menu->addActions({open_track_action_, open_log_action_, record_action_});
    file_menu->addSeparator();
    file_menu->addAction(settings_action_);
    file_menu->addSeparator();
    file_menu->addAction(quit_action);
    auto* simulation_menu = menuBar()->addMenu(tr("&Simulation"));
    simulation_menu->addActions({run_action_, pause_action_, step_action_, steering_action_});
    simulation_menu->addSeparator();
    simulation_menu->addAction(clear_destination_action_);
    simulation_menu->addSeparator();
    simulation_menu->addAction(autostart_action_);

    toolbar->addActions({open_action_, save_action_, settings_action_});
    toolbar->addSeparator();
    toolbar->addActions({run_action_, pause_action_, step_action_, steering_action_});
    toolbar->addSeparator();
    toolbar->addAction(record_action_);
    toolbar->addSeparator();
    toolbar->addWidget(seek_slider_);
    toolbar->addWidget(position_label_);
}

void MainWindow::move_vessel(core::geo::Position position) {
    profile_.delta.seed.navigation.position = position;
    // The vessel jumps: no track line and no leg from where it was.
    map_->break_track();
    if (profile_.delta.seed.destination) {
        profile_.delta.seed.destination->origin = position;
    }
    if (auto* source = delta_source()) {
        source->set_position(position);
        if (auto destination = source->current().destination) {
            destination->origin = position;
            source->set_destination(destination);
        }
        refresh_view();
    }
    map_->set_center(position);
    statusBar()->showMessage(tr("Vessel moved to %1").arg(format_position(position)), 3000);
}

void MainWindow::set_destination(core::geo::Position position, const QString& name) {
    core::model::Destination destination;
    destination.name = name.isEmpty() ? std::string{"WPT"} : name.toStdString();
    destination.position = position;
    destination.origin = profile_.delta.seed.navigation.position;
    // The leg starts where the vessel is now, not where the profile started it.
    if (const auto* simulation = runner_.simulation()) {
        destination.origin = simulation->state().navigation.position;
    }
    if (profile_.delta.seed.destination) {
        destination.arrival_radius_m = profile_.delta.seed.destination->arrival_radius_m;
    }
    profile_.delta.seed.destination = destination;
    if (auto* simulation = runner_.simulation()) {
        simulation->source().set_destination(destination);
    }
    refresh_view();
    statusBar()->showMessage(
        tr("Destination %1 set at %2")
            .arg(QString::fromStdString(destination.name), format_position(position)),
        3000);
}

void MainWindow::clear_destination() {
    profile_.delta.seed.destination.reset();
    if (auto* simulation = runner_.simulation()) {
        simulation->source().set_destination(std::nullopt);
    }
    refresh_view();
    statusBar()->showMessage(tr("Destination cleared"), 3000);
}

QString MainWindow::about_text() {
    const std::string_view version = core::version_description();
    return tr("<b>NMEA Simulator X %1</b><br>Free, open-source NMEA 0183 and Signal K data stream "
              "simulator.<br><a href=\"https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X\">"
              "github.com/Dimitrios-Kafetzis/NMEA_Simulator_X</a>")
        .arg(QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size())));
}

QString MainWindow::format_duration(std::chrono::milliseconds duration) {
    const auto total = std::max<long long>(0, duration.count() / 1000);
    const auto hours = total / 3600;
    const auto minutes = (total % 3600) / 60;
    const auto seconds = total % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

void MainWindow::refresh_view() {
    const auto* simulation = runner_.simulation();
    if (simulation == nullptr) {
        return;
    }
    const auto& state = simulation->state();
    dashboard_->update_state(state);
    map_->set_vessel(state.navigation.position, state.navigation.heading_true_deg,
                     state.navigation.course_over_ground_deg,
                     state.navigation.speed_over_ground_kn);
    if (state.destination) {
        map_->set_destination(state.destination->position, state.destination->origin);
    } else {
        map_->set_destination(std::nullopt, std::nullopt);
    }
    clear_destination_action_->setEnabled(state.destination.has_value());
}

void MainWindow::refresh_transport() {
    const auto duration = runner_.duration();
    const bool finite = duration.has_value() && duration->count() > 0;
    updating_slider_ = true;
    seek_slider_->setEnabled(finite);
    if (finite) {
        // QSlider works in int milliseconds; anything past about 24.8 days sticks at the end.
        const auto clamp = [](std::chrono::milliseconds value) {
            return static_cast<int>(
                std::min<long long>(value.count(), std::numeric_limits<int>::max()));
        };
        seek_slider_->setRange(0, clamp(*duration));
        if (!seek_slider_->isSliderDown()) {
            seek_slider_->setValue(clamp(runner_.position()));
        }
        position_label_->setText(QStringLiteral("%1 / %2").arg(format_duration(runner_.position()),
                                                               format_duration(*duration)));
    } else {
        seek_slider_->setRange(0, 0);
        position_label_->clear();
    }
    updating_slider_ = false;
    step_action_->setEnabled(runner_.simulation() != nullptr);
}

void MainWindow::seek_from_slider(int value) {
    if (updating_slider_ || !seek_slider_->isEnabled()) {
        return;
    }
    map_->break_track();
    runner_.seek(std::chrono::milliseconds{value});
}

bool MainWindow::load_track(const QString& path) {
    io::Profile profile = profile_;
    profile.mode = io::SimulationMode::Track;
    profile.track.path = path;
    return set_profile(profile, profile_path_);
}

bool MainWindow::load_log(const QString& path) {
    io::Profile profile = profile_;
    profile.mode = io::SimulationMode::Replay;
    profile.replay.path = path;
    return set_profile(profile, profile_path_);
}

bool MainWindow::set_recording(const QString& path) {
    if (!runner_.set_recording(path)) {
        report_error(tr("Cannot record"), tr("The log file %1 cannot be written").arg(path));
        // The runner sets no failed recording and emits no recording_changed for it; clearing
        // the recording emits one with an empty path, which unticks Record log.
        runner_.set_recording({});
        return false;
    }
    return true;
}

void MainWindow::open_track() {
    const QString start =
        AppSettings::dialog_directory(profile_.base_directory, profile_.track.path);
    const QString path = QFileDialog::getOpenFileName(this, tr("Open track"), start,
                                                      tr("Tracks (*.gpx *.kml);;All files (*)"));
    if (!path.isEmpty()) {
        load_track(path);
    }
}

void MainWindow::open_log() {
    const QString start =
        AppSettings::dialog_directory(profile_.base_directory, profile_.replay.path);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open log for replay"), start, tr("Logs (*.log *.nmea *.txt);;All files (*)"));
    if (!path.isEmpty()) {
        load_log(path);
    }
}

void MainWindow::toggle_recording(bool checked) {
    if (!checked) {
        runner_.set_recording({});
        return;
    }
    const QString suggested =
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .filePath(QStringLiteral("nmeasim-%1.log")
                          .arg(QDateTime::currentDateTimeUtc().toString(
                              QStringLiteral("yyyyMMdd-HHmmss"))));
    const QString path = QFileDialog::getSaveFileName(this, tr("Record log"), suggested,
                                                      tr("Logs (*.log);;All files (*)"));
    if (path.isEmpty()) {
        // Untick without calling this slot again.
        const QSignalBlocker blocker(record_action_);
        record_action_->setChecked(false);
        return;
    }
    set_recording(path);
}

void MainWindow::build_docks() {
    auto* console_dock = new QDockWidget(tr("Console"), this);
    console_dock->setObjectName(QStringLiteral("console_dock"));
    console_dock->setWidget(console_);
    addDockWidget(Qt::BottomDockWidgetArea, console_dock);

    auto* outputs_dock = new QDockWidget(tr("Outputs"), this);
    outputs_dock->setObjectName(QStringLiteral("outputs_dock"));
    outputs_dock->setWidget(outputs_);
    addDockWidget(Qt::RightDockWidgetArea, outputs_dock);

    auto* map_dock = new QDockWidget(tr("Map"), this);
    map_dock->setObjectName(QStringLiteral("map_dock"));
    map_dock->setWidget(map_);
    addDockWidget(Qt::LeftDockWidgetArea, map_dock);

    follow_action_ = new QAction(tr("Follow vessel on the map"), this);
    follow_action_->setCheckable(true);
    follow_action_->setChecked(map_->follows_vessel());
    follow_action_->setShortcut(Qt::Key_Home);
    connect(follow_action_, &QAction::toggled, map_, &map::MapWidget::set_follow_vessel);
    connect(map_, &map::MapWidget::follow_changed, follow_action_, &QAction::setChecked);
    online_tiles_action_ = new QAction(tr("Download map tiles"), this);
    online_tiles_action_->setCheckable(true);
    online_tiles_action_->setChecked(tile_cache_->online());
    online_tiles_action_->setToolTip(
        tr("Fetch missing tiles from the tile server. Off keeps the map to the tiles already "
           "on disk."));
    connect(online_tiles_action_, &QAction::toggled, this, [this](bool online) {
        tile_cache_->set_online(online);
        settings_.set_map_online(online);
        map_->update();
    });
    auto* clear_tiles_action = new QAction(tr("Clear map tile cache"), this);
    connect(clear_tiles_action, &QAction::triggered, this, [this] {
        tile_cache_->clear();
        map_->update();
        statusBar()->showMessage(tr("Map tile cache cleared"), 3000);
    });

    auto* view_menu = menuBar()->addMenu(tr("&View"));
    view_menu->addAction(map_dock->toggleViewAction());
    view_menu->addAction(console_dock->toggleViewAction());
    view_menu->addAction(outputs_dock->toggleViewAction());
    view_menu->addSeparator();
    view_menu->addAction(follow_action_);
    view_menu->addAction(online_tiles_action_);
    view_menu->addAction(clear_tiles_action);
    view_menu->addSeparator();

    auto* theme_menu = view_menu->addMenu(tr("&Theme"));
    auto* theme_group = new QActionGroup(this);
    const std::pair<theme::Mode, QString> looks[]{
        {theme::Mode::System, tr("Follow the system")},
        {theme::Mode::Night, tr("Night bridge (dark)")},
        {theme::Mode::Day, tr("Daylight (light)")},
    };
    for (const auto& [mode, text] : looks) {
        auto* action = theme_menu->addAction(text);
        action->setCheckable(true);
        action->setData(theme::to_string(mode));
        action->setChecked(theme::Theme::instance().mode() == mode);
        theme_group->addAction(action);
        theme_actions_.append(action);
        connect(action, &QAction::triggered, this, [this, chosen = mode] {
            theme::Theme::instance().apply(chosen);
            settings_.set_theme(theme::to_string(chosen));
        });
    }
}

void MainWindow::build_help_menu() {
    auto* about_action = new QAction(tr("&About"), this);
    connect(about_action, &QAction::triggered, this,
            [this] { QMessageBox::about(this, tr("About NMEA Simulator X"), about_text()); });
    auto* help_menu = menuBar()->addMenu(tr("&Help"));
    help_menu->addAction(about_action);
}

core::simulation::DeltaSource* MainWindow::delta_source() {
    auto* simulation = runner_.simulation();
    return simulation != nullptr
               ? dynamic_cast<core::simulation::DeltaSource*>(&simulation->source())
               : nullptr;
}

bool MainWindow::load_profile(const QString& path) {
    QString error;
    auto loaded = io::Profile::load(path, &error);
    if (!loaded) {
        report_error(tr("Cannot open profile"), error);
        return false;
    }
    return set_profile(*loaded, path);
}

void MainWindow::open_initial_profile(const QString& requested) {
    if (!requested.isEmpty()) {
        if (load_profile(requested)) {
            return;
        }
        // Reported by load_profile; the last profile is still better than the default one.
    }
    const QString last = settings_.last_profile_path();
    if (!last.isEmpty() && last != requested && QFile::exists(last)) {
        load_profile(last);
    }
}

bool MainWindow::set_profile(const io::Profile& profile, const QString& path) {
    const bool was_running = is_running();
    stop();
    QString error;
    if (!runner_.apply_profile(profile, &error)) {
        report_error(tr("Cannot apply profile"), error);
        if (was_running) {
            start();
        }
        return false;
    }
    profile_ = profile;
    profile_path_ = path;
    if (!path.isEmpty()) {
        settings_.set_last_profile_path(path);
    }
    map_->clear_track();
    const bool delta = profile_.mode == io::SimulationMode::Delta;
    dashboard_->set_overrides_enabled(delta);
    if (!delta) {
        // Steering needs the delta source; unticked here, it is not carried into the next
        // delta profile either.
        steering_action_->setChecked(false);
    }
    steering_action_->setEnabled(delta);
    if (const auto* source = delta_source()) {
        dashboard_->sync_overrides(*source);
    }
    if (const auto* simulation = runner_.simulation()) {
        refresh_view();
        map_->set_center(simulation->state().navigation.position);
        if (const auto* track =
                dynamic_cast<const core::simulation::TrackSource*>(&simulation->source())) {
            QList<core::geo::Position> route;
            for (const auto& point : track->config().track.points) {
                route.append(point.position);
            }
            map_->set_route(route);
        } else {
            map_->clear_route();
        }
    }
    refresh_transport();
    outputs_->refresh();
    update_title();
    if (was_running) {
        start();
    }
    return true;
}

void MainWindow::start() {
    if (is_running()) {
        return;
    }
    console_->clear();
    runner_.start();
    if (auto* source = delta_source()) {
        source->set_steering_mode(steering_action_->isChecked());
        dashboard_->sync_overrides(*source);
    }
    setFocus();
}

void MainWindow::stop() {
    runner_.stop();
    pause_action_->setChecked(false);
}

bool MainWindow::is_running() const {
    return runner_.is_running();
}

void MainWindow::toggle_run() {
    if (is_running()) {
        stop();
    } else {
        start();
    }
}

void MainWindow::toggle_pause(bool paused) {
    if (paused) {
        runner_.pause();
    } else {
        runner_.resume();
    }
    refresh_status();
}

void MainWindow::toggle_steering(bool enabled) {
    if (auto* source = delta_source()) {
        source->set_steering_mode(enabled);
        if (!enabled) {
            // Releasing an override leaves the rudder where it is; centre it first so that
            // the vessel stops turning.
            source->set_override(Parameter::RudderAngle, 0.0);
            source->clear_override(Parameter::RudderAngle);
        }
        dashboard_->sync_overrides(*source);
    }
    dashboard_->set_steering_mode(enabled);
}

void MainWindow::nudge(Parameter parameter, double delta) {
    if (auto* source = delta_source()) {
        source->nudge(parameter, delta);
        dashboard_->sync_overrides(*source);
        refresh_view();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (delta_source() == nullptr) {
        // Track and replay mode have nothing to nudge: the arrow keys go to the base class,
        // which ignores them so that they propagate like any unused key.
        QMainWindow::keyPressEvent(event);
        return;
    }
    const bool fine = !event->modifiers().testFlag(Qt::ShiftModifier);
    switch (event->key()) {
        case Qt::Key_Up:
            nudge(Parameter::SpeedOverGround, fine ? 0.1 : 1.0);
            return;
        case Qt::Key_Down:
            nudge(Parameter::SpeedOverGround, fine ? -0.1 : -1.0);
            return;
        case Qt::Key_Left:
            nudge(steering_action_->isChecked() ? Parameter::RudderAngle : Parameter::HeadingTrue,
                  fine ? -1.0 : -10.0);
            return;
        case Qt::Key_Right:
            nudge(steering_action_->isChecked() ? Parameter::RudderAngle : Parameter::HeadingTrue,
                  fine ? 1.0 : 10.0);
            return;
        default:
            QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    settings_.save_window(saveGeometry(), saveState());
    runner_.stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::new_profile() {
    set_profile(io::Profile::default_profile());
}

void MainWindow::edit_settings() {
    SettingsDialog dialog(profile_, this);
    if (dialog.exec() == QDialog::Accepted) {
        set_profile(dialog.profile(), profile_path_);
    }
}

void MainWindow::open_profile() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open profile"), AppSettings::profiles_directory(), tr("Profiles (*.json)"));
    if (!path.isEmpty()) {
        load_profile(path);
    }
}

bool MainWindow::save_profile() {
    if (profile_path_.isEmpty()) {
        return save_profile_as();
    }
    return save_profile_to(profile_path_);
}

bool MainWindow::save_profile_as() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save profile"), AppSettings::profiles_directory(), tr("Profiles (*.json)"));
    if (path.isEmpty()) {
        return false;
    }
    return save_profile_to(path);
}

bool MainWindow::save_profile_to(const QString& path) {
    QString error;
    if (!profile_.save(path, &error)) {
        report_error(tr("Cannot save profile"), error);
        return false;
    }
    // Only a file that was written becomes the profile's file.
    profile_path_ = path;
    settings_.set_last_profile_path(path);
    update_title();
    statusBar()->showMessage(tr("Saved %1").arg(path), 3000);
    return true;
}

void MainWindow::report_error(const QString& title, const QString& message) {
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, message), 10000);
    emit error_reported(title, message);
    // Hidden windows (the offscreen tests, or before main shows the window) get no modal box
    // that would block.
    if (isVisible()) {
        QMessageBox::warning(this, title, message);
    }
}

void MainWindow::update_title() {
    const QString name =
        profile_path_.isEmpty() ? profile_.name : QFileInfo(profile_path_).fileName();
    setWindowTitle(QStringLiteral("%1 - NMEA Simulator X").arg(name));
}

void MainWindow::apply_icons() {
    using theme::Icon;
    using theme::themed_icon;
    new_action_->setIcon(themed_icon(Icon::NewProfile));
    open_action_->setIcon(themed_icon(Icon::Open));
    save_action_->setIcon(themed_icon(Icon::Save));
    settings_action_->setIcon(themed_icon(Icon::Settings));
    open_track_action_->setIcon(themed_icon(Icon::Track));
    open_log_action_->setIcon(themed_icon(Icon::Log));
    record_action_->setIcon(themed_icon(Icon::Record));
    run_action_->setIcon(themed_icon(is_running() ? Icon::Stop : Icon::Start));
    pause_action_->setIcon(themed_icon(Icon::Pause));
    step_action_->setIcon(themed_icon(Icon::Step));
    steering_action_->setIcon(themed_icon(Icon::Steering));
    follow_action_->setIcon(themed_icon(Icon::Follow));
    clear_destination_action_->setIcon(themed_icon(Icon::Destination));
}

void MainWindow::update_actions() {
    const bool running = is_running();
    run_action_->setText(running ? tr("Stop") : tr("Start"));
    run_action_->setIcon(theme::themed_icon(running ? theme::Icon::Stop : theme::Icon::Start));
    pause_action_->setEnabled(running);
    if (!running) {
        pause_action_->setChecked(false);
    }
    refresh_transport();
    refresh_status();
}

void MainWindow::refresh_status() {
    const auto& colors = theme::Theme::instance().colors();
    QString state = tr("Stopped");
    QColor state_color = colors.inactive;
    if (is_running()) {
        const bool paused = runner_.is_paused();
        state = paused ? tr("Paused") : tr("Running");
        state_color = paused ? colors.warning : colors.ok;
    }
    run_led_->set_state(is_running() ? state_color : QColor{}, state.toUpper());
    const bool recording = !runner_.recording_path().isEmpty();
    recording_led_->setVisible(recording);
    recording_led_->set_state(colors.danger, tr("REC"));
    recording_led_->set_blinking(recording && is_running() && !runner_.is_paused());

    int open = 0;
    bool failed = false;
    const auto& outputs = runner_.outputs();
    for (const auto& channel : outputs) {
        const auto output_state = channel.transport->state();
        open += output_state == io::Transport::State::Open ? 1 : 0;
        failed = failed || output_state == io::Transport::State::Failed;
    }
    QColor outputs_color;
    if (is_running() && !outputs.empty()) {
        outputs_color =
            failed ? colors.danger
                   : (open == static_cast<int>(outputs.size()) ? colors.ok : colors.warning);
    }
    outputs_led_->set_state(outputs_color, tr("%1/%2 OUTPUTS").arg(open).arg(outputs.size()));
    QString mode;
    switch (profile_.mode) {
        case io::SimulationMode::Delta:
            break;
        case io::SimulationMode::Track:
            mode = tr(" (track %1)").arg(QFileInfo(profile_.track.path).fileName());
            break;
        case io::SimulationMode::Replay:
            mode = tr(" (replay %1)").arg(QFileInfo(profile_.replay.path).fileName());
            break;
    }
    status_label_->setText(tr("%1%2").arg(profile_.name, mode));
    counter_label_->setText(tr("%1 sentences").arg(runner_.sentences_emitted()));
}

}  // namespace nmeasim::app
