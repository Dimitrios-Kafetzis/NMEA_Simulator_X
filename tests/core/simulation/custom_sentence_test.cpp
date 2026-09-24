// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of operator-defined sentences: `nmeasim::core::simulation::frame_custom_sentence`,
/// `nmeasim::core::simulation::validate_custom_sentence` and their scheduling by
/// `nmeasim::core::simulation::SentenceScheduler::set_custom_sentences`.
///
/// The cases cover framing with a recomputed checksum, every reason a body is refused, and
/// how the scheduler names, times, disables and drops custom sentences. No fixture file is
/// read; the scheduler encodes `nmeasim::test::fixture_state`.

#include "core/fixtures.hpp"

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/simulation/custom_sentence.hpp>
#include <nmeasim/core/simulation/sentence_scheduler.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

using namespace std::chrono_literals;
namespace sim = nmeasim::core::simulation;

TEST_CASE("custom bodies are framed with a fresh checksum", "[simulation][custom]") {
    // Each expected checksum is the XOR of the characters between the delimiter and `*`,
    // computed in Python.
    CHECK(sim::frame_custom_sentence("$PXYZ,1,2,3") == "$PXYZ,1,2,3*17");
    CHECK(sim::frame_custom_sentence("PXYZ,1,2,3") == "$PXYZ,1,2,3*17");
    CHECK(sim::frame_custom_sentence("  $PXYZ,1,2,3*00\r\n") == "$PXYZ,1,2,3*17");
    CHECK(sim::frame_custom_sentence("!AIVDM,1,1,,A,13aEOK?P00PD2wVMdLDRhgvL289?,0") ==
          "!AIVDM,1,1,,A,13aEOK?P00PD2wVMdLDRhgvL289?,0*26");
    CHECK(sim::frame_custom_sentence("$IIXDR,P,1.013,B,BARO") == "$IIXDR,P,1.013,B,BARO*6F");
    CHECK(nmeasim::core::nmea0183::verify_checksum(*sim::frame_custom_sentence("$PXYZ,1,2,3")));
}

TEST_CASE("invalid custom bodies are refused with a reason", "[simulation][custom]") {
    CHECK_FALSE(sim::frame_custom_sentence("").has_value());
    CHECK(sim::validate_custom_sentence("   ")->find("empty") != std::string::npos);
    CHECK(sim::validate_custom_sentence("$PX")->find("address") != std::string::npos);
    CHECK(sim::validate_custom_sentence("$P-XYZ,1")->find("address") != std::string::npos);
    CHECK(sim::validate_custom_sentence("$PXYZ,1*2")->find("checksum") != std::string::npos);
    CHECK(sim::validate_custom_sentence("$PXYZ,a*b,c")->find("checksum") != std::string::npos);
    CHECK(sim::validate_custom_sentence("$PXYZ,caf\xc3\xa9")->find("printable") !=
          std::string::npos);
    CHECK(sim::validate_custom_sentence("$PXYZ,a$b")->find("printable") != std::string::npos);
    // With 75 characters of field the framed sentence has 84 characters, over the limit of
    // 80 without CR LF; with 71 it has exactly 80.
    CHECK(sim::validate_custom_sentence("$PXYZ," + std::string(75, 'x')) ==
          "The sentence exceeds 80 characters with its checksum");
    CHECK_FALSE(sim::validate_custom_sentence("$PXYZ," + std::string(71, 'x')).has_value());
    CHECK_FALSE(sim::validate_custom_sentence("$PXYZ,1,2,3*17").has_value());
}

