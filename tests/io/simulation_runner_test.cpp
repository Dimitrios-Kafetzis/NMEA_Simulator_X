// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::io::SimulationRunner`, which runs a profile on a timer and feeds its
/// outputs.
///
/// Covers applying a profile, starting, pausing, resuming, stepping, seeking and stopping in
/// the delta, track and replay modes; output filters and disabled outputs; a failing output
/// that does not stop the run; the profile start time; recording to a log; and the Signal K,
/// ViewSync and TAG block encodings together with custom sentences.
///
/// Fixtures read from `tests/fixtures/`: `tracks/timestamped.gpx` (five timed points over
/// 12 min 0.5 s), `tracks/malformed.gpx`, `logs/plain.nmea` (four rounds of eight sentences,
/// 500 ms apart) and `logs/garbage.txt`; `tracks/missing.gpx` deliberately does not exist.
/// Network outputs bind to port 0 on the loopback interface, so that the operating system
/// picks free ports; file outputs write to temporary directories.

#include "io/event_loop.hpp"

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/simulation/replay_source.hpp>
#include <nmeasim/core/simulation/track_source.hpp>
#include <nmeasim/io/simulation_runner.hpp>
#include <nmeasim/io/transports/log_transport.hpp>
#include <nmeasim/io/transports/tcp_server_transport.hpp>
#include <nmeasim/io/transports/websocket_server_transport.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUrl>
#include <QWebSocket>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using nmeasim::io::OutputConfig;
using nmeasim::io::Profile;
using nmeasim::io::SimulationMode;
using nmeasim::io::SimulationRunner;
using nmeasim::test::wait_until;

namespace {

/// Returns the default profile with a fast tick, every registry sentence due every 100 ms and
/// no outputs.
///
/// The 20 ms tick and 100 ms periods keep the tests short: a few hundred milliseconds of wall
/// clock produce several rounds of sentences. Each registry sentence keeps its default enabled
/// state and an empty talker, which keeps the registry default. Tests add the outputs they
/// need.
///
/// @return A profile in delta mode, seeded by `nmeasim::io::Profile::default_profile`.
Profile fast_profile() {
    Profile profile = Profile::default_profile();
    profile.tick_ms = 20;
    profile.outputs.clear();
    for (const auto& descriptor :
         nmeasim::core::nmea0183::SentenceRegistry::standard().descriptors()) {
        profile.sentences[std::string{descriptor.id}] = {descriptor.enabled_by_default, "", 100ms};
    }
    return profile;
}

/// Returns the path of a test fixture.
///
/// @param relative Path relative to `tests/fixtures/`, such as `logs/plain.nmea`.
/// @return The absolute path, built from the `NMEASIM_FIXTURES_DIR` definition that
///   `tests/CMakeLists.txt` passes to the compiler. The file need not exist.
QString fixture(const char* relative) {
    return QStringLiteral(NMEASIM_FIXTURES_DIR "/") + QLatin1String(relative);
}

/// Reads the non-empty lines of a text file.
///
/// Fails the current test case through `REQUIRE` when the file cannot be opened.
///
/// @param path Path of the file, usually an output file written by the runner.
/// @return The lines in file order, with surrounding white space (including `CR`) removed;
///   blank lines are skipped.
std::vector<std::string> read_sentences(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    std::vector<std::string> lines;
    for (const auto& line : file.readAll().split('\n')) {
        if (!line.trimmed().isEmpty()) {
            lines.push_back(line.trimmed().toStdString());
        }
    }
    return lines;
}

}  // namespace

