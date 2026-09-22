#pragma once

#include <nmeasim/io/transport.hpp>

#include <QHostAddress>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>

namespace nmeasim::io {

/// Listens for TCP clients and sends every line to all of them.
class TcpServerTransport final : public Transport {
    Q_OBJECT

public:
    /// `port` 0 asks the operating system for a free port; see `port()` after opening.
    explicit TcpServerTransport(quint16 port, QHostAddress bind_address = QHostAddress::Any,
                                QObject* parent = nullptr);
    ~TcpServerTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;
    [[nodiscard]] int client_count() const override;

    /// The port actually listened on.
    [[nodiscard]] quint16 port() const;

private:
    void accept_connections();
    void drop_client(QTcpSocket* client);

    QTcpServer server_;
    QList<QTcpSocket*> clients_;
    quint16 requested_port_;
    QHostAddress bind_address_;
};

}  // namespace nmeasim::io
