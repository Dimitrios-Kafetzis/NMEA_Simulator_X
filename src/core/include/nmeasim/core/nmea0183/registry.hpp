#pragma once

#include <nmeasim/core/nmea0183/encoders.hpp>

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// Catalogue of every sentence the simulator can emit.
namespace nmeasim::core::nmea0183 {

/// Functional group a sentence belongs to. Users can enable or disable whole groups.
enum class SentenceGroup {
    Gnss,
    Time,
    Heading,
    Speed,
    Depth,
    Wind,
    Steering,
};

[[nodiscard]] std::string_view to_string(SentenceGroup group) noexcept;

/// Static description of one sentence: identity, defaults and the encoder that produces it.
struct SentenceDescriptor {
    /// Unique key used in configuration, e.g. "RMC" or "MWV-T". Usually the formatter, with a
    /// suffix when one formatter has several variants.
    std::string_view id;
    /// Three-character sentence formatter, e.g. "RMC".
    std::string_view formatter;
    /// Talker used unless configuration overrides it.
    std::string_view default_talker;
    SentenceGroup group;
    std::chrono::milliseconds default_period;
    bool enabled_by_default;
    std::string_view description;
    Encoder encoder;
};

class SentenceRegistry {
public:
    /// The built-in catalogue, in a stable order suitable for display.
    [[nodiscard]] static const SentenceRegistry& standard();

    [[nodiscard]] std::span<const SentenceDescriptor> descriptors() const noexcept;

    /// Finds a descriptor by id, or nullptr when unknown.
    [[nodiscard]] const SentenceDescriptor* find(std::string_view id) const noexcept;

private:
    explicit SentenceRegistry(std::vector<SentenceDescriptor> descriptors);

    std::vector<SentenceDescriptor> descriptors_;
};

/// Runs an encoder and, if any resulting sentence exceeds the NMEA 0183 length limit, retries
/// with fewer position decimals until every sentence fits or precision reaches two decimals.
[[nodiscard]] std::vector<std::string> encode_within_limit(const SentenceDescriptor& descriptor,
                                                           const model::VesselState& state,
                                                           std::string_view talker,
                                                           EncoderOptions options);

}  // namespace nmeasim::core::nmea0183
