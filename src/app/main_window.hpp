#pragma once

#include <QMainWindow>

namespace nmeasim::app {

/// Top-level window of the desktop application.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace nmeasim::app
