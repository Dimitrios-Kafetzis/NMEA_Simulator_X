// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the `nmeasim::io` transports and of network interface enumeration.
///
/// Covers `nmeasim::io::TcpServerTransport`, `nmeasim::io::TcpClientTransport`,
/// `nmeasim::io::UdpTransport`, `nmeasim::io::WebSocketServerTransport`,
/// `nmeasim::io::FileTransport`, `nmeasim::io::SerialTransport` and
/// `nmeasim::io::LogTransport`: opening and closing, state changes and errors, the bytes each
/// one delivers, client counts, reconnection, and the `nmeasim::io::to_string` names of
/// transport states and UDP modes. It also checks `nmeasim::io::ipv4_interfaces` and
/// `nmeasim::io::find_ipv4_interface`.
///
/// The network tests (tagged `[integration]`) talk to Qt sockets over the loopback interface
/// and bind to port 0, so that the operating system picks a free port and parallel test runs
/// do not collide. Files are written to temporary directories; the serial test uses a device
/// name that does not exist. The file reads no fixture.

#include "io/event_loop.hpp"

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/io/network_interfaces.hpp>
#include <nmeasim/io/transports/file_transport.hpp>
#include <nmeasim/io/transports/log_transport.hpp>
#include <nmeasim/io/transports/serial_transport.hpp>
#include <nmeasim/io/transports/tcp_client_transport.hpp>
#include <nmeasim/io/transports/tcp_server_transport.hpp>
#include <nmeasim/io/transports/udp_transport.hpp>
#include <nmeasim/io/transports/websocket_server_transport.hpp>

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QWebSocket>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using nmeasim::io::Transport;
using nmeasim::test::wait_until;

namespace {

/// The line the transport tests write: a valid GLL sentence with its `CR LF` terminator.
///
/// It reports 49 degrees 16.45 minutes north, 123 degrees 11.12 minutes west at 22:54:44 UTC.
/// Its checksum `1D` is the XOR of the characters between `$` and `*`.
const QByteArray kLine{"$GPGLL,4916.45,N,12311.12,W,225444,A,*1D\r\n"};

}  // namespace

TEST_CASE("TCP server delivers lines to every client and tracks disconnects",
          "[io][transport][integration]") {
    // Port 0 lets the operating system pick a free port; `port()` reports it after `open()`.
    nmeasim::io::TcpServerTransport server(0, QHostAddress::LocalHost);
    QSignalSpy clients_changed(&server, &Transport::client_count_changed);
    REQUIRE(server.open());
    REQUIRE(server.is_open());
    REQUIRE(server.port() != 0);
    CHECK(server.description().contains(QStringLiteral("TCP server on 127.0.0.1:")));

    QTcpSocket first;
    QTcpSocket second;
    first.connectToHost(QHostAddress::LocalHost, server.port());
    second.connectToHost(QHostAddress::LocalHost, server.port());
    REQUIRE(wait_until([&] { return server.client_count() == 2; }));

    server.write(kLine);
    REQUIRE(wait_until([&] { return first.bytesAvailable() > 0 && second.bytesAvailable() > 0; }));
    CHECK(first.readAll() == kLine);
    CHECK(second.readAll() == kLine);
    CHECK(server.bytes_written() == 2 * kLine.size());

    first.disconnectFromHost();
    REQUIRE(wait_until([&] { return server.client_count() == 1; }));
    // One emission for each of the two connections and one for the disconnection.
    CHECK(clients_changed.count() >= 3);

    server.close();
    CHECK(server.state() == Transport::State::Closed);
    CHECK(server.client_count() == 0);
    REQUIRE(wait_until([&] { return second.state() == QAbstractSocket::UnconnectedState; }));
}

TEST_CASE("TCP server reports a port that is already in use", "[io][transport][integration]") {
    // The first server takes a free port chosen by the operating system (port 0); the second
    // asks for that same port.
    nmeasim::io::TcpServerTransport first(0, QHostAddress::LocalHost);
    REQUIRE(first.open());
    nmeasim::io::TcpServerTransport second(first.port(), QHostAddress::LocalHost);
    QSignalSpy errors(&second, &Transport::error_occurred);
    CHECK_FALSE(second.open());
    CHECK(second.state() == Transport::State::Failed);
    CHECK(errors.count() == 1);
    CHECK(second.last_error().contains(QStringLiteral("Cannot listen")));
}

