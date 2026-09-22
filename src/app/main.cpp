#include "app_settings.hpp"
#include "main_window.hpp"

#include <nmeasim/core/version.hpp>

#include <QApplication>
#include <QFile>
#include <QString>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NMEASimulatorX"));
    QApplication::setApplicationDisplayName(QStringLiteral("NMEA Simulator X"));
    QApplication::setOrganizationName(QStringLiteral("NMEASimulatorX"));
    QApplication::setOrganizationDomain(QStringLiteral("dimitrios-kafetzis.github.io"));
    QApplication::setApplicationVersion(QString::fromUtf8(
        nmeasim::core::kVersion.data(), static_cast<qsizetype>(nmeasim::core::kVersion.size())));

    nmeasim::app::MainWindow window;
    window.show();

    nmeasim::app::AppSettings settings;
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1 && QFile::exists(arguments.at(1))) {
        window.load_profile(arguments.at(1));
    } else if (!settings.last_profile_path().isEmpty() &&
               QFile::exists(settings.last_profile_path())) {
        window.load_profile(settings.last_profile_path());
    }
    if (settings.autostart()) {
        window.start();
    }
    return QApplication::exec();
}
