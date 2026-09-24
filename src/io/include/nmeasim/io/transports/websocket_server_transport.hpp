// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `WebSocketServerTransport`, the output that accepts WebSocket clients and sends every line
/// as a text frame.
///
/// Profiles select it with the output type `websocket-server` and the keys `bind_address` and
/// `port`; the command-line option `--websocket` adds one. With the `signalk` encoding it
/// serves Signal K deltas, and port 3000 matches the default of a Signal K server.
///
/// @see docs/reference/transports.md
/// @see https://signalk.org/specification/1.7.0/doc/streaming_api.html

#pragma once

#include <nmeasim/io/transport.hpp>

#include <QHostAddress>
#include <QList>
#include <QString>
#include <QWebSocket>
#include <QWebSocketServer>

#include <functional>
#include <utility>

namespace nmeasim::io {

/// Accepts WebSocket clients and sends every line to all of them as a text frame.
///
/// The server speaks plain `ws://`, without TLS. A greeting can be configured, as a fixed text
/// or as a function called for each client; it is sent to each client as soon as it
/// connects, before any line. `SimulationRunner` sets a function that builds the Signal K
/// `hello` message as the greeting of outputs with the `signalk` encoding. Clients that
/// disconnect or report a socket error are removed at once, messages received from clients
/// are ignored, and `client_count_changed` is emitted whenever a client is added or removed.
/// Errors while accepting a connection are reported through `error_occurred` and leave the
/// server open.
class WebSocketServerTransport final : public Transport {
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
    explicit WebSocketServerTransport(quint16 port, QHostAddress bind_address = QHostAddress::Any,
                                      QObject* parent = nullptr);
    /// Destroys the transport, closing it and every client connection first.
    ~WebSocketServerTransport() override;

    /// Returns the bind address and the port.
    ///
    /// @return For example `WebSocket server on 127.0.0.1:3000`, with the port actually
    ///   listened on while open.
    [[nodiscard]] QString description() const override;
    /// Starts listening on the bind address and port.
    ///
    /// Returns true at once when already open. Fails with `Cannot listen on <address>:<port>`
    /// and the reason, for example when the port is already in use.
    ///
    /// @return True when the server listens, false when listening failed.
    bool open() override;
    /// Closes every client connection, stops listening and moves to `State::Closed`.
    ///
    /// Emits `client_count_changed` with 0, even when no client was connected.
    void close() override;
    /// Sends `line` as one text frame to every connected client.
    ///
    /// The line is decoded as UTF-8 and sent with its terminator. Does nothing when no client
    /// is connected.
    ///
    /// @param line One complete line or message, terminator included, encoded in UTF-8.
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

    /// Sets the text sent to every client immediately after it connects.
    ///
    /// Applies to clients that connect afterwards; clients already connected do not receive
    /// it. Replaces a function set by `set_greeting_function`.
    ///
    /// @param greeting The greeting, sent as one text frame; empty disables it.
    void set_greeting(QString greeting) {
        greeting_ = std::move(greeting);
        greeting_function_ = nullptr;
    }
    /// Sets a function that builds the greeting of each client when it connects.
    ///
    /// The function is called once per client, on the transport's thread, just before the
    /// greeting is sent, so that a greeting carrying a time or a state is current. Applies to
    /// clients that connect afterwards and replaces a text set by `set_greeting`.
    ///
    /// @param greeting Returns the greeting, sent as one text frame; an empty result sends
    ///   none. An empty function disables the greeting.
    void set_greeting_function(std::function<QString()> greeting) {
        greeting_function_ = std::move(greeting);
        greeting_.clear();
    }
    /// Returns the greeting a client connecting now would receive.
    ///
    /// @return The result of the function set by `set_greeting_function` when one is set,
    ///   otherwise the text set by `set_greeting`; empty when neither is set.
    [[nodiscard]] QString greeting() const {
        return greeting_function_ ? greeting_function_() : greeting_;
    }

private:
    /// Takes every pending connection from the server, starts tracking it, sends it the
    /// greeting and emits `client_count_changed`. Connected to
    /// `QWebSocketServer::newConnection`.
    void accept_connections();
    /// Stops tracking `client`, schedules its deletion and emits `client_count_changed`.
    ///
    /// Connected to the `disconnected` and `errorOccurred` signals of every client; a client
    /// that is no longer tracked is ignored, so a client reporting both is removed once.
    ///
    /// @param client The client socket to remove.
    void drop_client(QWebSocket* client);

    /// The listening server, in non-secure mode, identifying itself as `NMEA Simulator X`
    /// during the handshake.
    QWebSocketServer server_;
    /// Connected clients, in order of connection; deleted with `deleteLater` when removed.
    QList<QWebSocket*> clients_;
    /// Port passed to the constructor; 0 for a port chosen by the operating system.
    quint16 requested_port_;
    /// Local address to listen on.
    QHostAddress bind_address_;
    /// Text sent to each client on connection; empty for none, or when `greeting_function_`
    /// is set.
    QString greeting_;
    /// Builds the greeting of each client on connection; empty to use `greeting_` instead.
    std::function<QString()> greeting_function_;
};

}  // namespace nmeasim::io
