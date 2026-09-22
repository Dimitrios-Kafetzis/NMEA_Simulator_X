#include <nmeasim/io/serial_ports.hpp>

#include <QSerialPortInfo>

namespace nmeasim::io {

std::vector<SerialPortDescriptor> available_serial_ports() {
    const auto ports = QSerialPortInfo::availablePorts();

    std::vector<SerialPortDescriptor> result;
    result.reserve(static_cast<std::size_t>(ports.size()));
    for (const auto& port : ports) {
        result.push_back({
            .system_location = port.systemLocation().toStdString(),
            .port_name = port.portName().toStdString(),
            .description = port.description().toStdString(),
            .manufacturer = port.manufacturer().toStdString(),
        });
    }
    return result;
}

}  // namespace nmeasim::io
