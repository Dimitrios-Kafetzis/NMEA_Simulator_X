#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <string>
#include <string_view>
#include <vector>

/// Encoders that turn a vessel state into NMEA 0183 sentences.
///
/// Each encoder returns every sentence it produces for one emission: most return exactly one,
/// GSV returns as many as needed to list the satellites in view. Encoders never throw and
/// never emit a sentence over the length limit for the given options; the registry lowers the
/// position precision if a sentence would otherwise be too long.
namespace nmeasim::core::nmea0183 {

/// Options that change how an encoder formats its fields.
struct EncoderOptions {
    /// Fractional minute digits in latitude and longitude fields. 4 digits resolve 0.19 m.
    int position_decimals{4};
};

/// Everything an encoder needs to produce its sentences for one emission.
struct EncoderContext {
    /// The vessel state to encode; it must outlive the encoder call.
    const model::VesselState& state;
    /// Two-character talker identifier already resolved from configuration.
    std::string_view talker;
    /// Formatting options, possibly lowered by encode_within_limit().
    EncoderOptions options{};
};

/// Signature shared by every encoder; stored in SentenceDescriptor::encoder.
using Encoder = std::vector<std::string> (*)(const EncoderContext&);

// GNSS
/// RMC: time, status, position, speed and course over ground, date, variation and mode.
std::vector<std::string> encode_rmc(const EncoderContext& context);
/// GGA: time, position, fix quality, satellites in use, HDOP, altitude, geoid separation.
std::vector<std::string> encode_gga(const EncoderContext& context);
/// GLL: position, time, status and mode indicator.
std::vector<std::string> encode_gll(const EncoderContext& context);
/// GSA: automatic mode, 3D fix (1 when none), PRNs in use, PDOP, HDOP and VDOP.
std::vector<std::string> encode_gsa(const EncoderContext& context);
/// GSV: satellites in view with synthetic elevation, azimuth and SNR, four per sentence.
std::vector<std::string> encode_gsv(const EncoderContext& context);
/// VTG: course over ground true and magnetic, speed over ground in knots and km/h, mode.
std::vector<std::string> encode_vtg(const EncoderContext& context);
/// ZDA: UTC time, day, month, four-digit year and a zero local zone offset.
std::vector<std::string> encode_zda(const EncoderContext& context);

// Heading and speed
/// HDG: magnetic heading with deviation and variation, each with its E/W letter.
std::vector<std::string> encode_hdg(const EncoderContext& context);
/// HDM: heading referenced to magnetic north.
std::vector<std::string> encode_hdm(const EncoderContext& context);
/// HDT: heading referenced to true north.
std::vector<std::string> encode_hdt(const EncoderContext& context);
/// VHW: true and magnetic heading, speed through the water in knots and km/h.
std::vector<std::string> encode_vhw(const EncoderContext& context);
/// VBW: longitudinal water speed and ground speed resolved along and across the hull.
std::vector<std::string> encode_vbw(const EncoderContext& context);
/// ROT: rate of turn in degrees per minute, negative to port.
std::vector<std::string> encode_rot(const EncoderContext& context);

// Depth and water
/// DPT: depth below the transducer and the transducer offset, in metres.
std::vector<std::string> encode_dpt(const EncoderContext& context);
/// DBT: depth below the transducer in feet, metres and fathoms.
std::vector<std::string> encode_dbt(const EncoderContext& context);
/// MTW: water temperature in degrees Celsius.
std::vector<std::string> encode_mtw(const EncoderContext& context);

// Wind
/// MWV with reference 'R': apparent wind angle relative to the bow and speed in knots.
std::vector<std::string> encode_mwv_apparent(const EncoderContext& context);
/// MWV with reference 'T': true wind angle relative to the bow and speed in knots.
std::vector<std::string> encode_mwv_true(const EncoderContext& context);
/// MWD: true wind direction, true and magnetic, speed in knots and metres per second.
std::vector<std::string> encode_mwd(const EncoderContext& context);

// Steering
/// RSA: rudder angle in the starboard (or single) rudder field; the port field is empty.
std::vector<std::string> encode_rsa(const EncoderContext& context);

// Autopilot: one sentence each while a destination is set, nothing otherwise
/// APB: cross-track error, steer direction, arrival flags, leg and destination bearings.
std::vector<std::string> encode_apb(const EncoderContext& context);
/// RMB: cross-track error, destination position, range, bearing and closing velocity.
std::vector<std::string> encode_rmb(const EncoderContext& context);
/// XTE: cross-track error in nautical miles and the direction to steer.
std::vector<std::string> encode_xte(const EncoderContext& context);

// Propulsion: one sentence per engine, nothing without engines
/// RPM: shaft revolutions of each engine, zero while it is stopped.
std::vector<std::string> encode_rpm(const EncoderContext& context);
/// XDR: coolant temperature and tachometer of each engine, named `ENGINE#n`.
std::vector<std::string> encode_xdr(const EncoderContext& context);

// AIS own vessel: the position report and the static data report, each as VDO (own vessel)
// or VDM (as other receivers would relay it)
/// VDO position report (message type 1, 2 or 3) of the own vessel.
std::vector<std::string> encode_vdo_position(const EncoderContext& context);
/// VDO static and voyage data (message type 5) of the own vessel, in two fragments.
std::vector<std::string> encode_vdo_static(const EncoderContext& context);
/// VDM position report of the own vessel, framed as a received message.
std::vector<std::string> encode_vdm_position(const EncoderContext& context);
/// VDM static and voyage data of the own vessel, framed as a received message.
std::vector<std::string> encode_vdm_static(const EncoderContext& context);

/// Restricts a waypoint name to the characters NMEA 0183 allows in a field and to
/// `model::kMaxWaypointNameLength` characters. An empty result becomes "WPT".
[[nodiscard]] std::string sanitize_waypoint_name(std::string_view name);

/// Mode indicator letter used by RMC, GLL and VTG: 'A' autonomous, 'D' differential,
/// 'N' no fix.
[[nodiscard]] char mode_indicator(const model::GnssFix& fix) noexcept;

/// Status letter used by RMC and GLL: 'A' valid, 'V' invalid.
[[nodiscard]] char status_indicator(const model::GnssFix& fix) noexcept;

}  // namespace nmeasim::core::nmea0183
