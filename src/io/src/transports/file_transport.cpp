// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `FileTransport`: opening the file in append or truncate mode and
/// writing flushed lines.

#include <nmeasim/io/transports/file_transport.hpp>

namespace nmeasim::io {

FileTransport::FileTransport(QString path, bool append, QObject* parent)
    : Transport(parent), file_(std::move(path)), append_(append) {}

FileTransport::~FileTransport() {
    FileTransport::close();
}

QString FileTransport::description() const {
    return QStringLiteral("File %1").arg(file_.fileName());
}

bool FileTransport::open() {
    if (is_open()) {
        return true;
    }
    set_state(State::Opening);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    mode |= append_ ? QIODevice::Append : QIODevice::Truncate;
    if (!file_.open(mode)) {
        fail(QStringLiteral("Cannot open %1 for writing: %2")
                 .arg(file_.fileName(), file_.errorString()));
        return false;
    }
    set_state(State::Open);
    return true;
}

void FileTransport::close() {
    if (file_.isOpen()) {
        file_.close();
    }
    set_state(State::Closed);
}

void FileTransport::write(const QByteArray& line) {
    if (!is_open()) {
        return;
    }
    count_bytes(file_.write(line));
    file_.flush();
}

}  // namespace nmeasim::io
