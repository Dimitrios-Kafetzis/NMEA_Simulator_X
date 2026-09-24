// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The vessel state: the snapshot of the simulated vessel that every encoder reads.
///
/// `VesselState` groups navigation, GNSS, steering, water, wind, engines, the active
/// destination and the AIS static data. docs/reference/profile.md describes how a profile
/// seeds each value.

#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// The simulated vessel and its environment at one instant, part of the `nmeasim::core`
/// library.
///
/// Every quantity is stored in the unit named by its suffix: angles in degrees, speeds in
/// knots, lengths in metres, temperatures in degrees Celsius. Angles are true unless the
/// name says magnetic. A simulation source advances the state on every tick; encoders only
/// read it. The types are plain aggregates that validate nothing: the sources keep the
/// values in the documented ranges and the encoders clamp where a protocol field demands it.
namespace nmeasim::core::model {

/// Quality of the GNSS solution; the enumerator values are the GGA fix quality codes.
///
/// @see NMEA 0183, sentence GGA, field "GPS quality indicator".
enum class FixQuality {
    Invalid = 0,       ///< No valid fix, sent as GGA quality 0.
    Gps = 1,           ///< Autonomous GNSS fix, sent as GGA quality 1.
    Differential = 2,  ///< Differential fix, sent as GGA quality 2.
};

/// State of the GNSS receiver: fix, satellites and dilution of precision.
///
/// The satellites themselves are synthetic: the encoders report the first satellites of a
/// fixed constellation of twelve.
struct GnssFix {
    /// Whether the receiver has a position fix.
    ///
    /// `false` simulates a receiver that has lost its fix: the status fields of RMC and GLL
    /// become `V`, the mode indicator `N`, positions, speed and course are sent empty, GGA
    /// reports quality 0, AIS sends its "not available" values and the Signal K position,
    /// course and speed paths are omitted.
    bool has_fix{true};
    /// Fix quality sent in GGA while `has_fix` is true; `Differential` also makes the mode
    /// indicator `D` and sets the AIS position accuracy flag.
    FixQuality quality{FixQuality::Gps};
    /// Satellites used in the solution, reported in GGA, GSA and Signal K; the NMEA encoders
    /// clamp it to [0, 12].
    int satellites_in_use{8};
    /// Satellites listed in GSV, clamped to [0, 12] and never fewer than those in use.
    int satellites_in_view{10};
    /// Horizontal dilution of precision, dimensionless, reported in GGA, GSA and Signal K.
    double hdop{0.9};
    /// Position dilution of precision, dimensionless, reported in GSA and Signal K.
    double pdop{1.7};
    /// Vertical dilution of precision, dimensionless, reported in GSA.
    double vdop{1.4};
    /// Height of the geoid above the WGS 84 ellipsoid at the current position, negative
    /// when the geoid lies below it; reported in GGA and Signal K.
    double geoid_separation_m{0.0};
};

/// Position, motion and heading of the vessel.
///
/// Magnetic quantities are derived, not stored: true = magnetic + variation for the heading
/// and the course over ground, and magnetic = compass + deviation for the heading read from
/// the ship's compass.
struct Navigation {
    /// Position of the GNSS antenna, the reference point of the AIS dimensions.
    geo::Position position;
    /// Antenna altitude above mean sea level, sent in GGA and Signal K; negative below it.
    double altitude_m{0.0};
    /// Course over ground in degrees true, in [0, 360); sent in RMC, VTG, VBW and AIS.
    double course_over_ground_deg{0.0};
    /// Speed over ground, not negative; sent in RMC, VTG, VBW and the AIS position report.
    double speed_over_ground_kn{0.0};
    /// Heading of the bow in degrees true, in [0, 360); sent in HDT, VHW and AIS.
    double heading_true_deg{0.0};
    /// Magnetic variation of the chart position, positive east, sent in RMC and HDG.
    double magnetic_variation_deg{0.0};
    /// Deviation of the ship's compass, positive east, sent in HDG; it applies to the
    /// compass heading only.
    double magnetic_deviation_deg{0.0};
    /// Speed through the water along the heading, not negative; sent in VHW and VBW.
    double speed_through_water_kn{0.0};
    /// Rate of turn, positive to starboard (heading increasing), sent in ROT and AIS.
    double rate_of_turn_deg_per_min{0.0};

