#include <nmeasim/io/transports/stdout_transport.hpp>

#include <iostream>

namespace nmeasim::io {

StdoutTransport::StdoutTransport(QObject* parent) : Transport(parent) {}

QString StdoutTransport::description() const {
    return QStringLiteral("Standard output");
}

bool StdoutTransport::open() {
    set_state(State::Open);
    return true;
}

void StdoutTransport::close() {
    set_state(State::Closed);
}

void StdoutTransport::write(const QByteArray& line) {
    if (!is_open()) {
        return;
    }
    std::cout.write(line.constData(), line.size());
    std::cout.flush();
    count_bytes(line.size());
}

}  // namespace nmeasim::io
