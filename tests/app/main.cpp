// Test runner for the desktop application. A QApplication with the offscreen platform lets
// widgets be created and driven without a display, and QSettings is redirected to a temporary
// directory so that tests never touch the operator's preferences.
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NMEASimulatorX-tests"));
    QApplication::setOrganizationName(QStringLiteral("NMEASimulatorX-tests"));

    QTemporaryDir settings_directory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_directory.path());

    return Catch::Session().run(argc, argv);
}