TEST_CASE("the runner streams filtered sentences to its outputs", "[io][runner][integration]") {
    Profile profile = fast_profile();
    OutputConfig everything;
    everything.type = OutputConfig::Type::TcpServer;
    // Port 0 lets the operating system pick a free port for each of the servers below.
    everything.port = 0;
    everything.bind_address = QStringLiteral("127.0.0.1");
    profile.outputs.append(everything);

    OutputConfig only_rmc = everything;
    only_rmc.filter = {QStringLiteral("RMC")};
    profile.outputs.append(only_rmc);

    OutputConfig disabled = everything;
    disabled.enabled = false;
    profile.outputs.append(disabled);

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    REQUIRE(runner.outputs().size() == 2);
    QSignalSpy started(&runner, &SimulationRunner::started);
    QSignalSpy ticks(&runner, &SimulationRunner::ticked);
    QSignalSpy emitted(&runner, &SimulationRunner::sentence_emitted);

    runner.start();
    CHECK(runner.is_running());
    CHECK(started.count() == 1);
    const auto* all =
        dynamic_cast<const nmeasim::io::TcpServerTransport*>(runner.outputs()[0].transport.get());
    const auto* rmc =
        dynamic_cast<const nmeasim::io::TcpServerTransport*>(runner.outputs()[1].transport.get());
    REQUIRE(all != nullptr);
    REQUIRE(rmc != nullptr);
    REQUIRE(all->is_open());
    REQUIRE(rmc->is_open());

    QTcpSocket all_client;
    QTcpSocket rmc_client;
    all_client.connectToHost(QHostAddress::LocalHost, all->port());
    rmc_client.connectToHost(QHostAddress::LocalHost, rmc->port());
    REQUIRE(wait_until([&] { return all->client_count() == 1 && rmc->client_count() == 1; }));

    // 15 ticks of 20 ms span at least three periods of the 100 ms sentences.
    REQUIRE(wait_until([&] { return ticks.count() >= 15; }, 5000));
    REQUIRE(wait_until(
        [&] { return all_client.bytesAvailable() > 0 && rmc_client.bytesAvailable() > 0; }));
    runner.stop();
    CHECK_FALSE(runner.is_running());

    const auto all_lines =
        QString::fromUtf8(all_client.readAll()).split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
    const auto rmc_lines =
        QString::fromUtf8(rmc_client.readAll()).split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
    CHECK(all_lines.size() > rmc_lines.size());
    CHECK_FALSE(rmc_lines.isEmpty());
    for (const auto& line : rmc_lines) {
        CHECK(line.startsWith(QStringLiteral("$GPRMC,")));
    }
    for (const auto& line : all_lines) {
        CHECK(nmeasim::core::nmea0183::verify_checksum(line.toStdString()));
    }
    CHECK(runner.sentences_emitted() >= all_lines.size());
    CHECK(emitted.count() == runner.sentences_emitted() + runner.state_messages_sent());
    CHECK(runner.outputs()[0].lines_sent >= all_lines.size());
    CHECK(runner.outputs()[1].lines_sent < runner.outputs()[0].lines_sent);
}

TEST_CASE("pausing stops emission and resuming continues it", "[io][runner][integration]") {
    Profile profile = fast_profile();
    OutputConfig file;
    file.type = OutputConfig::Type::File;
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    file.path = directory.filePath(QStringLiteral("run.log"));
    profile.outputs.append(file);

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    QSignalSpy paused(&runner, &SimulationRunner::paused_changed);
    runner.start();
    REQUIRE(wait_until([&] { return runner.sentences_emitted() > 0; }));

    runner.pause();
    CHECK(runner.is_paused());
    const auto before = runner.sentences_emitted();
    // A condition that never holds pumps the event loop for 150 ms, about seven ticks.
    wait_until([] { return false; }, 150);
    CHECK(runner.sentences_emitted() == before);

    runner.resume();
    CHECK_FALSE(runner.is_paused());
    REQUIRE(wait_until([&] { return runner.sentences_emitted() > before; }));
    // One emission for the pause and one for the resumption.
    CHECK(paused.count() == 2);

    QSignalSpy stopped(&runner, &SimulationRunner::stopped);
    runner.stop();
    CHECK(stopped.count() == 1);
    CHECK(runner.outputs().front().transport->state() == nmeasim::io::Transport::State::Closed);
}

TEST_CASE("a failing output is reported and the run continues on the others",
          "[io][runner][integration]") {
    Profile profile = fast_profile();
    OutputConfig bad_serial;
    bad_serial.type = OutputConfig::Type::Serial;
    bad_serial.serial.port_name = QStringLiteral("/dev/nmeasim-no-such-port");
    profile.outputs.append(bad_serial);
    OutputConfig tcp;
    tcp.type = OutputConfig::Type::TcpServer;
    // Port 0 lets the operating system pick a free port.
    tcp.port = 0;
    tcp.bind_address = QStringLiteral("127.0.0.1");
    profile.outputs.append(tcp);

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    QSignalSpy errors(&runner, &SimulationRunner::output_error);
    runner.start();
    CHECK(runner.is_running());
    CHECK(errors.count() == 1);
    CHECK(errors.first().at(0).toString().contains(QStringLiteral("Serial port")));
    CHECK(runner.outputs()[0].transport->state() == nmeasim::io::Transport::State::Failed);
    CHECK(runner.outputs()[1].transport->is_open());
    REQUIRE(wait_until([&] { return runner.sentences_emitted() > 0; }));
    runner.stop();
}

