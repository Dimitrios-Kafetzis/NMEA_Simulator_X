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
    CHECK(descriptors.size() == 25U);

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