TEST_CASE("TCP client connects, sends and reconnects", "[io][transport][integration]") {
    QTcpServer peer;
    // Port 0 lets the operating system pick a free port for the peer.
    REQUIRE(peer.listen(QHostAddress::LocalHost, 0));

    // A reconnect interval of 100 ms keeps the reconnection below well inside the wait.
    nmeasim::io::TcpClientTransport client(QStringLiteral("127.0.0.1"), peer.serverPort(), 100);
    REQUIRE(client.open());
    REQUIRE(wait_until([&] { return peer.hasPendingConnections(); }));
    auto* accepted = peer.nextPendingConnection();
    REQUIRE(wait_until([&] { return client.is_open(); }));
    CHECK(client.client_count() == 1);

    client.write(kLine);
    REQUIRE(wait_until([&] { return accepted->bytesAvailable() > 0; }));
    CHECK(accepted->readAll() == kLine);

    // The peer drops the connection; the client must come back by itself.
    accepted->close();
    REQUIRE(wait_until([&] { return client.state() == Transport::State::Opening; }));
    REQUIRE(wait_until([&] { return peer.hasPendingConnections(); }));
    auto* reconnected = peer.nextPendingConnection();
    REQUIRE(wait_until([&] { return client.is_open(); }));
    client.write(kLine);
    REQUIRE(wait_until([&] { return reconnected->bytesAvailable() > 0; }));

    client.close();
    CHECK(client.state() == Transport::State::Closed);
    CHECK(client.client_count() == 0);
}

TEST_CASE("TCP client reports a refused connection and keeps trying",
          "[io][transport][integration]") {
    quint16 port = 0;
    {
        // Port 0 lets the operating system pick a free port; closing the server leaves a port
        // on which connections are refused.
        QTcpServer closed;
        REQUIRE(closed.listen(QHostAddress::LocalHost, 0));
        port = closed.serverPort();
    }
    // A reconnect interval of 100 ms keeps the retries well inside the waits.
    nmeasim::io::TcpClientTransport client(QStringLiteral("127.0.0.1"), port, 100);
    QSignalSpy errors(&client, &Transport::error_occurred);
    REQUIRE(client.open());
    // Windows retries a refused connection for about two seconds before reporting it, so the
    // waits are generous.
    REQUIRE(wait_until([&] { return client.state() == Transport::State::Failed; }, 15000));
    CHECK_FALSE(client.last_error().isEmpty());
    CHECK(errors.count() >= 1);
    // The client keeps retrying while failed, and connects once a server listens.
    REQUIRE(wait_until([&] { return errors.count() >= 2; }, 15000));
    QTcpServer peer;
    REQUIRE(peer.listen(QHostAddress::LocalHost, port));
    REQUIRE(wait_until([&] { return client.is_open(); }, 15000));
    client.close();
    CHECK(client.state() == Transport::State::Closed);
}

TEST_CASE("UDP unicast sends one datagram per line", "[io][transport][integration]") {
    QUdpSocket receiver;
    // Port 0 lets the operating system pick a free port for the receiver.
    REQUIRE(receiver.bind(QHostAddress::LocalHost, 0));

    nmeasim::io::UdpConfig config;
    config.mode = nmeasim::io::UdpConfig::Mode::Unicast;
    config.address = QStringLiteral("127.0.0.1");
    config.port = receiver.localPort();
    nmeasim::io::UdpTransport udp(config);
    REQUIRE(udp.open());
    CHECK(udp.destination() == QHostAddress(QHostAddress::LocalHost));
    CHECK(udp.description() == QStringLiteral("UDP unicast to 127.0.0.1:%1").arg(config.port));

    udp.write(kLine);
    udp.write(kLine);
    REQUIRE(wait_until([&] { return receiver.pendingDatagramSize() > 0; }));
    int datagrams = 0;
    while (receiver.hasPendingDatagrams()) {
        const auto datagram = receiver.receiveDatagram().data();
        CHECK(datagram == kLine);
        ++datagrams;
        // The second datagram may not have arrived when the first is read.
        if (datagrams < 2) {
            wait_until([&] { return receiver.hasPendingDatagrams(); }, 500);
        }
    }
    CHECK(datagrams == 2);
    udp.close();
    CHECK(udp.state() == Transport::State::Closed);
}

TEST_CASE("UDP broadcast without an address falls back to the limited broadcast address",
          "[io][transport]") {
    nmeasim::io::UdpConfig config;
    config.mode = nmeasim::io::UdpConfig::Mode::Broadcast;
    config.address.clear();
    config.interface_name = QStringLiteral("no-such-interface");
    nmeasim::io::UdpTransport udp(config);
    // Binding may or may not succeed depending on the host, but the destination is resolved
    // first and must be the limited broadcast address.
    (void)udp.open();
    CHECK(udp.destination() == QHostAddress(QHostAddress::Broadcast));
}

