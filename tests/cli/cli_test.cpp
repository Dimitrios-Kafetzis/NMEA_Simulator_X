// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim`, the command-line tool (`src/cli/main.cpp`), run as a separate process.
///
/// Covers the exit status of `--help` and `--version`, the status 2 of every command-line
/// error, whether CLI11 or the tool itself finds it, the strict reading of `--udp`,
/// `--serial`, `--destination`, `--duration` and `--speed`, and that valid values are still
/// accepted. Every run is short: it either fails while parsing or streams for a fraction of a
/// second to standard output or to a UDP port on the loopback interface. The file reads the
/// fixtures `tracks/timestamped.gpx` and `logs/plain.nmea` from `tests/fixtures`.

#include <QList>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace {

/// What one run of the command-line tool left behind.
struct CliRun {
    /// Exit status of the process; -1 when it could not be started, crashed or did not finish
    /// in time.
    int exit_code{-1};
    /// Everything the tool wrote to standard output.
    QString out;
    /// Everything the tool wrote to standard error.
    QString err;
};

/// Runs the `nmeasim` that this build produced and waits for it to end.
///
/// The executable is the one named by the `NMEASIM_CLI_PATH` compile definition, which
/// `tests/CMakeLists.txt` sets to the location of the `nmeasim_cli` target. Standard input is
/// closed at once. A run that does not end within the timeout is killed.
///
/// @param arguments The command-line arguments, without the program name.
/// @param timeout_ms How long to wait for the process to end, in milliseconds.
/// @return The exit status and the captured output.
CliRun run_cli(const QStringList& arguments, int timeout_ms = 30000) {
    QProcess process;
    process.start(QStringLiteral(NMEASIM_CLI_PATH), arguments);
    CliRun result;
    if (!process.waitForStarted()) {
        return result;
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(timeout_ms)) {
        process.kill();
        process.waitForFinished();
    } else if (process.exitStatus() == QProcess::NormalExit) {
        result.exit_code = process.exitCode();
    }
    result.out = QString::fromUtf8(process.readAllStandardOutput());
    result.err = QString::fromUtf8(process.readAllStandardError());
    return result;
}

/// Returns the absolute path of a file in the test fixtures directory.
///
/// @param relative Path below `tests/fixtures`, for example `"logs/plain.nmea"`.
/// @return The path, built from the `NMEASIM_FIXTURES_DIR` compile definition.
QString fixture(const char* relative) {
    return QStringLiteral(NMEASIM_FIXTURES_DIR "/") + QLatin1String(relative);
}

/// Returns `run` with the options of a short, quiet run that writes to standard output.
///
/// @param extra Options appended after the common ones; they must not repeat `--duration`,
///   which CLI11 refuses to take twice.
/// @return `run --stdout --quiet --duration 0.3` followed by `extra`.
QStringList quick_run(const QStringList& extra) {
    return QStringList{QStringLiteral("run"), QStringLiteral("--stdout"), QStringLiteral("--quiet"),
                       QStringLiteral("--duration"), QStringLiteral("0.3")} +
           extra;
}

}  // namespace

TEST_CASE("help and version exit with status 0", "[cli]") {
    const QList<QStringList> cases = {
        {QStringLiteral("--help")},
        {QStringLiteral("--version")},
        {QStringLiteral("run"), QStringLiteral("--help")},
        {QStringLiteral("profile"), QStringLiteral("init"), QStringLiteral("--help")},
    };
    for (const auto& arguments : cases) {
        INFO(arguments.join(QLatin1Char(' ')).toStdString());
        const auto result = run_cli(arguments);
        CHECK(result.exit_code == 0);
        CHECK_FALSE(result.out.isEmpty());
    }
    CHECK(run_cli({QStringLiteral("--version")}).out.startsWith(QStringLiteral("NMEASimulatorX ")));
}

