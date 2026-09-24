// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `Simulation`, which advances a source and dispatches to the recorded
/// sentences or the sentence schedule.

#include <nmeasim/core/simulation/simulation.hpp>

namespace nmeasim::core::simulation {

Simulation::Simulation(std::unique_ptr<Source> source, SentenceScheduler scheduler)
    : source_(std::move(source)), scheduler_(std::move(scheduler)) {}

std::vector<EmittedSentence> Simulation::step(std::chrono::milliseconds dt) {
    // The source moves before anything is encoded, so even the first round of sentences
    // describes the state at the end of the step rather than the seed.
    elapsed_ += dt;
    const auto& state = source_->advance(dt);
    if (source_->provides_sentences()) {
        return source_->take_sentences();
    }
    return scheduler_.due(elapsed_, state);
}

std::vector<EmittedSentence> Simulation::step_once(std::chrono::milliseconds dt) {
    if (!source_->provides_sentences()) {
        return step(dt);
    }
    source_->step_once();
    return source_->take_sentences();
}

void Simulation::seek(std::chrono::milliseconds position) {
    // Resetting the schedule sends every sentence at the next step, so that receivers see the
    // new position at once rather than up to one period later.
    source_->seek(position);
    scheduler_.reset();
}

void Simulation::reset() {
    elapsed_ = std::chrono::milliseconds{0};
    source_->reset();
    scheduler_.reset();
}

}  // namespace nmeasim::core::simulation