    /// Returns the heading referenced to magnetic north, as HDM, VHW and Signal K send it.
    ///
    /// Deviation does not apply: it is an error of the ship's compass, which
    /// heading_compass_deg() includes.
    ///
    /// @return `heading_true_deg - magnetic_variation_deg`, normalised to [0, 360).
    [[nodiscard]] double heading_magnetic_deg() const noexcept;

    /// Returns the heading as the ship's compass reports it, as HDG sends it.
    ///
    /// @return `heading_true_deg - magnetic_variation_deg - magnetic_deviation_deg`,
    ///         normalised to [0, 360).
    [[nodiscard]] double heading_compass_deg() const noexcept;

    /// Returns the course over ground referenced to magnetic north.
    ///
    /// Deviation does not apply: the course comes from the GNSS receiver, not the compass.
    ///
    /// @return `course_over_ground_deg - magnetic_variation_deg`, normalised to [0, 360).
    [[nodiscard]] double course_over_ground_magnetic_deg() const noexcept;
};

/// Steering gear state.
struct Steering {
    /// Rudder angle, positive to starboard, sent in RSA; the delta source clamps it to its
    /// configured maximum (35 degrees by default) either side.
    double rudder_angle_deg{0.0};
};

/// Depth sounder and water temperature readings.
struct Water {
    /// Depth of water below the transducer, not negative; sent in DPT and DBT.
    double depth_below_transducer_m{10.0};
    /// Offset of the transducer, sent in DPT: positive is the distance from the transducer up
    /// to the water line, negative the distance down to the keel.
    double transducer_offset_m{0.0};
    /// Sea water temperature, sent in MTW.
    double temperature_c{18.0};
};

/// True and apparent wind measured on board.
///
/// The apparent values are derived from the true wind and the vessel's motion by
/// `physics::apparent_wind` on every tick.
struct Wind {
    /// Direction the true wind blows from, in degrees true, in [0, 360).
    double true_direction_deg{0.0};
    /// True wind speed, not negative; sent in MWD and the true MWV.
    double true_speed_kn{0.0};
    /// Angle the apparent wind comes from relative to the bow, clockwise, in [0, 360).
    double apparent_angle_deg{0.0};
    /// Apparent wind speed, sent in the relative MWV.
    double apparent_speed_kn{0.0};