TEST_CASE("sentences and state messages are counted apart", "[io][runner]") {
    Profile profile = fast_profile();
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    OutputConfig nmea;
    nmea.type = OutputConfig::Type::File;
    nmea.path = directory.filePath(QStringLiteral("sentences.nmea"));
    profile.outputs.append(nmea);
    OutputConfig signalk = nmea;
    signalk.path = directory.filePath(QStringLiteral("signalk.jsonl"));
    signalk.encoding = OutputConfig::Encoding::SignalK;
    profile.outputs.append(signalk);
    OutputConfig viewsync = nmea;
    viewsync.path = directory.filePath(QStringLiteral("viewsync.txt"));
    viewsync.encoding = OutputConfig::Encoding::ViewSync;
    profile.outputs.append(viewsync);

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    QSignalSpy emitted(&runner, &SimulationRunner::sentence_emitted);
    runner.step();
    runner.stop();
    qint64 sentences = 0;
    qint64 messages = 0;
    for (const auto& call : emitted) {
        const auto id = call.at(0).toString();
        if (id == QStringLiteral("SIGNALK") || id == QStringLiteral("VIEWSYNC")) {
            ++messages;
        } else {
            ++sentences;
        }
    }
    // The first step sends every due sentence, one Signal K delta and one ViewSync packet.
    REQUIRE(sentences > 0);
    CHECK(messages == 2);
    CHECK(runner.sentences_emitted() == sentences);
    CHECK(runner.state_messages_sent() == messages);
    CHECK(runner.outputs()[0].lines_sent == sentences);
    CHECK(runner.outputs()[1].lines_sent == 1);
    CHECK(runner.outputs()[2].lines_sent == 1);

    // Applying a profile starts both counts again.
    REQUIRE(runner.apply_profile(profile, &error));
    CHECK(runner.sentences_emitted() == 0);
    CHECK(runner.state_messages_sent() == 0);
}

TEST_CASE("the simulated clock starts from the profile start time", "[io][runner]") {
    Profile profile = fast_profile();
    profile.start_time = QDateTime(QDate(2026, 9, 22), QTime(12, 34, 56), QTimeZone::utc());
    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    const auto expected = std::chrono::system_clock::time_point{
        std::chrono::milliseconds{profile.start_time->toMSecsSinceEpoch()}};
    CHECK(runner.simulation()->state().time_utc == expected);
}

TEST_CASE("a track profile drives a track source and ends the run at the last point",
          "[io][runner][track]") {
    Profile profile = fast_profile();
    profile.mode = SimulationMode::Track;
    profile.track.path = fixture("tracks/timestamped.gpx");
    profile.outputs.append([] {
        OutputConfig out;
        out.type = OutputConfig::Type::Stdout;
        out.enabled = false;
        return out;
    }());

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    auto* source =
        dynamic_cast<nmeasim::core::simulation::TrackSource*>(&runner.simulation()->source());
    REQUIRE(source != nullptr);
    // The fixture's points run from 10:00:00 to 10:12:00.500 UTC and start at 37.9 N.
    CHECK(runner.duration() == 12min + 500ms);
    CHECK(runner.position() == 0ms);
    CHECK(runner.simulation()->state().navigation.position.latitude_deg == Catch::Approx(37.9));
    // Environment values come from the profile seed.
    CHECK(runner.simulation()->state().water.depth_below_transducer_m == Catch::Approx(12.4));

    QSignalSpy ticks(&runner, &SimulationRunner::ticked);
    // The second point of the track is at 10:03:00, 37.91 N.
    runner.seek(3min);
    CHECK(ticks.count() == 1);
    CHECK(runner.position() == 3min);
    CHECK(runner.simulation()->state().navigation.position.latitude_deg ==
          Catch::Approx(37.91).margin(1e-9));

    QSignalSpy finished(&runner, &SimulationRunner::finished);
    QSignalSpy stopped(&runner, &SimulationRunner::stopped);
    runner.seek(*runner.duration() - 100ms);
    runner.start();
    REQUIRE(wait_until([&] { return stopped.count() == 1; }, 5000));
    CHECK(finished.count() == 1);
    CHECK_FALSE(runner.is_running());
    CHECK(runner.sentences_emitted() > 0);
    // The last point of the track is at 23.62 E.
    CHECK(runner.simulation()->state().navigation.position.longitude_deg ==
          Catch::Approx(23.62).margin(1e-9));

    // A missing or unreadable track file is a profile error, not a crash.
    profile.track.path = fixture("tracks/missing.gpx");
    CHECK_FALSE(runner.apply_profile(profile, &error));
    CHECK(error.contains(QStringLiteral("Cannot read")));
    profile.track.path = fixture("tracks/malformed.gpx");
    CHECK_FALSE(runner.apply_profile(profile, &error));
    CHECK(error.contains(QStringLiteral("Invalid XML")));
}

