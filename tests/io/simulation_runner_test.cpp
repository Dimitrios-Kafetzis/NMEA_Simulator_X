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

QString fixture(const char* relative) {
    return QStringLiteral(NMEASIM_FIXTURES_DIR "/") + QLatin1String(relative);
}

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
    CHECK(emitted.count() == runner.sentences_emitted());
    CHECK(runner.outputs()[0].sentences_sent >= all_lines.size());
    CHECK(runner.outputs()[1].sentences_sent < runner.outputs()[0].sentences_sent);
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
    wait_until([] { return false; }, 150);
    CHECK(runner.sentences_emitted() == before);

    runner.resume();
    CHECK_FALSE(runner.is_paused());
    REQUIRE(wait_until([&] { return runner.sentences_emitted() > before; }));
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
    CHECK(runner.duration() == 12min + 500ms);
    CHECK(runner.position() == 0ms);
    CHECK(runner.simulation()->state().navigation.position.latitude_deg == Catch::Approx(37.9));
    // Environment values come from the profile seed.
    CHECK(runner.simulation()->state().water.depth_below_transducer_m == Catch::Approx(12.4));

    QSignalSpy ticks(&runner, &SimulationRunner::ticked);
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
    CHECK(runner.simulation()->state().navigation.speed_over_ground_kn == Catch::Approx(6.5));

    // Seeking skips the rest of the first round; resuming plays the remaining three rounds.
    runner.seek(400ms);
    CHECK(runner.position() == 400ms);
    QSignalSpy finished(&runner, &SimulationRunner::finished);
    QSignalSpy stopped(&runner, &SimulationRunner::stopped);
    runner.resume();
    REQUIRE(wait_until([&] { return stopped.count() == 1; }, 5000));
    CHECK(finished.count() == 1);
    CHECK(runner.sentences_emitted() == 2 + 24);

    const auto original = read_sentences(fixture("logs/plain.nmea"));
    const auto replayed = read_sentences(file.path);
    REQUIRE(replayed.size() == 26);
    CHECK(replayed[0] == original[0]);
    CHECK(replayed[1] == original[1]);
    CHECK(replayed[2] == original[8]);
    CHECK(replayed.back() == original.back());
    const auto depths = read_sentences(depth_only.path);
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
    CHECK(runner.outputs()[1].sentences_sent == static_cast<qint64>(lines.size()));
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
    REQUIRE(wait_until([&] { return runner.outputs()[0].sentences_sent >= 3; }, 5000));
    runner.stop();

    const auto packets = read_sentences(viewsync.path);
    REQUIRE(packets.size() >= 3);
    CHECK(packets[0].starts_with("0,37.98"));
    CHECK(packets[1].starts_with("1,37.98"));
    CHECK(packets[0].ends_with(",mars"));
    CHECK(std::count(packets[0].begin(), packets[0].end(), ',') == 9);

    const auto lines = read_sentences(tagged.path);
    REQUIRE_FALSE(lines.empty());
    bool baro_seen = false;
    for (const auto& line : lines) {
        CHECK(line.starts_with("\\s:GP0001,c:"));
        const auto sentence = line.substr(line.find('\\', 1) + 1);
        CHECK(nmeasim::core::nmea0183::verify_checksum(sentence));
        CHECK((sentence.starts_with("$GPRMC,") || sentence == "$IIXDR,P,1.013,B,BARO*6F"));
        baro_seen = baro_seen || sentence.starts_with("$IIXDR");
    }
    CHECK(baro_seen);
}
