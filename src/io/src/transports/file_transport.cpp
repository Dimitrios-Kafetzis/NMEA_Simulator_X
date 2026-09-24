// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `FileTransport`: opening the file in append or truncate mode and
/// writing flushed lines unchanged.

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
    // Binary mode: the lines already end in CR LF, which text mode would turn into CR CR LF
    // on Windows.
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    mode |= append_ ? QIODevice::Append : QIODevice::Truncate;
    if (!file_.open(mode)) {
        fail(QStringLiteral("Cannot open %1 for writing: %2")
                 .arg(file_.fileName(), file_.errorString()));
        return false;
    }
    // Every later open continues the file, so that stopping and starting a run keeps what
    // the first part of the run wrote.
    append_ = true;
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
