// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim`, the command-line tool (`src/cli/main.cpp`), run as a separate process.
///
/// Covers the exit status of `--help` and `--version`, the status 2 of every command-line
/// error, whether CLI11 or the tool itself finds it, the strict reading of `--udp`,
/// `--serial`, `--destination`, `--duration` and `--speed`, that valid values are still
/// accepted, that `--encoding`, `--tag-block` and `--tag-source` apply to a profile's outputs,
/// that `--track` and `--replay` keep the profile's loop and timestamp settings unless a flag
/// is given, and that a subnet broadcast address selects UDP broadcast. Every run is short: it
/// either fails while parsing or streams for a fraction of a second to standard output, a
/// temporary log file or a UDP port (on the loopback interface, or broadcast on a local
/// subnet). The file reads the fixtures `tracks/timestamped.gpx` and `logs/plain.nmea` from
/// `tests/fixtures` and writes profiles into temporary directories.

#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
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

/// Writes the built-in default profile, changed by a function, to a file.
///
/// The profile is taken from `nmeasim profile show`, so it is the one the tool itself uses.
/// Fails the running test case when the tool does not print it.
///
/// @param directory Directory the file is written to, as `profile.json`.
/// @param change Function that edits the profile's JSON object before it is written.
/// @return The path of the written file.
QString write_profile(const QTemporaryDir& directory,
                      const std::function<void(QJsonObject&)>& change) {
    const auto shown = run_cli({QStringLiteral("profile"), QStringLiteral("show")});
    REQUIRE(shown.exit_code == 0);
    QJsonObject profile = QJsonDocument::fromJson(shown.out.toUtf8()).object();
    REQUIRE_FALSE(profile.isEmpty());
    change(profile);
    const QString path = directory.filePath(QStringLiteral("profile.json"));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(profile).toJson());
    return path;
}

/// Sets one key of a nested object of a profile.
///
/// @param profile The profile's JSON object.
/// @param section Key of the object below `simulation`, such as `track`.
/// @param key Key inside that object, such as `loop`.
/// @param value The new value.
void set_simulation_key(QJsonObject& profile, const QString& section, const QString& key,
                        const QJsonValue& value) {
    QJsonObject simulation = profile.value(QStringLiteral("simulation")).toObject();
    QJsonObject object = simulation.value(section).toObject();
    object.insert(key, value);
    simulation.insert(section, object);
    profile.insert(QStringLiteral("simulation"), simulation);
}

/// Reads the length of the source from the status line of a run that was not quiet.
///
/// @param err Standard error of the run, which holds a line such as
///   `running profile 'Default' following track.gpx (12.3 s) with a 100 ms tick`.
/// @return The length in seconds as printed, such as `12.3`, as a standard string so that
///   Catch2 can print it; empty when there is none.
std::string source_length(const QString& err) {
    static const QRegularExpression pattern(QStringLiteral(R"(\(([0-9.]+) s[,)])"));
    return pattern.match(err).captured(1).toStdString();
}

/// Returns the subnet broadcast address of a local interface that can broadcast.
///
/// @return The first IPv4 subnet broadcast address, other than `255.255.255.255`, of an
///   interface that is up, running, able to broadcast and not the loopback; null when there
///   is none.
QHostAddress subnet_broadcast_address() {
    for (const auto& interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            !flags.testFlag(QNetworkInterface::CanBroadcast) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto& entry : interface.addressEntries()) {
            const QHostAddress broadcast = entry.broadcast();
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !broadcast.isNull() &&
                broadcast != QHostAddress(QHostAddress::Broadcast)) {
                return broadcast;
            }
        }
    }
    return {};
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

