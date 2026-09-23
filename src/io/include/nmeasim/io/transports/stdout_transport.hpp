#pragma once

#include <nmeasim/io/transport.hpp>

namespace nmeasim::io {

/// Writes lines to standard output, flushing after each one so pipes see them immediately.
class StdoutTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a transport that writes to the process's standard output.
    explicit StdoutTransport(QObject* parent = nullptr);

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;
};

}  // namespace nmeasim::io
