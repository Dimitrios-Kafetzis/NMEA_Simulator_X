// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `FileTransport`, the output that writes lines to a plain text file.
///
/// Profiles select it with the output type `file`; for a timestamped recording that can be
/// replayed, see `LogTransport` in `log_transport.hpp`.
///
/// @see docs/reference/transports.md

#pragma once

#include <nmeasim/io/transport.hpp>

#include <QFile>
#include <QString>

namespace nmeasim::io {

/// Writes lines to a file, flushing after every write so that the file can be tailed while the
/// simulator runs.
///
/// Lines are written byte for byte as given, without timestamps or header: the file is
/// opened in binary mode, so the CR LF that ends every sentence reaches the file unchanged on
/// every platform.
///
/// @see LogTransport
class FileTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a closed transport for the file at `path`; nothing is opened until `open`.
    ///
    /// @param path Path of the file, absolute or relative to the working directory of the
    ///   process. The directory must exist; the file is created when it does not.
    /// @param append True to append to an existing file, false to truncate it. Only the first
    ///   successful `open` truncates; later opens of the same transport continue the file,
    ///   so stopping and restarting the simulation does not lose what was written.
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller.
    explicit FileTransport(QString path, bool append = true, QObject* parent = nullptr);
    /// Destroys the transport, closing the file first.
    ~FileTransport() override;

    /// Returns `File ` followed by the path.
    ///
    /// @return For example `File /tmp/output.nmea`.
    [[nodiscard]] QString description() const override;
    /// Opens the file for writing in binary mode, appending or, on the first successful open
    /// of a transport created with `append` false, truncating.
    ///
    /// Returns true at once when already open. Fails with `Cannot open <path> for writing`
    /// and the system's reason when the file cannot be opened, for example because its
    /// directory does not exist or is not writable.
    ///
    /// @return True when the file is open, false when opening failed.
    bool open() override;
    /// Closes the file, flushing what is still buffered, and moves to `State::Closed`.
    void close() override;
    /// Writes `line` to the file and flushes it.
    ///
    /// Write errors, such as a full disk, are not reported; they only leave `bytes_written`
    /// unchanged.
    ///
    /// @param line One complete line, terminator included.
    void write(const QByteArray& line) override;

    /// Returns the path of the file written to.
    ///
    /// @return The path as passed to the constructor.
    [[nodiscard]] QString path() const { return file_.fileName(); }

private:
    /// The output file; also holds the path while closed.
    QFile file_;
    /// True to open in append mode, false to truncate. Set to true by the first successful
    /// `open`.
    bool append_;
};

}  // namespace nmeasim::io
