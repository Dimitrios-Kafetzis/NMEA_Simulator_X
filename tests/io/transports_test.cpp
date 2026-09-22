#include "io/event_loop.hpp"

#include <nmeasim/io/network_interfaces.hpp>
#include <nmeasim/io/transports/file_transport.hpp>
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

using nmeasim::io::Transport;
using nmeasim::test::wait_until;

namespace {

const QByteArray kLine{"$GPGLL,4916.45,N,12311.12,W,225444,A,*1D\r\n"};

}  // namespace

TEST_CASE("TCP server delivers lines to every client and tracks disconnects",
          "[io][transport][integration]") {
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
    CHECK(clients_changed.count() >= 3);

    server.close();
    CHECK(server.state() == Transport::State::Closed);
    CHECK(server.client_count() == 0);
    REQUIRE(wait_until([&] { return second.state() == QAbstractSocket::UnconnectedState; }));
}

TEST_CASE("TCP server reports a port that is already in use", "[io][transport][integration]") {
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
    REQUIRE(peer.listen(QHostAddress::LocalHost, 0));

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

TEST_CASE("UDP unicast sends one datagram per line", "[io][transport][integration]") {
    QUdpSocket receiver;
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

TEST_CASE("WebSocket server greets new clients and sends lines as text frames",
          "[io][transport][integration]") {
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
    QFile reader(path);
    REQUIRE(reader.open(QIODevice::ReadOnly));
    CHECK(reader.readAll().count("$GPGLL") == 1);

    nmeasim::io::FileTransport bad(directory.filePath(QStringLiteral("missing/dir/output.log")));
    CHECK_FALSE(bad.open());
    CHECK(bad.state() == Transport::State::Failed);
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
