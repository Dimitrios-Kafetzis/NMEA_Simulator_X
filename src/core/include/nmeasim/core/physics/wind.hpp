#pragma once

/// Wind vector calculations.
namespace nmeasim::core::physics {

/// Apparent wind as experienced on board a moving vessel.
struct ApparentWind {
    /// Angle the apparent wind comes from, relative to the bow, clockwise, in [0, 360).
    double angle_relative_deg{0.0};
    /// Direction the apparent wind comes from, referenced to true north, in [0, 360).
    double direction_true_deg{0.0};
    double speed_kn{0.0};
};

/// Combines the true wind with the vessel's motion through the water.
///
/// `true_direction_deg` is the direction the wind blows from, referenced to true north.
/// A vessel making way into a head wind feels a stronger wind from ahead; running before
/// the wind it feels a weaker one from astern.
[[nodiscard]] ApparentWind apparent_wind(double true_direction_deg, double true_speed_kn,
                                         double heading_true_deg, double speed_kn) noexcept;

}  // namespace nmeasim::core::physics