TEST_CASE("UDP rejects an invalid destination", "[io][transport]") {
    nmeasim::io::UdpConfig config;
    config.address = QStringLiteral("not an address");
    nmeasim::io::UdpTransport udp(config);
    CHECK_FALSE(udp.open());
    CHECK(udp.state() == Transport::State::Failed);
    CHECK(udp.last_error().contains(QStringLiteral("Invalid UDP destination")));
}

TEST_CASE("UDP rejects an IPv6 destination", "[io][transport]") {
    nmeasim::io::UdpConfig config;
    config.address = QStringLiteral("::1");
    nmeasim::io::UdpTransport udp(config);
    QSignalSpy errors(&udp, &Transport::error_occurred);
    CHECK_FALSE(udp.open());
    CHECK(udp.state() == Transport::State::Failed);
    CHECK(errors.count() == 1);
    CHECK(udp.last_error().contains(QStringLiteral("IPv4")));
}

TEST_CASE("UDP multicast finds its interface by system or descriptive name",
          "[io][transport][integration]") {
    const auto interfaces = nmeasim::io::ipv4_interfaces();
    const auto loopback = std::find_if(interfaces.begin(), interfaces.end(),
                                       [](const auto& info) { return info.is_loopback; });
    REQUIRE(loopback != interfaces.end());
    for (const auto& name : {loopback->name, loopback->human_name}) {
        INFO(name.toStdString());
        nmeasim::io::UdpConfig config;
        config.mode = nmeasim::io::UdpConfig::Mode::Multicast;
        // 239.255.0.1 is in the administratively scoped multicast range of RFC 2365.
        config.address = QStringLiteral("239.255.0.1");
        config.interface_name = name;
        nmeasim::io::UdpTransport udp(config);
        QSignalSpy errors(&udp, &Transport::error_occurred);
        CHECK(udp.open());
        CHECK(errors.isEmpty());
    }
}

TEST_CASE("WebSocket server greets new clients and sends lines as text frames",
          "[io][transport][integration]") {
    // Port 0 lets the operating system pick a free port; `port()` reports it after `open()`.
    nmeasim::io::WebSocketServerTransport server(0, QHostAddress::LocalHost);
    server.set_greeting(QStringLiteral("{\"name\":\"hello\"}"));
    REQUIRE(server.open());
    REQUIRE(server.port() != 0);

    QWebSocket client;
    QStringList received;
    QObject::connect(&client, &QWebSocket::textMessageReceived,
                     [&](const QString& message) { received.append(message); });
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    REQUIRE(wait_until([&] { return server.client_count() == 1 && received.size() == 1; }));
    CHECK(received.first() == QStringLiteral("{\"name\":\"hello\"}"));

    server.write(kLine);
    REQUIRE(wait_until([&] { return received.size() == 2; }));
    CHECK(received.last() == QString::fromUtf8(kLine));

    client.close();
    REQUIRE(wait_until([&] { return server.client_count() == 0; }));
    server.close();
    CHECK(server.state() == Transport::State::Closed);
}

TEST_CASE("file transport appends lines and flushes immediately", "[io][transport]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("output.log"));

    {
        nmeasim::io::FileTransport file(path, false);
        REQUIRE(file.open());
        file.write(kLine);
        file.write(kLine);
        CHECK(file.bytes_written() == 2 * kLine.size());
        QFile reader(path);
        REQUIRE(reader.open(QIODevice::ReadOnly));
        CHECK(reader.readAll().count("$GPGLL") == 2);
    }
    {
        nmeasim::io::FileTransport file(path, true);
        REQUIRE(file.open());
        file.write(kLine);
    }
    {
        nmeasim::io::FileTransport file(path, false);
        REQUIRE(file.open());
        file.write(kLine);
    }
    // The second argument is `append`: the second transport adds a third line, then the third
    // transport truncates the file, so only its own line remains.
    QFile reader(path);
    REQUIRE(reader.open(QIODevice::ReadOnly));
    CHECK(reader.readAll().count("$GPGLL") == 1);

    nmeasim::io::FileTransport bad(directory.filePath(QStringLiteral("missing/dir/output.log")));
    CHECK_FALSE(bad.open());
    CHECK(bad.state() == Transport::State::Failed);
}

TEST_CASE("file transport writes the bytes as given and truncates only on its first open",
          "[io][transport]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("output.nmea"));
    {
        QFile existing(path);
        REQUIRE(existing.open(QIODevice::WriteOnly));
        existing.write("old contents\n");
    }
    nmeasim::io::FileTransport file(path, false);
    REQUIRE(file.open());
    file.write(kLine);
    // Stopping and starting a run closes and reopens the transport: the file is continued,
    // not emptied again.
    file.close();
    REQUIRE(file.open());
    file.write(kLine);
    file.close();
    QFile reader(path);
    // Binary mode on both sides: CR LF must reach the file unchanged on every platform.
    REQUIRE(reader.open(QIODevice::ReadOnly));
    CHECK(reader.readAll() == kLine + kLine);
}

