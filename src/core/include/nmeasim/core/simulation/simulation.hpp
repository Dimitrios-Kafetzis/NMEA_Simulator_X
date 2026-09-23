#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/sentence_scheduler.hpp>
#include <nmeasim/core/simulation/source.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

/// Ties a source and a sentence schedule together on a simulated clock.
///
/// The host (CLI or desktop application) owns the real timers and calls `step` at its tick
/// rate; the simulation never blocks, sleeps or touches I/O.
namespace nmeasim::core::simulation {

class Simulation {
public:
    Simulation(std::unique_ptr<Source> source, SentenceScheduler scheduler);

    /// Advances simulated time by `dt` and returns the sentences due at the new time. For a
    /// source that provides sentences (a log replay) the recorded sentences that became due
    /// are returned instead and the schedule is not consulted.
    [[nodiscard]] std::vector<EmittedSentence> step(std::chrono::milliseconds dt);

    /// The smallest step a host can take while paused: one recorded sentence for a source
    /// that provides sentences, otherwise the same as `step(dt)`.
    [[nodiscard]] std::vector<EmittedSentence> step_once(std::chrono::milliseconds dt);

    /// Moves a finite source to `position` and makes every sentence due again.
    void seek(std::chrono::milliseconds position);

    /// Time elapsed since start or the last reset.
    [[nodiscard]] std::chrono::milliseconds elapsed() const noexcept { return elapsed_; }

    [[nodiscard]] const model::VesselState& state() const noexcept { return source_->current(); }
    [[nodiscard]] Source& source() noexcept { return *source_; }
    [[nodiscard]] const Source& source() const noexcept { return *source_; }
    [[nodiscard]] SentenceScheduler& scheduler() noexcept { return scheduler_; }
    [[nodiscard]] const SentenceScheduler& scheduler() const noexcept { return scheduler_; }

    /// True once the source has nothing more to produce.
    [[nodiscard]] bool finished() const noexcept { return source_->finished(); }

    /// Restarts the source and the schedule.
    void reset();

private:
    std::unique_ptr<Source> source_;
    SentenceScheduler scheduler_;
    std::chrono::milliseconds elapsed_{0};
};

}  // namespace nmeasim::core::simulation
