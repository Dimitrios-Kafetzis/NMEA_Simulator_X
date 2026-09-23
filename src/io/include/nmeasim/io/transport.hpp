#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/// Base class of every output channel the simulator can write to.
///
/// A transport carries complete lines (NMEA sentences or Signal K messages, terminator
/// included) from the simulator to one or more consumers. Transports live on the thread that
/// runs the Qt event loop and report their status through signals so that both the CLI and
/// the desktop application can show it.
namespace nmeasim::io {

/// Abstract output channel: TCP server or client, UDP, WebSocket server, serial port, file,
/// log or standard output.
class Transport : public QObject {
    Q_OBJECT

public:
    /// Life-cycle state, reported through `state_changed`.
    enum class State {
        /// Not opened yet, or closed.
        Closed,
        /// Opening, or waiting to reconnect (TCP client).
        Opening,
        /// Ready; `write` delivers data.
        Open,
        /// Opening failed or the device went away; `last_error` says why.
        Failed,
    };
    Q_ENUM(State)

    /// Creates a transport in the Closed state.
    explicit Transport(QObject* parent = nullptr);

    /// Human-readable summary such as "TCP server on port 10110".
    [[nodiscard]] virtual QString description() const = 0;

    /// Starts listening, connecting or opening. Returns false when that fails immediately;
    /// asynchronous failures are reported through `error_occurred` and `state_changed`.
    virtual bool open() = 0;
    /// Stops listening, disconnects or closes the file, then moves to the Closed state.
    /// Stops any automatic reconnection. Safe to call when already closed.
    virtual void close() = 0;

    /// Sends one line to every connected consumer. Silently drops data while not open.
    virtual void write(const QByteArray& line) = 0;

    /// Current life-cycle state.
    [[nodiscard]] State state() const noexcept { return state_; }
    /// True in the Open state.
    [[nodiscard]] bool is_open() const noexcept { return state_ == State::Open; }
    /// Message of the most recent failure; empty when none has occurred.
    [[nodiscard]] QString last_error() const { return last_error_; }
    /// Number of consumers currently attached. Point-to-point transports report 1 when open.
    [[nodiscard]] virtual int client_count() const { return is_open() ? 1 : 0; }
    /// Total bytes written to consumers since construction, greetings and headers included.
    [[nodiscard]] qint64 bytes_written() const noexcept { return bytes_written_; }

signals:
    /// Emitted when the state changes to `state`.
    void state_changed(nmeasim::io::Transport::State state);
    /// Emitted for every error, whether or not it moves the transport to Failed.
    void error_occurred(const QString& message);
    /// Emitted when a client connects or disconnects, with the new `count`.
    void client_count_changed(int count);

protected:
    /// Moves to `state`, emitting `state_changed` only when it differs from the current one.
    void set_state(State state);
    /// Records an error, moves to the Failed state and emits `error_occurred`.
    void fail(const QString& message);
    /// Adds `count` to `bytes_written`; negative values (write errors) are ignored.
    void count_bytes(qint64 count) noexcept;

private:
    State state_{State::Closed};
    QString last_error_;
    qint64 bytes_written_{0};
};

/// Lower-case name of `state`: `closed`, `opening`, `open` or `failed`.
[[nodiscard]] QString to_string(Transport::State state);

}  // namespace nmeasim::io
