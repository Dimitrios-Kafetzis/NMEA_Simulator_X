// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Apparent wind on board a moving vessel.
///
/// `apparent_wind` combines the true wind with the vessel's velocity through the water; the
/// simulation sources call it on every tick to fill the apparent values of `model::Wind`.

#pragma once

/// Physical models of the simulated environment, part of the `nmeasim::core` library.
///
/// It currently holds the wind triangle that turns the true wind into the apparent wind
/// felt on board.
namespace nmeasim::core::physics {

/// Apparent wind as experienced on board a moving vessel.
///
/// @see apparent_wind
struct ApparentWind {
    /// Angle the apparent wind comes from, relative to the bow, clockwise, in [0, 360).
    double angle_relative_deg{0.0};
    /// Direction the apparent wind comes from, in degrees true, in [0, 360).
    double direction_true_deg{0.0};
    /// Apparent wind speed, not negative.
    double speed_kn{0.0};
};

/// Combines the true wind with the vessel's motion through the water.
///
/// The apparent wind velocity is the true wind velocity minus the vessel's velocity. A
/// vessel making way into a head wind feels a stronger wind from ahead; running before the
/// wind it feels a weaker one from astern. When the apparent wind speed is below 1e-9 kn its
/// direction is undefined, so the true wind direction is reported instead.
///
/// @param true_direction_deg Direction the true wind blows from, in degrees true; any
///        value, it is normalised.
/// @param true_speed_kn True wind speed, not negative.
/// @param heading_true_deg Heading of the bow in degrees true; the vessel is assumed to move
///        along it, without leeway.
/// @param speed_kn Speed of the vessel through the water.
/// @return The apparent wind speed, its direction and its angle relative to the bow.
[[nodiscard]] ApparentWind apparent_wind(double true_direction_deg, double true_speed_kn,
                                         double heading_true_deg, double speed_kn) noexcept;

}  // namespace nmeasim::core::physics
