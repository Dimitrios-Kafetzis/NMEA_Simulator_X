// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the version constants and `nmeasim::core::version_description` from the
/// configured version.hpp.
///
/// The case checks that the description starts with `nmeasim::core::kVersion`, that the
/// version string matches its three numeric components, and the shape of the git suffix of
/// a development build. No fixture file is read.

#include <nmeasim/core/version.hpp>

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <string_view>

TEST_CASE("the version description starts with the semantic version", "[core][version]") {
    const std::string_view description = nmeasim::core::version_description();
    REQUIRE(description.starts_with(nmeasim::core::kVersion));

    CHECK(nmeasim::core::kVersion == std::format("{}.{}.{}", nmeasim::core::kVersionMajor,
                                                 nmeasim::core::kVersionMinor,
                                                 nmeasim::core::kVersionPatch));

    // Anything beyond the version is the git description of a development build.
    const std::string_view suffix = description.substr(nmeasim::core::kVersion.size());
    if (!suffix.empty()) {
        CHECK(suffix.starts_with(" ("));
        CHECK(suffix.ends_with(")"));
        CHECK(suffix.size() > 3);
    }
}
