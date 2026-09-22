#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>

#include <functional>

namespace nmeasim::test {

/// Pumps the Qt event loop until `condition` holds or `timeout_ms` elapses.
inline bool wait_until(const std::function<bool()>& condition, int timeout_ms = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeout_ms) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return true;
}

}  // namespace nmeasim::test
