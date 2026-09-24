// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The delta simulation mode: seed values that drift within configured bounds, with operator
/// overrides, nudges and rudder steering.
///
/// `DeltaSource` is the endless `Source` configured by a `DeltaConfig`; `Parameter` names the
/// values an operator can pin or nudge while it runs. The rules it applies on every step are
/// described in docs/explanation/simulation-model.md, section "Delta mode".

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/source.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <random>

namespace nmeasim::core::simulation {

/// A value the operator can override or nudge while the simulation runs.
///
/// Each enumerator names a field of `model::VesselState`. A value written through an override
/// or a nudge is normalised or clamped as stated for each enumerator.
enum class Parameter {
    /// `navigation.heading_true_deg`, degrees true, normalised into [0, 360).
    HeadingTrue,
    /// `navigation.speed_over_ground_kn`, knots; a negative value is raised to zero.
    SpeedOverGround,
    /// `navigation.speed_through_water_kn`, knots; a negative value is raised to zero. It
    /// follows the speed over ground unless overridden.
    SpeedThroughWater,
    /// `navigation.altitude_m`, metres above mean sea level; it never drifts.
    Altitude,
    /// `water.depth_below_transducer_m`, metres; a negative value is raised to zero.
    Depth,
    /// `water.temperature_c`, degrees Celsius.
    WaterTemperature,
    /// `wind.true_direction_deg`, the direction the true wind blows from, degrees true,
    /// normalised into [0, 360).
    WindDirectionTrue,
    /// `wind.true_speed_kn`, knots; a negative value is raised to zero.
    WindSpeedTrue,
    /// `steering.rudder_angle_deg`, degrees, positive to starboard, clamped to
    /// plus or minus `DeltaConfig::max_rudder_angle_deg`; it never drifts.
    RudderAngle,
};

/// How far a value may wander from its seed and how fast.
///
/// The value performs a bounded random walk: each step moves it by a uniformly distributed
/// amount of at most `step_per_second` times the step length in seconds, and the result is
/// clamped to [seed - `amplitude`, seed + `amplitude`]. A value outside that band, where a
/// released override can leave it, instead moves the full `step_per_second` times the step
/// length towards the band on every step until it is back inside. When either member is
/// zero or negative the value does not drift and stays where it is, which is its seed unless
/// an override moved it.
struct Variation {
    /// Largest distance from the seed, in the value's own unit.
    double amplitude{0.0};
    /// Largest random change per second of simulated time, in the value's own unit.
    double step_per_second{0.0};
};

/// Configuration of a `DeltaSource`: the seed state and the drift of each value.
///
/// A variation of `{0.0, 0.0}` freezes that value.
struct DeltaConfig {
    /// Initial state: position, time and every value the source starts from and drifts
    /// around. Destinations and engines set on the source are stored here as well, so that
    /// they survive a reset.
    model::VesselState seed;

    /// Drift of the true heading, degrees; the bound is measured on the angle from the seed
    /// heading, so it also holds across north.
    Variation heading{2.0, 0.5};
    /// Drift of the speed over ground, knots; the speed never goes below zero.
    Variation speed{0.3, 0.1};
    /// Drift of the depth below the transducer, metres; the depth never goes below zero.
    Variation depth{1.5, 0.2};
    /// Drift of the water temperature, degrees Celsius.
    Variation water_temperature{0.2, 0.05};
    /// Drift of the true wind direction, degrees; the bound is measured on the angle from
    /// the seed direction, so it also holds across north.
    Variation wind_direction{10.0, 2.0};
    /// Drift of the true wind speed, knots; the speed never goes below zero.
    Variation wind_speed{2.0, 0.5};

    /// Gain of the steering model, in degrees per minute of rate of turn per degree of
    /// rudder; with the default, ten degrees of rudder turn the vessel six degrees per
    /// minute.
    double turn_rate_per_rudder_deg{0.6};
    /// Largest rudder angle either side, in degrees; rudder overrides and nudges are clamped
    /// to plus or minus this value.
    double max_rudder_angle_deg{35.0};

