// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::core::simulation::SentenceScheduler` and
/// `nmeasim::core::simulation::next_due_after`, which decide which NMEA 0183 sentences are
/// due at a simulated time.
///
/// The cases cover the registry defaults, emission at time zero and after each period,
/// per-sentence periods, enable flags and talkers, group switches, the fallback for a
/// non-positive period, reset, and the rule that keeps a sentence on its cadence without
/// bursts after a stall or drift from steps that do not divide the period, and a check that
/// proprietary custom sentences are told apart from registry formatters when counting. No
/// fixture file is read; the sentences are encoded from `nmeasim::test::fixture_state`.

#include "core/fixtures.hpp"

#include <nmeasim/core/simulation/sentence_scheduler.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using namespace std::chrono_literals;
namespace sim = nmeasim::core::simulation;
namespace nmea = nmeasim::core::nmea0183;

namespace {

/// Returns the formatter of a framed sentence, read from its address field.
///
/// The address is the text between the start delimiter (`$` or `!`) and the first comma or
/// `*`. A proprietary address, which starts with `P` and has no talker, is returned whole;
/// any other address gives its last three characters, whatever the length of its talker.
///
/// @param text A framed sentence such as `$GPRMC,...*hh`, `!AIVDO,...*hh` or `$PXYZ,1*hh`.
/// @return The formatter, such as `RMC` or `VDO`, or the whole proprietary address, such as
///   `PXYZ`; empty when the text does not start with `$` or `!`.
std::string_view formatter_of(std::string_view text) {
    if (text.empty() || (text.front() != '$' && text.front() != '!')) {
        return {};
    }
    const auto address = text.substr(1, text.find_first_of(",*") - 1);
    if (address.starts_with('P') || address.size() <= 3) {
        return address;
    }
    return address.substr(address.size() - 3);
}

/// Counts the sentences of one formatter.
///
/// @param sentences Sentences returned by the scheduler.
/// @param formatter Three-letter sentence formatter, such as `RMC`, or the whole address of
///   a proprietary sentence, such as `PXYZ`; compared with `formatter_of` of each text.
/// @return The number of sentences whose formatter is `formatter`; a sentence split into
///   several lines, such as GSV, counts once per line.
int count_formatter(const std::vector<sim::EmittedSentence>& sentences,
                    std::string_view formatter) {
    return static_cast<int>(std::count_if(
        sentences.begin(), sentences.end(),
        [&](const sim::EmittedSentence& s) { return formatter_of(s.text) == formatter; }));
}

}  // namespace

TEST_CASE("the scheduler starts with registry defaults", "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    CHECK(scheduler.setting("RMC").enabled);
    CHECK(scheduler.setting("RMC").period == 1000ms);
    CHECK(scheduler.setting("RMC").talker.empty());
    CHECK_FALSE(scheduler.setting("MWV-T").enabled);
    CHECK_FALSE(scheduler.setting("NOPE").enabled);
}

TEST_CASE("everything enabled is due at time zero, then again after its period",
          "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    const auto state = nmeasim::test::fixture_state();

    const auto first = scheduler.due(0ms, state);
    CHECK(count_formatter(first, "RMC") == 1);
    CHECK(first.front().id == "RMC");
    // The fixture has ten satellites in view, four per GSV sentence.
    CHECK(count_formatter(first, "GSV") == 3);
    CHECK(count_formatter(first, "MWV") == 1);  // only the apparent variant is on by default

    CHECK(scheduler.due(500ms, state).empty());
    CHECK(scheduler.due(999ms, state).empty());
    const auto second = scheduler.due(1000ms, state);
    CHECK(count_formatter(second, "RMC") == 1);
}

TEST_CASE("proprietary custom sentences are counted by address and not as registry formatters",
          "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    scheduler.set_custom_sentences({{"GARMIN", "$PGRME,15.0,M,45.0,M,25.0,M", 1000ms, true},
                                    {"XYZ", "$PXYZ,1", 1000ms, true}});
    const auto first = scheduler.due(0ms, nmeasim::test::fixture_state());
    CHECK(count_formatter(first, "PGRME") == 1);
    CHECK(count_formatter(first, "PXYZ") == 1);
    // The characters after a two-letter talker's position are not a formatter here.
    CHECK(count_formatter(first, "RME") == 0);
    CHECK(count_formatter(first, "RMC") == 1);
}

