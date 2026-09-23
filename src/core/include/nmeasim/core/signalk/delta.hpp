#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

/// Signal K delta messages and the hello message, produced from the vessel state without
/// any JSON library: the documents are small and their shape is fixed.
namespace nmeasim::core::signalk {

struct SignalKOptions {
    /// The context the values belong to, such as `vessels.urn:mrn:imo:mmsi:239000001` or
    /// `aircraft.urn:mrn:signalk:uuid:...`. Empty derives the vessel context from the MMSI.
    std::string context;
    /// Source label reported in every update.
    std::string source_label{"nmeasim"};
};

/// One path of a delta with its value already rendered as JSON.
struct PathValue {
    std::string path;
    std::string json_value;
};

/// The context derived from the state: `vessels.urn:mrn:imo:mmsi:<mmsi>`.
[[nodiscard]] std::string default_context(const model::VesselState& state);

/// The context used for `options`: its own when set, otherwise the default one.
[[nodiscard]] std::string effective_context(const SignalKOptions& options,
                                            const model::VesselState& state);

/// Every path the simulator publishes for the state, in a stable order, SI units and
/// radians as the specification demands.
[[nodiscard]] std::vector<PathValue> path_values(const model::VesselState& state);

/// A delta message with every path for which `admit` returns true (all paths when `admit`
/// is empty), without line terminator.
[[nodiscard]] std::string encode_delta(const model::VesselState& state,
                                       const SignalKOptions& options,
                                       const std::function<bool(std::string_view)>& admit = {});

/// The hello message sent to a client when it connects, naming this server and the self
/// context.
[[nodiscard]] std::string encode_hello(const SignalKOptions& options,
                                       const model::VesselState& state,
                                       std::chrono::system_clock::time_point now);

/// The Signal K identifier of an engine: its label lower-cased without the word "engine" and
/// without punctuation, or `engine<n>` (numbered from 1) when nothing is left.
[[nodiscard]] std::string engine_id(std::string_view label, std::size_t index);

/// A JSON string literal with quotes and escapes.
[[nodiscard]] std::string json_string(std::string_view text);

/// A JSON number; non-finite values become `null`.
[[nodiscard]] std::string json_number(double value);

}  // namespace nmeasim::core::signalk
