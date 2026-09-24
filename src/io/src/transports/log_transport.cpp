// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `LogTransport`: writing the header of a new log and timestamping every
/// sentence with the wall clock.

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/io/transports/log_transport.hpp>

#include <QDateTime>

#include <chrono>
#include <string>

namespace nmeasim::io {

namespace {

/// Returns the current wall-clock time, truncated to whole milliseconds.
///
/// @return The system clock's time since the Unix epoch, which is UTC, with the millisecond
///   resolution that the log format records.
std::chrono::system_clock::time_point now_utc() {
    return std::chrono::system_clock::time_point{
        std::chrono::milliseconds{QDateTime::currentMSecsSinceEpoch()}};
}

}  // namespace

LogTransport::LogTransport(QString path, bool append, QObject* parent)
    : Transport(parent), file_(std::move(path)), append_(append) {}

LogTransport::~LogTransport() {
    LogTransport::close();
}

QString LogTransport::description() const {
    return QStringLiteral("Log %1").arg(file_.fileName());
}

bool LogTransport::open() {
    if (is_open()) {
        return true;
    }
    set_state(State::Opening);
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    mode |= append_ ? QIODevice::Append : QIODevice::Truncate;
    if (!file_.open(mode)) {
        fail(QStringLiteral("Cannot open %1 for writing: %2")
                 .arg(file_.fileName(), file_.errorString()));
        return false;
    }
    // Only an empty file gets a header, so appending to an earlier recording continues it
    // without a second one.
    if (file_.size() == 0) {
        const auto started = now_utc();
        std::string header{core::log::kHeaderLine};
        header += '\n';
        header += core::log::format_header_line("recorded", core::time::format_iso8601(started));
        header += '\n';
        if (!profile_name_.isEmpty()) {
            header += core::log::format_header_line("profile", profile_name_.toStdString());
            header += '\n';
        }
        count_bytes(file_.write(header.data(), static_cast<qint64>(header.size())));
    }
    // Every later open in the same recording continues the file.
    append_ = true;
    set_state(State::Open);
    return true;
}

void LogTransport::close() {
    if (file_.isOpen()) {
        file_.close();
    }
    set_state(State::Closed);
}

void LogTransport::write(const QByteArray& line) {
    if (!is_open()) {
        return;
    }
    QByteArray sentence = line;
    while (sentence.endsWith('\n') || sentence.endsWith('\r')) {
        sentence.chop(1);
    }
    std::string text = core::log::format_log_line(
        now_utc(),
        std::string_view{sentence.constData(), static_cast<std::size_t>(sentence.size())});
    text += '\n';
    count_bytes(file_.write(text.data(), static_cast<qint64>(text.size())));
    file_.flush();
    ++lines_written_;
}

}  // namespace nmeasim::io
