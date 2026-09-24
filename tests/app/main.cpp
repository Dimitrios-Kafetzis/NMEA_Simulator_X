// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Entry point of `nmeasim_app_tests`, the Catch2 suite for the desktop application.
///
/// The suite has its own `main` instead of the one Catch2 provides because widgets can only
/// be created once a `QApplication` exists. The runner forces the Qt platform plugin to
/// `offscreen`, so that windows are created, laid out and painted without a display; the
/// `ctest` registration in `tests/CMakeLists.txt` sets `QT_QPA_PLATFORM=offscreen` as well.
/// It also redirects `QSettings` to INI files in a temporary directory, deleted when the run
/// ends, under the application and organisation name `NMEASimulatorX-tests`, so that the tests
/// never read or overwrite the operator's preferences. In those settings it switches tile
/// downloads off (`map/online`) and points the map tile cache (`map/cache_directory`) at a
/// second temporary directory, so that a `MainWindow` never reaches the tile server and never
/// reads or fills a tile cache outside the run.

#include "app_settings.hpp"

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <catch2/catch_session.hpp>

/// Creates the offscreen `QApplication`, isolates the settings and the tile cache, and runs the
/// Catch2 session.
///
/// @param argc Number of command-line arguments.
/// @param argv Command-line arguments; Qt removes the options it recognises, and the rest are
///     passed to Catch2 (test names, tags such as `"[.screenshot]"`, reporter options).
/// @return The exit code of the Catch2 session: 0 when every selected test passed, non-zero
///     when a test failed.
int main(int argc, char* argv[]) {
    // Set before the QApplication exists, which reads it to choose the platform plugin. It
    // overrides any value in the environment, so a run from a desktop session opens no windows.
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NMEASimulatorX-tests"));
    QApplication::setOrganizationName(QStringLiteral("NMEASimulatorX-tests"));

    QTemporaryDir settings_directory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_directory.path());

    QTemporaryDir cache_directory;
    {
        nmeasim::app::AppSettings settings;
        settings.set_map_online(false);
        settings.set_map_cache_directory(cache_directory.path());
    }

    return Catch::Session().run(argc, argv);
}
