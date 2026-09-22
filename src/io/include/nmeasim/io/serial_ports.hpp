#pragma once

#include <string>
#include <vector>

namespace nmeasim::io {

/// Describes one serial port found on the host machine.
struct SerialPortDescriptor {
    /// Platform-specific device path, e.g. `/dev/ttyUSB0`, `COM3` or `/dev/cu.usbserial-1420`.
    std::string system_location;
    /// Short port name as reported by the operating system, e.g. `ttyUSB0` or `COM3`.
    std::string port_name;
    /// Free-text description supplied by the driver, may be empty.
    std::string description;
    /// Manufacturer supplied by the driver, may be empty.
    std::string manufacturer;
};

/// Enumerates the serial ports currently present on this machine.
///
/// No filtering is applied: every port the operating system reports is returned so that
/// unusual devices (for example `/dev/ttySC*` on single-board computers) are never hidden.
[[nodiscard]] std::vector<SerialPortDescriptor> available_serial_ports();

}  // namespace nmeasim::io
