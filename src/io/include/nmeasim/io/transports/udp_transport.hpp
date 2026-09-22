#pragma once

#include <nmeasim/io/transport.hpp>

#include <QHostAddress>
#include <QString>
#include <QUdpSocket>

namespace nmeasim::io {

struct UdpConfig {
    enum class Mode {
        Unicast,
        Broadcast,
        Multicast,
    };
    Mode mode{Mode::Unicast};
    /// Destination. For broadcast it may be left empty to derive the subnet broadcast address
    /// from `interface_name`, falling back to 255.255.255.255.
    QString address{QStringLiteral("127.0.0.1")};
    quint16 port{10110};
    /// Interface to send from; empty lets the operating system choose.
    QString interface_name;
    int multicast_ttl{1};
};

/// Sends every line as one UDP datagram.
class UdpTransport final : public Transport {
    Q_OBJECT

public:
    explicit UdpTransport(UdpConfig config, QObject* parent = nullptr);
    ~UdpTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;

    /// The address datagrams are sent to after resolving broadcast defaults.
    [[nodiscard]] QHostAddress destination() const { return destination_; }
    [[nodiscard]] const UdpConfig& config() const noexcept { return config_; }

private:
    bool resolve_destination();

    QUdpSocket socket_;
    UdpConfig config_;
    QHostAddress destination_;
};

[[nodiscard]] QString to_string(UdpConfig::Mode mode);

}  // namespace nmeasim::io
