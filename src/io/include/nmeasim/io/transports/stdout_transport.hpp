// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `StdoutTransport`, the output that writes lines to the standard output of the process.
///
/// Profiles select it with the output type `stdout` and the command-line option `--stdout`
/// adds one, so that the command-line tool can feed another program through a pipe.
///
/// @see docs/reference/transports.md

#pragma once

#include <nmeasim/io/transport.hpp>

namespace nmeasim::io {

/// Writes lines to standard output, flushing after each one so that a pipe sees it
/// immediately.
///
/// Opening cannot fail and write errors, such as a closed pipe, are not detected: every line
/// written while open counts in full in `bytes_written`.
class StdoutTransport final : public Transport {
    Q_OBJECT

public:
    /// Creates a closed transport that writes to `std::cout` once opened.
    ///
    /// @param parent Qt parent that owns the transport; null leaves ownership to the caller.
    explicit StdoutTransport(QObject* parent = nullptr);

    /// Returns `Standard output`.
    ///
    /// @return The fixed text `Standard output`.
    [[nodiscard]] QString description() const override;
    /// Moves to `State::Open`; standard output needs no opening.
    ///
    /// @return Always true.
    bool open() override;
    /// Moves to `State::Closed`; standard output itself stays open.
    void close() override;
    /// Writes `line` to `std::cout` and flushes it.
    ///
    /// @param line One complete line, terminator included.
    void write(const QByteArray& line) override;
};

}  // namespace nmeasim::io
