#include <nmeasim/core/physics/wind.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/units.hpp>

#include <algorithm>
#include <cmath>

namespace nmeasim::core::simulation {

namespace {

constexpr std::size_t index_of(Parameter parameter) noexcept {
    return static_cast<std::size_t>(parameter);
}

}  // namespace

DeltaSource::DeltaSource(DeltaConfig config)
    : config_(std::move(config)), state_(config_.seed), generator_(config_.random_seed) {}

void DeltaSource::reset() {
    state_ = config_.seed;
    overrides_.fill(std::nullopt);
    generator_.seed(config_.random_seed);
}

double DeltaSource::read(Parameter parameter) const noexcept {
    switch (parameter) {
        case Parameter::HeadingTrue:
            return state_.navigation.heading_true_deg;
        case Parameter::SpeedOverGround:
            return state_.navigation.speed_over_ground_kn;
        case Parameter::SpeedThroughWater:
            return state_.navigation.speed_through_water_kn;
        case Parameter::Altitude:
            return state_.navigation.altitude_m;
        case Parameter::Depth:
            return state_.water.depth_below_transducer_m;
        case Parameter::WaterTemperature:
            return state_.water.temperature_c;
        case Parameter::WindDirectionTrue:
            return state_.wind.true_direction_deg;
        case Parameter::WindSpeedTrue:
            return state_.wind.true_speed_kn;
        case Parameter::RudderAngle:
            return state_.steering.rudder_angle_deg;
    }
    return 0.0;
}

void DeltaSource::write(Parameter parameter, double value) noexcept {
    switch (parameter) {
        case Parameter::HeadingTrue:
            state_.navigation.heading_true_deg = geo::normalize_bearing(value);
            return;
        case Parameter::SpeedOverGround:
            state_.navigation.speed_over_ground_kn = std::max(0.0, value);
            return;
        case Parameter::SpeedThroughWater:
            state_.navigation.speed_through_water_kn = std::max(0.0, value);
            return;
        case Parameter::Altitude:
            state_.navigation.altitude_m = value;
            return;
        case Parameter::Depth:
            state_.water.depth_below_transducer_m = std::max(0.0, value);
            return;
        case Parameter::WaterTemperature:
            state_.water.temperature_c = value;
            return;
        case Parameter::WindDirectionTrue:
            state_.wind.true_direction_deg = geo::normalize_bearing(value);
            return;
        case Parameter::WindSpeedTrue:
            state_.wind.true_speed_kn = std::max(0.0, value);
            return;
        case Parameter::RudderAngle:
            state_.steering.rudder_angle_deg =
                std::clamp(value, -config_.max_rudder_angle_deg, config_.max_rudder_angle_deg);
            return;
    }
}

void DeltaSource::set_override(Parameter parameter, double value) {
    overrides_[index_of(parameter)] = value;
    write(parameter, value);
}

void DeltaSource::clear_override(Parameter parameter) {
    overrides_[index_of(parameter)].reset();
}

std::optional<double> DeltaSource::override_value(Parameter parameter) const {
    return overrides_[index_of(parameter)];
}

void DeltaSource::nudge(Parameter parameter, double delta) {
    set_override(parameter, read(parameter) + delta);
}

void DeltaSource::set_position(geo::Position position) {
    state_.navigation.position = position;
}

void DeltaSource::set_satellites(int in_use, int in_view) noexcept {
    state_.gnss.satellites_in_use = in_use;
    state_.gnss.satellites_in_view = std::max(in_use, in_view);
}

double DeltaSource::drift(double current, double seed, const Variation& variation, double seconds) {
    if (variation.amplitude <= 0.0 || variation.step_per_second <= 0.0) {
        return current;
    }
    const double step = variation.step_per_second * seconds;
    std::uniform_real_distribution<double> distribution(-step, step);
    const double next = current + distribution(generator_);
    return std::clamp(next, seed - variation.amplitude, seed + variation.amplitude);
}

const model::VesselState& DeltaSource::advance(std::chrono::milliseconds dt) {
    const double seconds = std::chrono::duration<double>(dt).count();
    const auto& seed = config_.seed;
    auto& navigation = state_.navigation;

    state_.time_utc += dt;

    // Heading: steering mode integrates the rudder, otherwise the heading drifts unless
    // pinned by an override.
    const double previous_heading = navigation.heading_true_deg;
    if (steering_mode_) {
        navigation.rate_of_turn_deg_per_min =
            state_.steering.rudder_angle_deg * config_.turn_rate_per_rudder_deg;
        navigation.heading_true_deg = geo::normalize_bearing(
            previous_heading + navigation.rate_of_turn_deg_per_min * seconds / 60.0);
    } else if (!overrides_[index_of(Parameter::HeadingTrue)]) {
        // Drift is computed on an unwrapped heading so that the seed bound works across north.
        const double offset =
            std::remainder(previous_heading - seed.navigation.heading_true_deg, 360.0);
        const double drifted = drift(offset, 0.0, config_.heading, seconds);
        navigation.heading_true_deg =
            geo::normalize_bearing(seed.navigation.heading_true_deg + drifted);
        navigation.rate_of_turn_deg_per_min =
            seconds > 0.0 ? std::remainder(navigation.heading_true_deg - previous_heading, 360.0) *
                                60.0 / seconds
                          : 0.0;
    } else {
        navigation.rate_of_turn_deg_per_min = 0.0;
    }

    if (!overrides_[index_of(Parameter::SpeedOverGround)]) {
        navigation.speed_over_ground_kn =
            std::max(0.0, drift(navigation.speed_over_ground_kn,
                                seed.navigation.speed_over_ground_kn, config_.speed, seconds));
    }
    if (!overrides_[index_of(Parameter::SpeedThroughWater)]) {
        // Without current or leeway the water speed equals the ground speed.
        navigation.speed_through_water_kn = navigation.speed_over_ground_kn;
    }
    if (!overrides_[index_of(Parameter::Depth)]) {
        state_.water.depth_below_transducer_m =
            std::max(0.0, drift(state_.water.depth_below_transducer_m,
                                seed.water.depth_below_transducer_m, config_.depth, seconds));
    }
    if (!overrides_[index_of(Parameter::WaterTemperature)]) {
        state_.water.temperature_c = drift(state_.water.temperature_c, seed.water.temperature_c,
                                           config_.water_temperature, seconds);
    }
    if (!overrides_[index_of(Parameter::WindDirectionTrue)]) {
        const double offset =
            std::remainder(state_.wind.true_direction_deg - seed.wind.true_direction_deg, 360.0);
        state_.wind.true_direction_deg = geo::normalize_bearing(
            seed.wind.true_direction_deg + drift(offset, 0.0, config_.wind_direction, seconds));
    }
    if (!overrides_[index_of(Parameter::WindSpeedTrue)]) {
        state_.wind.true_speed_kn = std::max(
            0.0,
            drift(state_.wind.true_speed_kn, seed.wind.true_speed_kn, config_.wind_speed, seconds));
    }

    update_derived_values(seconds);
    return state_;
}

void DeltaSource::update_derived_values(double seconds) {
    auto& navigation = state_.navigation;

    // The vessel tracks its heading; course over ground equals heading without set and drift.
    navigation.course_over_ground_deg = navigation.heading_true_deg;

    const double distance_m = units::knots_to_mps(navigation.speed_over_ground_kn) * seconds;
    if (distance_m > 0.0) {
        navigation.position =
            geo::direct(navigation.position, navigation.course_over_ground_deg, distance_m);
    }

    const auto apparent =
        physics::apparent_wind(state_.wind.true_direction_deg, state_.wind.true_speed_kn,
                               navigation.heading_true_deg, navigation.speed_through_water_kn);
    state_.wind.apparent_angle_deg = apparent.angle_relative_deg;
    state_.wind.apparent_speed_kn = apparent.speed_kn;
}

}  // namespace nmeasim::core::simulation