TEST_CASE("per-sentence periods and enable flags are honoured", "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    const auto state = nmeasim::test::fixture_state();
    scheduler.set_period_for_all(1000ms);
    scheduler.configure("HDT", {.enabled = true, .talker = "", .period = 200ms});
    scheduler.set_enabled("GSV", false);
    scheduler.set_group_enabled(nmea::SentenceGroup::Wind, false);

    int hdt = 0;
    int rmc = 0;
    int gsv = 0;
    int wind = 0;
    for (auto now = 0ms; now < 2000ms; now += 100ms) {
        const auto sentences = scheduler.due(now, state);
        hdt += count_formatter(sentences, "HDT");
        rmc += count_formatter(sentences, "RMC");
        gsv += count_formatter(sentences, "GSV");
        wind += count_formatter(sentences, "MWV") + count_formatter(sentences, "MWD");
    }
    // Over the steps from 0 to 1900 ms, HDT is due every 200 ms and RMC at 0 and 1000 ms.
    CHECK(hdt == 10);
    CHECK(rmc == 2);
    CHECK(gsv == 0);
    CHECK(wind == 0);
}

TEST_CASE("a stalled host does not burst-catch-up", "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    const auto state = nmeasim::test::fixture_state();
    (void)scheduler.due(0ms, state);
    const auto late = scheduler.due(10'000ms, state);
    CHECK(count_formatter(late, "RMC") == 1);
    CHECK(scheduler.due(10'500ms, state).empty());
}

TEST_CASE("talker overrides apply per sentence", "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    const auto state = nmeasim::test::fixture_state();
    scheduler.configure("RMC", {.enabled = true, .talker = "GN", .period = 1000ms});
    // A one-letter talker is invalid and ignored, so HDT keeps its default talker HE.
    scheduler.configure("HDT", {.enabled = true, .talker = "X", .period = 1000ms});

    const auto sentences = scheduler.encode_all(state);
    CHECK(std::any_of(sentences.begin(), sentences.end(), [](const sim::EmittedSentence& s) {
        return s.id == "RMC" && s.text.starts_with("$GNRMC,");
    }));
    CHECK(std::any_of(sentences.begin(), sentences.end(), [](const sim::EmittedSentence& s) {
        return s.id == "HDT" && s.text.starts_with("$HEHDT,");
    }));
}

TEST_CASE("a non-positive period falls back to the registry default", "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    scheduler.configure("RMC", {.enabled = true, .talker = "", .period = 0ms});
    CHECK(scheduler.setting("RMC").period == 1000ms);
    scheduler.set_period_for_all(0ms);
    CHECK(scheduler.setting("RMC").period == 1000ms);
}

TEST_CASE("reset makes every sentence due again", "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    const auto state = nmeasim::test::fixture_state();
    (void)scheduler.due(0ms, state);
    CHECK(scheduler.due(100ms, state).empty());
    scheduler.reset();
    CHECK_FALSE(scheduler.due(100ms, state).empty());
}

TEST_CASE("the next due time follows the slot, not the late step", "[simulation][scheduler]") {
    CHECK(sim::next_due_after(1000ms, 1000ms, 1000ms) == 2000ms);
    CHECK(sim::next_due_after(1000ms, 1089ms, 1000ms) == 2000ms);
    CHECK(sim::next_due_after(1000ms, 1999ms, 1000ms) == 2000ms);
    // More than a period behind: restart from now instead of catching up with a burst.
    CHECK(sim::next_due_after(1000ms, 2000ms, 1000ms) == 3000ms);
    CHECK(sim::next_due_after(0ms, 50000ms, 1000ms) == 51000ms);
}

TEST_CASE("steps shorter than a tenth of the period keep a one second sentence at 1 Hz",
          "[simulation][scheduler]") {
    sim::SentenceScheduler scheduler;
    const auto state = nmeasim::test::fixture_state();

    // 99 ms steps: every emission is late by up to one step, which must not accumulate.
    std::vector<std::chrono::milliseconds> fixes;
    for (auto now = 0ms; now < 20000ms; now += 99ms) {
        if (count_formatter(scheduler.due(now, state), "RMC") == 1) {
            fixes.push_back(now);
        }
    }
    REQUIRE(fixes.size() == 20);
    for (std::size_t i = 0; i < fixes.size(); ++i) {
        const auto slot = std::chrono::milliseconds{1000 * static_cast<long long>(i)};
        CHECK(fixes[i] >= slot);
        CHECK(fixes[i] < slot + 99ms);
    }
}
