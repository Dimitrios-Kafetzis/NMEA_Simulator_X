#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/source.hpp>
#include <nmeasim/core/track/track.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

/// The track-following mode: the vessel sails a GPX or KML track.
namespace nmeasim::core::simulation {

/// What a finite source does when it reaches its end.
enum class EndBehaviour {
    /// Stay at the last point with zero speed and report `finished()`.
    Stop,
    /// Start again from the first point.
    Loop,
};

struct TrackConfig {
    track::Track track;
    /// Environment values (depth, wind, GNSS) and, for a track without timestamps, the
    /// start time. The position, course and speed come from the track.
    model::VesselState seed;
    /// Speed used along legs whose points carry neither timestamps nor a recorded speed.
    double speed_kn{6.0};
    /// When false, timestamps in the track are ignored and every leg is sailed at
    /// `speed_kn` or the recorded point speed.
    bool use_timestamps{true};
    EndBehaviour end{EndBehaviour::Stop};
};

/// Follows a track leg by leg.
///
/// Every leg has a duration: the difference of its end points' timestamps when the track is
/// timed, otherwise its length divided by the speed to sail it at. The position along the
/// current leg is the geodesic interpolation for the elapsed fraction of that duration, so the
/// output rate and the point density of the file are independent. Course over ground is the
/// recorded course of the leg's start point when the file has one, otherwise the leg's
/// initial bearing; heading follows the course.
class TrackSource final : public Source {
public:
    explicit TrackSource(TrackConfig config);

    const model::VesselState& advance(std::chrono::milliseconds dt) override;
    [[nodiscard]] const model::VesselState& current() const noexcept override { return state_; }
    void reset() override;
    [[nodiscard]] bool finished() const noexcept override { return finished_; }
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const noexcept override;
    [[nodiscard]] std::chrono::milliseconds position() const noexcept override;
    void seek(std::chrono::milliseconds position) override;

    /// Jumps to a point of the track; the index is clamped to the last point.
    void jump_to_point(std::size_t index);
    /// Index of the track point most recently passed.
    [[nodiscard]] std::size_t point_index() const noexcept;

    /// True when the source follows the track's own timestamps.
    [[nodiscard]] bool timed() const noexcept { return timed_; }
    [[nodiscard]] const TrackConfig& config() const noexcept { return config_; }

    /// Simulates losing or regaining the GNSS fix.
    void set_fix(bool has_fix) noexcept { state_.gnss.has_fix = has_fix; }

private:
    struct Leg {
        geo::Position from;
        geo::Position to;
        double length_m{0.0};
        double bearing_deg{0.0};
        /// Offset of the leg's start from the start of the track, in seconds.
        double start_s{0.0};
        double duration_s{0.0};
        double speed_kn{0.0};
        std::optional<double> course_deg;
        std::optional<double> elevation_m;
        std::size_t point_index{0};
    };

    void build_legs();
    void seek_seconds(double progress_s);
    void update_state(double dt_seconds);
    [[nodiscard]] const Leg& leg_at(double progress_s) const noexcept;

    TrackConfig config_;
    std::vector<Leg> legs_;
    bool timed_{false};
    double total_s_{0.0};
    /// Elapsed time along the track, in seconds.
    double progress_s_{0.0};
    /// Elapsed simulated time since start, which keeps growing when the track loops.
    double elapsed_s_{0.0};
    bool finished_{false};
    model::VesselState state_;
};

}  // namespace nmeasim::core::simulation