TEST_CASE("serial transport fails cleanly on a missing device", "[io][transport]") {
    nmeasim::io::SerialConfig config;
    config.port_name = QStringLiteral("/dev/nmeasim-no-such-port");
    config.baud_rate = 38400;
    nmeasim::io::SerialTransport serial(config);
    CHECK(serial.description() ==
          QStringLiteral("Serial port /dev/nmeasim-no-such-port at 38400 baud"));
    CHECK_FALSE(serial.open());
    CHECK(serial.state() == Transport::State::Failed);
    CHECK(serial.last_error().contains(QStringLiteral("Cannot open serial port")));
    serial.write(kLine);
    CHECK(serial.bytes_written() == 0);
}

TEST_CASE("network interface enumeration includes loopback", "[io][network]") {
    const auto interfaces = nmeasim::io::ipv4_interfaces();
    bool loopback_found = false;
    for (const auto& info : interfaces) {
        if (info.is_loopback) {
            loopback_found = true;
            CHECK(info.address.isLoopback());
        }
        CHECK_FALSE(info.name.isEmpty());
    }
    CHECK(loopback_found);
    CHECK(nmeasim::io::find_ipv4_interface(QStringLiteral("no-such-interface")).address.isNull());
}

TEST_CASE("transport states have names", "[io][transport]") {
    CHECK(nmeasim::io::to_string(Transport::State::Closed) == QStringLiteral("closed"));
    CHECK(nmeasim::io::to_string(Transport::State::Open) == QStringLiteral("open"));
    CHECK(nmeasim::io::to_string(nmeasim::io::UdpConfig::Mode::Multicast) ==
          QStringLiteral("multicast"));
}

TEST_CASE("log transport writes a header once and timestamps every line", "[io][transport]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("record.log"));
    {
        nmeasim::io::LogTransport log(path, false);
        log.set_profile_name(QStringLiteral("Harbour"));
        CHECK(log.description() == QStringLiteral("Log %1").arg(path));
        REQUIRE(log.open());
        log.write(kLine);
        log.write(kLine);
        CHECK(log.lines_written() == 2);
        // The byte count includes the header and the timestamp in front of every line.
        CHECK(log.bytes_written() > 2 * kLine.size());
        log.close();
        // Reopening within the same recording continues the file without a second header.
        REQUIRE(log.open());
        log.write(kLine);
        CHECK(log.lines_written() == 3);
    }
    QFile reader(path);
    REQUIRE(reader.open(QIODevice::ReadOnly | QIODevice::Text));
    const auto text = reader.readAll().toStdString();
    std::string error;
    const auto parsed = nmeasim::core::log::parse_log(text, {}, &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->header.at("format") == "1");
    CHECK(parsed->header.at("profile") == "Harbour");
    CHECK(parsed->header.contains("recorded"));
    CHECK(parsed->timing == nmeasim::core::log::TimingSource::Timestamps);
    REQUIRE(parsed->entries.size() == 3);
    CHECK(parsed->entries[0].sentence == kLine.trimmed().toStdString());
    CHECK(parsed->entries[0].recorded_at.has_value());
    CHECK(text.find("# NMEA Simulator X log 1") == 0);
    CHECK(text.find("# NMEA Simulator X log 1", 1) == std::string::npos);

    // Truncating starts a fresh file with a new header, without a profile line because no
    // profile name is set; appending to a file keeps it.
    {
        nmeasim::io::LogTransport fresh(path, false);
        REQUIRE(fresh.open());
        fresh.write(kLine);
    }
    {
        nmeasim::io::LogTransport more(path, true);
        REQUIRE(more.open());
        more.write(kLine);
    }
    REQUIRE(reader.seek(0));
    const auto again = nmeasim::core::log::parse_log(reader.readAll().toStdString(), {}, &error);
    REQUIRE(again.has_value());
    CHECK(again->entries.size() == 2);
    CHECK_FALSE(again->header.contains("profile"));

    nmeasim::io::LogTransport bad(directory.filePath(QStringLiteral("missing/dir/record.log")));
    QSignalSpy errors(&bad, &Transport::error_occurred);
    CHECK_FALSE(bad.open());
    CHECK(bad.state() == Transport::State::Failed);
    CHECK(errors.count() == 1);
}
