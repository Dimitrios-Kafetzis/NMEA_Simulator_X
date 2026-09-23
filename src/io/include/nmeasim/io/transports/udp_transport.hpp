#pragma once

#include <nmeasim/io/transport.hpp>

#include <QHostAddress>
#include <QString>
#include <QUdpSocket>

namespace nmeasim::io {

/// Settings of a UDP output.
struct UdpConfig {
    /// How datagrams are addressed.
    enum class Mode {
        /// To a single host.
        Unicast,
        /// To every host on a subnet.
        Broadcast,
        /// To a multicast group.
        Multicast,
    };
    /// Addressing mode, the `mode` key: `unicast`, `broadcast` or `multicast`.
    Mode mode{Mode::Unicast};
    /// Destination. For broadcast it may be left empty to derive the subnet broadcast address
    /// from `interface_name`, falling back to 255.255.255.255.
    QString address{QStringLiteral("127.0.0.1")};
    /// Destination port.
    quint16 port{10110};
    /// Interface to send from; empty lets the operating system choose.
    QString interface_name;
    /// Time to live of multicast datagrams; 1 keeps them on the local network.
    int multicast_ttl{1};
};

/// Sends every line as one UDP datagram.
class UdpTransport final : public Transport {
    Q_OBJECT

public:
    /// Sends to the destination described by `config`, resolved when `open` is called.
    explicit UdpTransport(UdpConfig config, QObject* parent = nullptr);
    ~UdpTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;

    /// The address datagrams are sent to after resolving broadcast defaults.
    [[nodiscard]] QHostAddress destination() const { return destination_; }
    /// The settings the transport was created with.
    [[nodiscard]] const UdpConfig& config() const noexcept { return config_; }

private:
    bool resolve_destination();

    QUdpSocket socket_;
    UdpConfig config_;
    QHostAddress destination_;
};

/// Lower-case name of `mode`: `unicast`, `broadcast` or `multicast`.
[[nodiscard]] QString to_string(UdpConfig::Mode mode);

}  // namespace nmeasim::io
