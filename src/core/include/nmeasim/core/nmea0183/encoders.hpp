// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Encoders that turn a vessel state into NMEA 0183 sentences.
///
/// There is one encoder per registry entry, all with the signature Encoder: GNSS (RMC, GGA,
/// GLL, GSA, GSV, VTG, ZDA), heading and speed (HDG, HDM, HDT, VHW, VBW, ROT), depth and water
/// (DPT, DBT, MTW), wind (MWV, MWD), steering (RSA), autopilot (APB, RMB, XTE), propulsion
/// (RPM, XDR) and the AIS own-vessel messages framed as VDO or VDM. The registry in
/// `registry.hpp` maps each profile identifier to its encoder and calls it through
/// encode_within_limit(). `docs/reference/nmea0183-sentences.md` lists every field with a
/// golden example.
///
/// Each encoder returns every sentence it produces for one emission, framed with `$` or `!`
/// and a checksum and without the CR LF terminator. Most return exactly one; GSV returns one
/// per four satellites in view; the autopilot sentences return none while no destination is
/// set; the propulsion sentences return one per engine. Numbers use a fixed number of
/// decimals per field and never render negative zero. Encoders never throw and do not check
/// the length limit themselves: encode_within_limit() lowers the position precision when a
/// sentence would otherwise be too long.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace nmeasim::core::nmea0183 {

/// Options that change how an encoder formats its fields.
struct EncoderOptions {
    /// Fractional minute digits in latitude and longitude fields, including the destination
    /// position in RMB.
    ///
    /// Four digits resolve 0.0001 minute, about 0.19 m. encode_within_limit() lowers the
    /// value one digit at a time, down to 2, when a sentence would exceed the length limit.
    int position_decimals{4};
};

/// Everything an encoder needs to produce its sentences for one emission.
///
/// The context refers to the state and the talker without owning them; both must outlive
/// the encoder call.
struct EncoderContext {
    /// The vessel state to encode, owned by the caller; encoders only read it.
    const model::VesselState& state;
    /// Two-character talker identifier, such as `GP`, already resolved from the profile or
    /// the registry default; sent as given.
    std::string_view talker;
    /// Formatting options, possibly lowered by encode_within_limit().
    EncoderOptions options{};
};

/// Pointer to an encoder function, the signature every encoder shares; stored in
/// SentenceDescriptor::encoder.
///
/// The function returns the framed sentences for one emission, in transmission order, and
/// possibly none.
using Encoder = std::vector<std::string> (*)(const EncoderContext&);

// GNSS

/// Encodes RMC, the recommended minimum navigation data.
///
/// Fields: UTC time `hhmmss.ss`, status (status_indicator()), latitude and longitude, speed
/// over ground in knots and course over ground in degrees true with one decimal, date
/// `ddmmyy`, magnetic variation with one decimal and its `E` or `W` letter (`E` for zero), and
/// the mode indicator (mode_indicator()). Without a fix the position, speed and course fields
/// are empty.
///
/// @param context The state, talker and position precision to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence RMC.
std::vector<std::string> encode_rmc(const EncoderContext& context);

/// Encodes GGA, the GNSS fix data.
///
/// Fields: UTC time, latitude and longitude, the fix quality code of model::FixQuality,
/// satellites in use clamped to [0, 12] as two digits, HDOP, altitude above mean sea level in
/// metres, `M`, geoid separation in metres, `M`, and two empty fields for the age of
/// differential data and the differential station. Without a fix the sentence reports quality
/// `0`, satellites `00` and empty position, HDOP, altitude and geoid separation.
///
/// @param context The state, talker and position precision to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence GGA.
std::vector<std::string> encode_gga(const EncoderContext& context);

/// Encodes GLL, the geographic position.
///
/// Fields: latitude and longitude (empty without a fix), UTC time, status
/// (status_indicator()) and mode indicator (mode_indicator()).
///
/// @param context The state, talker and position precision to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence GLL.
std::vector<std::string> encode_gll(const EncoderContext& context);

/// Encodes GSA, the active satellites and dilution of precision.
///
/// Fields: selection mode `A` (automatic), fix type `3` (3D) with a fix and `1` (none)
/// without, twelve PRN slots of which the first `satellites_in_use` (clamped to [0, 12]) hold
/// the PRNs of the fixed simulated constellation (02, 05, 07, 09, ...) and the rest are empty,
/// then PDOP, HDOP and VDOP with one decimal. Without a fix all PRN and DOP fields are empty.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence GSA.
std::vector<std::string> encode_gsa(const EncoderContext& context);