TEST_CASE("a replay profile re-sends the log, steps one sentence at a time and seeks",
          "[io][runner][replay][integration]") {
    Profile profile = fast_profile();
    profile.mode = SimulationMode::Replay;
    profile.replay.path = fixture("logs/plain.nmea");
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    OutputConfig file;
    file.type = OutputConfig::Type::File;
    file.path = directory.filePath(QStringLiteral("replayed.nmea"));
    file.append = false;
    profile.outputs.append(file);
    OutputConfig depth_only = file;
    depth_only.path = directory.filePath(QStringLiteral("depth.nmea"));
    depth_only.filter = {QStringLiteral("DPT")};
    profile.outputs.append(depth_only);

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    REQUIRE(dynamic_cast<nmeasim::core::simulation::ReplaySource*>(
                &runner.simulation()->source()) != nullptr);
    // The log's four rounds are time-stamped 10:00:00.00, 00.50, 01.00 and 01.50.
    CHECK(runner.duration() == 1500ms);

    // Stepping before the run starts it paused and emits exactly one recorded sentence.
    QSignalSpy emitted(&runner, &SimulationRunner::sentence_emitted);
    QSignalSpy paused(&runner, &SimulationRunner::paused_changed);
    runner.step();
    CHECK(runner.is_running());
    CHECK(runner.is_paused());
    CHECK(paused.count() == 1);
    REQUIRE(emitted.count() == 1);
    CHECK(emitted.first().at(0).toString() == QStringLiteral("RMC"));
    CHECK(emitted.first().at(1).toString().startsWith(QStringLiteral("$GPRMC,100000.00")));
    runner.step();
    CHECK(emitted.count() == 2);
    CHECK(emitted.last().at(0).toString() == QStringLiteral("GGA"));
    // The log's RMC sentences report 6.5 kn.
    CHECK(runner.simulation()->state().navigation.speed_over_ground_kn == Catch::Approx(6.5));

    // Seeking skips the rest of the first round; resuming plays the remaining three rounds.
    runner.seek(400ms);
    CHECK(runner.position() == 400ms);
    QSignalSpy finished(&runner, &SimulationRunner::finished);
    QSignalSpy stopped(&runner, &SimulationRunner::stopped);
    runner.resume();
    REQUIRE(wait_until([&] { return stopped.count() == 1; }, 5000));
    CHECK(finished.count() == 1);
    // The two stepped sentences plus the three remaining rounds of eight.
    CHECK(runner.sentences_emitted() == 2 + 24);

    const auto original = read_sentences(fixture("logs/plain.nmea"));
    const auto replayed = read_sentences(file.path);
    REQUIRE(replayed.size() == 26);
    CHECK(replayed[0] == original[0]);
    CHECK(replayed[1] == original[1]);
    // The first sentence of the second round.
    CHECK(replayed[2] == original[8]);
    CHECK(replayed.back() == original.back());
    const auto depths = read_sentences(depth_only.path);
    // One DPT per replayed round; the two steps of the first round sent only RMC and GGA.
    CHECK(depths.size() == 3);
    for (const auto& line : depths) {
        CHECK(line.starts_with("$SDDPT"));
    }

    profile.replay.path = fixture("logs/garbage.txt");
    CHECK_FALSE(runner.apply_profile(profile, &error));
    CHECK(error.contains(QStringLiteral("No valid NMEA sentence")));
}

TEST_CASE("stepping the delta simulation takes one tick and seeking is ignored", "[io][runner]") {
    Profile profile = fast_profile();
    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    CHECK_FALSE(runner.duration().has_value());
    const auto start = runner.simulation()->state().time_utc;
    runner.step();
    CHECK(runner.is_paused());
    // One tick of the 20 ms `tick_ms` of `fast_profile`.
    CHECK(runner.simulation()->state().time_utc - start == 20ms);
    CHECK(runner.sentences_emitted() > 0);
    runner.seek(5min);
    CHECK(runner.position() == 0ms);
    runner.step();
    CHECK(runner.simulation()->state().time_utc - start == 40ms);
    runner.stop();
    CHECK_FALSE(runner.is_paused());
}

