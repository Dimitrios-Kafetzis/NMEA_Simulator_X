// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// AIS messages of the own vessel and their framing into VDO and VDM sentences.
///
/// `pack_position_report` and `pack_static_data` build the bits of messages 1 to 3 and 5
/// from a `model::VesselState`; after `armor`, `frame_payload` splits the payload into
/// sentences. The NMEA 0183 encoders call these functions for the registry entries `VDO-POS`,
/// `VDO-STATIC`, `VDM-POS` and `VDM-STATIC`.

#pragma once

#include <nmeasim/core/ais/sixbit.hpp>
#include <nmeasim/core/model/vessel_state.hpp>

#include <string>
#include <vector>

/// AIS messages of the own vessel as a class A station, part of the `nmeasim::core` library.
///
/// It packs the position report (message types 1, 2 and 3) and the static and voyage data
/// report (message type 5) from the vessel state, armours them into six-bit ASCII and frames
/// them as VDO or VDM sentences. Decoding received AIS traffic is not part of it.
///
/// @see ITU-R M.1371-5, Annex 8.
namespace nmeasim::core::ais {

/// Packs a class A position report from the state.
///
/// The fields and the values sent are:
/// - message type: `state.ais.position_report_type` when it is 1, 2 or 3, otherwise 1;
/// - repeat indicator 0, MMSI `state.ais.mmsi` (its low 30 bits);
/// - navigational status: `state.ais.navigation_status` clamped to [0, 15];
/// - rate of turn: `rate_of_turn_code` of the rate of turn;
/// - speed over ground in tenths of a knot, rounded and clamped to [0, 1022] (1022 means
///   102.2 kn or more); 1023, "not available", without a fix;
/// - position accuracy: 1 with a differential fix, 0 otherwise;
/// - longitude and latitude in 1/10000 minute, east and north positive; 181 and 91 degrees,
///   "not available", without a fix;
/// - course over ground in tenths of a degree, rounded and clamped to [0, 3599]; 3600, "not
///   available", without a fix;
/// - true heading in whole degrees, rounded and clamped to [0, 359], so 511 ("not
///   available") is never sent;
/// - time stamp: the UTC second of `state.time_utc`, in [0, 59];
/// - manoeuvre indicator 0 (not available), spare 0, RAIM flag 0, radio status 0.
///
/// @param state Vessel state to report; "without a fix" means `state.gnss.has_fix` is false.
/// @return The 168 bits of the message.
/// @see ITU-R M.1371-5, Annex 8, messages 1, 2 and 3.
[[nodiscard]] BitPacker pack_position_report(const model::VesselState& state);

/// Packs a static and voyage data report (message type 5) from the state.
///
/// The fields and the values sent are:
/// - repeat indicator 0, MMSI `state.ais.mmsi`, AIS version indicator 0;
/// - IMO number, call sign (7 characters) and name (20 characters) from `state.ais`, text
///   upper-cased, padded with `@` and truncated;
/// - type of ship and cargo: `state.ais.ship_type` clamped to [0, 255];
/// - dimensions in whole metres, rounded and clamped to [0, 511] towards bow and stern and
///   to [0, 63] towards port and starboard;
/// - type of position fixing device 1 (GPS);
/// - ETA month 0, day 0, hour 24 and minute 60, all "not available";
/// - maximum static draught in tenths of a metre, clamped to [0, 25.5] m;
/// - destination, 20 characters, all `@` when empty;
/// - DTE 0 (data terminal available) and one spare bit.
///
/// @param state Vessel state; only `state.ais` is read.
/// @return The 424 bits of the message.
/// @see ITU-R M.1371-5, Annex 8, message 5.
[[nodiscard]] BitPacker pack_static_data(const model::VesselState& state);

/// Splits an armoured payload into the VDO or VDM sentences that carry it.
///
/// Each sentence carries at most 60 payload characters, so that every sentence stays within
/// the 82-character limit of NMEA 0183: a position report fits one sentence, a static data
/// report needs two. The fields are the total number of sentences, the number of this
/// sentence from 1, the sequential message id (empty for a single sentence), the radio
/// channel `A`, the payload fragment and the fill bits (0 in every sentence but the last).
/// An empty payload still yields one sentence.
///
/// @param talker Two-character talker identifier, normally `AI`.
/// @param formatter Sentence formatter: `VDO` for the own vessel or `VDM` for a received
///        target. It is not checked.
/// @param payload Armoured payload as `armor` returns it.
/// @param sequence Sequential message id shared by the sentences of a multi-sentence
///        message, clamped to [0, 9]; ignored when one sentence suffices.
/// @return The sentences in order, each starting with `!` and ending with its checksum,
///         without line terminator.
/// @see NMEA 0183 (IEC 61162-1), sentences VDM and VDO.
[[nodiscard]] std::vector<std::string> frame_payload(std::string_view talker,
                                                     std::string_view formatter,
                                                     const Payload& payload, int sequence);

/// Returns the AIS rate-of-turn code for a rate of turn.
///
/// The code is 4.733 times the square root of the absolute rate, rounded, with the rate's
/// sign, and clamped to [-126, 126]. The special values -128 (not available) and plus or
/// minus 127 (turning without a turn indicator) are never produced.
///
/// @param rate_deg_per_min Rate of turn in degrees per minute, positive to starboard.
/// @return The code in [-126, 126], positive to starboard.
/// @see ITU-R M.1371-5, Annex 8, message 1, field "rate of turn".
[[nodiscard]] int rate_of_turn_code(double rate_deg_per_min) noexcept;

}  // namespace nmeasim::core::ais
