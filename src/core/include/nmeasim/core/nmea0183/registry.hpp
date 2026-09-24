// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The catalogue of every NMEA 0183 sentence the simulator can emit.
///
/// `SentenceRegistry::standard` lists each sentence with its registry id, formatter, default
/// talker, group, default period and encoder; the ids are the keys of the `sentences.settings`
/// object and of the output filters in a profile. `encode_within_limit` runs a sentence's
/// encoder and lowers the position precision when a sentence would exceed the length limit.
///
/// @see NMEA 0183 (IEC 61162-1).

#pragma once

#include <nmeasim/core/nmea0183/encoders.hpp>

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// NMEA 0183 sentences: framing and checksums, field formatting, the sentence catalogue, the
/// encoders, the decoder and TAG blocks.
///
/// Part of the `core` library. It follows NMEA 0183 (IEC 61162-1) for sentences and
/// IEC 61162-450 for TAG blocks. `SentenceRegistry` lists every sentence the simulator can
/// emit with the encoder that writes it from a `model::VesselState` through `SentenceBuilder`;
/// `parse_sentence` and `apply_sentence` read sentences back into a state, for example while a
/// log is replayed.
namespace nmeasim::core::nmea0183 {

/// Functional group a sentence belongs to.
///
/// A whole group can be enabled or disabled at once
/// (`simulation::SentenceScheduler::set_group_enabled`), and the interface and the
/// command-line tool show the group of each sentence by its display name from `to_string`.
enum class SentenceGroup {
    Gnss,        ///< GNSS receiver: RMC, GGA, GLL, GSA, GSV and VTG; displayed as "GNSS".
    Time,        ///< Time and date: ZDA; displayed as "Time".
    Heading,     ///< Heading and rate of turn: HDG, HDM, HDT and ROT; displayed as "Heading".
    Speed,       ///< Speed log: VHW and VBW; displayed as "Speed".
    Depth,       ///< Echo sounder: DPT, DBT and water temperature MTW; displayed as "Depth".
    Wind,        ///< Wind instrument: MWV (apparent and true) and MWD; displayed as "Wind".
    Steering,    ///< Rudder angle: RSA; displayed as "Steering".
    Autopilot,   ///< Navigation to the destination: APB, RMB and XTE; displayed as "Autopilot".
    Propulsion,  ///< Engines: RPM and XDR; displayed as "Propulsion".
    Ais,         ///< Own-vessel AIS: VDO and VDM; displayed as "AIS".
};

/// Returns the display name of a sentence group.
///
/// @param group The group to name.
/// @return The name, for example `"GNSS"` or `"AIS"`, or `"Unknown"` for a value that is not
///         an enumerator. The view refers to a string literal and stays valid for the whole
///         program.
[[nodiscard]] std::string_view to_string(SentenceGroup group) noexcept;

/// Static description of one sentence: its identity, its defaults and the encoder that
/// produces it.
///
/// The string views of the descriptors in `SentenceRegistry::standard` refer to string
/// literals and stay valid for the whole program.
struct SentenceDescriptor {
    /// Unique key used in the profile and on the command line, for example `"RMC"` or
    /// `"MWV-T"`.
    ///
    /// It is the formatter, with a suffix when one formatter has several variants.
    std::string_view id;
    /// Three-character sentence formatter, for example `"RMC"`.
    std::string_view formatter;
    /// Two-character talker identifier used unless the profile sets another.
    std::string_view default_talker;
    /// Functional group the sentence is enabled or disabled with.
    SentenceGroup group;
    /// Interval between two emissions unless the profile sets another; positive.
    std::chrono::milliseconds default_period;
    /// Whether the sentence is sent when the profile does not mention it.
    bool enabled_by_default;
    /// One-line summary of the sentence contents, shown in the interface and by the
    /// command-line tool.
    std::string_view description;
    /// Function that produces the sentences for one emission from a vessel state; never null
    /// in the standard registry.
    Encoder encoder;
};

/// Immutable list of sentence descriptors; the only instance is `standard`.
///
/// The constructor is private, so no catalogue other than `standard` can be built. `standard` is
/// safe to call from any thread, and the registry is never modified after its initialisation.
class SentenceRegistry {
public:
    /// Returns the built-in catalogue of all 29 sentences.
    ///
    /// The catalogue is built on first use.
    ///
    /// @return The registry, valid for the whole program. Its descriptors are in a stable
    ///         order, grouped as in `SentenceGroup`, that the interface uses for display.
    [[nodiscard]] static const SentenceRegistry& standard();

    /// Returns every descriptor in catalogue order.
    ///
    /// @return A view of the descriptors, valid as long as the registry.
    [[nodiscard]] std::span<const SentenceDescriptor> descriptors() const noexcept;

    /// Finds a descriptor by its registry id.
    ///
    /// @param id The registry id, for example `"MWV-T"`; the comparison is case-sensitive.
    /// @return The descriptor, valid as long as the registry, or `nullptr` when no descriptor
    ///         has that id.
    [[nodiscard]] const SentenceDescriptor* find(std::string_view id) const noexcept;

private:
    /// Creates a registry holding `descriptors` in the given order.
    ///
    /// @param descriptors The catalogue; the ids are expected to be unique and are not
    ///                    checked.
    explicit SentenceRegistry(std::vector<SentenceDescriptor> descriptors);

    /// The descriptors in catalogue order.
    std::vector<SentenceDescriptor> descriptors_;
};

/// Runs the encoder of a sentence, lowering the position precision until every sentence fits
/// the NMEA 0183 length limit.
///
/// The encoder is first run with `options.position_decimals`, raised to 2 when it is lower.
/// While any resulting sentence is longer than `kMaxSentenceLengthWithoutTerminator`, it is
/// run again with one decimal fewer, down to two decimals; the result of the last attempt is
/// returned even if it is still too long, so a sentence is never dropped for its length.
///
/// @param descriptor The sentence to encode.
/// @param state The vessel state to encode; it is only used during the call.
/// @param talker Two-character talker identifier to send, already resolved from the profile.
/// @param options Formatting options; `options.position_decimals` is the preferred number of
///                fractional minute digits, of which at least 2 are used.
/// @return The sentences of one emission, framed with checksums and without line terminator.
///         Empty only when the encoder has nothing to report, for example APB without a
///         destination.
[[nodiscard]] std::vector<std::string> encode_within_limit(const SentenceDescriptor& descriptor,
                                                           const model::VesselState& state,
                                                           std::string_view talker,
                                                           EncoderOptions options);

}  // namespace nmeasim::core::nmea0183
