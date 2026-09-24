// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `WebSocketServerTransport` on `QWebSocketServer`: accepting and greeting
/// clients and sending text frames.

#include <nmeasim/io/transports/websocket_server_transport.hpp>

#include <utility>

namespace nmeasim::io {

WebSocketServerTransport::WebSocketServerTransport(quint16 port, QHostAddress bind_address,
                                                   QObject* parent)
    : Transport(parent),
      server_(QStringLiteral("NMEA Simulator X"), QWebSocketServer::NonSecureMode),
      requested_port_(port),
      bind_address_(std::move(bind_address)) {
    connect(&server_, &QWebSocketServer::newConnection, this,
            &WebSocketServerTransport::accept_connections);
    connect(&server_, &QWebSocketServer::acceptError, this,
            [this](QAbstractSocket::SocketError) { emit error_occurred(server_.errorString()); });
}

WebSocketServerTransport::~WebSocketServerTransport() {
    WebSocketServerTransport::close();
}

QString WebSocketServerTransport::description() const {
    return QStringLiteral("WebSocket server on %1:%2").arg(bind_address_.toString()).arg(port());
}

bool WebSocketServerTransport::open() {
    if (is_open()) {
        return true;
    }
    set_state(State::Opening);
    if (!server_.listen(bind_address_, requested_port_)) {
        fail(QStringLiteral("Cannot listen on %1:%2: %3")
                 .arg(bind_address_.toString())
                 .arg(requested_port_)
                 .arg(server_.errorString()));
        return false;
    }
    set_state(State::Open);
    return true;
}

void WebSocketServerTransport::close() {
    for (auto* client : std::exchange(clients_, {})) {
        // Detached first so that closing the client does not reach drop_client, which would
        // emit a client count for every client.
        client->disconnect(this);
        client->close();
        client->deleteLater();
    }
    if (server_.isListening()) {
        server_.close();
    }
    set_state(State::Closed);
    emit client_count_changed(0);
}

void WebSocketServerTransport::write(const QByteArray& line) {
    if (!is_open()) {
        return;
    }
    const QString text = QString::fromUtf8(line);
    for (auto* client : clients_) {
        if (client->isValid()) {
            count_bytes(client->sendTextMessage(text));
        }
    }
}

int WebSocketServerTransport::client_count() const {
    return static_cast<int>(clients_.size());
}

quint16 WebSocketServerTransport::port() const {
    return server_.isListening() ? server_.serverPort() : requested_port_;
}

void WebSocketServerTransport::accept_connections() {
    while (auto* client = server_.nextPendingConnection()) {
        clients_.append(client);
        connect(client, &QWebSocket::disconnected, this, [this, client] { drop_client(client); });
        connect(client, &QWebSocket::errorOccurred, this,
                [this, client](QAbstractSocket::SocketError) { drop_client(client); });
        if (const QString text = greeting(); !text.isEmpty()) {
            count_bytes(client->sendTextMessage(text));
        }
        emit client_count_changed(client_count());
    }
}

void WebSocketServerTransport::drop_client(QWebSocket* client) {
    if (!clients_.removeAll(client)) {
        return;
    }
    client->disconnect(this);
    client->deleteLater();
    emit client_count_changed(client_count());
}

}  // namespace nmeasim::io
