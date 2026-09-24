// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// A helper that lets the `nmeasim::io` tests wait for asynchronous Qt events.
///
/// Sockets, servers and timers in `nmeasim::io` only make progress while the Qt event loop
/// runs, and a Catch2 test case runs outside any event loop. `nmeasim::test::wait_until`
/// pumps the loop of the `QCoreApplication` that the suite's `main` creates until a
/// condition holds, so that a test can wait for a connection, a datagram or a timer tick
/// without sleeping for a fixed time.

#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>

#include <functional>

namespace nmeasim::test {

/// Pumps the Qt event loop until a condition holds or a timeout expires.
///
/// Evaluates `condition` first, so a condition that already holds returns at once without
/// processing any event. Otherwise it processes pending events for up to 20 milliseconds at a
/// time and evaluates `condition` again after each round. Passing a condition that never holds
/// turns the call into a wait of `timeout_ms` during which events are still delivered.
///
/// @param condition Predicate evaluated on the calling thread between rounds of event
///   processing; it must be callable repeatedly and should be cheap.
/// @param timeout_ms Wall-clock time in milliseconds after which the wait gives up; 3000 ms
///   by default. The timeout is checked between rounds of event processing, so the call can
///   overrun it by one round.
/// @return True as soon as `condition` returns true; false when it still returns false after
///   `timeout_ms` milliseconds have elapsed.
/// @note Events are delivered only for the calling thread, which in this suite is the thread
///   of the `QCoreApplication` created in `main`.
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
