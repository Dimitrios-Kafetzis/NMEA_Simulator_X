// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The track-following mode: the vessel sails a GPX or KML track leg by leg.
///
/// `TrackSource` is the finite `Source` configured by a `TrackConfig`. The file also defines
/// `EndBehaviour`, which `ReplaySource` shares. ADR 0011 explains the design, and
/// docs/explanation/simulation-model.md, section "Track mode", the model.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/source.hpp>
#include <nmeasim/core/track/track.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace nmeasim::core::simulation {

/// What a finite source does when it reaches its end.
enum class EndBehaviour {
    /// Stay at the end and report `Source::finished()`; a track holds its last point with
    /// zero speed.
    Stop,
    /// Start again from the beginning, carrying the time that ran past the end into the next
    /// pass.
    Loop,
};

/// Configuration of a `TrackSource`.
struct TrackConfig {
    /// The track to follow; with fewer than two points the vessel stays at the only point, or
    /// at the seed position when there is none.
    track::Track track;
    /// Everything the track does not provide: depth, water temperature, true wind and GNSS
    /// quality, which do not drift, and the start time of a track that is not followed on
    /// its timestamps. Position, course, heading and speed come from the track.
    model::VesselState seed;
    /// Speed in knots along legs whose start point has no recorded speed, when the track is
    /// not followed on its timestamps.
    ///
    /// Such legs, and those with a recorded speed, are sailed at 0.1 knots or more, so that a
    /// zero speed never makes a leg last for ever.
    double speed_kn{6.0};
    /// Whether to follow the track's timestamps when every point has one and the times never
    /// decrease; false sails every leg at the recorded point speed or `speed_kn`.
    bool use_timestamps{true};
    /// Whether to stop at the last point or start again from the first.
    EndBehaviour end{EndBehaviour::Stop};
};

/// Follows a track leg by leg, one leg per pair of consecutive points.
///
/// Every leg has a duration: on a timed track the difference of its end points' timestamps,
/// otherwise its length divided by the speed to sail it at (the start point's recorded speed,
/// or `TrackConfig::speed_kn`, but at least 0.1 knots). The speed over ground shown is the
/// speed the leg is sailed at, except on a timed track whose start point has a recorded
/// speed, which is shown as recorded. The position is the WGS 84 geodesic point at the elapsed
/// fraction of the current leg, so the output rate and the point density of the file are
/// independent. Course over ground is the start point's recorded course when it has one,
/// otherwise the leg's initial bearing; heading equals course, speed through water equals
/// speed over ground, and the rate of turn follows from the heading change over the step.
/// Altitude is interpolated when both ends of the leg have an elevation. The apparent wind
/// is recomputed from the seed's true wind and the motion.
///
/// Time: the position along the track (`position()`) is simulated time since the first
/// point. On a timed track the state's `time_utc` is the first point's timestamp plus that
/// position, so it rewinds when the track loops and jumps when it seeks. On an untimed track
/// it is the seed's time plus the total simulated time advanced since the start or the last
/// reset, which keeps growing through loops and is not changed by a seek.
///
/// @see GeographicLib::Geodesic
class TrackSource final : public Source {
public:
    /// Builds the legs and places the vessel at the first point.
    ///
    /// @param config The track, seed and playback settings.
    explicit TrackSource(TrackConfig config);

    /// Moves the vessel `dt` further along the track.
    ///
    /// At the end, a stopping track holds the last point with zero speed and becomes
    /// finished; a looping one wraps the surplus time into the next pass. A track whose total
    /// duration is zero, such as a single point, becomes finished on the first advance even
    /// when it loops. Once finished, `advance` changes nothing.
    ///
    /// @param dt Simulated time to advance by.
    /// @return The new state.
    /// @pre `dt` is not negative.
    const model::VesselState& advance(std::chrono::milliseconds dt) override;
    /// Returns the current state.
    ///
    /// @return The state owned by the source, valid for its lifetime.
    [[nodiscard]] const model::VesselState& current() const noexcept override { return state_; }
    /// Returns to the first point with the seed state and restarts the clock.
    ///
    /// The destination set with `set_destination` survives; a fix set with `set_fix` is
    /// lost.
    void reset() override;
    /// Returns whether the source stopped at the end of the track.
    ///
    /// @return True once a track configured with `EndBehaviour::Stop` has reached its end.
    [[nodiscard]] bool finished() const noexcept override { return finished_; }
    /// Returns the time it takes to sail the whole track.
    ///
    /// @return The sum of the leg durations, rounded to the nearest millisecond; zero for a
    ///   track with fewer than two points. Never `std::nullopt`.
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const noexcept override;
    /// Returns the elapsed time along the track.
    ///
    /// @return The time since the first point, rounded to the nearest millisecond, in
    ///   [0, `duration()`].
    [[nodiscard]] std::chrono::milliseconds position() const noexcept override;
    /// Moves the vessel to a time along the track at once.
    ///
    /// The state is recomputed from the leg table with a zero rate of turn. Seeking to the end
    /// of a stopping track finishes it; seeking anywhere else clears `finished()`.
    ///
    /// @param position Time since the first point, clamped to [0, `duration()`].
    void seek(std::chrono::milliseconds position) override;

