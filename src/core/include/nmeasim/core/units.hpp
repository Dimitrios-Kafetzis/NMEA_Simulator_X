// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Unit conversion constants and functions used throughout the simulator.
///
/// The factors follow from exact definitions (the international nautical mile of 1852 m,
/// the international foot of 0.3048 m, the fathom of 6 ft); those that are reciprocals of
/// such definitions are rounded to double precision. The functions are `constexpr`.

#pragma once

/// Unit conversions, part of the `nmeasim::core` library.
///
/// Internal state is kept in the units marine users think in: degrees, knots, metres and
/// degrees Celsius. Encoders convert to whatever a protocol demands with the constants and
/// functions here: kilometres per hour and feet for NMEA 0183, metres per second and kelvin
/// for Signal K.
namespace nmeasim::core::units {

/// Metres in one international nautical mile, exact by definition.
inline constexpr double kMetresPerNauticalMile{1852.0};
/// Seconds in one hour.
inline constexpr double kSecondsPerHour{3600.0};
/// International feet in one metre: 1 / 0.3048, rounded to double precision.
inline constexpr double kFeetPerMetre{3.280839895013123};
/// Fathoms of six international feet in one metre: 1 / 1.8288, rounded to double precision.
inline constexpr double kFathomsPerMetre{0.5468066491688539};
/// Kilometres per hour in one knot, exact by definition of the nautical mile.
inline constexpr double kKilometresPerHourPerKnot{1.852};
/// Metres per second in one knot: 1852 / 3600.
inline constexpr double kMetresPerSecondPerKnot{kMetresPerNauticalMile / kSecondsPerHour};
/// Kelvin value of 0 degrees Celsius, exact by definition.
inline constexpr double kKelvinOffset{273.15};

/// Converts a speed from knots to kilometres per hour.
///
/// @param knots Speed in knots.
/// @return The speed in kilometres per hour.
[[nodiscard]] constexpr double knots_to_kmh(double knots) noexcept {
    return knots * kKilometresPerHourPerKnot;
}

/// Converts a speed from knots to metres per second.
///
/// @param knots Speed in knots.
/// @return The speed in metres per second.
[[nodiscard]] constexpr double knots_to_mps(double knots) noexcept {
    return knots * kMetresPerSecondPerKnot;
}

/// Converts a speed from metres per second to knots.
///
/// @param mps Speed in metres per second.
/// @return The speed in knots.
[[nodiscard]] constexpr double mps_to_knots(double mps) noexcept {
    return mps / kMetresPerSecondPerKnot;
}

/// Converts a length from metres to international feet.
///
/// @param metres Length in metres.
/// @return The length in feet.
[[nodiscard]] constexpr double metres_to_feet(double metres) noexcept {
    return metres * kFeetPerMetre;
}

/// Converts a length from metres to fathoms.
///
/// @param metres Length in metres.
/// @return The length in fathoms.
[[nodiscard]] constexpr double metres_to_fathoms(double metres) noexcept {
    return metres * kFathomsPerMetre;
}

/// Converts a temperature from degrees Celsius to kelvin.
///
/// @param celsius Temperature in degrees Celsius.
/// @return The temperature in kelvin.
[[nodiscard]] constexpr double celsius_to_kelvin(double celsius) noexcept {
    return celsius + kKelvinOffset;
}

}  // namespace nmeasim::core::units