TEST_CASE("recording writes every emitted sentence to a log next to the outputs",
          "[io][runner][integration]") {
    Profile profile = fast_profile();
    profile.name = QStringLiteral("Recorded run");
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.log"));

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    CHECK_FALSE(runner.is_recording());
    QSignalSpy recording(&runner, &SimulationRunner::recording_changed);
    REQUIRE(runner.set_recording(path));
    CHECK(runner.is_recording());
    CHECK(runner.recording_path() == path);
    CHECK(recording.count() == 1);

    runner.start();
    REQUIRE(wait_until([&] { return runner.sentences_emitted() >= 40; }, 5000));
    runner.stop();
    const auto first_run = runner.sentences_emitted();
    CHECK(runner.recorder()->lines_written() == first_run);
    CHECK(runner.recorder()->state() == nmeasim::io::Transport::State::Closed);

    // A second run continues the same file; the counter keeps counting across runs.
    runner.start();
    REQUIRE(wait_until([&] { return runner.sentences_emitted() >= first_run + 10; }, 5000));
    runner.stop();
    const auto total = runner.sentences_emitted();
    CHECK(total > first_run);
    CHECK(runner.recorder()->lines_written() == total);

    QFile reader(path);
    REQUIRE(reader.open(QIODevice::ReadOnly | QIODevice::Text));
    std::string parse_error;
    const auto log =
        nmeasim::core::log::parse_log(reader.readAll().toStdString(), {}, &parse_error);
    REQUIRE(log.has_value());
    CHECK(log->header.at("profile") == "Recorded run");
    CHECK(log->entries.size() == static_cast<std::size_t>(total));
    CHECK(log->timing == nmeasim::core::log::TimingSource::Timestamps);
    for (const auto& entry : log->entries) {
        CHECK(nmeasim::core::nmea0183::verify_checksum(entry.sentence));
    }

    REQUIRE(runner.set_recording({}));
    CHECK_FALSE(runner.is_recording());
    CHECK(recording.count() == 2);
    CHECK(recording.last().at(0).toString().isEmpty());

    // An unwritable path is reported, and the run still works.
    QSignalSpy errors(&runner, &SimulationRunner::output_error);
    runner.start();
    CHECK_FALSE(runner.set_recording(directory.filePath(QStringLiteral("no/such/dir/x.log"))));
    CHECK(errors.count() == 1);
    CHECK(runner.is_running());
    runner.stop();
}

