// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `SerialTransport`, the output that writes lines to a serial port, and its `SerialConfig`.
///
/// Profiles select it with the output type `serial`; the keys of the profile object match
/// the fields of `SerialConfig`. `available_serial_ports` in `serial_ports.hpp` lists the
/// ports that can be chosen.
///
/// @see docs/reference/transports.md

#pragma once

#include <nmeasim/io/transport.hpp>

#include <QSerialPort>
#include <QString>

namespace nmeasim::io {

/// Settings of a serial port output, applied when the port is opened.
///
/// The defaults are the NMEA 0183 line settings: 4800 baud, 8 data bits, no parity, one stop
/// bit, no flow control.
///
/// @see NMEA 0183 (IEC 61162-1) for 4800 baud; IEC 61162-2 for 38400 baud.
struct SerialConfig {
    /// Device to open: a path such as `/dev/ttyUSB0` or a name such as `COM3`, as listed by
    /// `available_serial_ports`. Required: the profile loader rejects a serial output
    /// without it.
    QString port_name;
    /// Line speed in bits per second; must be positive.
    ///
    /// 4800 is the NMEA 0183 default and 38400 the high-speed variant used for AIS. Any other
    /// value the driver supports is accepted; one it rejects makes `SerialTransport::open`
    /// fail.
    qint32 baud_rate{4800};
    /// Bits per character, `QSerialPort::Data5` to `QSerialPort::Data8`.
    QSerialPort::DataBits data_bits{QSerialPort::Data8};
    /// Parity: none, even, odd, mark or space.
    QSerialPort::Parity parity{QSerialPort::NoParity};
    /// Stop bits: 1, 1.5 or 2.
    QSerialPort::StopBits stop_bits{QSerialPort::OneStop};
    /// Flow control: none, hardware (RTS/CTS) or software (XON/XOFF).
    QSerialPort::FlowControl flow_control{QSerialPort::NoFlowControl};
};

/// Writes lines to a serial port.
///
/// The port is opened write-only. An unplugged device closes the port and moves the
/// transport to `State::Failed` with the message `Serial port <name> was disconnected`; it
/// does not reopen by itself. Other runtime errors of the port are reported through
/// `error_occurred` and leave the transport open.
class SerialTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a closed transport for the port described by `config`; nothing is opened until
    /// `open`.
    ///
    /// @param config Port name and line settings, copied into the transport.
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller.
    explicit SerialTransport(SerialConfig config, QObject* parent = nullptr);
    /// Destroys the transport, closing the port first.
    ~SerialTransport() override;

    /// Returns the port name and baud rate.
    ///
    /// @return For example `Serial port /dev/ttyUSB0 at 4800 baud`.
    [[nodiscard]] QString description() const override;
    /// Opens the port write-only and applies the line settings of `config`.
    ///
    /// Returns true at once when already open. Fails when the device is missing, busy or not
    /// accessible, and when the driver rejects a setting, in which case the port is closed
    /// again. `last_error` then names the port, the failed step and the reason reported by
    /// the port.
    ///
    /// @return True when the port is open and configured, false when either step failed.
    bool open() override;
    /// Closes the port and moves to `State::Closed`.
    void close() override;
    /// Queues `line` for transmission on the port.
    ///
    /// The port sends it asynchronously at the configured baud rate; the transport sets no
    /// limit on how much can be queued when lines are produced faster than the line speed
    /// allows.
    ///
    /// @param line One complete line, terminator included.
    void write(const QByteArray& line) override;

    /// Returns the settings the transport was created with.
    ///
    /// @return A reference valid for the lifetime of the transport.
    [[nodiscard]] const SerialConfig& config() const noexcept { return config_; }

private:
    /// The port device.
    QSerialPort port_;
    /// Port name and line settings, fixed at construction.
    SerialConfig config_;
    /// True while `open` opens and configures the port, so that errors raised by the port
    /// during that time are reported once by `open` rather than also as runtime errors.
    bool opening_{false};
};

}  // namespace nmeasim::io
