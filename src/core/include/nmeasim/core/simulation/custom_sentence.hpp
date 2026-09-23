#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

/// Sentences typed in by the operator and sent on their own period, with the checksum
/// computed by the simulator.
namespace nmeasim::core::simulation {

struct CustomSentence {
    /// Identifier used by output filters and the console; must not clash with a registry
    /// id. Empty ids are given `CUSTOM-n` by the scheduler.
    std::string id;
    /// The sentence body: an optional `$` or `!`, the address, the fields, and optionally
    /// an old `*hh` and line terminator, both ignored. Example: `$PXYZ,1,2,3` or `PXYZ,1,2,3`.
    std::string body;
    std::chrono::milliseconds period{1000};
    bool enabled{true};
};

/// Frames a custom body: keeps or adds the start delimiter, drops any existing checksum and
/// terminator, and appends the correct checksum. Returns nullopt when the body is empty,
/// carries characters outside printable ASCII or reserved ones, has no address, or would not
/// fit the length limit.
[[nodiscard]] std::optional<std::string> frame_custom_sentence(std::string_view body);

/// Why `frame_custom_sentence` refuses a body, or nullopt when it is acceptable.
[[nodiscard]] std::optional<std::string> validate_custom_sentence(std::string_view body);

}  // namespace nmeasim::core::simulation
