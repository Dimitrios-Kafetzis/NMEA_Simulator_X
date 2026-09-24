// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `ReplaySource`: the replay clock, its end and loop handling, stepping
/// and seeking.

#include <nmeasim/core/nmea0183/decoder.hpp>
#include <nmeasim/core/simulation/replay_source.hpp>

#include <algorithm>
#include <utility>

namespace nmeasim::core::simulation {

ReplaySource::ReplaySource(ReplayConfig config)
    : config_(std::move(config)), state_(config_.seed) {}

void ReplaySource::emit_entry(std::size_t index) {
    const auto& entry = config_.log.entries[index];
    if (const auto parsed = nmea0183::parse_sentence(entry.sentence)) {
        nmea0183::apply_sentence(*parsed, state_);
        pending_.push_back({parsed->formatter, entry.sentence});
    } else {
        pending_.push_back({{}, entry.sentence});
    }
}

void ReplaySource::rewind() {
    cursor_ = 0;
    state_ = config_.seed;
}

const model::VesselState& ReplaySource::advance(std::chrono::milliseconds dt) {
    const auto& entries = config_.log.entries;
    if (finished_ || entries.empty()) {
        return state_;
    }
    clock_ += dt;
    bool wrapped = false;
    while (true) {
        while (cursor_ < entries.size() && entries[cursor_].offset <= clock_) {
            emit_entry(cursor_);
            ++cursor_;
        }
        if (cursor_ < entries.size()) {
            break;
        }
        if (config_.end == EndBehaviour::Loop && !wrapped && config_.log.duration().count() > 0) {
            // Keep the time that ran past the last entry, so a loop has no hiccup.
            clock_ -= config_.log.duration();
            rewind();
            wrapped = true;
            continue;
        }
        if (config_.end == EndBehaviour::Loop) {
            // A log whose entries all share one offset would spin forever; loop once per tick.
            clock_ = std::chrono::milliseconds{0};
            rewind();
        } else {
            clock_ = config_.log.duration();
            finished_ = true;
        }
        break;
    }
    return state_;
}

void ReplaySource::step_once() {
    const auto& entries = config_.log.entries;
    if (entries.empty()) {
        return;
    }
    if (cursor_ >= entries.size()) {
        if (config_.end != EndBehaviour::Loop) {
            finished_ = true;
            return;
        }
        rewind();
    }
    finished_ = false;
    clock_ = entries[cursor_].offset;
    emit_entry(cursor_);
    ++cursor_;
    if (cursor_ >= entries.size() && config_.end == EndBehaviour::Stop) {
        finished_ = true;
    }
}

std::vector<EmittedSentence> ReplaySource::take_sentences() {
    return std::exchange(pending_, {});
}

void ReplaySource::reset() {
    clock_ = std::chrono::milliseconds{0};
    finished_ = false;
    pending_.clear();
    rewind();
}

std::optional<std::chrono::milliseconds> ReplaySource::duration() const noexcept {
    return config_.log.duration();
}

std::chrono::milliseconds ReplaySource::position() const noexcept {
    return std::min(clock_, config_.log.duration());
}

void ReplaySource::seek(std::chrono::milliseconds position) {
    const auto& entries = config_.log.entries;
    clock_ = std::clamp(position, std::chrono::milliseconds{0}, config_.log.duration());
    pending_.clear();
    rewind();
    // Entries at or before the new position count as already played: rebuild the state from
    // them without emitting.
    while (cursor_ < entries.size() && entries[cursor_].offset <= clock_) {
        if (const auto parsed = nmea0183::parse_sentence(entries[cursor_].sentence)) {
            nmea0183::apply_sentence(*parsed, state_);
        }
        ++cursor_;
    }
    finished_ = cursor_ >= entries.size() && config_.end == EndBehaviour::Stop;
}

}  // namespace nmeasim::core::simulation
