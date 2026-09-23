#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/source.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <random>

/// The delta simulation mode: seed values that drift within configured bounds, with manual
/// overrides and steering.
namespace nmeasim::core::simulation {

/// A value the operator can override or nudge while the simulation runs.
enum class Parameter {
    HeadingTrue,
    SpeedOverGround,
    SpeedThroughWater,
    Altitude,
    Depth,
    WaterTemperature,
    WindDirectionTrue,
    WindSpeedTrue,
    RudderAngle,
};

/// How far each value may wander from its seed and how fast, per second of simulated time.
/// A zero amplitude freezes the value at its seed.
struct Variation {
    /// Largest distance from the seed, in the value's own unit.
    double amplitude{0.0};
    /// Largest random change per second of simulated time, in the value's own unit.
    double step_per_second{0.0};
};

/// Configuration of a `DeltaSource`: the seed state and the drift of each value.
struct DeltaConfig {
    /// Initial values. The position, time and everything else start from here.
    model::VesselState seed;

    /// Drift of the true heading, degrees; bounded across north.
    Variation heading{2.0, 0.5};
    /// Drift of the speed over ground, knots; never below zero.
    Variation speed{0.3, 0.1};
    /// Drift of the depth below the transducer, metres; never below zero.
    Variation depth{1.5, 0.2};
    /// Drift of the water temperature, degrees Celsius.
    Variation water_temperature{0.2, 0.05};
    /// Drift of the true wind direction, degrees; bounded across north.
    Variation wind_direction{10.0, 2.0};
    /// Drift of the true wind speed, knots; never below zero.
    Variation wind_speed{2.0, 0.5};

    /// Rate of turn per degree of rudder, in degrees per minute per degree.
    double turn_rate_per_rudder_deg{0.6};
    /// Rudder angles, overridden or nudged, are clamped to plus or minus this, in degrees.
    double max_rudder_angle_deg{35.0};

    /// Seed of the pseudo-random generator so that runs are reproducible.
    unsigned int random_seed{2026};
};

/// Endless source whose values drift at random around their seeds.
///
/// Each tick moves the drifting values by a uniform random step within their `Variation`,
/// derives course over ground from the heading and speed through water from the speed over
/// ground, advances the position along the course, and recomputes the apparent wind.
class DeltaSource final : public Source {
public:
    /// Starts from `config.seed` with the generator seeded from `config.random_seed`.
    explicit DeltaSource(DeltaConfig config);

    const model::VesselState& advance(std::chrono::milliseconds dt) override;
    [[nodiscard]] const model::VesselState& current() const noexcept override { return state_; }
    void reset() override;

    /// Pins a parameter to a fixed value; the random drift stops for it.
    void set_override(Parameter parameter, double value);
    /// Releases an override; the value drifts again from where it is.
    void clear_override(Parameter parameter);
    /// The value a parameter is pinned to, or nullopt when it drifts freely.
    [[nodiscard]] std::optional<double> override_value(Parameter parameter) const;
    /// Adds `delta` to a parameter's current value and pins it there (keyboard nudging).
    void nudge(Parameter parameter, double delta);

    /// In steering mode the heading follows the rudder instead of drifting.
    void set_steering_mode(bool enabled) noexcept { steering_mode_ = enabled; }
    /// True while the heading follows the rudder.
    [[nodiscard]] bool steering_mode() const noexcept { return steering_mode_; }

    /// Moves the vessel instantly.
    void set_position(geo::Position position);
    /// Sets or clears the destination; it survives `reset`.
    void set_destination(std::optional<model::Destination> destination) override;
    /// Replaces one engine; an index past the end appends. Survives `reset`.
    void set_engine(std::size_t index, model::Engine engine);
    /// Removes one engine; an index past the end is ignored. Survives `reset`.
    void remove_engine(std::size_t index);

    /// Simulates losing or regaining the GNSS fix.
    void set_fix(bool has_fix) noexcept { state_.gnss.has_fix = has_fix; }
    /// Sets the satellite counts; the count in view is raised to at least the count in use.
    void set_satellites(int in_use, int in_view) noexcept;

    /// The configuration, including the destination and engines set since construction.
    [[nodiscard]] const DeltaConfig& config() const noexcept { return config_; }

private:
    [[nodiscard]] double read(Parameter parameter) const noexcept;
    void write(Parameter parameter, double value) noexcept;
    [[nodiscard]] double drift(double current, double seed, const Variation& variation,
                               double seconds);
    void update_derived_values(double seconds);

    DeltaConfig config_;
    model::VesselState state_;
    std::array<std::optional<double>, 9> overrides_{};
    bool steering_mode_{false};
    std::mt19937 generator_;
};

}  // namespace nmeasim::core::simulation
