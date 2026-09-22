#include "main_window.hpp"

#include <nmeasim/core/version.hpp>

#include <QLabel>
#include <QStatusBar>
#include <QString>

namespace nmeasim::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("NMEA Simulator X"));
    setMinimumSize(640, 400);

    auto* placeholder = new QLabel(
        QStringLiteral("NMEA Simulator X %1\n\nThe simulator dashboard arrives in milestone M2.")
            .arg(QString::fromUtf8(core::kVersion.data(), static_cast<int>(core::kVersion.size()))),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);

    statusBar()->showMessage(QStringLiteral("Ready"));
}

}  // namespace nmeasim::app
