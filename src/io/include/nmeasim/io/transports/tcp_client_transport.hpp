// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `TcpClientTransport`, the output that connects to a TCP server and reconnects by itself.
///
/// Profiles select it with the output type `tcp-client` and the keys `host`, `port` and
/// `reconnect_ms`.
///
/// @see docs/reference/transports.md

#pragma once

#include <nmeasim/io/transport.hpp>

#include <QTcpSocket>
#include <QTimer>

namespace nmeasim::io {

/// Connects to a TCP server, sends it every line, and reconnects automatically after a failed
/// attempt or a dropped connection.
///
/// The transport is in `State::Opening` during the first attempt and after the server
/// dropped the connection, and moves to `State::Open` when a connection is established. An
/// attempt that fails (the connection is refused, the host is not found, the attempt times
/// out) moves it to `State::Failed`, with `last_error` saying why; it stays there, retrying,
/// until an attempt succeeds. Every error, a peer closing the connection included, is
/// reported through `error_occurred` and followed by a new attempt after the reconnect
/// interval, until `close` is called. Lines written while disconnected are dropped, and data
/// received from the server is discarded.
class TcpClientTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a closed transport; no connection is attempted until `open`.
    ///
    /// @param host Host name or IP address of the server; a name is looked up when
    ///   connecting, and a failed lookup is retried like a refused connection.
    /// @param port TCP port of the server, for example 10110 for NMEA 0183.
    /// @param reconnect_interval_ms Delay in milliseconds between a failed attempt or a dropped
    ///   connection and the next attempt. The default of 2000 matches the profile default of
    ///   `reconnect_ms`.
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller.
    TcpClientTransport(QString host, quint16 port, int reconnect_interval_ms = 2000,
                       QObject* parent = nullptr);
    /// Destroys the transport, closing the connection first.
    ~TcpClientTransport() override;

    /// Returns the server's host and port.
    ///
    /// @return For example `TCP client to 127.0.0.1:10110`.
    [[nodiscard]] QString description() const override;
    /// Starts connecting to the server and keeps trying until `close`.
    ///
    /// Moves to `State::Open` at once when the socket is still connected, otherwise to
    /// `State::Opening` and starts a connection attempt; the move to `State::Open` follows
    /// asynchronously once the server accepts.
    ///
    /// @return Always true: connection failures are asynchronous; they are reported through
    ///   `state_changed` and `error_occurred`, and trigger a new attempt.
    bool open() override;
    /// Stops reconnecting, aborts the connection and moves to `State::Closed`.
    ///
    /// Aborting discards data queued in the socket that has not been sent yet.
    void close() override;
    /// Queues `line` on the connected socket.
    ///
    /// The line is dropped unless the transport is open and the socket connected. The
    /// transport sets no limit on how much can be queued when the server reads slowly.
    ///
    /// @param line One complete line, terminator included.
    void write(const QByteArray& line) override;

private:
    /// Starts a connection attempt, unless the transport is closed or the socket is not in
    /// the unconnected state. Connected to the timeout of `reconnect_timer_`.
    void connect_now();
    /// Starts the reconnect timer, unless the transport is closed or the timer is already
    /// running; a disconnection and an error reported for the same event therefore cause
    /// one attempt only.
    void schedule_reconnect();

    /// The connection to the server; reused for every attempt.
    QTcpSocket socket_;
    /// Single-shot timer whose interval is the reconnect delay; its timeout calls
    /// `connect_now`.
    QTimer reconnect_timer_;
    /// Host name or IP address of the server.
    QString host_;
    /// TCP port of the server.
    quint16 port_;
    /// True between `open` and `close`: the transport should be connected and keeps
    /// reconnecting.
    bool wanted_open_{false};
};

}  // namespace nmeasim::io
