#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// The simulated vessel and its environment at one instant.
///
/// Every quantity is stored in the unit named by its suffix. Angles are degrees, speeds are
/// knots, lengths are metres and temperatures are degrees Celsius. Encoders never mutate a
/// state; sources produce a new one every simulation tick.
namespace nmeasim::core::model {

/// Quality of the GNSS solution, using the GGA fix quality codes.
enum class FixQuality {
    Invalid = 0,
    Gps = 1,
    Differential = 2,
};

/// State of the GNSS receiver: fix, satellites and dilution of precision.
struct GnssFix {
    /// False simulates a receiver that has lost its fix: status fields become 'V', positions
    /// are omitted and the fix quality is reported as invalid.
    bool has_fix{true};
    /// Fix quality sent in GGA; `Differential` also makes the mode indicator 'D'.
    FixQuality quality{FixQuality::Gps};
    /// Satellites used in the solution, reported in GGA and GSA; clamped to 0-12.
    int satellites_in_use{8};
    /// Satellites listed in GSV, clamped to 0-12 and never fewer than those in use.
    int satellites_in_view{10};
    /// Horizontal dilution of precision reported in GGA and GSA.
    double hdop{0.9};
    /// Position dilution of precision reported in GSA.
    double pdop{1.7};
    /// Vertical dilution of precision reported in GSA.
    double vdop{1.4};
    /// Height of the geoid above the WGS84 ellipsoid at the current position.
    double geoid_separation_m{0.0};
};

/// Position, motion and heading of the vessel.
struct Navigation {
    /// Position of the GNSS antenna.
    geo::Position position;
    /// Antenna altitude above mean sea level, sent in GGA.
    double altitude_m{0.0};
    /// Course over ground, referenced to true north.
    double course_over_ground_deg{0.0};
    /// Speed over ground, sent in RMC, VTG and the AIS position report.
    double speed_over_ground_kn{0.0};
    /// Heading of the bow, referenced to true north.
    double heading_true_deg{0.0};
    /// Magnetic variation, positive east. True = magnetic + variation + deviation.
    double magnetic_variation_deg{0.0};
    /// Compass deviation, positive east.
    double magnetic_deviation_deg{0.0};
    /// Speed through the water along the heading, sent in VHW and VBW.
    double speed_through_water_kn{0.0};
    /// Rate of turn, positive to starboard.
    double rate_of_turn_deg_per_min{0.0};

    /// Magnetic heading as a compass sensor would report it.
    [[nodiscard]] double heading_magnetic_deg() const noexcept;
    /// Course over ground referenced to magnetic north.
    [[nodiscard]] double course_over_ground_magnetic_deg() const noexcept;
};

/// Steering gear state.
struct Steering {
    /// Rudder angle, positive to starboard.
    double rudder_angle_deg{0.0};
};

/// Depth sounder and water temperature readings.
struct Water {
    /// Depth of water below the transducer, sent in DPT and DBT.
    double depth_below_transducer_m{10.0};
    /// Positive: distance from transducer to water line. Negative: to the keel.
    double transducer_offset_m{0.0};
    /// Sea water temperature, sent in MTW.
    double temperature_c{18.0};
};

/// True and apparent wind measured on board.
struct Wind {
    /// Direction the true wind blows from, referenced to true north.
    double true_direction_deg{0.0};
    /// True wind speed, sent in MWD and the true MWV.
    double true_speed_kn{0.0};
    /// Apparent wind angle relative to the bow, clockwise, in [0, 360).
    double apparent_angle_deg{0.0};
    /// Apparent wind speed, sent in the relative MWV.
    double apparent_speed_kn{0.0};

    /// Angle of the true wind relative to the bow, clockwise, in [0, 360).
    [[nodiscard]] double true_angle_relative_deg(double heading_true_deg) const noexcept;
};

/// One propulsion engine. Engines are numbered in `VesselState::engines` order: from 1 in RPM,
/// from 0 in the XDR transducer names `ENGINE#n`.
struct Engine {
    /// Display name; the Signal K `propulsion.<id>` path is derived from it.
    std::string label{"Engine"};
    /// False reports zero revolutions regardless of `revolutions_rpm`.
    bool running{false};
    /// Shaft revolutions per minute while running, sent in RPM and XDR.
    double revolutions_rpm{0.0};
    /// Coolant temperature, sent in XDR.
    double coolant_temperature_c{20.0};
};

/// The waypoint the vessel steers for. The autopilot sentences (APB, RMB, XTE) describe the
/// leg from `origin` to `position`; the cross-track error is the distance of the vessel from
/// that leg.
struct Destination {
    /// Waypoint identifier sent in the sentences; printable ASCII without NMEA reserved
    /// characters, at most `kMaxWaypointNameLength` characters after sanitising.
    std::string name{"WPT"};
    /// Position of the waypoint, sent in RMB.
    geo::Position position;
    /// Start of the leg, normally the vessel's position when the destination was set.
    geo::Position origin;
    /// Radius of the arrival circle around the destination.
    double arrival_radius_m{100.0};
};

/// Longest waypoint identifier the encoders send.
inline constexpr std::size_t kMaxWaypointNameLength{16};

/// Static data of the own vessel for the AIS messages, plus the message options.
struct AisStatic {
    /// Maritime Mobile Service Identity, nine digits.
    std::uint32_t mmsi{239000001};
    /// IMO number, 0 when the vessel has none.
    std::uint32_t imo_number{0};
    /// Vessel name, at most 20 characters of the AIS six-bit alphabet.
    std::string name{"NMEA SIMULATOR X"};
    /// Call sign, at most 7 characters.
    std::string call_sign{"SIMX"};
    /// Type of ship and cargo code (ITU-R M.1371 table), 37 is a pleasure craft.
    int ship_type{37};
    /// Distance from the reference position (the GNSS antenna) to the bow, at most 511 m in
    /// the message.
    double dimension_to_bow_m{12.0};
    /// Distance from the reference position to the stern, at most 511 m in the message.
    double dimension_to_stern_m{4.0};
    /// Distance from the reference position to port, at most 63 m in the message.
    double dimension_to_port_m{3.0};
    /// Distance from the reference position to starboard, at most 63 m in the message.
    double dimension_to_starboard_m{3.0};
    /// Maximum present static draught, sent in tenths of a metre up to 25.5 m.
    double draught_m{1.8};
    /// Voyage destination, at most 20 characters; empty for none.
    std::string destination;
    /// Navigational status code, 0 is "under way using engine", 8 "under way sailing".
    int navigation_status{0};
    /// Message type of the position report: 1 scheduled, 2 assigned, 3 in response to an
    /// interrogation.
    int position_report_type{1};
};

/// Complete snapshot of the vessel that every encoder reads.
struct VesselState {
    /// Simulation time in UTC, sent in the time and date fields.
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
    /// Engines in profile order; empty when the vessel reports no propulsion.
    std::vector<Engine> engines;
    /// The active waypoint, when one is set. Without it no autopilot sentence is sent.
    std::optional<Destination> destination;
    /// Own-vessel AIS static data and message options.
    AisStatic ais;
};

}  // namespace nmeasim::core::model
