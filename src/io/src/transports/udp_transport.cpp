#include <nmeasim/io/network_interfaces.hpp>
#include <nmeasim/io/transports/udp_transport.hpp>

#include <QNetworkInterface>
#include <QVariant>

namespace nmeasim::io {

QString to_string(UdpConfig::Mode mode) {
    switch (mode) {
        case UdpConfig::Mode::Unicast:
            return QStringLiteral("unicast");
        case UdpConfig::Mode::Broadcast:
            return QStringLiteral("broadcast");
        case UdpConfig::Mode::Multicast:
            return QStringLiteral("multicast");
    }
    return QStringLiteral("unknown");
}

UdpTransport::UdpTransport(UdpConfig config, QObject* parent)
    : Transport(parent), config_(std::move(config)) {
    connect(&socket_, &QUdpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit error_occurred(socket_.errorString()); });
}

UdpTransport::~UdpTransport() {
    UdpTransport::close();
}

QString UdpTransport::description() const {
    return QStringLiteral("UDP %1 to %2:%3")
        .arg(to_string(config_.mode),
             destination_.isNull() ? config_.address : destination_.toString())
        .arg(config_.port);
}

bool UdpTransport::resolve_destination() {
    if (config_.mode == UdpConfig::Mode::Broadcast && config_.address.trimmed().isEmpty()) {
        const auto info = find_ipv4_interface(config_.interface_name);
        destination_ =
            info.broadcast.isNull() ? QHostAddress(QHostAddress::Broadcast) : info.broadcast;
        return true;
    }
    destination_ = QHostAddress(config_.address.trimmed());
    if (destination_.isNull()) {
        fail(QStringLiteral("Invalid UDP destination address '%1'").arg(config_.address));
        return false;
    }
    return true;
}

bool UdpTransport::open() {
    if (is_open()) {
        return true;
    }
    set_state(State::Opening);
    if (!resolve_destination()) {
        return false;
    }

    QHostAddress bind_address{QHostAddress::AnyIPv4};
    if (!config_.interface_name.isEmpty()) {
        const auto info = find_ipv4_interface(config_.interface_name);
        if (info.address.isNull()) {
            fail(QStringLiteral("Network interface '%1' has no IPv4 address")
                     .arg(config_.interface_name));
            return false;
        }
        bind_address = info.address;
    }
    if (!socket_.bind(bind_address, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        fail(QStringLiteral("Cannot bind UDP socket to %1: %2")
                 .arg(bind_address.toString(), socket_.errorString()));
        return false;
    }

    if (config_.mode == UdpConfig::Mode::Multicast) {
        socket_.setSocketOption(QAbstractSocket::MulticastTtlOption,
                                QVariant(config_.multicast_ttl));
        if (!config_.interface_name.isEmpty()) {
            socket_.setMulticastInterface(
                QNetworkInterface::interfaceFromName(config_.interface_name));
        }
    }
    set_state(State::Open);
    return true;
}

void UdpTransport::close() {
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.close();
    }
    set_state(State::Closed);
}

void UdpTransport::write(const QByteArray& line) {
    if (!is_open()) {
        return;
    }
    count_bytes(socket_.writeDatagram(line, destination_, config_.port));
}

}  // namespace nmeasim::io
