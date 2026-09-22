#include <nmeasim/core/simulation/simulation.hpp>

namespace nmeasim::core::simulation {

Simulation::Simulation(std::unique_ptr<Source> source, SentenceScheduler scheduler)
    : source_(std::move(source)), scheduler_(std::move(scheduler)) {}

std::vector<EmittedSentence> Simulation::step(std::chrono::milliseconds dt) {
    elapsed_ += dt;
    const auto& state = source_->advance(dt);
    return scheduler_.due(elapsed_, state);
}

void Simulation::reset() {
    elapsed_ = std::chrono::milliseconds{0};
    source_->reset();
    scheduler_.reset();
}

}  // namespace nmeasim::core::simulation
