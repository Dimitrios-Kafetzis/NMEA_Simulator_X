// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the sentence catalogue of `nmeasim/core/nmea0183/registry.hpp`.
///
/// Covers nmeasim::core::nmea0183::SentenceRegistry::standard() and its lookup by id,
/// nmeasim::core::nmea0183::encode_within_limit() and the display names of
/// nmeasim::core::nmea0183::SentenceGroup. Every registered encoder is run on the three
/// fixture states of `tests/core/fixtures.hpp` and each sentence is checked for its
/// checksum, the length limit and its address. No fixture file is read.

#include "core/fixtures.hpp"

#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/registry.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>
#include <string>

namespace nmea = nmeasim::core::nmea0183;

TEST_CASE("the standard registry lists every supported sentence once", "[nmea0183][registry]") {
    const auto& registry = nmea::SentenceRegistry::standard();
    const auto descriptors = registry.descriptors();
    CHECK(descriptors.size() == 29U);

    std::set<std::string> ids;
    for (const auto& descriptor : descriptors) {
        CHECK(descriptor.formatter.size() == 3);
        CHECK(descriptor.default_talker.size() == 2);
        CHECK(descriptor.encoder != nullptr);
        CHECK(descriptor.default_period.count() > 0);
        CHECK_FALSE(descriptor.description.empty());
        CHECK(ids.insert(std::string{descriptor.id}).second);
    }
}

TEST_CASE("descriptors are found by id", "[nmea0183][registry]") {
    const auto& registry = nmea::SentenceRegistry::standard();
    const auto* rmc = registry.find("RMC");
    REQUIRE(rmc != nullptr);
    CHECK(rmc->formatter == "RMC");
    CHECK(rmc->default_talker == "GP");
    CHECK(rmc->group == nmea::SentenceGroup::Gnss);
    CHECK(rmc->enabled_by_default);

    const auto* mwv_true = registry.find("MWV-T");
    REQUIRE(mwv_true != nullptr);
    CHECK(mwv_true->formatter == "MWV");
    CHECK_FALSE(mwv_true->enabled_by_default);

    CHECK(registry.find("XYZ") == nullptr);
}

TEST_CASE("every registered encoder produces compliant sentences", "[nmea0183][registry]") {
    const auto& registry = nmea::SentenceRegistry::standard();
    const std::array states{nmeasim::test::fixture_state(),
                            nmeasim::test::fixture_state_without_fix(),
                            nmeasim::test::fixture_state_extreme()};
    for (const auto& state : states) {
        for (const auto& descriptor : registry.descriptors()) {
            INFO("sentence " << descriptor.id);
            const auto sentences =
                nmea::encode_within_limit(descriptor, state, descriptor.default_talker, {});
            REQUIRE_FALSE(sentences.empty());
            for (const auto& sentence : sentences) {
                INFO(sentence);
                CHECK(nmea::verify_checksum(sentence));
                CHECK(nmea::fits_limit(sentence));
                CHECK(sentence.front() ==
                      (descriptor.group == nmea::SentenceGroup::Ais ? '!' : '$'));
                CHECK(sentence.substr(1, 2) == descriptor.default_talker);
                CHECK(sentence.substr(3, 3) == descriptor.formatter);
            }
        }
    }
}

TEST_CASE("encode_within_limit lowers position precision only when needed",
          "[nmea0183][registry]") {
    const auto& registry = nmea::SentenceRegistry::standard();
    const auto* rmc = registry.find("RMC");
    REQUIRE(rmc != nullptr);
    const auto state = nmeasim::test::fixture_state();

    const auto normal = nmea::encode_within_limit(*rmc, state, "GP", {.position_decimals = 4});
    CHECK(nmeasim::test::body_of(normal.front()).find("3759.0280") != std::string::npos);

    const auto high = nmea::encode_within_limit(*rmc, state, "GP", {.position_decimals = 6});
    CHECK(nmeasim::test::body_of(high.front()).find("3759.028000") != std::string::npos);
}

TEST_CASE("encode_within_limit uses at least two decimals and sends what cannot fit",
          "[nmea0183][registry]") {
    const auto& registry = nmea::SentenceRegistry::standard();
    const auto* rmc = registry.find("RMC");
    REQUIRE(rmc != nullptr);
    const auto state = nmeasim::test::fixture_state();

    // Fewer than two decimals are raised to two, the least the limit handling goes down to.
    for (const int decimals : {1, 0, -3}) {
        INFO(decimals);
        const auto low =
            nmea::encode_within_limit(*rmc, state, "GP", {.position_decimals = decimals});
        REQUIRE(low.size() == 1U);
        CHECK(nmeasim::test::body_of(low.front()).find(",3759.03,N,02343.65,E,") !=
              std::string::npos);
    }

    // A sentence still too long at two decimals, here because of an oversized talker, is
    // sent with two decimals rather than dropped.
    const std::string talker(40, 'X');
    const auto oversized = nmea::encode_within_limit(*rmc, state, talker, {.position_decimals = 4});
    REQUIRE(oversized.size() == 1U);
    CHECK_FALSE(nmea::fits_limit(oversized.front()));
    CHECK(nmeasim::test::body_of(oversized.front()).find(",3759.03,N,02343.65,E,") !=
          std::string::npos);
}

TEST_CASE("the length limit without terminator derives from the NMEA 0183 limit",
          "[nmea0183][registry]") {
    CHECK(nmea::kMaxSentenceLength == 82U);
    CHECK(nmea::kMaxSentenceLengthWithoutTerminator == nmea::kMaxSentenceLength - 2U);
}

TEST_CASE("sentence groups have display names", "[nmea0183][registry]") {
    CHECK(nmea::to_string(nmea::SentenceGroup::Gnss) == "GNSS");
    CHECK(nmea::to_string(nmea::SentenceGroup::Wind) == "Wind");
    CHECK(nmea::to_string(nmea::SentenceGroup::Steering) == "Steering");
    CHECK(nmea::to_string(nmea::SentenceGroup::Autopilot) == "Autopilot");
    CHECK(nmea::to_string(nmea::SentenceGroup::Propulsion) == "Propulsion");
}

TEST_CASE("encoders that depend on optional state emit nothing without it",
          "[nmea0183][registry]") {
    const auto& registry = nmea::SentenceRegistry::standard();
    auto state = nmeasim::test::fixture_state();
    state.destination.reset();
    state.engines.clear();
    for (const auto id : {"APB", "RMB", "XTE", "RPM", "XDR"}) {
        const auto* descriptor = registry.find(id);
        REQUIRE(descriptor != nullptr);
        CHECK(
            nmea::encode_within_limit(*descriptor, state, descriptor->default_talker, {}).empty());
    }
}
