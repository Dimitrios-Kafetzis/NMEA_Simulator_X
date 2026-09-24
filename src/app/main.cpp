// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Entry point of the desktop application, `NMEASimulatorX`.
///
/// The executable only sets the application's identity, applies the stored look, opens the
/// main window and chooses the profile to start with; everything else lives in the
/// `nmeasim_app_lib` library, which the tests link and drive without this file, as ADR 0009
/// decides.
///
/// @see docs/adr/0009-desktop-shell.md
/// @see docs/reference/desktop-app.md, section "Command line".

#include "app_settings.hpp"
#include "main_window.hpp"
#include "theme/theme.hpp"

#include <nmeasim/core/version.hpp>

#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QString>

/// Starts the desktop application and runs its event loop until the main window closes.
///
/// Usage: `NMEASimulatorX [profile.json]`. The startup sequence:
///
/// 1. Creates the `QApplication`, which removes Qt's own options (such as `-platform` or
///    `-style`) from the arguments.
/// 2. Sets the application and organisation names, the organisation domain, the display name
///    and the version. The names (and, on macOS, the domain) select where `QSettings` and
///    `QStandardPaths` keep the preferences, the profiles and the tile cache, so they must be
///    set before the first `AppSettings` exists; the version is `nmeasim::core::kVersion`.
/// 3. Sets the desktop file name and the window icon.
/// 4. Applies the look stored under `appearance/theme` before any widget exists, so that the
///    window is built with its palette and *View → Theme* ticks the stored entry.
/// 5. Creates and shows the main window, which applies the built-in default profile.
/// 6. Loads the profile named by the first argument when that file exists; otherwise the
///    profile stored under `profile/last_path` when that file exists. A path that does not
///    exist is ignored without a message, and further arguments are ignored.
/// 7. Starts the simulation when `simulation/autostart` is set.
///
/// There is no other command-line option: an argument such as `--help` is taken as a profile
/// path and ignored because no such file exists.
///
/// @param argc Number of command-line arguments, including the program name.
/// @param argv The command-line arguments; `QApplication` requires both to stay valid for its
///   lifetime, which is the whole of this function.
/// @return The exit code of `QApplication::exec`: 0 after a normal quit.
int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NMEASimulatorX"));
    QApplication::setApplicationDisplayName(QStringLiteral("NMEA Simulator X"));
    QApplication::setOrganizationName(QStringLiteral("NMEASimulatorX"));
    QApplication::setOrganizationDomain(QStringLiteral("dimitrios-kafetzis.github.io"));
    QApplication::setApplicationVersion(QString::fromUtf8(
        nmeasim::core::kVersion.data(), static_cast<qsizetype>(nmeasim::core::kVersion.size())));
    // The desktop file name lets Wayland compositors match the window to its icon and launcher.
    QGuiApplication::setDesktopFileName(QStringLiteral(NMEASIM_APP_ID));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/nmeasimulatorx.png")));

    nmeasim::app::AppSettings settings;
    nmeasim::app::theme::Theme::instance().apply(
        nmeasim::app::theme::mode_from_string(settings.theme()));

    nmeasim::app::MainWindow window;
    // Shown before a profile is loaded, so that a profile that cannot be opened is reported in
    // a message box over the window: `MainWindow` shows dialogs only while it is visible.
    window.show();

    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1 && QFile::exists(arguments.at(1))) {
        window.load_profile(arguments.at(1));
    } else if (!settings.last_profile_path().isEmpty() &&
               QFile::exists(settings.last_profile_path())) {
        window.load_profile(settings.last_profile_path());
    }
    // After loading, so that autostart runs the chosen profile rather than the default one.
    if (settings.autostart()) {
        window.start();
    }
    return QApplication::exec();
}
