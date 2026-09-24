// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `Source` interface that every simulation mode implements.
///
/// A source produces the vessel state as simulated time advances. Its transport members
/// (`duration`, `position`, `seek`, `step_once`) have harmless defaults for endless sources,
/// so that hosts drive a delta simulation, a track and a log replay through one interface;
/// ADR 0011 explains the design.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/emitted_sentence.hpp>

#include <chrono>
#include <optional>
#include <vector>

namespace nmeasim::core::simulation {

/// Base class of the simulation modes: delta drift, track following and log replay.
///
/// A source owns its vessel state and changes it only when one of its members is called; it
/// has no clock of its own. Simulated time advances by the `dt` given to `advance`, and the
/// `time_utc` of the state follows the rules of each mode. An endless source (`DeltaSource`)
/// keeps the defaults of the transport members; a finite one (`TrackSource`, `ReplaySource`)
/// reports a duration, a position within it and `finished()`, and can seek.
///
/// Copying and moving are protected, so that only a derived class can be copied and a source
/// held through a `Source` reference cannot be sliced.
///
/// @see DeltaSource
/// @see TrackSource
/// @see ReplaySource
class Source {
public:
    /// Destroys the source and the state it owns.
    virtual ~Source() = default;

    /// Advances the source by `dt` of simulated time.
    ///
    /// @param dt Simulated time to advance by. Zero is allowed; a replay then still emits the
    ///   entries due at its current offset.
    /// @return The state at the new time, the same object `current` returns.
    /// @pre `dt` is not negative; time only moves backwards through `seek` or `reset`.
    virtual const model::VesselState& advance(std::chrono::milliseconds dt) = 0;

    /// Returns the most recently produced state.
    ///
    /// @return The state the source owns. The reference stays valid for the lifetime of the
    ///   source; the object it refers to changes with every call that moves the source.
    [[nodiscard]] virtual const model::VesselState& current() const noexcept = 0;

    /// Returns the source to its initial state and position zero.
    ///
    /// What survives a reset (for example a destination set by the host) is stated by each
    /// source.
    virtual void reset() = 0;

    /// Returns whether a finite source has nothing more to produce.
    ///
    /// @return True once a source configured to stop at its end has reached it; always false
    ///   for an endless source and for a looping one. The default returns false.
    [[nodiscard]] virtual bool finished() const noexcept { return false; }

    /// Returns the length of a finite source in simulated time.
    ///
    /// @return The duration, zero for a source with nothing to play, or `std::nullopt` for an
    ///   endless source. The default returns `std::nullopt`.
    [[nodiscard]] virtual std::optional<std::chrono::milliseconds> duration() const noexcept {
        return std::nullopt;
    }

    /// Returns the elapsed position within a finite source.
    ///
    /// @return The position in [0, `duration()`]; it restarts from zero when a looping source
    ///   wraps. The default, for endless sources, returns zero.
    [[nodiscard]] virtual std::chrono::milliseconds position() const noexcept { return {}; }

    /// Moves a finite source to a position; endless sources ignore it.
    ///
    /// The position argument, time since the start of the source, is clamped to [0, `duration()`].
    /// The state reflects the new position immediately, without a call to `advance`. The default
    /// does nothing.
    virtual void seek(std::chrono::milliseconds /*position*/) {}

    /// Returns whether the source replays recorded sentences instead of producing a state for
    /// the encoders.
    ///
    /// @return True for a source whose sentences `Simulation` returns from `take_sentences`,
    ///   leaving the sentence schedule idle. The default returns false.
    [[nodiscard]] virtual bool provides_sentences() const noexcept { return false; }

    /// Hands over the sentences that became due since the previous call.
    ///
    /// @return The sentences produced by `advance`, `step_once` or both since the last call,
    ///   in the order they became due; the source keeps none of them. The default, for
    ///   sources that do not provide sentences, returns an empty vector.
    virtual std::vector<EmittedSentence> take_sentences() { return {}; }

    /// Advances a sentence-providing source by exactly one recorded sentence and moves its
    /// clock to that sentence's offset; other sources ignore it.
    ///
    /// The sentence is collected by the next `take_sentences`. The default does nothing.
    virtual void step_once() {}

    /// Sets or clears the destination that the autopilot sentences describe; sources that
    /// decode their state from recorded sentences ignore it.
    ///
    /// A destination argument of `std::nullopt` clears it. The default does
    /// nothing.
    virtual void set_destination(std::optional<model::Destination> /*destination*/) {}

protected:
    /// Creates the base of a source; only derived classes construct one.
    Source() = default;
    /// Copies the base of a source; protected so that only derived classes copy, which
    /// prevents slicing.
    Source(const Source&) = default;
    /// Copy-assigns the base of a source; protected so that only derived classes assign.
    ///
    /// @return This object.
    Source& operator=(const Source&) = default;
    /// Moves the base of a source; protected so that only derived classes move, which
    /// prevents slicing.
    Source(Source&&) = default;
    /// Move-assigns the base of a source; protected so that only derived classes assign.
    ///
    /// @return This object.
    Source& operator=(Source&&) = default;
};

}  // namespace nmeasim::core::simulation