TEST_CASE("the scheduler emits custom sentences on their own period", "[simulation][custom]") {
    sim::SentenceScheduler scheduler;
    scheduler.set_period_for_all(1000ms);
    scheduler.set_custom_sentences({
        {"", "$PXYZ,1,2,3", 500ms, true},
        {"BARO", "IIXDR,P,1.013,B,BARO", 0ms, true},
        {"OFF", "$PQRS,off", 1000ms, false},
        {"BAD", "not a sentence!", 1000ms, true},
        {"RMC", "$GPRMC,clash", 1000ms, true},
    });
    // OFF is kept, disabled; BAD cannot be framed and RMC clashes with a registry id, so
    // both are dropped. The empty id and the zero period are filled in.
    REQUIRE(scheduler.custom_sentences().size() == 3);
    CHECK(scheduler.custom_sentences()[0].id == "CUSTOM-1");
    CHECK(scheduler.custom_sentences()[0].period == 500ms);
    CHECK(scheduler.custom_sentences()[1].id == "BARO");
    CHECK(scheduler.custom_sentences()[1].period == 1000ms);
    CHECK(scheduler.custom_sentences()[2].id == "OFF");
    CHECK_FALSE(scheduler.custom_sentences()[2].enabled);

    const auto state = nmeasim::test::fixture_state();
    const auto first = scheduler.due(0ms, state);
    REQUIRE(first.size() >= 2);
    CHECK(first[first.size() - 2].id == "CUSTOM-1");
    CHECK(first[first.size() - 2].text == "$PXYZ,1,2,3*17");
    CHECK(first.back().id == "BARO");
    CHECK(first.back().text == "$IIXDR,P,1.013,B,BARO*6F");
    CHECK(std::none_of(first.begin(), first.end(),
                       [](const sim::EmittedSentence& s) { return s.id == "OFF"; }));

    // Between 100 ms and 2000 ms the 500 ms sentence is due at 500, 1000, 1500 and 2000 ms,
    // the one second sentence at 1000 and 2000 ms.
    int custom = 0;
    int baro = 0;
    for (auto now = 100ms; now <= 2000ms; now += 100ms) {
        for (const auto& sentence : scheduler.due(now, state)) {
            custom += sentence.id == "CUSTOM-1" ? 1 : 0;
            baro += sentence.id == "BARO" ? 1 : 0;
        }
    }
    CHECK(custom == 4);
    CHECK(baro == 2);

    const auto all = scheduler.encode_all(state);
    CHECK(std::count_if(all.begin(), all.end(), [](const sim::EmittedSentence& s) {
              return s.id == "CUSTOM-1" || s.id == "BARO";
          }) == 2);
    // After a reset everything is due again, custom sentences included.
    CHECK(scheduler.due(2050ms, state).empty());
    scheduler.reset();
    const auto again = scheduler.due(2050ms, state);
    CHECK(std::any_of(again.begin(), again.end(),
                      [](const sim::EmittedSentence& s) { return s.id == "BARO"; }));
    scheduler.set_custom_sentences({});
    CHECK(scheduler.custom_sentences().empty());
}

TEST_CASE("custom sentence ids are filled in and duplicates are found", "[simulation][custom]") {
    const std::vector<sim::CustomSentence> sentences{
        {"", "$PXYZ,1", 1000ms, true},
        {"BARO", "$PXYZ,2", 1000ms, true},
        {"", "$PXYZ,3", 1000ms, true},
    };
    CHECK(sim::effective_custom_id(sentences[0], 0) == "CUSTOM-1");
    CHECK(sim::effective_custom_id(sentences[1], 1) == "BARO");
    CHECK(sim::effective_custom_id(sentences[2], 2) == "CUSTOM-3");
    CHECK_FALSE(sim::find_duplicate_custom_id(sentences).has_value());
    CHECK_FALSE(sim::find_duplicate_custom_id({}).has_value());

    // An explicit id repeated later, and an explicit id equal to an automatic one.
    auto repeated = sentences;
    repeated.push_back({"BARO", "$PXYZ,4", 1000ms, true});
    CHECK(sim::find_duplicate_custom_id(repeated) == 3U);
    auto collision = sentences;
    collision[1].id = "CUSTOM-3";
    CHECK(sim::find_duplicate_custom_id(collision) == 2U);
}

TEST_CASE("the scheduler drops a custom sentence whose id is already taken",
          "[simulation][custom]") {
    sim::SentenceScheduler scheduler;
    scheduler.set_custom_sentences({
        {"CUSTOM-2", "$PXYZ,1", 1000ms, true},
        {"", "$PXYZ,2", 1000ms, true},
        {"BARO", "$PXYZ,3", 1000ms, true},
        {"BARO", "$PXYZ,4", 1000ms, true},
    });
    // The second sentence would be CUSTOM-2 and the fourth repeats BARO: the first sentence
    // with an id keeps it.
    REQUIRE(scheduler.custom_sentences().size() == 2);
    CHECK(scheduler.custom_sentences()[0].id == "CUSTOM-2");
    CHECK(scheduler.custom_sentences()[0].body == "$PXYZ,1");
    CHECK(scheduler.custom_sentences()[1].id == "BARO");
    CHECK(scheduler.custom_sentences()[1].body == "$PXYZ,3");
}
