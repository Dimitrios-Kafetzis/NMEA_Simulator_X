// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Signal K delta and hello messages built from the vessel state.
///
/// `path_values` lists every published path with its value, `encode_delta` wraps them in a
/// delta message and `encode_hello` produces the greeting a WebSocket server sends first.
/// docs/reference/signalk.md lists the paths, their units and when they are present.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

/// Signal K output, part of the `nmeasim::core` library.
///
/// It renders the vessel state as Signal K delta messages and the hello message, as one
/// line of JSON each, without any JSON library: the documents are small and their shape is
/// fixed. Values use the units of the specification (SI units and radians). Serving the
/// messages over WebSocket or TCP is the job of `nmeasim::io`.
///
/// @see https://signalk.org/specification/1.7.0/doc/
namespace nmeasim::core::signalk {

/// Settings shared by the delta and hello encoders, one set per output.
struct SignalKOptions {
    /// The context the values belong to, such as `vessels.urn:mrn:imo:mmsi:239000001` or
    /// `aircraft.urn:mrn:signalk:uuid:...`; sent as given. Empty derives the vessel context
    /// from the MMSI with `default_context`.
    std::string context;
    /// Source label reported as `source.label` in every update.
    std::string source_label{"nmeasim"};
};

/// One path of a delta with its value already rendered as JSON.
struct PathValue {
    /// Dotted Signal K path, such as `navigation.speedOverGround`.
    std::string path;
    /// The value as a JSON literal: a number, `null`, a string or an object.
    std::string json_value;
};

/// Returns the vessel context derived from the state's MMSI.
///
/// @param state Vessel state; only `state.ais.mmsi` is read.
/// @return `vessels.urn:mrn:imo:mmsi:` followed by the MMSI in decimal.
/// @see https://signalk.org/specification/1.7.0/doc/data_model.html
[[nodiscard]] std::string default_context(const model::VesselState& state);

/// Returns the context used for an output.
///
/// @param options Output settings.
/// @param state Vessel state, for the default context.
/// @return `options.context` when it is not empty, otherwise `default_context(state)`.
[[nodiscard]] std::string effective_context(const SignalKOptions& options,
                                            const model::VesselState& state);

/// Returns every path the simulator publishes for the state, with its value.
///
/// Angles are in radians, rates of turn in radians per second, speeds in metres per second,
/// lengths in metres, temperatures in kelvin and revolutions in hertz. Bearings are in
/// [0, 2π), relative angles in (-π, π] positive to starboard, and positions are objects of
/// `longitude` and `latitude` in degrees. Numbers are rounded to seven decimals.
///
/// Paths that need a GNSS fix (position, course and speed over ground, dilutions and antenna
/// data) are left out without one; the `navigation.courseRhumbline` paths appear only with
/// a destination; the `environment.depth` paths depend on the sign of the transducer
/// offset; each engine adds three `propulsion.<id>` paths.
///
/// @param state Vessel state to publish.
/// @return The paths in a fixed order (navigation, environment, steering, propulsion), each
///         path at most once.
/// @see https://signalk.org/specification/1.7.0/doc/data_model.html
[[nodiscard]] std::vector<PathValue> path_values(const model::VesselState& state);

/// Encodes a delta message with one update carrying the selected paths.
///
/// The update names the source by `options.source_label` with type `simulator` and is
/// timestamped with the simulated clock `state.time_utc`.
///
/// @param state Vessel state to publish.
/// @param options Context and source label.
/// @param admit Filter called with each path; a path is sent when it returns true. An empty
///        function sends every path. A filter that admits nothing yields an empty `values`
///        array.
/// @return One JSON document without line terminator.
/// @see https://signalk.org/specification/1.7.0/doc/data_model.html
[[nodiscard]] std::string encode_delta(const model::VesselState& state,
                                       const SignalKOptions& options,
                                       const std::function<bool(std::string_view)>& admit = {});

/// Encodes the hello message sent to a client when it connects.
///
/// The message names this server by `kProjectName` and `kVersion`, gives the effective
/// context as `self` and the roles `master` and `main`.
///
/// @param options Output settings, for the context.
/// @param state Vessel state, for the default context.
/// @param now Wall-clock time of the connection, sent as `timestamp`.
/// @return One JSON document without line terminator.
/// @see https://signalk.org/specification/1.7.0/doc/streaming_api.html
[[nodiscard]] std::string encode_hello(const SignalKOptions& options,
                                       const model::VesselState& state,
                                       std::chrono::system_clock::time_point now);

/// Returns the Signal K identifier of an engine.
///
/// The identifier is the `<id>` of the engine's `propulsion.<id>` paths. The label is
/// reduced to the characters `std::isalnum` accepts (the ASCII letters and digits in the
/// default locale), lower-cased, and its first occurrence of `engine` is removed:
/// `Port engine` becomes `port`, `Main-Diesel #2` becomes `maindiesel2`.
///
/// @param label Engine label from the profile.
/// @param index Zero-based position of the engine in `model::VesselState::engines`.
/// @return The identifier, or `engine` followed by `index + 1` when nothing is left of the
///         label.
[[nodiscard]] std::string engine_id(std::string_view label, std::size_t index);

/// Returns a JSON string literal for a text, quotes included.
///
/// Quotes, backslashes and control characters are escaped: `\n`, `\r` and `\t` by name, the
/// others as `\u00XX`. Bytes from 0x80 upwards are copied unchanged, so UTF-8 text passes
/// through; invalid UTF-8 is not detected.
///
/// @param text Text to quote.
/// @return The JSON string literal.
[[nodiscard]] std::string json_string(std::string_view text);

/// Returns a JSON number literal for a value.
///
/// The value is rounded to seven decimals in fixed notation and trailing zeros are trimmed,
/// so `12.0` becomes `12` and noise below the instruments' resolution disappears. Negative
/// zero, including values that round to it, becomes `0`.
///
/// @param value Value to format.
/// @return The number literal, or `null` when `value` is infinite or NaN.
[[nodiscard]] std::string json_number(double value);

}  // namespace nmeasim::core::signalk
