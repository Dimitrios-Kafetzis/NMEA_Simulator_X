#include "main_window.hpp"

#include <nmeasim/core/version.hpp>

#include <QApplication>
#include <QString>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NMEASimulatorX"));
    QApplication::setApplicationDisplayName(QStringLiteral("NMEA Simulator X"));
    QApplication::setOrganizationName(QStringLiteral("NMEASimulatorX"));
    QApplication::setOrganizationDomain(QStringLiteral("dimitrios-kafetzis.github.io"));
    QApplication::setApplicationVersion(QString::fromUtf8(
        nmeasim::core::kVersion.data(), static_cast<int>(nmeasim::core::kVersion.size())));

    nmeasim::app::MainWindow window;
    window.show();
    return QApplication::exec();
}
