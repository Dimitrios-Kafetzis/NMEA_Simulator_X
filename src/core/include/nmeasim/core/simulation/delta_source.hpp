#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/source.hpp>

#include <array>
#include <chrono>
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
    double amplitude{0.0};
    double step_per_second{0.0};
};

struct DeltaConfig {
    /// Initial values. The position, time and everything else start from here.
    model::VesselState seed;

    Variation heading{2.0, 0.5};
    Variation speed{0.3, 0.1};
    Variation depth{1.5, 0.2};
    Variation water_temperature{0.2, 0.05};
    Variation wind_direction{10.0, 2.0};
    Variation wind_speed{2.0, 0.5};

    /// Rate of turn per degree of rudder, in degrees per minute per degree.
    double turn_rate_per_rudder_deg{0.6};
    double max_rudder_angle_deg{35.0};

    /// Seed of the pseudo-random generator so that runs are reproducible.
    unsigned int random_seed{2026};
};

class DeltaSource final : public Source {
public:
    explicit DeltaSource(DeltaConfig config);

    const model::VesselState& advance(std::chrono::milliseconds dt) override;
    [[nodiscard]] const model::VesselState& current() const noexcept override { return state_; }
    void reset() override;

    /// Pins a parameter to a fixed value; the random drift stops for it.
    void set_override(Parameter parameter, double value);
    /// Releases an override; the value drifts again from where it is.
    void clear_override(Parameter parameter);
    [[nodiscard]] std::optional<double> override_value(Parameter parameter) const;
    /// Adds `delta` to a parameter's current value and pins it there (keyboard nudging).
    void nudge(Parameter parameter, double delta);

    /// In steering mode the heading follows the rudder instead of drifting.
    void set_steering_mode(bool enabled) noexcept { steering_mode_ = enabled; }
    [[nodiscard]] bool steering_mode() const noexcept { return steering_mode_; }

    /// Moves the vessel instantly.
    void set_position(geo::Position position);

    /// Simulates losing or regaining the GNSS fix.
    void set_fix(bool has_fix) noexcept { state_.gnss.has_fix = has_fix; }
    void set_satellites(int in_use, int in_view) noexcept;

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
