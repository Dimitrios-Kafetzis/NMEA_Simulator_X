// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Entry point of `nmeasim_cli_tests`, the Catch2 suite that runs the command-line tool.
///
/// The suite has its own `main` instead of Catch2's default one because the tests start
/// `nmeasim` with `QProcess`, whose pipes and process notifications need the event dispatcher
/// that a `QCoreApplication` sets up for the main thread. The application is created before
/// Catch2 runs any test case and lives until the session ends.

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