/// Encodes GSV, the satellites in view, four per sentence.
///
/// The number in view is the larger of `satellites_in_view` and `satellites_in_use`, each
/// clamped to [0, 12], and 0 without a fix. Every sentence carries the total number of
/// sentences, its own number from 1 and the number in view, followed by up to four
/// satellites, each as PRN, elevation in degrees, azimuth in degrees and SNR in dB. The
/// elevation, azimuth and SNR are synthetic but fixed per PRN, so the output is
/// deterministic. With no satellites in view a single sentence `1,1,00` without satellite
/// blocks is sent.
///
/// @param context The state and talker to encode with.
/// @return One to three sentences, in order.
/// @see NMEA 0183, sentence GSV.
std::vector<std::string> encode_gsv(const EncoderContext& context);

/// Encodes VTG, the course over ground and ground speed.
///
/// Fields: course over ground in degrees true, `T`, course over ground in degrees magnetic
/// (true course minus variation), `M`, speed over ground in knots, `N`, the same speed in
/// km/h, `K`, and the mode indicator (mode_indicator()). Without a fix the four values are
/// empty and their unit letters are kept.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence VTG.
std::vector<std::string> encode_vtg(const EncoderContext& context);

/// Encodes ZDA, the UTC time and date.
///
/// Fields: UTC time, day and month as two digits, the four-digit year, and local zone hours
/// and minutes, both always `00`. The time and date are sent whether or not there is a fix.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence ZDA.
std::vector<std::string> encode_zda(const EncoderContext& context);

// Heading and speed

/// Encodes HDG, the magnetic sensor heading with deviation and variation.
///
/// Fields: the compass heading model::Navigation::heading_magnetic_deg() (true heading minus
/// variation and deviation), the magnitude of the deviation with its `E` or `W` letter and
/// the magnitude of the variation with its letter, each with one decimal; a zero value is
/// sent as `E`.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence HDG.
std::vector<std::string> encode_hdg(const EncoderContext& context);

/// Encodes HDM, the heading referenced to magnetic north.
///
/// Fields: model::Navigation::heading_magnetic_deg() with one decimal, then `M`. That value
/// is the compass heading HDG sends: the true heading minus variation and deviation, which
/// equals the magnetic heading only while the deviation is zero.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence HDM.
std::vector<std::string> encode_hdm(const EncoderContext& context);

/// Encodes HDT, the heading referenced to true north.
///
/// Fields: `heading_true_deg` with one decimal, then `T`.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence HDT.
std::vector<std::string> encode_hdt(const EncoderContext& context);

/// Encodes VHW, the water speed and heading.
///
/// Fields: heading in degrees true, `T`, heading from
/// model::Navigation::heading_magnetic_deg() (the compass heading, as in encode_hdm()), `M`,
/// speed through the water in knots, `N`, and the same speed in km/h, `K`, each with one
/// decimal.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence VHW.
std::vector<std::string> encode_vhw(const EncoderContext& context);

/// Encodes VBW, the dual ground and water speed along and across the hull.
///
/// The longitudinal water speed is the speed through the water and the transverse water
/// speed is `0.0`, because the model has no leeway. The speed over ground is resolved onto
/// the hull through the drift angle between course over ground and true heading: the
/// longitudinal component is positive ahead, the transverse component positive to
/// starboard. The stern transverse speeds are `0.0`. All speeds are in knots with one
/// decimal and every status field is `A`, also without a fix.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence VBW.
std::vector<std::string> encode_vbw(const EncoderContext& context);

/// Encodes ROT, the rate of turn.
///
/// Fields: the rate of turn in degrees per minute with one decimal, negative to port, and
/// status `A`.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence ROT.
std::vector<std::string> encode_rot(const EncoderContext& context);

// Depth and water

/// Encodes DPT, the depth below the transducer with the transducer offset.
///
/// Fields: depth below the transducer in metres, the transducer offset in metres (positive
/// to the water line, negative to the keel), each with one decimal, and an empty maximum
/// range scale.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence DPT.
std::vector<std::string> encode_dpt(const EncoderContext& context);

/// Encodes DBT, the depth below the transducer in three units.
///
/// Fields: depth in feet, `f`, in metres, `M`, and in fathoms, `F`, each with one decimal.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence DBT.
std::vector<std::string> encode_dbt(const EncoderContext& context);

