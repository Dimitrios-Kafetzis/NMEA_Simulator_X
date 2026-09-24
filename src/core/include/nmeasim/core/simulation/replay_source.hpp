// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The replay mode: the sentences of a recorded log are sent again with their original
/// timing.
///
/// `ReplaySource` is the finite `Source` configured by a `ReplayConfig`; it provides the
/// recorded sentences themselves instead of a state for the encoders. ADR 0011 and ADR 0012
/// explain the design, and docs/explanation/simulation-model.md, section "Replay mode", the
/// behaviour.

#pragma once

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/emitted_sentence.hpp>
#include <nmeasim/core/simulation/source.hpp>
#include <nmeasim/core/simulation/track_source.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace nmeasim::core::simulation {

/// Configuration of a `ReplaySource`.
struct ReplayConfig {
    /// The parsed log to replay; its first entry has offset zero.
    log::Log log;
    /// The state before any entry has been replayed: it provides the values the log never
    /// carries (for example the depth in a GNSS-only log) and the clock until the first
    /// sentence with a time.
    model::VesselState seed;
    /// Whether to stop after the last entry or start again from the first.
    EndBehaviour end{EndBehaviour::Stop};
};

/// Replays the entries of a log as the replay clock passes their offsets.
///
/// The replay clock is simulated time since the first entry, advanced by `advance`. An entry
/// is due when its offset is at or before the clock; each due entry is queued, unchanged, for
/// `take_sentences` and decoded into the vessel state, so that the dashboard and the map
/// follow the replay. The state's `time_utc` therefore follows the time fields inside the
/// sentences, not the replay clock. A line that does not parse is still sent, with an empty
/// id, and leaves the state unchanged.
///
/// Pausing is the host's business: it stops calling `advance`. `step_once` emits exactly the
/// next entry, and `seek` moves the clock, rebuilding the state from the entries before the
/// new position without sending them.
///
/// At the end, a stopping replay holds the clock at the last offset and becomes finished. A
/// looping replay starts again from the seed state and the first entry, carrying all the time
/// that ran past the last entry into the next pass: should the carried time reach the end of
/// the new pass as well, that pass is played too, as often as `dt` spans passes, as a
/// looping `TrackSource` does. A looping log whose entries all share one offset has no
/// duration to carry time over and is played once per `advance`. An empty log produces
/// nothing and is finished from the start, whatever its end behaviour, so that a run of it
/// ends at once.
class ReplaySource final : public Source {
public:
    /// Creates a replay positioned before the first entry, with the state set to
    /// `config.seed`.
    ///
    /// Nothing is decoded or queued until the first `advance`, `step_once` or `seek`. A
    /// replay of an empty log is finished at once.
    ///
    /// @param config The log, the seed state and the end behaviour.
    explicit ReplaySource(ReplayConfig config);

    /// Advances the replay clock by `dt` and queues every entry it passes.
    ///
    /// Entries accumulate until `take_sentences` collects them. A finished replay and an
    /// empty log ignore the call.
    ///
    /// @param dt Simulated time to advance the replay clock by. With zero, the entries at
    ///   the current offset that have not been played yet are still played.
    /// @return The state after decoding the entries played.
    /// @pre `dt` is not negative.
    const model::VesselState& advance(std::chrono::milliseconds dt) override;
    /// Returns the state decoded so far.
    ///
    /// @return The state owned by the source, valid for its lifetime.
    [[nodiscard]] const model::VesselState& current() const noexcept override { return state_; }
    /// Returns to the start: clock zero, no entry played, the seed state, and no queued
    /// sentences; not finished, unless the log is empty.
    ///
    /// Unlike `seek` to zero, the entries at offset zero have not been played afterwards and
    /// are sent by the next `advance`.
    void reset() override;
    /// Returns whether a stopping replay has played its last entry.
    ///
    /// @return True after the last entry of a replay configured with `EndBehaviour::Stop`
    ///   has been played, or after a seek to its end; always true for an empty log and
    ///   always false for a looping replay of a log with entries.
    [[nodiscard]] bool finished() const noexcept override { return finished_; }
    /// Returns the length of the replay.
    ///
    /// @return The offset of the last entry, zero for an empty log. Never `std::nullopt`.
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const noexcept override;
    /// Returns the replay clock.
    ///
    /// @return The time since the first entry, in [0, `duration()`]. After a loop it counts
    ///   from zero again.
    [[nodiscard]] std::chrono::milliseconds position() const noexcept override;
    /// Moves the replay clock and rebuilds the state for the new position without sending
    /// anything.
    ///
    /// The queued sentences are discarded, the state returns to the seed, and every entry
    /// whose offset is at or before the new position is decoded into the state and counts as
    /// played, so that the instruments show the right values the moment the replay
    /// continues. Seeking to the end of a stopping replay finishes it; seeking anywhere else
    /// clears `finished()`, except for an empty log, which stays finished.
    ///
    /// @param position Time since the first entry, clamped to [0, `duration()`].
    void seek(std::chrono::milliseconds position) override;

    /// Returns true: a replay provides recorded sentences, and the sentence schedule stays
    /// idle.
    ///
    /// @return Always true.
    [[nodiscard]] bool provides_sentences() const noexcept override { return true; }
    /// Hands over the queued sentences.
    ///
    /// @return The entries played since the previous call, in log order, each with the
    ///   formatter decoded from it as id (empty when the line does not parse) and the
    ///   recorded line as text. The queue is empty afterwards.
    std::vector<EmittedSentence> take_sentences() override;
    /// Plays exactly the next entry and moves the replay clock to its offset.
    ///
    /// After the last entry, a stopping replay does nothing but report `finished()`, and a
    /// looping one starts again from the seed state with the first entry. An empty log
    /// ignores the call.
    void step_once() override;

    /// Returns the index of the next entry to play.
    ///
    /// @return An index into the log's entries, equal to `entry_count()` once every entry of
    ///   the current pass has been played.
    [[nodiscard]] std::size_t entry_index() const noexcept { return cursor_; }
    /// Returns the number of entries in the log.
    ///
    /// @return The number of entries.
    [[nodiscard]] std::size_t entry_count() const noexcept { return config_.log.entries.size(); }
    /// Returns the configuration the source was built with.
    ///
    /// @return The configuration, unchanged since construction.
    [[nodiscard]] const ReplayConfig& config() const noexcept { return config_; }

private:
    /// Decodes one entry into the state and queues it for `take_sentences`.
    ///
    /// @param index Index of the entry in the log.
    /// @pre `index` is less than `entry_count()`.
    void emit_entry(std::size_t index);
    /// Moves the cursor to the first entry and restores the seed state, leaving the clock,
    /// the queue and `finished_` as they are.
    void rewind();

    /// The log, seed and end behaviour.
    ReplayConfig config_;
    /// The state decoded from the entries played in the current pass, on top of the seed.
    model::VesselState state_;
    /// Sentences played but not yet collected by `take_sentences`.
    std::vector<EmittedSentence> pending_;
    /// The replay clock: simulated time since the first entry of the current pass.
    std::chrono::milliseconds clock_{0};
    /// Index of the next entry to play.
    std::size_t cursor_{0};
    /// Whether a stopping replay has played its last entry, or the log is empty.
    bool finished_{false};
};

}  // namespace nmeasim::core::simulation
