#pragma once

#include <nmeasim/core/ais/sixbit.hpp>
#include <nmeasim/core/model/vessel_state.hpp>

#include <string>
#include <vector>

/// AIS messages for the own vessel (ITU-R M.1371): the class A position report (message
/// types 1, 2 and 3) and the static and voyage data report (message type 5).
namespace nmeasim::core::ais {

/// Packs a position report from the state. The message type is `state.ais.position_report_type`
/// (1, 2 or 3, anything else is sent as 1). Without a GNSS fix the position, speed and course
/// carry their "not available" values.
[[nodiscard]] BitPacker pack_position_report(const model::VesselState& state);

/// Packs a static and voyage data report (type 5) from the state.
[[nodiscard]] BitPacker pack_static_data(const model::VesselState& state);

/// Splits an armored payload into the VDO or VDM sentences that carry it. `formatter` is
/// "VDO" or "VDM"; `sequence` is the sequential message id used when more than one
/// sentence is needed (0 to 9). Every sentence fits the NMEA 0183 length limit.
[[nodiscard]] std::vector<std::string> frame_payload(std::string_view talker,
                                                     std::string_view formatter,
                                                     const Payload& payload, int sequence);

/// The AIS rate-of-turn code for a rate in degrees per minute: 4.733 times the square root
/// of the rate, with its sign, clamped to plus or minus 126.
[[nodiscard]] int rate_of_turn_code(double rate_deg_per_min) noexcept;

}  // namespace nmeasim::core::ais
