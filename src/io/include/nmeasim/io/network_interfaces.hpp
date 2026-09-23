#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>

namespace nmeasim::io {

/// One IPv4 address of a network interface, with the broadcast address of its subnet.
struct NetworkInterfaceInfo {
    /// System name, e.g. `eth0`, `en0` or `Ethernet`.
    QString name;
    /// Descriptive name where the platform provides one.
    QString human_name;
    /// The IPv4 address itself.
    QHostAddress address;
    /// Broadcast address of the subnet; null when the platform reports none.
    QHostAddress broadcast;
    /// True for the loopback interface.
    bool is_loopback{false};
};

/// Lists every IPv4 address of every interface that is up, loopback included.
[[nodiscard]] QList<NetworkInterfaceInfo> ipv4_interfaces();

/// Finds the first IPv4 entry of the interface called `name`, or an empty optional-like
/// result with a null address when there is none.
[[nodiscard]] NetworkInterfaceInfo find_ipv4_interface(const QString& name);

}  // namespace nmeasim::io