    /// Seed of the pseudo-random generator, so that the same configuration always produces
    /// the same run.
    unsigned int random_seed{2026};
};

/// Endless source whose values drift at random around their seeds.
///
/// Each `advance` by `dt`, in this order: moves `time_utc` on by `dt`; moves the heading
/// (following the rudder in steering mode, held by an override, or drifting) and derives the
/// rate of turn from the change; drifts the speed over ground, depth, water temperature and
/// true wind unless overridden; sets the speed through water to the speed over ground unless
/// overridden, because the model has no current or leeway; sets course over ground to the
/// heading; moves the position along that course by speed times `dt` on the WGS 84 ellipsoid;
/// and recomputes the apparent wind from the true wind and the motion through the water.
///
/// Altitude, rudder angle, GNSS quality and every other field keep their seed or the last
/// value set by the host. Overrides and nudges take effect at once, without an `advance`.
///
/// @see docs/explanation/simulation-model.md, section "Delta mode".
/// @see GeographicLib::Geodesic
class DeltaSource final : public Source {
public:
    /// Creates a source whose state is `config.seed`, with the generator seeded from
    /// `config.random_seed`, no overrides and steering mode off.
    ///
    /// @param config The seed state and drift settings.
    explicit DeltaSource(DeltaConfig config);

    /// Advances the drift, the steering and the position by `dt`, as the class description
    /// lists.
    ///
    /// @param dt Simulated time to advance by.
    /// @return The new state.
    /// @pre `dt` is not negative.
    const model::VesselState& advance(std::chrono::milliseconds dt) override;
    /// Returns the current state.
    ///
    /// @return The state owned by the source, valid for its lifetime.
    [[nodiscard]] const model::VesselState& current() const noexcept override { return state_; }
    /// Returns to the seed state, clears every override and reseeds the generator, so that
    /// the run repeats exactly.
    ///
    /// The destination and the engines set since construction survive, because they are
    /// stored in the seed; the steering mode also survives. A position, fix or satellite
    /// count set by the host is lost.
    void reset() override;

    /// Pins a parameter to a fixed value, which the state takes at once; the random drift
    /// stops for it.
    ///
    /// @param parameter The value to pin.
    /// @param value The value in the parameter's unit; it is normalised or clamped as
    ///   `Parameter` states before it enters the state.
    /// @note In steering mode the heading follows the rudder even when it is overridden; the
    ///   override then holds the heading where the rudder left it once steering mode is
    ///   switched off.
    void set_override(Parameter parameter, double value);
    /// Releases an override so that the value is simulated again from where it is.
    ///
    /// A drifting value continues its walk from where it was pinned. A value pinned outside
    /// [seed - amplitude, seed + amplitude] drifts back gradually: each `advance` moves it
    /// towards that band by the full step of its `Variation` until it is inside, and it
    /// stays where it is when its variation is zero. The speed through water follows the
    /// speed over ground again; altitude and rudder angle stay where they are. Releasing a
    /// parameter that is not overridden does nothing.
    ///
    /// @param parameter The value to release.
    void clear_override(Parameter parameter);
    /// Returns the value an overridden parameter holds, which is the value the simulation
    /// uses.
    ///
    /// @param parameter The value to query.
    /// @return The value given to `set_override` or computed by `nudge`, after normalising
    ///   or clamping as `Parameter` states, or `std::nullopt` when the parameter is not
    ///   overridden. In steering mode an overridden heading is the heading the rudder has
    ///   turned the vessel to, because steering takes precedence over the override.
    [[nodiscard]] std::optional<double> override_value(Parameter parameter) const;
    /// Adds `delta` to a parameter's current value and pins it there.
    ///
    /// The desktop application's arrow keys are nudges of speed and heading, or of the rudder
    /// in steering mode.
    ///
    /// @param parameter The value to nudge.
    /// @param delta The change, in the parameter's unit; negative to decrease. The result is
    ///   normalised or clamped as `Parameter` states.
    void nudge(Parameter parameter, double delta);

