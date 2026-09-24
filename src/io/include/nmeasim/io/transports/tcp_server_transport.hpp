// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `TcpServerTransport`, the output that listens for TCP clients and sends every line to all
/// of them.
///
/// Profiles select it with the output type `tcp-server` and the keys `bind_address` and
/// `port`; the command-line option `--tcp-server` adds one. Port 10110 is the convention for
/// NMEA 0183 over TCP, used by OpenCPN and most gateways.
///
/// @see docs/reference/transports.md

#pragma once

#include <nmeasim/io/transport.hpp>

#include <QHostAddress>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>

namespace nmeasim::io {

/// Listens for TCP clients and sends every line to all of them.
///
/// Any number of clients may connect; each receives every line written after it connected.
/// A client that disconnects or reports a socket error is removed at once, and data received
/// from clients is discarded, since the simulator only talks. `client_count_changed` is
/// emitted whenever a client is added or removed. Errors while accepting a connection are
/// reported through `error_occurred` and leave the server open.
class TcpServerTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a closed server; it starts listening in `open`.
    ///
    /// @param port TCP port to listen on; 0 asks the operating system for a free port, which
    ///   `port` returns once the server is open.
    /// @param bind_address Local address to listen on. The default `QHostAddress::Any`
    ///   accepts connections on every interface, IPv4 and IPv6; `QHostAddress::LocalHost`
    ///   accepts local connections only.
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller.
    explicit TcpServerTransport(quint16 port, QHostAddress bind_address = QHostAddress::Any,
                                QObject* parent = nullptr);
    /// Destroys the transport, closing it and every client connection first.
    ~TcpServerTransport() override;

    /// Returns the bind address and the port.
    ///
    /// @return For example `TCP server on 127.0.0.1:10110`, with the port actually listened
    ///   on while open.
    [[nodiscard]] QString description() const override;
    /// Starts listening on the bind address and port.
    ///
    /// Returns true at once when already open. Fails with `Cannot listen on <address>:<port>`
    /// and the reason, for example when the port is already in use or needs privileges.
    ///
    /// @return True when the server listens, false when listening failed.
    bool open() override;
    /// Closes every client connection, stops listening and moves to `State::Closed`.
    ///
    /// Emits `client_count_changed` with 0, even when no client was connected.
    void close() override;
    /// Queues `line` on the socket of every connected client.
    ///
    /// Does nothing when no client is connected. Each socket sends asynchronously; the
    /// transport sets no limit on how much a slow client can have queued.
    ///
    /// @param line One complete line, terminator included.
    void write(const QByteArray& line) override;
    /// Returns the number of connected clients.
    ///
    /// @return The clients accepted and not yet removed; 0 while closed.
    [[nodiscard]] int client_count() const override;

    /// Returns the TCP port of the server.
    ///
    /// @return The port actually listened on while open, which is the one chosen by the
    ///   operating system when 0 was requested; otherwise the requested port.
    [[nodiscard]] quint16 port() const;

private:
    /// Takes every pending connection from the server, starts tracking it and emits
    /// `client_count_changed`. Connected to `QTcpServer::newConnection`.
    void accept_connections();
    /// Stops tracking `client`, schedules its deletion and emits `client_count_changed`.
    ///
    /// Connected to the `disconnected` and `errorOccurred` signals of every client; a client
    /// that is no longer tracked is ignored, so a client reporting both is removed once.
    ///
    /// @param client The client socket to remove; owned by `server_`.
    void drop_client(QTcpSocket* client);

    /// The listening socket; the Qt parent of every accepted client socket.
    QTcpServer server_;
    /// Connected clients, in order of connection. The sockets are owned by `server_` and
    /// deleted with `deleteLater` when removed.
    QList<QTcpSocket*> clients_;
    /// Port passed to the constructor; 0 for a port chosen by the operating system.
    quint16 requested_port_;
    /// Local address to listen on.
    QHostAddress bind_address_;
};

}  // namespace nmeasim::io
