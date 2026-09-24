// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `Simulation`, which advances a source and a sentence schedule together on a simulated
/// clock.
///
/// The host (the `io::SimulationRunner` used by the command-line tool and the desktop
/// application, or a test) owns the real timers and calls `Simulation::step` at its tick
/// rate.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/sentence_scheduler.hpp>
#include <nmeasim/core/simulation/source.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

/// The simulation engine of the Qt-free `core` library: the sources that produce the vessel
/// state over time, the schedule that turns it into sentences, and the `Simulation` that ties
/// them together.
///
/// A `Source` produces the vessel state in one of three modes: `DeltaSource` lets seed values
/// drift at random within bounds, with operator overrides and rudder steering; `TrackSource`
/// sails a GPX or KML track leg by leg; `ReplaySource` sends the sentences of a recorded log
/// again with their original timing. The `SentenceScheduler` holds each sentence's enable
/// flag, talker and period, together with the operator's `CustomSentence` list, and encodes
/// whatever is due; the result is a list of `EmittedSentence` values. `Simulation` owns a
/// source and a scheduler and advances both.
///
/// Time is simulated: it advances only when the host calls `Simulation::step` with the
/// duration to advance by, which the host usually measures on a wall clock. Nothing in this
/// namespace starts threads, sleeps, reads a clock or performs I/O, so a test can run hours
/// of simulated time in milliseconds.
///
/// @see docs/explanation/simulation-model.md
namespace nmeasim::core::simulation {

/// A source and a sentence schedule, advanced together by the host on a simulated clock.
///
/// Each `step` advances the source by `dt` and returns the sentences due at the new time:
/// either those the scheduler encodes from the new state, or, for a source that provides
/// sentences (a log replay), the recorded sentences that became due. The simulation keeps its
/// own elapsed time for the scheduler, which is independent of the source's position: a seek
/// or a looping source does not change it.
///
/// The simulation owns its source and its scheduler. It is movable but not copyable, because
/// it owns the source through a `std::unique_ptr`.
class Simulation {
public:
    /// Creates a simulation at elapsed time zero from a source and a schedule.
    ///
    /// @param source The source to advance; the simulation takes ownership of it.
    /// @param scheduler The sentence schedule, with its settings and custom sentences
    ///   already applied or to be changed later through `scheduler()`. Its registry must
    ///   outlive the simulation.
    /// @pre `source` is not null.
    Simulation(std::unique_ptr<Source> source, SentenceScheduler scheduler);

    /// Advances simulated time by `dt` and returns the sentences due at the new time.
    ///
    /// The elapsed time grows by `dt` and the source advances by `dt`. For a source that
    /// provides sentences, the recorded sentences that became due are returned and the
    /// schedule is not consulted; otherwise the scheduler encodes every sentence due at the
    /// new elapsed time from the new state. Everything enabled is due on the first step.
    ///
    /// Once a finite source is finished, stepping still works: the elapsed time keeps growing
    /// and a track source's final state keeps being encoded on schedule, while a finished
    /// replay returns nothing. The host decides what to do when `finished()` becomes true.
    ///
    /// @param dt Simulated time to advance by. `io::SimulationRunner` passes the wall-clock
    ///   time since its previous tick in whole milliseconds, capped at one second, and
    ///   carries the sub-millisecond remainder into the next tick so that the simulated
    ///   clock keeps pace with the real one.
    /// @return The due sentences in order: registry sentences in registry order, then custom
    ///   sentences, or the recorded sentences in log order. Empty when nothing is due.
    /// @pre `dt` is not negative.
    [[nodiscard]] std::vector<EmittedSentence> step(std::chrono::milliseconds dt);

    /// Takes the smallest step a host can take while paused.
    ///
    /// For a source that provides sentences, this emits exactly the next recorded sentence
    /// and moves the source's clock to its offset; `dt` is ignored and the elapsed time does
    /// not change. For any other source it is the same as `step(dt)`.
    ///
    /// @param dt Simulated time to advance a source that does not provide sentences by.
    /// @return The sentences produced: at most one recorded sentence (none at the end of a
    ///   replay that stops), or what `step(dt)` returns.
    /// @pre `dt` is not negative.
    [[nodiscard]] std::vector<EmittedSentence> step_once(std::chrono::milliseconds dt);

    /// Moves the source to `position` and makes every sentence due again at the next step.
    ///
    /// The elapsed time is not changed. An endless source ignores the seek, but the schedule
    /// is still reset.
    ///
    /// @param position Position within the source, clamped by the source to
    ///   [0, `source().duration()`].
    void seek(std::chrono::milliseconds position);

    /// Returns the simulated time since construction or the last reset.
    ///
    /// @return The sum of every `dt` passed to `step`, including through `step_once` for a
    ///   source that does not provide sentences. It is simulated time, not wall-clock time.
    [[nodiscard]] std::chrono::milliseconds elapsed() const noexcept { return elapsed_; }

    /// Returns the source's current state.
    ///
    /// @return The state owned by the source. The reference stays valid as long as the
    ///   simulation; the object changes with every step, seek and reset.
    [[nodiscard]] const model::VesselState& state() const noexcept { return source_->current(); }
    /// Returns the source, for mode-specific controls such as overrides or a seek.
    ///
    /// @return The owned source, valid as long as the simulation. Seeking through it directly
    ///   does not reset the schedule, unlike `seek`.
    [[nodiscard]] Source& source() noexcept { return *source_; }
    /// Returns the source, read-only.
    ///
    /// @return The owned source, valid as long as the simulation.
    [[nodiscard]] const Source& source() const noexcept { return *source_; }
    /// Returns the sentence schedule, for changing settings while running.
    ///
    /// @return The owned scheduler, valid as long as the simulation.
    [[nodiscard]] SentenceScheduler& scheduler() noexcept { return scheduler_; }
    /// Returns the sentence schedule, read-only.
    ///
    /// @return The owned scheduler, valid as long as the simulation.
    [[nodiscard]] const SentenceScheduler& scheduler() const noexcept { return scheduler_; }

    /// Returns whether the source has nothing more to produce.
    ///
    /// @return `Source::finished()` of the source: true once a finite source that stops at
    ///   its end has reached it, always false for an endless or looping source.
    [[nodiscard]] bool finished() const noexcept { return source_->finished(); }

    /// Restarts the source and the schedule and sets the elapsed time back to zero.
    ///
    /// Every enabled sentence is due again at the next step. The scheduler's settings and
    /// custom sentences are kept.
    void reset();

private:
    /// The source being advanced; never null.
    std::unique_ptr<Source> source_;
    /// The schedule consulted when the source does not provide sentences.
    SentenceScheduler scheduler_;
    /// Simulated time since construction or the last reset, the clock of the schedule.
    std::chrono::milliseconds elapsed_{0};
};

}  // namespace nmeasim::core::simulation