    /// Switches steering mode on or off.
    ///
    /// In steering mode the heading follows the rudder instead of drifting: the rate of turn
    /// is the rudder angle times `DeltaConfig::turn_rate_per_rudder_deg`, positive (to
    /// starboard) for a positive rudder angle, and it takes precedence over a heading
    /// override.
    ///
    /// @param enabled True to steer with the rudder, false to let the heading drift again.
    void set_steering_mode(bool enabled) noexcept { steering_mode_ = enabled; }
    /// Returns whether the heading follows the rudder.
    ///
    /// @return True in steering mode.
    [[nodiscard]] bool steering_mode() const noexcept { return steering_mode_; }

    /// Moves the vessel to a position at once; the position does not survive `reset`.
    ///
    /// @param position The new position of the GNSS antenna.
    void set_position(geo::Position position);
    /// Sets or clears the destination; the change survives `reset`.
    ///
    /// @param destination The new destination, or `std::nullopt` to clear it and stop the
    ///   autopilot sentences.
    void set_destination(std::optional<model::Destination> destination) override;
    /// Replaces or adds one engine; the change survives `reset`.
    ///
    /// @param index Position in the engine list; an index at or past the end appends the
    ///   engine at the end instead.
    /// @param engine The engine data.
    void set_engine(std::size_t index, model::Engine engine);
    /// Removes one engine; the change survives `reset`.
    ///
    /// @param index Position in the engine list; an index at or past the end is ignored.
    void remove_engine(std::size_t index);

    /// Simulates losing or regaining the GNSS fix; the change does not survive `reset`.
    ///
    /// @param has_fix False to report no fix, true to report a fix again.
    void set_fix(bool has_fix) noexcept { state_.gnss.has_fix = has_fix; }
    /// Sets the satellite counts; the change does not survive `reset`.
    ///
    /// @param in_use Number of satellites used in the fix; not checked.
    /// @param in_view Number of satellites in view; raised to `in_use` when smaller.
    void set_satellites(int in_use, int in_view) noexcept;

    /// Returns the configuration.
    ///
    /// @return The configuration given to the constructor, with the destination and engines
    ///   set since then in its seed.
    [[nodiscard]] const DeltaConfig& config() const noexcept { return config_; }

private:
    /// Returns the current value of a parameter from the state.
    ///
    /// @param parameter The value to read.
    /// @return The value in the parameter's unit.
    [[nodiscard]] double read(Parameter parameter) const noexcept;
    /// Writes a parameter into the state, normalised or clamped as `Parameter` states.
    ///
    /// @param parameter The value to write.
    /// @param value The value in the parameter's unit.
    void write(Parameter parameter, double value) noexcept;
    /// Moves a value one random step of its bounded walk.
    ///
    /// @param current The value now.
    /// @param seed The centre of the band the value must stay in.
    /// @param variation The half-width of the band and the step rate.
    /// @param seconds Length of the step in simulated seconds.
    /// @return `current` moved by a uniform random amount within plus or minus
    ///   `variation.step_per_second * seconds` and clamped to `seed` plus or minus
    ///   `variation.amplitude`; when `current` lies outside that band, `current` moved by the
    ///   full step towards the band, stopping at its edge; `current` unchanged when either
    ///   member of `variation` is zero or negative.
    [[nodiscard]] double drift(double current, double seed, const Variation& variation,
                               double seconds);
    /// Updates the values that follow from the others: course over ground, position and
    /// apparent wind.
    ///
    /// @param seconds Length of the step in simulated seconds, over which the vessel moves.
    void update_derived_values(double seconds);

    /// Configuration, including the destination and engines set since construction.
    DeltaConfig config_;
    /// The current state.
    model::VesselState state_;
    /// Whether each parameter is overridden, indexed by the enumerator's value; false when
    /// the parameter is simulated. The pinned value itself is the one in `state_`. The size
    /// is the number of `Parameter` enumerators.
    std::array<bool, 9> overrides_{};
    /// Whether the heading follows the rudder.
    bool steering_mode_{false};
    /// Pseudo-random generator of the drift, seeded from `DeltaConfig::random_seed`.
    std::mt19937 generator_;
};

}  // namespace nmeasim::core::simulation
