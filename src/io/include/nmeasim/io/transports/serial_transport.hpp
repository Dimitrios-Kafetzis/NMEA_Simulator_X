#pragma once

#include <nmeasim/io/transport.hpp>

#include <QSerialPort>
#include <QString>

namespace nmeasim::io {

struct SerialConfig {
    /// Device path such as `/dev/ttyUSB0` or `COM3`, or a port name from `available_serial_ports`.
    QString port_name;
    /// Any positive baud rate; 4800 is the NMEA 0183 default and 38400 the high-speed variant.
    qint32 baud_rate{4800};
    QSerialPort::DataBits data_bits{QSerialPort::Data8};
    QSerialPort::Parity parity{QSerialPort::NoParity};
    QSerialPort::StopBits stop_bits{QSerialPort::OneStop};
    QSerialPort::FlowControl flow_control{QSerialPort::NoFlowControl};
};

/// Writes lines to a serial port.
class SerialTransport final : public Transport {
    Q_OBJECT

public:
    explicit SerialTransport(SerialConfig config, QObject* parent = nullptr);
    ~SerialTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;

    [[nodiscard]] const SerialConfig& config() const noexcept { return config_; }

private:
    QSerialPort port_;
    SerialConfig config_;
};

}  // namespace nmeasim::io
