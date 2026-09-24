// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::simulation::ReplaySource`, which sends the sentences of a
/// recorded log again with their original timing, and of its use by
/// `nmeasim::core::simulation::Simulation`.
///
/// The cases cover emission as the replay clock passes the entry offsets, the state decoded
/// from the replayed sentences, stepping one entry at a time, seeking, looping, empty logs
/// and logs whose entries share one offset, a plain log timed by its sentences, sentences
/// the decoder does not know, and a simulation that returns the replayed sentences instead
/// of scheduling its own.
///
/// Fixture files: tests/fixtures/logs/recorded.log, 32 sentences recorded by the simulator
/// in four rounds of eight starting at 0, 500, 1000 and 1500 ms, the sentences of a round
/// 20 ms apart, so the last is at 1640 ms; and tests/fixtures/logs/plain.nmea, the same
/// sentences without time prefixes.

#include "core/fixtures.hpp"

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/simulation/replay_source.hpp>
#include <nmeasim/core/simulation/simulation.hpp>
#include <nmeasim/core/time/iso8601.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <string>

using Catch::Approx;
using namespace std::chrono_literals;
using nmeasim::core::time::parse_iso8601;
namespace sim = nmeasim::core::simulation;
namespace logfile = nmeasim::core::log;

namespace {

/// Returns a replay configuration for a log under tests/fixtures.
///
/// Fails the running test case, with the reason from `logfile::load_log`, when the log does
/// not load. The seed is `nmeasim::test::fixture_state` with a depth of 99 m, a value neither
/// fixture log contains, so a test can tell the seed from a decoded depth sentence.
///
/// @param name Path of the log relative to tests/fixtures, such as `logs/recorded.log`.
/// @param end What the replay does after its last entry.
/// @return The configuration.
sim::ReplayConfig config_for(const char* name, sim::EndBehaviour end = sim::EndBehaviour::Stop) {
    std::string error;
    auto loaded = logfile::load_log(nmeasim::test::fixture_path(name), {}, &error);
    INFO(error);
    REQUIRE(loaded.has_value());
    sim::ReplayConfig config;
    config.log = std::move(*loaded);
    config.seed = nmeasim::test::fixture_state();
    config.seed.water.depth_below_transducer_m = 99.0;
    config.end = end;
    return config;
}

}  // namespace

TEST_CASE("a replay emits the recorded sentences as their offsets pass", "[simulation][replay]") {
    sim::ReplaySource source(config_for("logs/recorded.log"));
    CHECK(source.provides_sentences());
    CHECK(source.duration() == 1640ms);
    CHECK(source.position() == 0ms);
    CHECK(source.entry_count() == 32);
    CHECK(source.entry_index() == 0);
    CHECK(source.take_sentences().empty());
    // Nothing has been decoded yet: the seed shows.
    CHECK(source.current().water.depth_below_transducer_m == Approx(99.0));

    // The first 50 ms pass the entries at 0, 20 and 40 ms: RMC, GGA and VTG.
    source.advance(50ms);
    auto sentences = source.take_sentences();
    REQUIRE(sentences.size() == 3);
    CHECK(sentences[0].id == "RMC");
    CHECK(sentences[0].text.starts_with("$GPRMC,100000.00"));
    CHECK(sentences[2].id == "VTG");
    CHECK(source.entry_index() == 3);
    CHECK(source.take_sentences().empty());
    // The state follows the decoded sentences.
    CHECK(source.current().time_utc == parse_iso8601("2026-09-23T10:00:00Z"));
    CHECK(source.current().navigation.position.latitude_deg == Approx(37.98380).margin(1e-5));
    CHECK(source.current().navigation.speed_over_ground_kn == Approx(6.5));

    // Up to 150 ms: the rest of the first round, HDT to MWV at 60 to 140 ms.
    source.advance(100ms);
    sentences = source.take_sentences();
    CHECK(sentences.size() == 5);
    CHECK(source.current().water.depth_below_transducer_m == Approx(12.4));
    CHECK(source.position() == 150ms);

    // The second round starts at exactly 500 ms.
    source.advance(349ms);
    CHECK(source.take_sentences().empty());
    source.advance(1ms);
    CHECK(source.take_sentences().size() == 1);
    CHECK_FALSE(source.finished());

    source.advance(10s);
    CHECK(source.take_sentences().size() == 23);
    CHECK(source.finished());
    CHECK(source.position() == 1640ms);
    CHECK(source.entry_index() == 32);
    source.advance(1s);
    CHECK(source.take_sentences().empty());

    source.reset();
    CHECK_FALSE(source.finished());
    CHECK(source.position() == 0ms);
    CHECK(source.entry_index() == 0);
    CHECK(source.current().water.depth_below_transducer_m == Approx(99.0));
}

TEST_CASE("stepping emits exactly one recorded sentence", "[simulation][replay]") {
    sim::ReplaySource source(config_for("logs/recorded.log"));
    source.step_once();
    auto sentences = source.take_sentences();
    REQUIRE(sentences.size() == 1);
    CHECK(sentences[0].id == "RMC");
    CHECK(source.position() == 0ms);
    source.step_once();
    CHECK(source.take_sentences()[0].id == "GGA");
    CHECK(source.position() == 20ms);
    for (int i = 0; i < 30; ++i) {
        source.step_once();
    }
    CHECK(source.take_sentences().size() == 30);
    CHECK(source.finished());
    CHECK(source.position() == 1640ms);
    source.step_once();
    CHECK(source.take_sentences().empty());

    // The 33rd step wraps round and plays the first entry of the second pass.
    sim::ReplaySource looping(config_for("logs/recorded.log", sim::EndBehaviour::Loop));
    for (int i = 0; i < 33; ++i) {
        looping.step_once();
    }
    CHECK_FALSE(looping.finished());
    CHECK(looping.take_sentences().size() == 33);
    CHECK(looping.entry_index() == 1);
    CHECK(looping.position() == 0ms);
}