/// Encodes MTW, the water temperature.
///
/// Fields: the temperature in degrees Celsius with one decimal, then `C`.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence MTW.
std::vector<std::string> encode_mtw(const EncoderContext& context);

// Wind

/// Encodes MWV with reference `R`: the apparent wind.
///
/// Fields: apparent wind angle relative to the bow, clockwise in [0, 360), `R`, apparent
/// wind speed in knots, `N`, and status `A`.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence MWV.
std::vector<std::string> encode_mwv_apparent(const EncoderContext& context);

/// Encodes MWV with reference `T`: the true wind relative to the bow.
///
/// Fields: the true wind direction minus the true heading, clockwise in [0, 360)
/// (model::Wind::true_angle_relative_deg()), `T`, true wind speed in knots, `N`, and status
/// `A`.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence MWV.
std::vector<std::string> encode_mwv_true(const EncoderContext& context);

/// Encodes MWD, the true wind direction and speed.
///
/// Fields: the direction the true wind blows from in degrees true, `T`, the same direction
/// in degrees magnetic (true minus variation, without deviation), `M`, the speed in knots,
/// `N`, and in metres per second, `M`, each with one decimal.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence MWD.
std::vector<std::string> encode_mwd(const EncoderContext& context);

// Steering

/// Encodes RSA, the rudder angle.
///
/// Fields: the rudder angle in degrees with one decimal, positive to starboard, in the
/// starboard (or single) rudder field with status `A`, then an empty port rudder field with
/// status `V` (not fitted).
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see NMEA 0183, sentence RSA.
std::vector<std::string> encode_rsa(const EncoderContext& context);

// Autopilot: one sentence each while a destination is set, nothing otherwise. The leg runs
// from Destination::origin to Destination::position and is solved by geo::solve_leg().

/// Encodes APB, autopilot sentence B.
///
/// Fields: statuses `A` and `A`, cross-track error magnitude in nautical miles with two
/// decimals, direction to steer (`L` when the vessel is to the right of the leg, `R`
/// otherwise), `N`, arrival circle entered (`A` within `arrival_radius_m` of the
/// destination, `V` otherwise), perpendicular passed (`A` once the vessel is level with or
/// beyond the destination along the leg), bearing from origin to destination, `T`, the
/// waypoint name (sanitize_waypoint_name()), bearing from the vessel to the destination, `T`,
/// the same bearing as heading to steer, `T`, and the mode indicator. Bearings are in degrees
/// true with one decimal.
///
/// @param context The state and talker to encode with.
/// @return One sentence, or none when `context.state.destination` is empty.
/// @see NMEA 0183, sentence APB.
std::vector<std::string> encode_apb(const EncoderContext& context);

/// Encodes RMB, the recommended minimum navigation information to the destination.
///
/// Fields: status `A`, cross-track error magnitude in nautical miles with two decimals,
/// direction to steer as in encode_apb(), an empty origin waypoint name, the destination
/// waypoint name (sanitize_waypoint_name()), the destination latitude and longitude with
/// `options.position_decimals`, the range to the destination in nautical miles capped at
/// 999.9, the true bearing to the destination, the closing velocity in knots (the component
/// of the speed over ground towards the destination, negative when moving away), the arrival
/// status (`A` within `arrival_radius_m`, `V` otherwise) and the mode indicator.
///
/// @param context The state, talker and position precision to encode with.
/// @return One sentence, or none when `context.state.destination` is empty.
/// @see NMEA 0183, sentence RMB.
std::vector<std::string> encode_rmb(const EncoderContext& context);

/// Encodes XTE, the cross-track error.
///
/// Fields: statuses `A` and `A`, cross-track error magnitude in nautical miles with two
/// decimals, direction to steer as in encode_apb(), `N`, and the mode indicator.
///
/// @param context The state and talker to encode with.
/// @return One sentence, or none when `context.state.destination` is empty.
/// @see NMEA 0183, sentence XTE.
std::vector<std::string> encode_xte(const EncoderContext& context);

// Propulsion: one sentence per engine, nothing without engines

/// Encodes RPM, the revolutions of each engine.
///
/// Fields per engine: source `E` (engine), the engine number counted from 1 in
/// `VesselState::engines` order, revolutions per minute with one decimal (`0.0` while the
/// engine is not running), an empty propeller pitch and status `A`.
///
/// @param context The state and talker to encode with.
/// @return One sentence per engine in profile order; none when the vessel has no engines.
/// @see NMEA 0183, sentence RPM.
std::vector<std::string> encode_rpm(const EncoderContext& context);

