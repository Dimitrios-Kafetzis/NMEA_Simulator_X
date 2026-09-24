// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of the `Transport` base class: state changes, failure reporting, byte
/// counting and the names of the states.

#include <nmeasim/io/transport.hpp>

namespace nmeasim::io {

Transport::Transport(QObject* parent) : QObject(parent) {}

void Transport::set_state(State state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    emit state_changed(state);
}

void Transport::fail(const QString& message) {
    last_error_ = message;
    set_state(State::Failed);
    emit error_occurred(message);
}

void Transport::count_bytes(qint64 count) noexcept {
    if (count > 0) {
        bytes_written_ += count;
    }
}

QString to_string(Transport::State state) {
    switch (state) {
        case Transport::State::Closed:
            return QStringLiteral("closed");
        case Transport::State::Opening:
            return QStringLiteral("opening");
        case Transport::State::Open:
            return QStringLiteral("open");
        case Transport::State::Failed:
            return QStringLiteral("failed");
    }
    // Unreachable for valid enumerators; keeps the function total for a value cast from an
    // integer.
    return QStringLiteral("unknown");
}

}  // namespace nmeasim::io