TEST_CASE("every command-line error exits with status 2", "[cli]") {
    const QString missing = fixture("tracks/no-such-file.gpx");
    const QString track = fixture("tracks/timestamped.gpx");
    const QString log = fixture("logs/plain.nmea");
    // Before the fix, CLI11 ended these with codes of its own from 100 upwards.
    const QList<QStringList> cases = {
        {QStringLiteral("--bogus")},
        {QStringLiteral("no-such-subcommand")},
        {QStringLiteral("profile")},
        {QStringLiteral("profile"), QStringLiteral("init")},
        {QStringLiteral("run"), QStringLiteral("--bogus")},
        {QStringLiteral("run"), QStringLiteral("--duration"), QStringLiteral("abc")},
        {QStringLiteral("run"), QStringLiteral("--tcp-server"), QStringLiteral("70000")},
        {QStringLiteral("run"), QStringLiteral("--profile"), missing},
        {QStringLiteral("run"), QStringLiteral("--track"), missing},
        {QStringLiteral("run"), QStringLiteral("--speed"), QStringLiteral("5")},
        {QStringLiteral("run"), QStringLiteral("--track"), track, QStringLiteral("--replay"), log},
        {QStringLiteral("run"), QStringLiteral("--replay"), log,
         QStringLiteral("--replay-interval"), QStringLiteral("0")},
        {QStringLiteral("run"), QStringLiteral("--tag-source"), QStringLiteral("GP0001")},
    };
    for (const auto& arguments : cases) {
        INFO(arguments.join(QLatin1Char(' ')).toStdString());
        const auto result = run_cli(arguments);
        CHECK(result.exit_code == 2);
        CHECK_FALSE(result.err.isEmpty());
    }
}

TEST_CASE("malformed and non-finite values exit with status 2", "[cli]") {
    const QString track = fixture("tracks/timestamped.gpx");
    // Each argument list is a run that, when wrongly accepted, streams for 0.3 s and ends
    // with status 0 (or 3 when the serial device cannot be opened).
    const QList<std::pair<QStringList, QString>> cases = {
        {{QStringLiteral("--udp"), QStringLiteral("127.0.0.1:40001x")}, QStringLiteral("--udp")},
        {{QStringLiteral("--udp"), QStringLiteral("127.0.0.1: 40001")}, QStringLiteral("--udp")},
        {{QStringLiteral("--udp"), QStringLiteral("127.0.0.1:+40001")}, QStringLiteral("--udp")},
        {{QStringLiteral("--udp"), QStringLiteral(":40001")}, QStringLiteral("--udp")},
        {{QStringLiteral("--serial"), QStringLiteral("nmeasim-no-such-device@4800baud")},
         QStringLiteral("--serial")},
        {{QStringLiteral("--serial"), QStringLiteral("@4800")}, QStringLiteral("--serial")},
        {{QStringLiteral("--destination"), QStringLiteral("nan,23.4")},
         QStringLiteral("--destination")},
        {{QStringLiteral("--destination"), QStringLiteral("37.7,inf")},
         QStringLiteral("--destination")},
        {{QStringLiteral("--track"), track, QStringLiteral("--speed"), QStringLiteral("inf")},
         QStringLiteral("--speed")},
    };
    for (const auto& [extra, option] : cases) {
        INFO(extra.join(QLatin1Char(' ')).toStdString());
        const auto result = run_cli(quick_run(extra));
        CHECK(result.exit_code == 2);
        CHECK(result.err.contains(option));
    }
    // Without the check, these runs never end (nan) or overflow the duration timer (inf); the
    // timeout of run_cli then ends them with the status -1.
    for (const auto* duration : {"inf", "nan", "-inf"}) {
        INFO(duration);
        const auto result =
            run_cli({QStringLiteral("run"), QStringLiteral("--stdout"), QStringLiteral("--quiet"),
                     QStringLiteral("--duration"), QLatin1String(duration)},
                    10000);
        CHECK(result.exit_code == 2);
        CHECK(result.err.contains(QStringLiteral("--duration")));
    }
}

TEST_CASE("well-formed values are still accepted", "[cli]") {
    SECTION("a UDP destination on the loopback interface") {
        const auto result =
            run_cli(quick_run({QStringLiteral("--udp"), QStringLiteral("127.0.0.1:40001")}));
        CHECK(result.exit_code == 0);
    }
    SECTION("a destination with white space around its parts") {
        const auto result = run_cli(quick_run(
            {QStringLiteral("--destination"), QStringLiteral(" 37.7466 , 23.4275 ,AEGINA")}));
        CHECK(result.exit_code == 0);
        CHECK(result.out.contains(QStringLiteral("RMB,")));
        CHECK(result.out.contains(QStringLiteral("AEGINA")));
    }
    SECTION("a log that ends the run by itself") {
        const auto result =
            run_cli({QStringLiteral("run"), QStringLiteral("--stdout"), QStringLiteral("--quiet"),
                     QStringLiteral("--replay"), fixture("logs/plain.nmea")});
        CHECK(result.exit_code == 0);
        CHECK(result.out.count(QStringLiteral("$GPRMC,")) == 4);
    }
}
