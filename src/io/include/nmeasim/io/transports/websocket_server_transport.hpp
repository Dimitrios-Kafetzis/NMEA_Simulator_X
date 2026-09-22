#pragma once

#include <nmeasim/io/transport.hpp>

#include <QHostAddress>
#include <QList>
#include <QWebSocket>
#include <QWebSocketServer>

namespace nmeasim::io {

/// Accepts WebSocket clients and sends every line as a text frame.
///
/// A greeting can be configured; it is sent to each client on connection. Signal K uses this
/// for its `hello` message.
class WebSocketServerTransport final : public Transport {
    Q_OBJECT

public:
    explicit WebSocketServerTransport(quint16 port, QHostAddress bind_address = QHostAddress::Any,
                                      QObject* parent = nullptr);
    ~WebSocketServerTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;
    [[nodiscard]] int client_count() const override;

    [[nodiscard]] quint16 port() const;

    /// Text sent to every client immediately after it connects. Empty disables it.
    void set_greeting(QString greeting) { greeting_ = std::move(greeting); }
    [[nodiscard]] QString greeting() const { return greeting_; }

private:
    void accept_connections();
    void drop_client(QWebSocket* client);

    QWebSocketServer server_;
    QList<QWebSocket*> clients_;
    quint16 requested_port_;
    QHostAddress bind_address_;
    QString greeting_;
};

}  // namespace nmeasim::io
