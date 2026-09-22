#pragma once

/// Unit conversion constants and helpers used throughout the simulator.
///
/// Internal state is kept in the units marine users think in: degrees, knots, metres and
/// degrees Celsius. Encoders convert to whatever a protocol demands.
namespace nmeasim::core::units {

inline constexpr double kMetresPerNauticalMile{1852.0};
inline constexpr double kSecondsPerHour{3600.0};
inline constexpr double kFeetPerMetre{3.280839895013123};
inline constexpr double kFathomsPerMetre{0.5468066491688539};
inline constexpr double kKilometresPerHourPerKnot{1.852};
inline constexpr double kMetresPerSecondPerKnot{kMetresPerNauticalMile / kSecondsPerHour};
inline constexpr double kKelvinOffset{273.15};

[[nodiscard]] constexpr double knots_to_kmh(double knots) noexcept {
    return knots * kKilometresPerHourPerKnot;
}

[[nodiscard]] constexpr double knots_to_mps(double knots) noexcept {
    return knots * kMetresPerSecondPerKnot;
}

[[nodiscard]] constexpr double mps_to_knots(double mps) noexcept {
    return mps / kMetresPerSecondPerKnot;
}

[[nodiscard]] constexpr double metres_to_feet(double metres) noexcept {
    return metres * kFeetPerMetre;
}

[[nodiscard]] constexpr double metres_to_fathoms(double metres) noexcept {
    return metres * kFathomsPerMetre;
}

[[nodiscard]] constexpr double celsius_to_kelvin(double celsius) noexcept {
    return celsius + kKelvinOffset;
}

}  // namespace nmeasim::core::units
