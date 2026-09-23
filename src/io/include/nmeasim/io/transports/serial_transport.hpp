#pragma once

#include <nmeasim/io/transport.hpp>

#include <QSerialPort>
#include <QString>

namespace nmeasim::io {

/// Settings of a serial port output.
struct SerialConfig {
    /// Device path such as `/dev/ttyUSB0` or `COM3`, or a port name from `available_serial_ports`.
    QString port_name;
    /// Any positive baud rate; 4800 is the NMEA 0183 default and 38400 the high-speed variant.
    qint32 baud_rate{4800};
    /// Bits per character, 5 to 8.
    QSerialPort::DataBits data_bits{QSerialPort::Data8};
    /// Parity: none, even, odd, mark or space.
    QSerialPort::Parity parity{QSerialPort::NoParity};
    /// Stop bits: 1, 1.5 or 2.
    QSerialPort::StopBits stop_bits{QSerialPort::OneStop};
    /// Flow control: none, hardware (RTS/CTS) or software (XON/XOFF).
    QSerialPort::FlowControl flow_control{QSerialPort::NoFlowControl};
};

/// Writes lines to a serial port.
class SerialTransport final : public Transport {
    Q_OBJECT

public:
    /// Writes to the port described by `config`, opened write-only when `open` is called.
    explicit SerialTransport(SerialConfig config, QObject* parent = nullptr);
    ~SerialTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;

    /// The settings the transport was created with.
    [[nodiscard]] const SerialConfig& config() const noexcept { return config_; }

private:
    QSerialPort port_;
    SerialConfig config_;
    bool opening_{false};
};

}  // namespace nmeasim::io
