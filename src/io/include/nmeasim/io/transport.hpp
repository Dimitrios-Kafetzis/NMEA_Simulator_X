// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `Transport` base class of every output channel, with its life-cycle `State`.
///
/// A transport carries complete lines (NMEA 0183 sentences, Signal K deltas or ViewSync
/// packets, terminator included) from the simulator to one or more consumers. The concrete
/// transports live in `nmeasim/io/transports/`; `SimulationRunner` builds one per enabled
/// profile output. Transports report their status through signals so that both the
/// command-line tool and the desktop application can show it.
///
/// @see docs/reference/transports.md

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace nmeasim::io {

/// Abstract output channel: TCP server or client, UDP, WebSocket server, serial port, file,
/// log or standard output.
///
/// A transport moves through the life cycle `State::Closed`, `State::Opening`,
/// `State::Open` and back to `State::Closed`, or to `State::Failed` when opening fails or the
/// device goes away. Only an open transport delivers what `write` is given; every
/// implementation silently drops lines in any other state. Transports send the payload as
/// given, byte for byte, except `LogTransport`, which records each line with a timestamp in
/// front and a single line feed at the end.
///
/// Implementations use Qt sockets, devices and timers, so a transport is used from the thread
/// that created it, and that thread runs a Qt event loop: asynchronous events (a client
/// connecting, a connection dropping, a reconnection timer) are processed there and the
/// corresponding signals are emitted there.
class Transport : public QObject {
    Q_OBJECT

public:
    /// Life-cycle state of a transport, reported through `state_changed`.
    enum class State {
        /// Not opened yet, or closed by `close`. The initial state.
        Closed,
        /// Opening, or waiting to reconnect (TCP client) after a connection attempt failed or
        /// the peer dropped the connection.
        Opening,
        /// Ready: `write` delivers data.
        Open,
        /// Opening failed or the device went away; `last_error` says why. `open` may be
        /// called again.
        Failed,
    };
    Q_ENUM(State)

    /// Creates a transport in the `State::Closed` state.
    ///
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller,
    ///   as `SimulationRunner` does with its `std::unique_ptr`.
    explicit Transport(QObject* parent = nullptr);

    /// Returns a human-readable summary of the channel and its settings.
    ///
    /// @return A one-line text such as `TCP server on 127.0.0.1:10110` or
    ///   `Serial port /dev/ttyUSB0 at 4800 baud`, used in error reports and status displays.
    [[nodiscard]] virtual QString description() const = 0;

    /// Starts listening, connecting or opening the device or file.
    ///
    /// On success the transport ends in `State::Open`, or in `State::Opening` when the
    /// connection is made asynchronously (TCP client). On failure it moves to
    /// `State::Failed` and emits `error_occurred`. Emits `state_changed` for every state it
    /// passes through.
    ///
    /// @return False when opening fails immediately; true otherwise. Asynchronous failures are
    ///   reported later through `error_occurred` and `state_changed`.
    virtual bool open() = 0;
    /// Stops listening, disconnects or closes the file, then moves to `State::Closed`.
    ///
    /// Stops any automatic reconnection and detaches every client. Safe to call in any state,
    /// including when already closed. Emits `state_changed` when the state changes.
    virtual void close() = 0;

    /// Sends one line to every connected consumer.
    ///
    /// Silently drops the line while the transport is not open. Write errors are not
    /// reported by the return value; transports that detect them report them through
    /// `error_occurred`.
    ///
    /// @param line One complete line, terminator included.
    virtual void write(const QByteArray& line) = 0;

    /// Returns the current life-cycle state.
    ///
    /// @return The state last set by the implementation; `State::Closed` initially.
    [[nodiscard]] State state() const noexcept { return state_; }
    /// Returns whether the transport is in `State::Open`.
    ///
    /// @return True in `State::Open`, false in every other state.
    [[nodiscard]] bool is_open() const noexcept { return state_ == State::Open; }
    /// Returns the message of the most recent failure.
    ///
    /// @return The message passed to the last call of `fail`; empty when none has occurred.
    ///   It is not cleared when the transport opens again.
    [[nodiscard]] QString last_error() const { return last_error_; }
    /// Returns the number of consumers currently attached.
    ///
    /// The default suits point-to-point transports; the TCP and WebSocket servers override
    /// it with their number of connected clients.
    ///
    /// @return 1 while open and 0 otherwise for point-to-point transports; the number of
    ///   connected clients for servers.
    [[nodiscard]] virtual int client_count() const { return is_open() ? 1 : 0; }
    /// Returns the total number of bytes written to consumers since construction.
    ///
    /// @return Bytes summed over every consumer, so a server with three clients counts each
    ///   line three times. Greetings and log headers are included; failed writes are not.
    [[nodiscard]] qint64 bytes_written() const noexcept { return bytes_written_; }

signals:
    /// Emitted when the life-cycle state changes, synchronously from inside `open`, `close`
    /// or `fail`, or from an asynchronous socket or device event.
    ///
    /// Not emitted when the new state equals the current one.
    ///
    /// @param state The new state.
    void state_changed(nmeasim::io::Transport::State state);
    /// Emitted for every error, whether or not it moves the transport to `State::Failed`.
    ///
    /// Emitted synchronously by `fail`, and by implementations for socket errors that do not
    /// end the transport, such as a TCP client losing its peer before reconnecting.
    ///
    /// @param message Human-readable description of the error.
    void error_occurred(const QString& message);
    /// Emitted when a client connects or disconnects.
    ///
    /// Only the servers (TCP and WebSocket) emit it; they also emit it with 0 from `close`,
    /// even when no client was attached.
    ///
    /// @param count The new number of connected clients.
    void client_count_changed(int count);

protected:
    /// Moves to `state`.
    ///
    /// Emits `state_changed` only when `state` differs from the current one.
    ///
    /// @param state The new life-cycle state.
    void set_state(State state);
    /// Records an error and moves to `State::Failed`.
    ///
    /// Stores `message` as `last_error`, then emits `state_changed` (unless the transport had
    /// already failed) and `error_occurred`, in that order.
    ///
    /// @param message Human-readable description of the failure.
    void fail(const QString& message);
    /// Adds a write result to `bytes_written`.
    ///
    /// @param count Bytes written, as returned by a Qt write call; zero and negative values
    ///   (write errors) are ignored.
    void count_bytes(qint64 count) noexcept;

private:
    /// Current life-cycle state; changed only through `set_state`.
    State state_{State::Closed};
    /// Message of the most recent failure; empty until `fail` is first called.
    QString last_error_;
    /// Bytes written since construction, summed over every consumer.
    qint64 bytes_written_{0};
};

/// Returns the lower-case name of a transport state, as the desktop application and the
/// reference pages show it.
///
/// @param state The state to name.
/// @return `closed`, `opening`, `open` or `failed`; `unknown` for a value outside the
///   enumeration.
[[nodiscard]] QString to_string(Transport::State state);

}  // namespace nmeasim::io
