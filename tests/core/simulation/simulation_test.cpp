#include "core/fixtures.hpp"

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/simulation/simulation.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>

using Catch::Approx;
using namespace std::chrono_literals;
namespace sim = nmeasim::core::simulation;

namespace {

sim::Simulation make_simulation() {
    sim::DeltaConfig config;
    config.seed = nmeasim::test::fixture_state();
    config.heading = {0.0, 0.0};
    config.speed = {0.0, 0.0};
    return sim::Simulation(std::make_unique<sim::DeltaSource>(config), sim::SentenceScheduler{});
}

}  // namespace

TEST_CASE("stepping advances time, state and emits due sentences", "[simulation]") {
    auto simulation = make_simulation();
    CHECK(simulation.elapsed() == 0ms);

    const auto first = simulation.step(100ms);
    CHECK(simulation.elapsed() == 100ms);
    CHECK_FALSE(first.empty());
    for (const auto& sentence : first) {
        CHECK(nmeasim::core::nmea0183::verify_checksum(sentence));
    }

    CHECK(simulation.step(100ms).empty());
    for (int i = 0; i < 8; ++i) {
        (void)simulation.step(100ms);
    }
    CHECK(simulation.elapsed() == 1000ms);
    CHECK(simulation.step(100ms).size() == first.size());

    const auto& state = simulation.state();
    CHECK(state.time_utc - nmeasim::test::fixture_state().time_utc == 1100ms);
    CHECK(state.navigation.position.latitude_deg > 37.9838);
}

TEST_CASE("the schedule and source can be reconfigured through the simulation", "[simulation]") {
    auto simulation = make_simulation();
    simulation.scheduler().set_period_for_all(500ms);
    auto& source = dynamic_cast<sim::DeltaSource&>(simulation.source());
    source.set_override(sim::Parameter::HeadingTrue, 90.0);

    (void)simulation.step(100ms);  // everything is due on the first step, next due at 600 ms
    CHECK(simulation.state().navigation.heading_true_deg == Approx(90.0));
    CHECK(simulation.step(400ms).empty());
    CHECK_FALSE(simulation.step(100ms).empty());
    CHECK_FALSE(simulation.finished());
}

TEST_CASE("reset restarts the clock, the source and the schedule", "[simulation]") {
    auto simulation = make_simulation();
    (void)simulation.step(2000ms);
    simulation.reset();
    CHECK(simulation.elapsed() == 0ms);
    CHECK(simulation.state().navigation.position.latitude_deg == Approx(37.9838));
    CHECK_FALSE(simulation.step(1ms).empty());
}
