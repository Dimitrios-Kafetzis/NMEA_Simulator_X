#include <nmeasim/io/transports/tcp_client_transport.hpp>

namespace nmeasim::io {

TcpClientTransport::TcpClientTransport(QString host, quint16 port, int reconnect_interval_ms,
                                       QObject* parent)
    : Transport(parent), host_(std::move(host)), port_(port) {
    reconnect_timer_.setSingleShot(true);
    reconnect_timer_.setInterval(reconnect_interval_ms);
    connect(&reconnect_timer_, &QTimer::timeout, this, &TcpClientTransport::connect_now);
    connect(&socket_, &QTcpSocket::connected, this, [this] { set_state(State::Open); });
    connect(&socket_, &QTcpSocket::disconnected, this, [this] {
        if (wanted_open_) {
            set_state(State::Opening);
            schedule_reconnect();
        }
    });
    connect(&socket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit error_occurred(socket_.errorString());
        if (wanted_open_) {
            set_state(State::Opening);
            schedule_reconnect();
        }
    });
    connect(&socket_, &QTcpSocket::readyRead, &socket_, [this] { socket_.readAll(); });
}

TcpClientTransport::~TcpClientTransport() {
    TcpClientTransport::close();
}

QString TcpClientTransport::description() const {
    return QStringLiteral("TCP client to %1:%2").arg(host_).arg(port_);
}

bool TcpClientTransport::open() {
    wanted_open_ = true;
    if (socket_.state() == QAbstractSocket::ConnectedState) {
        set_state(State::Open);
        return true;
    }
    set_state(State::Opening);
    connect_now();
    return true;
}

void TcpClientTransport::close() {
    wanted_open_ = false;
    reconnect_timer_.stop();
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.abort();
    }
    set_state(State::Closed);
}

void TcpClientTransport::write(const QByteArray& line) {
    if (is_open() && socket_.state() == QAbstractSocket::ConnectedState) {
        count_bytes(socket_.write(line));
    }
}

void TcpClientTransport::connect_now() {
    if (!wanted_open_ || socket_.state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    socket_.connectToHost(host_, port_);
}

void TcpClientTransport::schedule_reconnect() {
    if (wanted_open_ && !reconnect_timer_.isActive()) {
        reconnect_timer_.start();
    }
}

}  // namespace nmeasim::io