TEST_CASE("seeking rebuilds the state without emitting", "[simulation][replay]") {
    sim::ReplaySource source(config_for("logs/recorded.log"));
    source.seek(1000ms);
    CHECK(source.position() == 1000ms);
    CHECK(source.take_sentences().empty());
    // The entries up to 1000 ms (two rounds and the RMC of the third) have been applied to
    // the state.
    CHECK(source.entry_index() == 17);
    CHECK(source.current().water.depth_below_transducer_m == Approx(12.4));
    CHECK(source.current().time_utc == parse_iso8601("2026-09-23T10:00:01Z"));
    CHECK_FALSE(source.finished());
    source.advance(20ms);
    CHECK(source.take_sentences().size() == 1);

    // A seek to zero applies the entry at offset zero, unlike `reset`.
    source.seek(-5s);
    CHECK(source.position() == 0ms);
    CHECK(source.entry_index() == 1);
    source.seek(1h);
    CHECK(source.position() == 1640ms);
    CHECK(source.finished());
    CHECK(source.entry_index() == 32);
    source.seek(500ms);
    CHECK_FALSE(source.finished());
}

TEST_CASE("a looping replay carries the surplus time into the next pass", "[simulation][replay]") {
    sim::ReplaySource source(config_for("logs/recorded.log", sim::EndBehaviour::Loop));
    source.advance(1650ms);
    CHECK_FALSE(source.finished());
    // 32 entries of the first pass plus the first entry of the second (offset 0 <= 10 ms).
    CHECK(source.take_sentences().size() == 33);
    CHECK(source.position() == 10ms);
    CHECK(source.entry_index() == 1);

    // Entries all at one offset loop once per tick rather than forever.
    sim::ReplayConfig flat;
    flat.seed = nmeasim::test::fixture_state();
    flat.end = sim::EndBehaviour::Loop;
    flat.log.entries = {{0ms, "$HEHDT,45.0,T", {}}, {0ms, "$HEHDT,46.0,T", {}}};
    sim::ReplaySource spinning(flat);
    spinning.advance(100ms);
    CHECK(spinning.take_sentences().size() == 2);
    spinning.advance(100ms);
    CHECK(spinning.take_sentences().size() == 2);
    CHECK_FALSE(spinning.finished());

    sim::ReplayConfig empty;
    empty.seed = nmeasim::test::fixture_state();
    sim::ReplaySource nothing(empty);
    nothing.advance(100ms);
    nothing.step_once();
    CHECK(nothing.take_sentences().empty());
    CHECK_FALSE(nothing.finished());
    CHECK(nothing.duration() == 0ms);
}

TEST_CASE("a plain third-party log replays on the time inside its sentences",
          "[simulation][replay]") {
    sim::ReplaySource source(config_for("logs/plain.nmea"));
    // Each round takes the time of its RMC and GGA, from 10:00:00.00 to 10:00:01.50, and
    // the sentences without a time share it.
    CHECK(source.duration() == 1500ms);
    source.advance(100ms);
    CHECK(source.take_sentences().size() == 8);
    source.advance(400ms);
    CHECK(source.take_sentences().size() == 8);
    CHECK(source.current().navigation.heading_true_deg == Approx(45.0));
    // A sentence the decoder does not know still passes through.
    sim::ReplayConfig odd;
    odd.seed = nmeasim::test::fixture_state();
    odd.log.entries = {{0ms, "$PXYZ,1,2", {}}, {0ms, "not nmea", {}}};
    sim::ReplaySource passthrough(odd);
    passthrough.advance(1ms);
    const auto out = passthrough.take_sentences();
    REQUIRE(out.size() == 2);
    CHECK(out[0].id == "XYZ");
    CHECK(out[1].id.empty());
    CHECK(out[1].text == "not nmea");
}

TEST_CASE("a simulation returns replayed sentences instead of scheduling", "[simulation][replay]") {
    auto config = config_for("logs/recorded.log");
    sim::Simulation simulation(std::make_unique<sim::ReplaySource>(std::move(config)),
                               sim::SentenceScheduler{});
    // Entries at 0 to 100 ms; then one step plays only the DBT at 120 ms.
    auto sentences = simulation.step(100ms);
    REQUIRE(sentences.size() == 6);
    CHECK(sentences[0].id == "RMC");
    CHECK(sentences[0].text.starts_with("$GPRMC,100000.00"));
    CHECK(simulation.state().navigation.speed_over_ground_kn == Approx(6.5));
    CHECK(simulation.elapsed() == 100ms);

    sentences = simulation.step_once(100ms);
    REQUIRE(sentences.size() == 1);
    CHECK(sentences[0].id == "DBT");
    CHECK(simulation.source().position() == 120ms);

    simulation.seek(1500ms);
    CHECK(simulation.source().position() == 1500ms);
    // After the seek, the last seven entries at 1520 to 1640 ms remain.
    sentences = simulation.step(200ms);
    CHECK(sentences.size() == 7);
    CHECK(simulation.finished());

    simulation.reset();
    CHECK_FALSE(simulation.finished());
    CHECK(simulation.source().position() == 0ms);
    // Stepping a state source once is an ordinary step.
    sim::DeltaConfig delta;
    delta.seed = nmeasim::test::fixture_state();
    sim::Simulation ordinary(std::make_unique<sim::DeltaSource>(delta), sim::SentenceScheduler{});
    CHECK_FALSE(ordinary.step_once(100ms).empty());
    CHECK(ordinary.elapsed() == 100ms);
}
