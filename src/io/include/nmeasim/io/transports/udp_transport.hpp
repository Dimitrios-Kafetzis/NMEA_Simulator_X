// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `UdpTransport`, the output that sends every line as one UDP datagram, and its `UdpConfig`.
///
/// Profiles select it with the output type `udp`; the keys of the profile object are
/// `mode`, `address`, `port`, `interface` and `multicast_ttl`. The command-line option `--udp`
/// adds one. Port 10110 is the convention for NMEA 0183 over UDP and 42000 the port on which
/// Google Earth receives ViewSync packets.
///
/// @see docs/reference/transports.md

#pragma once

#include <nmeasim/io/transport.hpp>

#include <QHostAddress>
#include <QString>
#include <QUdpSocket>

namespace nmeasim::io {

/// Settings of a UDP output, applied when the transport is opened.
struct UdpConfig {
    /// How datagrams are addressed; the profile key `mode`.
    enum class Mode {
        /// To a single host; the profile value `unicast`.
        Unicast,
        /// To every host on a subnet; the profile value `broadcast`.
        Broadcast,
        /// To the members of a multicast group; the profile value `multicast`.
        Multicast,
    };
    /// Addressing mode. It decides how an empty `address` is resolved and whether
    /// `multicast_ttl` applies; the address itself is not checked against it.
    Mode mode{Mode::Unicast};
    /// Destination address as an IPv4 literal such as `192.168.1.255` or `239.192.0.1`.
    ///
    /// Host names are not resolved: an address that does not parse, or an IPv6 address, makes
    /// `UdpTransport::open` fail. In broadcast mode an empty address resolves to the subnet
    /// broadcast address of `interface_name`, or to the limited broadcast address
    /// `255.255.255.255` when no interface is chosen or the interface has no broadcast
    /// address. Leading and trailing spaces are ignored.
    QString address{QStringLiteral("127.0.0.1")};
    /// Destination UDP port.
    quint16 port{10110};
    /// Network interface to send from, by system name (`eth0`, `en0`) or descriptive name;
    /// empty lets the operating system choose.
    ///
    /// When set, the interface must be up and running and have an IPv4 address. It is looked
    /// up once, with `find_ipv4_interface`; the datagrams leave from its first IPv4 address,
    /// and in multicast mode it is also the multicast egress.
    QString interface_name;
    /// Time to live of multicast datagrams, in router hops; ignored in the other modes.
    ///
    /// The default of 1 keeps the datagrams on the local subnet. The value is passed to the
    /// socket unchecked; IP limits it to [0, 255].
    int multicast_ttl{1};
};

/// Sends every line as one UDP datagram to the destination of a `UdpConfig`.
///
/// UDP has no connection, so the transport knows nothing about receivers: `client_count` is
/// 1 while open, whether or not anyone listens. Socket errors, such as a datagram the system
/// refuses to send, are reported through `error_occurred` and leave the transport open.
class UdpTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a closed transport; the destination is resolved and the socket bound in
    /// `open`.
    ///
    /// @param config Addressing mode, destination, source interface and multicast settings,
    ///   copied into the transport.
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller.
    explicit UdpTransport(UdpConfig config, QObject* parent = nullptr);
    /// Destroys the transport, closing the socket first.
    ~UdpTransport() override;

    /// Returns the mode, destination and port.
    ///
    /// @return For example `UDP unicast to 127.0.0.1:10110`; the destination is the resolved
    ///   address once `open` has resolved it, and `UdpConfig::address` before.
    [[nodiscard]] QString description() const override;
    /// Resolves the destination, binds the socket and applies the multicast settings.
    ///
    /// Returns true at once when already open. The socket is bound to an ephemeral port on
    /// the first IPv4 address of `UdpConfig::interface_name`, or on every IPv4 address when no
    /// interface is set. Fails when the destination address does not parse or is not an IPv4
    /// address, when the interface is unknown, not up and running, or has no IPv4 address,
    /// and when the socket cannot be bound; `last_error` names the address or interface
    /// concerned.
    ///
    /// @return True when the transport is open, false when any step failed.
    bool open() override;
    /// Closes the socket and moves to `State::Closed`; the resolved destination is kept.
    void close() override;
    /// Sends `line` as one datagram to the destination.
    ///
    /// @param line One complete line, terminator included; it must fit in one datagram.
    void write(const QByteArray& line) override;

    /// Returns the address datagrams are sent to.
    ///
    /// @return The address resolved by the last `open`, with the broadcast default applied;
    ///   a null address before the first `open` or when the last one found the address
    ///   invalid or not IPv4.
    [[nodiscard]] QHostAddress destination() const { return destination_; }
    /// Returns the settings the transport was created with.
    ///
    /// @return A reference valid for the lifetime of the transport.
    [[nodiscard]] const UdpConfig& config() const noexcept { return config_; }

private:
    /// Sets `destination_` from `config_`, applying the broadcast default for an empty
    /// address.
    ///
    /// Calls `fail` when the address does not parse or is not an IPv4 address.
    ///
    /// @return True when a destination was set, false after a failure.
    bool resolve_destination();

    /// The sending socket, bound in `open`.
    QUdpSocket socket_;
    /// Settings passed to the constructor.
    UdpConfig config_;
    /// Destination resolved by `resolve_destination`; null until then.
    QHostAddress destination_;
};

/// Returns the lower-case name of a UDP addressing mode, as used in profiles.
///
/// @param mode The mode to name.
/// @return `unicast`, `broadcast` or `multicast`; `unknown` for a value outside the
///   enumeration.
[[nodiscard]] QString to_string(UdpConfig::Mode mode);

}  // namespace nmeasim::io
