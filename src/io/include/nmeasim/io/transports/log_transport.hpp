// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `LogTransport`, the recorder that writes timestamped sentences in the log format of
/// ADR 0012.
///
/// Profiles and the command-line option `--record` select it as the output type `log`;
/// `SimulationRunner::set_recording` also creates one for a recording started from the
/// desktop application. The files it writes are read back by `nmeasim::core::log::parse_log`
/// for replay.
///
/// @see docs/reference/log-format.md
/// @see docs/adr/0012-log-file-format.md

#pragma once

#include <nmeasim/io/transport.hpp>

#include <QFile>
#include <QString>

namespace nmeasim::io {

/// Records sentences to a log file: a `#` header when the file is empty, then one line per
/// sentence prefixed with the wall-clock UTC time.
///
/// A new or empty file starts with `nmeasim::core::log::kHeaderLine`, a `# recorded:` line
/// with the UTC time of the `open` call and, when a profile name is set, a `# profile:`
/// line. Each sentence then becomes one line: the ISO 8601 UTC time of the write with
/// millisecond resolution, a space and the sentence without its terminator, ended by a
/// single line feed. The file is flushed after every sentence so that it can be tailed.
///
/// @see nmeasim::core::log::format_log_line
/// @see nmeasim::core::log::format_header_line
class LogTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a closed recorder for the file at `path`; nothing is opened until `open`.
    ///
    /// @param path Path of the log file, absolute or relative to the working directory of the
    ///   process. The directory must exist; the file is created when it does not.
    /// @param append True to continue an existing file, false to truncate it. Only the first
    ///   successful `open` truncates; later opens of the same transport continue the
    ///   recording, so stopping and restarting the simulation does not lose it.
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller.
    explicit LogTransport(QString path, bool append = true, QObject* parent = nullptr);
    /// Destroys the transport, closing the file first.
    ~LogTransport() override;

    /// Returns `Log ` followed by the path.
    ///
    /// @return For example `Log /tmp/record.log`.
    [[nodiscard]] QString description() const override;
    /// Opens the file and writes the header when the file is empty.
    ///
    /// Returns true at once when already open. The header is written after truncation, to a
    /// new file, and to an existing file of size zero; a non-empty file is continued without
    /// a second header. Fails with `Cannot open <path> for writing` and the system's reason
    /// when the file cannot be opened.
    ///
    /// @return True when the file is open, false when opening failed.
    bool open() override;
    /// Closes the file, flushing what is still buffered, and moves to `State::Closed`.
    void close() override;
    /// Records `line` with the current wall-clock UTC time and flushes the file.
    ///
    /// Trailing carriage returns and line feeds are removed and replaced by a single line
    /// feed; the rest of the line is written unchanged. Write errors are not reported; they
    /// leave `bytes_written` unchanged but still count in `lines_written`.
    ///
    /// @param line One sentence, terminator included or not.
    void write(const QByteArray& line) override;

    /// Returns the path of the log file.
    ///
    /// @return The path as passed to the constructor.
    [[nodiscard]] QString path() const { return file_.fileName(); }
    /// Sets the profile name written to the `# profile:` header line.
    ///
    /// Takes effect at the next `open` that writes a header.
    ///
    /// @param name Name of the profile being recorded; empty omits the line.
    void set_profile_name(const QString& name) { profile_name_ = name; }
    /// Returns the number of sentences recorded since construction.
    ///
    /// @return Calls of `write` while open, over every open of this transport; header lines
    ///   are not counted.
    [[nodiscard]] qint64 lines_written() const noexcept { return lines_written_; }

private:
    /// The log file; also holds the path while closed.
    QFile file_;
    /// True to open in append mode, false to truncate. Set to true by the first successful
    /// `open`.
    bool append_;
    /// Value of the `# profile:` header line; empty omits it.
    QString profile_name_;
    /// Sentences recorded since construction.
    qint64 lines_written_{0};
};

}  // namespace nmeasim::io
