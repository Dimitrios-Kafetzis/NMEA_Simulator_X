#include <nmeasim/core/physics/wind.hpp>
#include <nmeasim/core/simulation/track_source.hpp>
#include <nmeasim/core/units.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace nmeasim::core::simulation {

namespace {

/// Slowest speed a leg is sailed at, so that a zero speed never makes a leg endless.
constexpr double kMinSpeedKn{0.1};
/// Legs shorter than this are treated as a stop at the same place.
constexpr double kZeroLengthM{1e-3};

std::chrono::milliseconds to_milliseconds(double seconds) {
    return std::chrono::round<std::chrono::milliseconds>(std::chrono::duration<double>{seconds});
}

}  // namespace

TrackSource::TrackSource(TrackConfig config) : config_(std::move(config)), state_(config_.seed) {
    build_legs();
    update_state(0.0);
}

void TrackSource::build_legs() {
    const auto& points = config_.track.points;
    legs_.clear();
    timed_ = config_.use_timestamps && points.size() >= 2 && config_.track.has_timestamps();
    double start_s = 0.0;
    double previous_bearing = points.empty() ? 0.0 : points.front().course_deg.value_or(0.0);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const auto& from = points[i];
        const auto& to = points[i + 1];
        const auto solution = geo::inverse(from.position, to.position);
        Leg leg;
        leg.from = from.position;
        leg.to = to.position;
        leg.length_m = solution.distance_m;
        leg.bearing_deg =
            solution.distance_m < kZeroLengthM ? previous_bearing : solution.initial_bearing_deg;
        previous_bearing = leg.bearing_deg;
        if (timed_) {
            leg.duration_s = std::chrono::duration<double>(*to.time - *from.time).count();
            leg.speed_kn = from.speed_kn.value_or(
                leg.duration_s > 0.0 ? units::mps_to_knots(leg.length_m / leg.duration_s) : 0.0);
        } else {
            leg.speed_kn = std::max(kMinSpeedKn, from.speed_kn.value_or(config_.speed_kn));
            leg.duration_s = leg.length_m / units::knots_to_mps(leg.speed_kn);
        }
        leg.start_s = start_s;
        leg.course_deg = from.course_deg;
        leg.elevation_m = from.elevation_m;
        leg.point_index = i;
        start_s += leg.duration_s;
        legs_.push_back(leg);
    }
    total_s_ = start_s;
}

const TrackSource::Leg& TrackSource::leg_at(double progress_s) const noexcept {
    const auto it =
        std::upper_bound(legs_.begin(), legs_.end(), progress_s,
                         [](double value, const Leg& leg) { return value < leg.start_s; });
    const auto index = std::max<std::ptrdiff_t>(std::distance(legs_.begin(), it) - 1, 0);
    return legs_[static_cast<std::size_t>(index)];
}

void TrackSource::update_state(double dt_seconds) {
    const auto& points = config_.track.points;
    const auto& seed = config_.seed;
    auto& navigation = state_.navigation;
    const double previous_heading = navigation.heading_true_deg;

    if (points.empty()) {
        return;
    }
    if (legs_.empty() || (finished_ && config_.end == EndBehaviour::Stop)) {
        const auto& last = points.back();
        navigation.position = last.position;
        navigation.course_over_ground_deg = last.course_deg.value_or(
            legs_.empty() ? seed.navigation.course_over_ground_deg : legs_.back().bearing_deg);
        navigation.speed_over_ground_kn = 0.0;
        navigation.altitude_m = last.elevation_m.value_or(seed.navigation.altitude_m);
    } else {
        const Leg& leg = leg_at(progress_s_);
        const double fraction =
            leg.duration_s > 0.0
                ? std::clamp((progress_s_ - leg.start_s) / leg.duration_s, 0.0, 1.0)
                : 1.0;
        if (fraction <= 0.0 || leg.length_m < kZeroLengthM) {
            navigation.position = leg.from;
        } else if (fraction >= 1.0) {
            navigation.position = leg.to;
        } else {
            navigation.position = geo::direct(leg.from, leg.bearing_deg, fraction * leg.length_m);
        }
        navigation.course_over_ground_deg = leg.course_deg.value_or(leg.bearing_deg);
        navigation.speed_over_ground_kn = leg.speed_kn;
        const auto& to = points[leg.point_index + 1];
        if (leg.elevation_m && to.elevation_m) {
            navigation.altitude_m =
                *leg.elevation_m + fraction * (*to.elevation_m - *leg.elevation_m);
        } else {
            navigation.altitude_m = leg.elevation_m.value_or(seed.navigation.altitude_m);
        }
    }
    navigation.heading_true_deg = navigation.course_over_ground_deg;
    navigation.speed_through_water_kn = navigation.speed_over_ground_kn;
    navigation.rate_of_turn_deg_per_min =
        dt_seconds > 0.0 ? std::remainder(navigation.heading_true_deg - previous_heading, 360.0) *
                               60.0 / dt_seconds
                         : 0.0;

    if (timed_) {
        state_.time_utc = *points.front().time + to_milliseconds(progress_s_);
    } else {
        state_.time_utc = seed.time_utc + to_milliseconds(elapsed_s_);
    }

    const auto apparent =
        physics::apparent_wind(state_.wind.true_direction_deg, state_.wind.true_speed_kn,
                               navigation.heading_true_deg, navigation.speed_through_water_kn);
    state_.wind.apparent_angle_deg = apparent.angle_relative_deg;
    state_.wind.apparent_speed_kn = apparent.speed_kn;
}

const model::VesselState& TrackSource::advance(std::chrono::milliseconds dt) {
    if (finished_) {
        return state_;
    }
    const double seconds = std::chrono::duration<double>(dt).count();
    progress_s_ += seconds;
    elapsed_s_ += seconds;
    if (progress_s_ >= total_s_) {
        if (config_.end == EndBehaviour::Loop && total_s_ > 0.0) {
            progress_s_ = std::fmod(progress_s_, total_s_);
        } else {
            progress_s_ = total_s_;
            finished_ = true;
        }
    }
    update_state(seconds);
    return state_;
}

void TrackSource::reset() {
    progress_s_ = 0.0;
    elapsed_s_ = 0.0;
    finished_ = false;
    state_ = config_.seed;
    update_state(0.0);
}

std::optional<std::chrono::milliseconds> TrackSource::duration() const noexcept {
    return to_milliseconds(total_s_);
}

std::chrono::milliseconds TrackSource::position() const noexcept {
    return to_milliseconds(progress_s_);
}

void TrackSource::seek_seconds(double progress_s) {
    progress_s_ = std::clamp(progress_s, 0.0, total_s_);
    finished_ = progress_s_ >= total_s_ && config_.end == EndBehaviour::Stop;
    update_state(0.0);
}

void TrackSource::seek(std::chrono::milliseconds position) {
    seek_seconds(std::chrono::duration<double>(position).count());
}

void TrackSource::jump_to_point(std::size_t index) {
    if (legs_.empty() || index == 0) {
        seek_seconds(0.0);
    } else if (index >= legs_.size()) {
        seek_seconds(total_s_);
    } else {
        seek_seconds(legs_[index].start_s);
    }
}

std::size_t TrackSource::point_index() const noexcept {
    if (legs_.empty()) {
        return 0;
    }
    if (progress_s_ >= total_s_) {
        return legs_.size();
    }
    return leg_at(progress_s_).point_index;
}

}  // namespace nmeasim::core::simulation
