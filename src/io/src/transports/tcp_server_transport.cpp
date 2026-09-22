#include <nmeasim/io/transports/tcp_server_transport.hpp>

#include <utility>

namespace nmeasim::io {

TcpServerTransport::TcpServerTransport(quint16 port, QHostAddress bind_address, QObject* parent)
    : Transport(parent), requested_port_(port), bind_address_(std::move(bind_address)) {
    connect(&server_, &QTcpServer::newConnection, this, &TcpServerTransport::accept_connections);
    connect(&server_, &QTcpServer::acceptError, this,
            [this](QAbstractSocket::SocketError) { emit error_occurred(server_.errorString()); });
}

TcpServerTransport::~TcpServerTransport() {
    TcpServerTransport::close();
}

QString TcpServerTransport::description() const {
    return QStringLiteral("TCP server on %1:%2").arg(bind_address_.toString()).arg(port());
}

bool TcpServerTransport::open() {
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

void TcpServerTransport::close() {
    for (auto* client : std::exchange(clients_, {})) {
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

void TcpServerTransport::write(const QByteArray& line) {
    if (!is_open()) {
        return;
    }
    for (auto* client : clients_) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            count_bytes(client->write(line));
        }
    }
}

int TcpServerTransport::client_count() const {
    return static_cast<int>(clients_.size());
}

quint16 TcpServerTransport::port() const {
    return server_.isListening() ? server_.serverPort() : requested_port_;
}

void TcpServerTransport::accept_connections() {
    while (auto* client = server_.nextPendingConnection()) {
        clients_.append(client);
        connect(client, &QTcpSocket::disconnected, this, [this, client] { drop_client(client); });
        connect(client, &QTcpSocket::errorOccurred, this,
                [this, client](QAbstractSocket::SocketError) { drop_client(client); });
        // Discard anything a client sends; the simulator is a talker only.
        connect(client, &QTcpSocket::readyRead, client, [client] { client->readAll(); });
        emit client_count_changed(client_count());
    }
}

void TcpServerTransport::drop_client(QTcpSocket* client) {
    if (!clients_.removeAll(client)) {
        return;
    }
    client->disconnect(this);
    client->deleteLater();
    emit client_count_changed(client_count());
}

}  // namespace nmeasim::io
