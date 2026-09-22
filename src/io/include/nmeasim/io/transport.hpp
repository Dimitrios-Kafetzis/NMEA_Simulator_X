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

class Transport : public QObject {
    Q_OBJECT

public:
    enum class State {
        Closed,
        Opening,
        Open,
        Failed,
    };
    Q_ENUM(State)

    explicit Transport(QObject* parent = nullptr);

    /// Human-readable summary such as "TCP server on port 10110".
    [[nodiscard]] virtual QString description() const = 0;

    /// Starts listening, connecting or opening. Returns false when that fails immediately;
    /// asynchronous failures are reported through `error_occurred` and `state_changed`.
    virtual bool open() = 0;
    virtual void close() = 0;

    /// Sends one line to every connected consumer. Silently drops data while not open.
    virtual void write(const QByteArray& line) = 0;

    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] bool is_open() const noexcept { return state_ == State::Open; }
    [[nodiscard]] QString last_error() const { return last_error_; }
    /// Number of consumers currently attached. Point-to-point transports report 1 when open.
    [[nodiscard]] virtual int client_count() const { return is_open() ? 1 : 0; }
    [[nodiscard]] qint64 bytes_written() const noexcept { return bytes_written_; }

signals:
    void state_changed(nmeasim::io::Transport::State state);
    void error_occurred(const QString& message);
    void client_count_changed(int count);

protected:
    void set_state(State state);
    /// Records an error, moves to the Failed state and emits `error_occurred`.
    void fail(const QString& message);
    void count_bytes(qint64 count) noexcept;

private:
    State state_{State::Closed};
    QString last_error_;
    qint64 bytes_written_{0};
};

[[nodiscard]] QString to_string(Transport::State state);

}  // namespace nmeasim::io
