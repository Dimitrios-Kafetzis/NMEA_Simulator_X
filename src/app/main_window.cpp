#include "main_window.hpp"

#include "dialogs/settings_dialog.hpp"
#include "widgets/console_widget.hpp"
#include "widgets/dashboard_widget.hpp"
#include "widgets/outputs_widget.hpp"

#include <nmeasim/core/version.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

namespace nmeasim::app {

using core::simulation::Parameter;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      dashboard_(new DashboardWidget(this)),
      console_(new ConsoleWidget(this)),
      outputs_(new OutputsWidget(this)),
      status_label_(new QLabel(this)),
      counter_label_(new QLabel(this)) {
    setWindowTitle(QStringLiteral("NMEA Simulator X"));
    setMinimumSize(900, 600);
    setFocusPolicy(Qt::StrongFocus);
    setCentralWidget(dashboard_);

    build_actions();
    build_docks();

    statusBar()->addWidget(status_label_, 1);
    statusBar()->addPermanentWidget(counter_label_);

    connect(&runner_, &io::SimulationRunner::ticked, this, [this] {
        if (const auto* simulation = runner_.simulation()) {
            dashboard_->update_state(simulation->state());
        }
        refresh_status();
    });
    connect(&runner_, &io::SimulationRunner::sentence_emitted, console_,
            &ConsoleWidget::append_sentence);
    connect(&runner_, &io::SimulationRunner::started, this, &MainWindow::update_actions);
    connect(&runner_, &io::SimulationRunner::stopped, this, &MainWindow::update_actions);
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

    restoreGeometry(settings_.window_geometry());
    restoreState(settings_.window_state());
    autostart_action_->setChecked(settings_.autostart());

    set_profile(profile_);
    update_actions();
    refresh_status();
}

MainWindow::~MainWindow() {
    runner_.stop();
}

void MainWindow::build_actions() {
    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName(QStringLiteral("main_toolbar"));
    toolbar->setMovable(false);

    new_action_ = new QAction(tr("&New profile"), this);
    new_action_->setShortcut(QKeySequence::New);
    connect(new_action_, &QAction::triggered, this, &MainWindow::new_profile);
    open_action_ = new QAction(tr("&Open profile..."), this);
    open_action_->setShortcut(QKeySequence::Open);
    connect(open_action_, &QAction::triggered, this, &MainWindow::open_profile);
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
    steering_action_ = new QAction(tr("Steering mode"), this);
    steering_action_->setCheckable(true);
    steering_action_->setToolTip(
        tr("Steer with the rudder (left and right arrows) instead of setting the heading"));
    connect(steering_action_, &QAction::toggled, this, &MainWindow::toggle_steering);
    autostart_action_ = new QAction(tr("Start automatically on launch"), this);
    autostart_action_->setCheckable(true);
    connect(autostart_action_, &QAction::toggled, this,
            [this](bool checked) { settings_.set_autostart(checked); });

    auto* quit_action = new QAction(tr("&Quit"), this);
    quit_action->setShortcut(QKeySequence::Quit);
    connect(quit_action, &QAction::triggered, this, &QWidget::close);
    auto* about_action = new QAction(tr("&About"), this);
    connect(about_action, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this, tr("About NMEA Simulator X"),
            tr("<b>NMEA Simulator X %1</b><br>Free, open-source NMEA 0183 and Signal K data stream "
               "simulator.<br><a href=\"https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X\">"
               "github.com/Dimitrios-Kafetzis/NMEA_Simulator_X</a>")
                .arg(QString::fromUtf8(core::kVersion.data(),
                                       static_cast<qsizetype>(core::kVersion.size()))));
    });

    auto* file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addActions({new_action_, open_action_, save_action_, save_as_action_});
    file_menu->addSeparator();
    file_menu->addAction(settings_action_);
    file_menu->addSeparator();
    file_menu->addAction(quit_action);
    auto* simulation_menu = menuBar()->addMenu(tr("&Simulation"));
    simulation_menu->addActions({run_action_, pause_action_, steering_action_});
    simulation_menu->addSeparator();
    simulation_menu->addAction(autostart_action_);
    auto* help_menu = menuBar()->addMenu(tr("&Help"));
    help_menu->addAction(about_action);

    toolbar->addActions({open_action_, save_action_, settings_action_});
    toolbar->addSeparator();
    toolbar->addActions({run_action_, pause_action_, steering_action_});
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

    auto* view_menu = menuBar()->addMenu(tr("&View"));
    view_menu->addAction(console_dock->toggleViewAction());
    view_menu->addAction(outputs_dock->toggleViewAction());
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
    set_profile(*loaded, path);
    return true;
}

void MainWindow::set_profile(const io::Profile& profile, const QString& path) {
    const bool was_running = is_running();
    stop();
    profile_ = profile;
    profile_path_ = path;
    if (!path.isEmpty()) {
        settings_.set_last_profile_path(path);
    }
    QString error;
    if (!runner_.apply_profile(profile_, &error)) {
        report_error(tr("Cannot apply profile"), error);
    }
    if (const auto* source = delta_source()) {
        dashboard_->update_state(source->current());
        dashboard_->sync_overrides(*source);
    }
    outputs_->refresh();
    update_title();
    if (was_running) {
        start();
    }
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
        dashboard_->update_state(source->current());
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
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
    QString error;
    if (!profile_.save(profile_path_, &error)) {
        report_error(tr("Cannot save profile"), error);
        return false;
    }
    statusBar()->showMessage(tr("Saved %1").arg(profile_path_), 3000);
    return true;
}

bool MainWindow::save_profile_as() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save profile"), AppSettings::profiles_directory(), tr("Profiles (*.json)"));
    if (path.isEmpty()) {
        return false;
    }
    profile_path_ = path;
    settings_.set_last_profile_path(path);
    update_title();
    return save_profile();
}

void MainWindow::report_error(const QString& title, const QString& message) {
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, message), 10000);
    emit error_reported(title, message);
    if (isVisible()) {
        QMessageBox::warning(this, title, message);
    }
}

void MainWindow::update_title() {
    const QString name =
        profile_path_.isEmpty() ? profile_.name : QFileInfo(profile_path_).fileName();
    setWindowTitle(QStringLiteral("%1 - NMEA Simulator X").arg(name));
}

void MainWindow::update_actions() {
    const bool running = is_running();
    run_action_->setText(running ? tr("Stop") : tr("Start"));
    pause_action_->setEnabled(running);
    if (!running) {
        pause_action_->setChecked(false);
    }
    refresh_status();
}

void MainWindow::refresh_status() {
    QString state = tr("Stopped");
    if (is_running()) {
        state = runner_.is_paused() ? tr("Paused") : tr("Running");
    }
    status_label_->setText(tr("%1 - %2").arg(state, profile_.name));
    counter_label_->setText(tr("%1 sentences").arg(runner_.sentences_emitted()));
}

}  // namespace nmeasim::app