    /// Returns the angle the true wind comes from relative to the bow.
    ///
    /// @param heading_true_deg Heading of the bow in degrees true.
    /// @return The angle clockwise from the bow, in [0, 360).
    [[nodiscard]] double true_angle_relative_deg(double heading_true_deg) const noexcept;
};

/// One propulsion engine.
///
/// Engines are numbered in `VesselState::engines` order: from 1 in RPM, from 0 in the XDR
/// transducer names `ENGINE#n`.
struct Engine {
    /// Display name, from which `signalk::engine_id` derives the identifier of the engine's
    /// Signal K `propulsion` paths.
    std::string label{"Engine"};
    /// Whether the engine runs; `false` reports zero revolutions regardless of
    /// `revolutions_rpm` and the Signal K state `stopped`.
    bool running{false};
    /// Shaft revolutions per minute while running, sent in RPM and XDR.
    double revolutions_rpm{0.0};
    /// Coolant temperature, sent in XDR and Signal K.
    double coolant_temperature_c{20.0};
};

/// The waypoint the vessel steers for.
///
/// The autopilot sentences (APB, RMB, XTE) describe the leg from `origin` to `position`;
/// the cross-track error is the distance of the vessel from that leg. The vessel does not
/// steer towards it by itself.
///
/// @see geo::solve_leg
struct Destination {
    /// Waypoint identifier sent in the sentences. The encoders drop spaces, non-printable
    /// characters and NMEA reserved characters, keep at most `kMaxWaypointNameLength`
    /// characters and send `WPT` when nothing is left.
    std::string name{"WPT"};
    /// Position of the waypoint, sent in RMB.
    geo::Position position;
    /// Start of the leg, normally the vessel's position when the destination was set.
    geo::Position origin;
    /// Radius of the arrival circle around the destination; the arrival flags of APB and RMB
    /// are set once the vessel is within it.
    double arrival_radius_m{100.0};
};

/// Longest waypoint identifier, in characters, that the encoders send; longer names are
/// truncated.
inline constexpr std::size_t kMaxWaypointNameLength{16};

/// Static data of the own vessel for the AIS messages, plus the message options.
///
/// The AIS encoder clamps the numbers to the range of their fields and truncates the texts,
/// as the member comments say.
///
/// @see ITU-R M.1371-5, Annex 8, messages 1 and 5.
struct AisStatic {
    /// Maritime Mobile Service Identity, nine digits; also forms the default Signal K
    /// context. The default carries the Greek maritime identification digits 239.
    std::uint32_t mmsi{239000001};
    /// IMO ship identification number, 0 when the vessel has none.
    std::uint32_t imo_number{0};
    /// Vessel name, at most 20 characters of the AIS six-bit alphabet; longer names are
    /// truncated.
    std::string name{"NMEA SIMULATOR X"};
    /// Call sign, at most 7 characters; longer ones are truncated.
    std::string call_sign{"SIMX"};
    /// Type of ship and cargo code, in [0, 255]; 37 is a pleasure craft, 36 a sailing
    /// vessel.
    int ship_type{37};
    /// Distance from the reference position (the GNSS antenna) to the bow, sent in whole
    /// metres up to 511.
    double dimension_to_bow_m{12.0};
    /// Distance from the reference position to the stern, sent in whole metres up to 511.
    double dimension_to_stern_m{4.0};
    /// Distance from the reference position to port, sent in whole metres up to 63.
    double dimension_to_port_m{3.0};
    /// Distance from the reference position to starboard, sent in whole metres up to 63.
    double dimension_to_starboard_m{3.0};
    /// Maximum present static draught, sent in tenths of a metre up to 25.5 m.
    double draught_m{1.8};
    /// Voyage destination, at most 20 characters; empty for none.
    std::string destination;
    /// Navigational status code in [0, 15]: 0 is "under way using engine", 8 "under way
    /// sailing".
    int navigation_status{0};
    /// Message type of the position report: 1 scheduled, 2 assigned, 3 in response to an
    /// interrogation; any other value is sent as 1.
    int position_report_type{1};
};

/// Complete snapshot of the vessel that every encoder reads.
///
/// A default-constructed state describes a vessel at 0 degrees north, 0 degrees east at the
/// Unix epoch, with a GPS fix, no engines and no destination; a profile seed normally
/// replaces it.
struct VesselState {
    /// Simulation time in UTC, sent in the time and date fields; the Unix epoch by default.
    std::chrono::system_clock::time_point time_utc{};
    /// Position, motion and heading.
    Navigation navigation;
    /// GNSS receiver state.
    GnssFix gnss;
    /// Rudder state.
    Steering steering;
    /// Depth and water temperature.
    Water water;
    /// True and apparent wind.
    Wind wind;
    /// Engines in profile order; empty when the vessel reports no propulsion, in which case
    /// RPM and XDR send nothing.
    std::vector<Engine> engines;
    /// The active waypoint, or `std::nullopt` when none is set; without one no autopilot
    /// sentence and no Signal K course path is sent.
    std::optional<Destination> destination;
    /// Own-vessel AIS static data and message options.
    AisStatic ais;
};

}  // namespace nmeasim::core::model
