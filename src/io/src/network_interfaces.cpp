#include <nmeasim/io/network_interfaces.hpp>

#include <QNetworkInterface>

namespace nmeasim::io {

QList<NetworkInterfaceInfo> ipv4_interfaces() {
    QList<NetworkInterfaceInfo> result;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning)) {
            continue;
        }
        const auto entries = interface.addressEntries();
        for (const auto& entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            result.append({interface.name(), interface.humanReadableName(), entry.ip(),
                           entry.broadcast(), flags.testFlag(QNetworkInterface::IsLoopBack)});
        }
    }
    return result;
}

NetworkInterfaceInfo find_ipv4_interface(const QString& name) {
    const auto interfaces = ipv4_interfaces();
    for (const auto& info : interfaces) {
        if (info.name == name || info.human_name == name) {
            return info;
        }
    }
    return {};
}

}  // namespace nmeasim::io
