// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The entry point of the `nmeasim_io_tests` suite, which runs Catch2 inside a Qt application.
///
/// The suite has its own `main` instead of Catch2's default one because the code under test,
/// `nmeasim::io`, needs a `QCoreApplication`: TCP, UDP and WebSocket sockets, their servers and
/// the timers of `nmeasim::io::SimulationRunner` and `nmeasim::io::TcpClientTransport` only
/// work with the event dispatcher that the application sets up for the main thread. The
/// application is created before Catch2 runs any test case and lives until the session ends;
/// the test cases drive its event loop with `nmeasim::test::wait_until` rather than
/// `QCoreApplication::exec`.

#include <QCoreApplication>

#include <catch2/catch_session.hpp>

/// Creates the Qt application and runs the Catch2 test session.
///
/// @param argc Number of command-line arguments, as passed by the operating system.
/// @param argv Command-line arguments. They reach both Qt and Catch2, so the usual Catch2
///   options (test name filters, tags, reporters) select and report the test cases.
/// @return The exit code of the Catch2 session: 0 when every selected test case passes,
///   non-zero otherwise.
int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    return Catch::Session().run(argc, argv);
}