TEST_CASE("encoding and TAG block options apply to the outputs of a profile", "[cli]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString recording = directory.filePath(QStringLiteral("recording.log"));
    const QString profile = write_profile(directory, [&recording](QJsonObject& json) {
        json.insert(QStringLiteral("outputs"),
                    QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("stdout")}},
                               QJsonObject{{QStringLiteral("type"), QStringLiteral("log")},
                                           {QStringLiteral("path"), recording},
                                           {QStringLiteral("append"), false}}});
    });
    const QStringList run = {
        QStringLiteral("run"),     QStringLiteral("--profile"),  profile,
        QStringLiteral("--quiet"), QStringLiteral("--duration"), QStringLiteral("0.3")};

    SECTION("without the options the outputs keep their own settings") {
        const auto result = run_cli(run);
        CHECK(result.exit_code == 0);
        CHECK(result.out.startsWith(QLatin1Char('$')));
    }
    SECTION("--encoding changes the profile's outputs but not its recording") {
        const auto result =
            run_cli(run + QStringList{QStringLiteral("--encoding"), QStringLiteral("signalk")});
        CHECK(result.exit_code == 0);
        CHECK(result.out.startsWith(QLatin1Char('{')));
        CHECK(result.out.contains(QStringLiteral("\"updates\"")));
        QFile log(recording);
        REQUIRE(log.open(QIODevice::ReadOnly));
        CHECK(log.readAll().contains("$GPRMC,"));
    }
    SECTION("--tag-block and --tag-source prefix the profile's outputs") {
        const auto result =
            run_cli(run + QStringList{QStringLiteral("--tag-block"), QStringLiteral("--tag-source"),
                                      QStringLiteral("GP0001")});
        CHECK(result.exit_code == 0);
        CHECK(result.out.startsWith(QStringLiteral("\\s:GP0001")));
    }
    SECTION("an unknown encoding is an error without an output option too") {
        const auto result =
            run_cli(run + QStringList{QStringLiteral("--encoding"), QStringLiteral("morse")});
        CHECK(result.exit_code == 2);
        CHECK(result.err.contains(QStringLiteral("--encoding")));
    }
}

TEST_CASE("track and replay options keep the profile's loop and timestamp settings unless given",
          "[cli]") {
    const QString track = fixture("tracks/timestamped.gpx");
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString profile = write_profile(directory, [](QJsonObject& json) {
        set_simulation_key(json, QStringLiteral("track"), QStringLiteral("loop"), true);
        set_simulation_key(json, QStringLiteral("track"), QStringLiteral("use_timestamps"), false);
        set_simulation_key(json, QStringLiteral("replay"), QStringLiteral("loop"), true);
    });
    // Not quiet, so that the status line shows the length of the source and whether it loops.
    const QStringList run = {QStringLiteral("run"), QStringLiteral("--stdout"),
                             QStringLiteral("--duration"), QStringLiteral("0.3")};

    const auto timed = run_cli(run + QStringList{QStringLiteral("--track"), track});
    const auto untimed = run_cli(
        run + QStringList{QStringLiteral("--track"), track, QStringLiteral("--ignore-timestamps")});
    REQUIRE(timed.exit_code == 0);
    REQUIRE(untimed.exit_code == 0);
    // The fixture's timestamps give it another length than the track speed does.
    REQUIRE_FALSE(source_length(timed.err).empty());
    REQUIRE(source_length(timed.err) != source_length(untimed.err));
    CHECK_FALSE(timed.err.contains(QStringLiteral(", looping)")));

    const auto from_profile = run_cli(
        run + QStringList{QStringLiteral("--profile"), profile, QStringLiteral("--track"), track});
    CHECK(from_profile.exit_code == 0);
    CHECK(from_profile.err.contains(QStringLiteral(", looping)")));
    CHECK(source_length(from_profile.err) == source_length(untimed.err));

    const auto replay =
        run_cli(run + QStringList{QStringLiteral("--profile"), profile, QStringLiteral("--replay"),
                                  fixture("logs/plain.nmea")});
    CHECK(replay.exit_code == 0);
    CHECK(replay.err.contains(QStringLiteral(", looping)")));

    const auto flag =
        run_cli(run + QStringList{QStringLiteral("--track"), track, QStringLiteral("--loop")});
    CHECK(flag.exit_code == 0);
    CHECK(flag.err.contains(QStringLiteral(", looping)")));
}

TEST_CASE("broadcast addresses select UDP broadcast", "[cli]") {
    const QStringList run = {QStringLiteral("run"), QStringLiteral("--duration"),
                             QStringLiteral("0.2"), QStringLiteral("--udp")};
    const auto limited = run_cli(run + QStringList{QStringLiteral("255.255.255.255:40002")});
    CHECK(limited.err.contains(QStringLiteral("output: UDP broadcast to 255.255.255.255:40002")));

    const QHostAddress subnet = subnet_broadcast_address();
    if (subnet.isNull()) {
        SKIP("No local interface has a subnet broadcast address");
    }
    const QString target = subnet.toString() + QStringLiteral(":40002");
    INFO(target.toStdString());
    const auto result = run_cli(run + QStringList{target});
    CHECK(result.err.contains(QStringLiteral("output: UDP broadcast to ") + target));
    CHECK(result.exit_code == 0);
}
