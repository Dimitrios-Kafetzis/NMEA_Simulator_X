#pragma once

#include <nmeasim/io/transport.hpp>

#include <QTcpSocket>
#include <QTimer>

namespace nmeasim::io {

/// Connects to a TCP server and reconnects automatically after a disconnection.
class TcpClientTransport final : public Transport {
    Q_OBJECT

public:
    /// Connects to `host`:`port` when opened, retrying `reconnect_interval_ms` milliseconds
    /// after a failed attempt or a dropped connection.
    TcpClientTransport(QString host, quint16 port, int reconnect_interval_ms = 2000,
                       QObject* parent = nullptr);
    ~TcpClientTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;

private:
    void connect_now();
    void schedule_reconnect();

    QTcpSocket socket_;
    QTimer reconnect_timer_;
    QString host_;
    quint16 port_;
    bool wanted_open_{false};
};

}  // namespace nmeasim::io