    /// Moves the vessel to a point of the track at once, as `seek` to the time the vessel
    /// passes it.
    ///
    /// @param index Index into `TrackConfig::track`'s points; an index at or past the last
    ///   point moves to the end of the track, which finishes a stopping track.
    void jump_to_point(std::size_t index);
    /// Returns the index of the track point most recently passed.
    ///
    /// @return The index of the current leg's start point, the index of the last point once
    ///   the end is reached, or zero for a track with fewer than two points.
    [[nodiscard]] std::size_t point_index() const noexcept;

    /// Returns whether the source follows the track's own timestamps.
    ///
    /// @return True when `TrackConfig::use_timestamps` is set, the track has at least two
    ///   points, and every point has a timestamp that is not earlier than the one before.
    [[nodiscard]] bool timed() const noexcept { return timed_; }
    /// Returns the configuration.
    ///
    /// @return The configuration given to the constructor, with the destination set since
    ///   then in its seed.
    [[nodiscard]] const TrackConfig& config() const noexcept { return config_; }

    /// Simulates losing or regaining the GNSS fix; the change survives `advance` and `seek`
    /// but not `reset`.
    ///
    /// @param has_fix False to report no fix, true to report a fix again.
    void set_fix(bool has_fix) noexcept { state_.gnss.has_fix = has_fix; }
    /// Sets or clears the destination; the change survives `reset` and `seek`.
    ///
    /// @param destination The new destination, or `std::nullopt` to clear it and stop the
    ///   autopilot sentences.
    void set_destination(std::optional<model::Destination> destination) override;

private:
    /// One leg of the track, between two consecutive points.
    struct Leg {
        /// Position of the start point.
        geo::Position from;
        /// Position of the end point.
        geo::Position to;
        /// Geodesic length of the leg, in metres.
        double length_m{0.0};
        /// Initial bearing from `from` to `to`, degrees true.
        ///
        /// A leg shorter than a millimetre has no bearing of its own and takes the previous
        /// leg's, or for the first leg the first point's recorded course or zero, so that a
        /// stop does not turn the bow.
        double bearing_deg{0.0};
        /// Offset of the leg's start from the start of the track, in seconds.
        double start_s{0.0};
        /// Time it takes to sail the leg, in seconds; zero for a timed leg whose points share
        /// a timestamp.
        double duration_s{0.0};
        /// Speed over ground shown along the leg, in knots.
        double speed_kn{0.0};
        /// Recorded course of the start point, degrees true, when the file has one.
        std::optional<double> course_deg;
        /// Recorded elevation of the start point, in metres, when the file has one.
        std::optional<double> elevation_m;
        /// Index of the start point in the track's points.
        std::size_t point_index{0};
    };

    /// Rebuilds the leg table, `timed_` and `total_s_` from the configuration.
    void build_legs();
    /// Moves to a time along the track and recomputes the state with a zero rate of turn.
    ///
    /// @param progress_s Time since the first point, in seconds, clamped to [0, `total_s_`].
    void seek_seconds(double progress_s);
    /// Recomputes the state from the current leg and the elapsed times.
    ///
    /// @param dt_seconds Length of the step that led here, in seconds, used for the rate of
    ///   turn; zero gives a zero rate of turn.
    void update_state(double dt_seconds);
    /// Returns the leg the vessel is on at a time along the track.
    ///
    /// @param progress_s Time since the first point, in seconds.
    /// @return The last leg that starts at or before `progress_s`, so that legs of zero
    ///   duration are skipped; the first leg for a negative time.
    /// @pre `legs_` is not empty.
    [[nodiscard]] const Leg& leg_at(double progress_s) const noexcept;

    /// Configuration, including the destination set since construction.
    TrackConfig config_;
    /// One leg per pair of consecutive points, in track order; empty for fewer than two
    /// points.
    std::vector<Leg> legs_;
    /// Whether the leg durations come from the track's timestamps.
    bool timed_{false};
    /// Total duration of the track, in seconds.
    double total_s_{0.0};
    /// Elapsed time along the track, in seconds, in [0, `total_s_`].
    double progress_s_{0.0};
    /// Simulated time advanced since the start or the last reset, in seconds; it keeps
    /// growing when the track loops and drives the clock of an untimed track.
    double elapsed_s_{0.0};
    /// Whether a stopping track has reached its end.
    bool finished_{false};
    /// The current state.
    model::VesselState state_;
};

}  // namespace nmeasim::core::simulation