/// Encodes XDR, the transducer measurements of each engine.
///
/// Each sentence carries two transducers named `ENGINE#n`, with `n` counted from 0 in
/// `VesselState::engines` order as Signal K and common gateways expect: a temperature
/// transducer (`C`, coolant temperature, `C` for degrees Celsius) and a tachometer (`T`,
/// revolutions per minute, `R`), both with one decimal. A stopped engine reports `0.0`
/// revolutions.
///
/// @param context The state and talker to encode with.
/// @return One sentence per engine in profile order; none when the vessel has no engines.
/// @see NMEA 0183, sentence XDR.
std::vector<std::string> encode_xdr(const EncoderContext& context);

// AIS own vessel: the position report and the static data report, each as VDO (own vessel)
// or VDM (as other receivers would relay it). The sentences start with `!` and carry the
// six-bit armoured payload; see docs/reference/ais.md for the fields.

/// Encodes the own vessel's AIS position report as VDO.
///
/// The message type is `AisStatic::position_report_type` when it is 1, 2 or 3, and 1
/// otherwise. The payload of 168 bits fits one sentence, sent on channel `A` with an empty
/// sequential message id.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see ITU-R M.1371-5, Annex 8, messages 1, 2 and 3.
/// @see NMEA 0183, sentence VDO.
std::vector<std::string> encode_vdo_position(const EncoderContext& context);

/// Encodes the own vessel's AIS static and voyage data report (message type 5) as VDO.
///
/// The payload of 424 bits needs two sentences, sent on channel `A`, which share a
/// sequential message id derived from the simulated clock: the UTC seconds modulo 10.
///
/// @param context The state and talker to encode with.
/// @return Two sentences, in order.
/// @see ITU-R M.1371-5, Annex 8, message 5.
/// @see NMEA 0183, sentence VDO.
std::vector<std::string> encode_vdo_static(const EncoderContext& context);

/// Encodes the own vessel's AIS position report as VDM, as another station would receive it.
///
/// The payload and framing are those of encode_vdo_position(); only the formatter differs,
/// for consumers that ignore VDO.
///
/// @param context The state and talker to encode with.
/// @return One sentence.
/// @see ITU-R M.1371-5, Annex 8, messages 1, 2 and 3.
/// @see NMEA 0183, sentence VDM.
std::vector<std::string> encode_vdm_position(const EncoderContext& context);

/// Encodes the own vessel's AIS static and voyage data report as VDM, as another station
/// would receive it.
///
/// The payload and framing are those of encode_vdo_static(); only the formatter differs.
///
/// @param context The state and talker to encode with.
/// @return Two sentences, in order.
/// @see ITU-R M.1371-5, Annex 8, message 5.
/// @see NMEA 0183, sentence VDM.
std::vector<std::string> encode_vdm_static(const EncoderContext& context);

/// Restricts a waypoint name to the characters an NMEA 0183 field may carry.
///
/// Keeps the characters is_text_field_character() allows except space: the printable ASCII
/// characters other than those NMEA 0183 reserves (`,`, `*`, `$`, `!`, `\`, `^` and `~`);
/// spaces, control characters and bytes outside ASCII are dropped. The result is truncated to
/// `model::kMaxWaypointNameLength` characters.
///
/// @param name The configured waypoint name, of any length.
/// @return The sanitised name, or `WPT` when no character is left.
[[nodiscard]] std::string sanitize_waypoint_name(std::string_view name);

/// Returns the mode indicator letter sent by RMC, GLL, VTG, APB, RMB and XTE.
///
/// @param fix The GNSS receiver state.
/// @return `N` (no fix) when `fix.has_fix` is false, otherwise `D` (differential) when the
///     quality is model::FixQuality::Differential and `A` (autonomous) for any other quality.
[[nodiscard]] char mode_indicator(const model::GnssFix& fix) noexcept;

/// Returns the status letter sent by RMC and GLL.
///
/// @param fix The GNSS receiver state.
/// @return `A` (data valid) when `fix.has_fix` is true, `V` (receiver warning) otherwise.
[[nodiscard]] char status_indicator(const model::GnssFix& fix) noexcept;

}  // namespace nmeasim::core::nmea0183
