#include "io/event_loop.hpp"

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/io/simulation_runner.hpp>
#include <nmeasim/io/transports/tcp_server_transport.hpp>

#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace std::chrono_literals;
using nmeasim::io::OutputConfig;
using nmeasim::io::Profile;
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
