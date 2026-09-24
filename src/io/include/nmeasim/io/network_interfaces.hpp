// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Enumeration of the host's IPv4 network interfaces, with their subnet broadcast addresses.
///
/// The UDP transport uses it to resolve its source interface and broadcast address; the
/// command-line tool and the desktop application use it to list the interfaces and the
/// addresses clients can connect to.
///
/// @see docs/reference/transports.md

#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>

namespace nmeasim::io {

/// One IPv4 address of a network interface, with the broadcast address of its subnet.
///
/// A default-constructed value, with a null `address`, stands for "no such interface" in the
/// result of `find_ipv4_interface`.
struct NetworkInterfaceInfo {
    /// System name, for example `eth0`, `en0` or `Ethernet`.
    QString name;
    /// Descriptive name where the platform provides one; otherwise the same as `name`.
    QString human_name;
    /// The IPv4 address itself; null only in the "not found" value.
    QHostAddress address;
    /// Broadcast address of the subnet; null when the platform reports none.
    QHostAddress broadcast;
    /// True for the loopback interface.
    bool is_loopback{false};
};

/// Lists every IPv4 address of every interface that is up and running, loopback included.
///
/// Interfaces that are down, or up but not running, are skipped, as are IPv6 addresses. The
/// list is read afresh on every call.
///
/// @return One entry per IPv4 address, in the order the operating system reports the
///   interfaces and their addresses; an interface with several addresses contributes several
///   entries. Empty when there is none.
[[nodiscard]] QList<NetworkInterfaceInfo> ipv4_interfaces();

/// Finds the first IPv4 address of the interface with a given name.
///
/// @param name System name or descriptive name of the interface, compared exactly
///   (case-sensitive) with `NetworkInterfaceInfo::name` and `NetworkInterfaceInfo::human_name`.
/// @return The first entry of `ipv4_interfaces` whose name matches, or a default-constructed
///   value with a null `address` when no interface that is up and running has that name and
///   an IPv4 address.
[[nodiscard]] NetworkInterfaceInfo find_ipv4_interface(const QString& name);

}  // namespace nmeasim::io
