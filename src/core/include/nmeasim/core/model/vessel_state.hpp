#pragma once

#include <nmeasim/core/geo/geodesic.hpp>

#include <chrono>
#include <cstddef>
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

struct GnssFix {
    /// False simulates a receiver that has lost its fix: status fields become 'V', positions
    /// are omitted and the fix quality is reported as invalid.
    bool has_fix{true};
    FixQuality quality{FixQuality::Gps};
    int satellites_in_use{8};
    int satellites_in_view{10};
    double hdop{0.9};
    double pdop{1.7};
    double vdop{1.4};
    /// Height of the geoid above the WGS84 ellipsoid at the current position.
    double geoid_separation_m{0.0};
};

struct Navigation {
    geo::Position position;
    double altitude_m{0.0};
    double course_over_ground_deg{0.0};
    double speed_over_ground_kn{0.0};
    double heading_true_deg{0.0};
    /// Magnetic variation, positive east. True = magnetic + variation + deviation.
    double magnetic_variation_deg{0.0};
    /// Compass deviation, positive east.
    double magnetic_deviation_deg{0.0};
    double speed_through_water_kn{0.0};
    /// Rate of turn, positive to starboard.
    double rate_of_turn_deg_per_min{0.0};

    /// Magnetic heading as a compass sensor would report it.
    [[nodiscard]] double heading_magnetic_deg() const noexcept;
    /// Course over ground referenced to magnetic north.
    [[nodiscard]] double course_over_ground_magnetic_deg() const noexcept;
};

struct Steering {
    /// Rudder angle, positive to starboard.
    double rudder_angle_deg{0.0};
};

struct Water {
    double depth_below_transducer_m{10.0};
    /// Positive: distance from transducer to water line. Negative: to the keel.
    double transducer_offset_m{0.0};
    double temperature_c{18.0};
};

struct Wind {
    /// Direction the true wind blows from, referenced to true north.
    double true_direction_deg{0.0};
    double true_speed_kn{0.0};
    /// Apparent wind angle relative to the bow, clockwise, in [0, 360).
    double apparent_angle_deg{0.0};
    double apparent_speed_kn{0.0};

    /// Angle of the true wind relative to the bow, clockwise, in [0, 360).
    [[nodiscard]] double true_angle_relative_deg(double heading_true_deg) const noexcept;
};

struct Engine {
    std::string label{"Engine"};
    bool running{false};
    double revolutions_rpm{0.0};
    double coolant_temperature_c{20.0};
};

/// The waypoint the vessel steers for. The autopilot sentences (APB, RMB, XTE) describe the
/// leg from `origin` to `position`; the cross-track error is the distance of the vessel from
/// that leg.
struct Destination {
    /// Waypoint identifier sent in the sentences; printable ASCII without NMEA reserved
    /// characters, at most `kMaxWaypointNameLength` characters after sanitising.
    std::string name{"WPT"};
    geo::Position position;
    /// Start of the leg, normally the vessel's position when the destination was set.
    geo::Position origin;
    /// Radius of the arrival circle around the destination.
    double arrival_radius_m{100.0};
};

/// Longest waypoint identifier the encoders send.
inline constexpr std::size_t kMaxWaypointNameLength{16};

struct VesselState {
    std::chrono::system_clock::time_point time_utc{};
    Navigation navigation;
    GnssFix gnss;
    Steering steering;
    Water water;
    Wind wind;
    std::vector<Engine> engines;
    /// The active waypoint, when one is set. Without it no autopilot sentence is sent.
    std::optional<Destination> destination;
};

}  // namespace nmeasim::core::model
