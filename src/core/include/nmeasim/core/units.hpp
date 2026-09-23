#pragma once

/// Unit conversion constants and helpers used throughout the simulator.
///
/// Internal state is kept in the units marine users think in: degrees, knots, metres and
/// degrees Celsius. Encoders convert to whatever a protocol demands.
namespace nmeasim::core::units {

/// Length of one international nautical mile.
inline constexpr double kMetresPerNauticalMile{1852.0};
/// Seconds in one hour.
inline constexpr double kSecondsPerHour{3600.0};
/// International feet in one metre.
inline constexpr double kFeetPerMetre{3.280839895013123};
/// Fathoms (6 ft) in one metre.
inline constexpr double kFathomsPerMetre{0.5468066491688539};
/// Kilometres per hour in one knot.
inline constexpr double kKilometresPerHourPerKnot{1.852};
/// Metres per second in one knot.
inline constexpr double kMetresPerSecondPerKnot{kMetresPerNauticalMile / kSecondsPerHour};
/// Kelvin value of 0 degrees Celsius.
inline constexpr double kKelvinOffset{273.15};

/// Converts a speed in knots to kilometres per hour.
[[nodiscard]] constexpr double knots_to_kmh(double knots) noexcept {
    return knots * kKilometresPerHourPerKnot;
}

/// Converts a speed in knots to metres per second.
[[nodiscard]] constexpr double knots_to_mps(double knots) noexcept {
    return knots * kMetresPerSecondPerKnot;
}

/// Converts a speed in metres per second to knots.
[[nodiscard]] constexpr double mps_to_knots(double mps) noexcept {
    return mps / kMetresPerSecondPerKnot;
}

/// Converts a length in metres to feet.
[[nodiscard]] constexpr double metres_to_feet(double metres) noexcept {
    return metres * kFeetPerMetre;
}

/// Converts a length in metres to fathoms.
[[nodiscard]] constexpr double metres_to_fathoms(double metres) noexcept {
    return metres * kFathomsPerMetre;
}

/// Converts a temperature in degrees Celsius to kelvin.
[[nodiscard]] constexpr double celsius_to_kelvin(double celsius) noexcept {
    return celsius + kKelvinOffset;
}

}  // namespace nmeasim::core::units
