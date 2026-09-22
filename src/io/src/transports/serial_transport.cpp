#include <nmeasim/io/transports/serial_transport.hpp>

namespace nmeasim::io {

SerialTransport::SerialTransport(SerialConfig config, QObject* parent)
    : Transport(parent), config_(std::move(config)) {
    connect(&port_, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        // Failures while opening are reported by open() itself; only report runtime errors.
        if (error == QSerialPort::NoError || opening_) {
            return;
        }
        if (error == QSerialPort::ResourceError && port_.isOpen()) {
            // The device was unplugged.
            port_.close();
            fail(QStringLiteral("Serial port %1 was disconnected").arg(config_.port_name));
            return;
        }
        emit error_occurred(port_.errorString());
    });
    connect(&port_, &QSerialPort::readyRead, &port_, [this] { port_.readAll(); });
}

SerialTransport::~SerialTransport() {
    SerialTransport::close();
}

QString SerialTransport::description() const {
    return QStringLiteral("Serial port %1 at %2 baud")
        .arg(config_.port_name)
        .arg(config_.baud_rate);
}

bool SerialTransport::open() {
    if (is_open()) {
        return true;
    }
    set_state(State::Opening);
    opening_ = true;
    port_.setPortName(config_.port_name);
    if (!port_.open(QIODevice::WriteOnly)) {
        opening_ = false;
        fail(QStringLiteral("Cannot open serial port %1: %2")
                 .arg(config_.port_name, port_.errorString()));
        return false;
    }
    const bool configured =
        port_.setBaudRate(config_.baud_rate) && port_.setDataBits(config_.data_bits) &&
        port_.setParity(config_.parity) && port_.setStopBits(config_.stop_bits) &&
        port_.setFlowControl(config_.flow_control);
    opening_ = false;
    if (!configured) {
        const QString reason = port_.errorString();
        port_.close();
        fail(QStringLiteral("Cannot configure serial port %1: %2").arg(config_.port_name, reason));
        return false;
    }
    set_state(State::Open);
    return true;
}

void SerialTransport::close() {
    if (port_.isOpen()) {
        port_.close();
    }
    set_state(State::Closed);
}

void SerialTransport::write(const QByteArray& line) {
    if (is_open() && port_.isOpen()) {
        count_bytes(port_.write(line));
    }
}

}  // namespace nmeasim::io