TEST_CASE("Signal K outputs greet with hello and send deltas on their own period",
          "[io][runner][signalk][integration]") {
    Profile profile = fast_profile();
    profile.delta.seed.destination =
        nmeasim::core::model::Destination{"AEGINA", {37.7466, 23.4275}, {38.0, 23.7}};
    OutputConfig websocket;
    websocket.type = OutputConfig::Type::WebSocketServer;
    // Port 0 lets the operating system pick a free port.
    websocket.port = 0;
    websocket.bind_address = QStringLiteral("127.0.0.1");
    websocket.encoding = OutputConfig::Encoding::SignalK;
    websocket.period_ms = 100;
    websocket.signalk.context = "aircraft.urn:mrn:signalk:uuid:test";
    profile.outputs.append(websocket);
    OutputConfig wind_only;
    wind_only.type = OutputConfig::Type::File;
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    wind_only.path = directory.filePath(QStringLiteral("wind.jsonl"));
    wind_only.encoding = OutputConfig::Encoding::SignalK;
    wind_only.period_ms = 100;
    wind_only.filter = {QStringLiteral("environment.wind")};
    profile.outputs.append(wind_only);

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    QSignalSpy emitted(&runner, &SimulationRunner::sentence_emitted);
    runner.start();
    const auto* server = dynamic_cast<const nmeasim::io::WebSocketServerTransport*>(
        runner.outputs()[0].transport.get());
    REQUIRE(server != nullptr);
    REQUIRE(server->is_open());
    CHECK(server->greeting().startsWith(QStringLiteral("{\"name\":\"NMEASimulatorX\"")));
    CHECK(server->greeting().contains(
        QStringLiteral("\"self\":\"aircraft.urn:mrn:signalk:uuid:test\"")));

    QWebSocket client;
    QStringList received;
    QObject::connect(&client, &QWebSocket::textMessageReceived, &client,
                     [&received](const QString& message) { received.append(message); });
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server->port())));
    // The greeting followed by at least three deltas.
    REQUIRE(wait_until([&] { return received.size() >= 4; }, 5000));
    runner.stop();
    CHECK(received.first() == server->greeting());
    const auto delta = QJsonDocument::fromJson(received.at(1).toUtf8());
    REQUIRE(delta.isObject());
    CHECK(delta.object().value(QStringLiteral("context")).toString() ==
          QStringLiteral("aircraft.urn:mrn:signalk:uuid:test"));
    const auto values = delta.object()
                            .value(QStringLiteral("updates"))
                            .toArray()
                            .first()
                            .toObject()
                            .value(QStringLiteral("values"))
                            .toArray();
    QStringList paths;
    for (const auto& value : values) {
        paths.append(value.toObject().value(QStringLiteral("path")).toString());
    }
    // Cross-track error is present because the seed has a destination; `port` is the Signal K
    // identifier of the default profile's "Port engine".
    CHECK(paths.contains(QStringLiteral("navigation.position")));
    CHECK(paths.contains(QStringLiteral("navigation.courseRhumbline.crossTrackError")));
    CHECK(paths.contains(QStringLiteral("propulsion.port.revolutions")));

    // The filtered file output carries wind paths only, and the console signal saw deltas.
    const auto lines = read_sentences(wind_only.path);
    REQUIRE_FALSE(lines.empty());
    for (const auto& line : lines) {
        CHECK(line.find("environment.wind.") != std::string::npos);
        CHECK(line.find("navigation.") == std::string::npos);
    }
    bool signalk_seen = false;
    for (const auto& call : emitted) {
        signalk_seen = signalk_seen || call.at(0).toString() == QStringLiteral("SIGNALK");
    }
    CHECK(signalk_seen);
    CHECK(runner.outputs()[1].lines_sent == static_cast<qint64>(lines.size()));
}

TEST_CASE("ViewSync, TAG blocks and custom sentences reach the outputs",
          "[io][runner][integration]") {
    Profile profile = fast_profile();
    profile.custom_sentences = {{"BARO", "$IIXDR,P,1.013,B,BARO", 100ms, true}};
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    OutputConfig viewsync;
    viewsync.type = OutputConfig::Type::File;
    viewsync.path = directory.filePath(QStringLiteral("viewsync.txt"));
    viewsync.encoding = OutputConfig::Encoding::ViewSync;
    viewsync.period_ms = 100;
    viewsync.viewsync.planet = "mars";
    profile.outputs.append(viewsync);
    OutputConfig tagged;
    tagged.type = OutputConfig::Type::File;
    tagged.path = directory.filePath(QStringLiteral("tagged.nmea"));
    tagged.tag_block.enabled = true;
    tagged.tag_block.options.source = "GP0001";
    tagged.filter = {QStringLiteral("RMC"), QStringLiteral("BARO")};
    profile.outputs.append(tagged);

    SimulationRunner runner;
    QString error;
    REQUIRE(runner.apply_profile(profile, &error));
    runner.start();
    REQUIRE(wait_until([&] { return runner.outputs()[0].lines_sent >= 3; }, 5000));
    runner.stop();

    const auto packets = read_sentences(viewsync.path);
    REQUIRE(packets.size() >= 3);
    // Each packet starts with its counter and the latitude of the default seed, 37.9838, and
    // has ten comma-separated fields ending with the planet.
    CHECK(packets[0].starts_with("0,37.98"));
    CHECK(packets[1].starts_with("1,37.98"));
    CHECK(packets[0].ends_with(",mars"));
    CHECK(std::count(packets[0].begin(), packets[0].end(), ',') == 9);

    const auto lines = read_sentences(tagged.path);
    REQUIRE_FALSE(lines.empty());
    bool baro_seen = false;
    // Every line starts with a TAG block holding the source and a UNIX time; the sentence
    // follows its closing backslash. `6F` is the checksum of the custom body.
    for (const auto& line : lines) {
        CHECK(line.starts_with("\\s:GP0001,c:"));
        const auto sentence = line.substr(line.find('\\', 1) + 1);
        CHECK(nmeasim::core::nmea0183::verify_checksum(sentence));
        CHECK((sentence.starts_with("$GPRMC,") || sentence == "$IIXDR,P,1.013,B,BARO*6F"));
        baro_seen = baro_seen || sentence.starts_with("$IIXDR");
    }
    CHECK(baro_seen);
}
