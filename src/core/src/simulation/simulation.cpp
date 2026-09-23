#include <nmeasim/core/simulation/simulation.hpp>

namespace nmeasim::core::simulation {

Simulation::Simulation(std::unique_ptr<Source> source, SentenceScheduler scheduler)
    : source_(std::move(source)), scheduler_(std::move(scheduler)) {}

std::vector<EmittedSentence> Simulation::step(std::chrono::milliseconds dt) {
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
    source_->seek(position);
    scheduler_.reset();
}

void Simulation::reset() {
    elapsed_ = std::chrono::milliseconds{0};
    source_->reset();
    scheduler_.reset();
}

}  // namespace nmeasim::core::simulation
