// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The *Simulation* tab of the settings dialog, `SimulationPage`.
///
/// The page edits the `simulation` object of a profile: the mode and its track and replay
/// settings, the profile name, the clock, the random seed, the initial vessel values, the GNSS
/// receiver, the drift, the destination and the steering model. `docs/reference/desktop-app.md`
/// lists its groups and `docs/reference/profile.md` the profile keys they map to.

#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QWidget>

#include <array>

namespace nmeasim::app {

/// Settings tab for the simulation mode, the profile name, the clock, the vessel seed values,
/// their drift and the steering model.
///
/// `SettingsDialog` calls `load` once with its copy of the profile, `validate` when the
/// operator presses *OK*, and `store` into the same copy when every page is valid. The page
/// keeps no profile of its own: the widgets are the only state, and `store` writes only the
/// fields listed at the widget members, so the rest of the profile passes through unchanged.
///
/// Each widget enforces a range, given at its member. `load` clamps a profile value outside
/// that range to the nearest limit and rounds it to the number of decimals shown, so a value
/// that has more precision than the widget comes back rounded from `store`.
///
/// Every widget is created in the constructor as a descendant of the page and is deleted with
/// it; the widget pointers are never null and stay valid for the lifetime of the page. The
/// widgets are laid out in two columns inside a scroll area, so the page fits a small dialog.
/// The class declares no signals or slots of its own.
///
/// @see SettingsDialog
class SimulationPage : public QWidget {
    Q_OBJECT

public:
    /// Creates the page with every widget in its initial state, the destination fields and the
    /// start time disabled and the group boxes enabled for delta mode.
    ///
    /// The widgets show no profile until `load` is called.
    ///
    /// @param parent The owner of the page, normally the `SettingsDialog`; when null, the
    ///     caller owns the page.
    explicit SimulationPage(QWidget* parent = nullptr);

    /// Shows the simulation settings of a profile in the widgets.
    ///
    /// Sets every widget from the field named at its member and enables the group boxes for
    /// the profile's mode. Without a fixed start time, the start time field shows the current
    /// UTC time rounded down to the second, as the value to start from when the operator
    /// ticks *Fixed start time*. Without a destination, the destination fields show an empty
    /// id, the seed position and a radius of 100 m.
    ///
    /// @param profile The profile to show; only read during the call.
    void load(const io::Profile& profile);
    /// Writes the widget values into the simulation fields of a profile.
    ///
    /// Only the fields named at the widget members are written; the rest of `profile`,
    /// including the seed engines and AIS data that `VesselPage` edits, is left as it is. The
    /// track and replay settings are written in every mode, so switching modes keeps them.
    /// The paths and the name are trimmed, and an empty name becomes `Untitled`. An unticked
    /// *Fixed start time* resets `io::Profile::start_time` and an unticked destination resets
    /// the seed destination.
    ///
    /// A ticked destination is written with the id `WPT` when the id field is empty. Its leg
    /// origin is kept when the profile already has a destination at exactly the same latitude
    /// and longitude; otherwise the leg starts at the seed position written by this call.
    ///
    /// @param profile The profile to update, normally the dialog's copy.
    /// @note `store` does not validate: it writes an empty track or replay path as it is.
    ///     `SettingsDialog` calls it only after `validate` returned an empty string.
    void store(io::Profile& profile) const;
    /// Checks that the chosen mode has the file it needs.
    ///
    /// @return A message for the operator, `Choose the track file to follow.` in track mode
    ///     or `Choose the log file to replay.` in replay mode when the path is empty after
    ///     trimming; an empty string when the settings are valid. Whether the file exists is
    ///     not checked here.
    [[nodiscard]] QString validate() const;

    // Widgets are public so that tests can drive them like an operator would.

    /// Chooses what drives the vessel, `io::Profile::mode`.
    ///
    /// The items are delta simulation, follow a track and replay a log, in the order of the
    /// `io::SimulationMode` enumerators, so the current index is the enumerator's value.
    /// Changing it enables the track group in track mode, the replay group in replay mode and
    /// the drift group in delta mode.
    QComboBox* mode_combo;
    /// Track file to follow, `io::TrackSettings::path`; a GPX or KML file, trimmed by `store`.
    QLineEdit* track_path_edit;
    /// Opens a file dialog for a `.gpx` or `.kml` track and puts the chosen path into
    /// `track_path_edit`; cancelling leaves the field unchanged.
    QPushButton* track_browse_button;
    /// Speed along legs whose points carry neither timestamps nor a recorded speed,
    /// `io::TrackSettings::speed_kn`; [0.1, 999.9] knots with one decimal.
    ///
    /// The minimum keeps the value positive, which the profile requires.
    QDoubleSpinBox* track_speed_spin;
    /// Whether the track's own timestamps set the pace, `io::TrackSettings::use_timestamps`.
    QCheckBox* track_timestamps_check;
    /// Whether the track starts again at its first point, `io::TrackSettings::loop`.
    QCheckBox* track_loop_check;
    /// Log file to replay, `io::ReplaySettings::path`; trimmed by `store`.
    QLineEdit* replay_path_edit;
    /// Opens a file dialog for a `.log`, `.nmea` or `.txt` file and puts the chosen path into
    /// `replay_path_edit`; cancelling leaves the field unchanged.
    QPushButton* replay_browse_button;
    /// Whether the log starts again at its first entry, `io::ReplaySettings::loop`.
    QCheckBox* replay_loop_check;
    /// Spacing of the entries when the log carries no time information,
    /// `io::ReplaySettings::fixed_interval_ms`; [1, 60000] ms, the range the profile accepts.
    QSpinBox* replay_interval_spin;

    /// Display name of the profile, `io::Profile::name`; `store` writes `Untitled` for an
    /// empty or blank name.
    QLineEdit* name_edit;
    /// Length of one simulation step, `io::Profile::tick_ms`; [10, 10000] ms, the range the
    /// profile accepts.
    QSpinBox* tick_spin;
    /// Whether the run starts at `start_time_edit` rather than at the wall-clock time;
    /// unticked means `io::Profile::start_time` is `std::nullopt`.
    ///
    /// Toggling it enables or disables `start_time_edit`.
    QCheckBox* fixed_start_check;
    /// Simulated start time in UTC, `io::Profile::start_time`, shown as
    /// `yyyy-MM-dd HH:mm:ss`; enabled only while `fixed_start_check` is ticked.
    QDateTimeEdit* start_time_edit;
    /// Seed of the pseudo-random generator of the drift,
    /// `core::simulation::DeltaConfig::random_seed`; [0, 1000000000].
    ///
    /// A larger seed, which a profile file can hold, does not survive the page: `load` shows
    /// it as the maximum, or as 0 when it exceeds the largest `int`, and `store` writes that
    /// value back.
    QSpinBox* random_seed_spin;

    /// Seed latitude, positive north, the latitude of `core::model::Navigation::position`;
    /// [-90, 90] degrees with six decimals.
    QDoubleSpinBox* latitude_spin;
    /// Seed longitude, positive east, the longitude of `core::model::Navigation::position`;
    /// [-180, 180] degrees with six decimals.
    QDoubleSpinBox* longitude_spin;
    /// Antenna altitude above mean sea level, `core::model::Navigation::altitude_m`;
    /// [-500, 20000] m with one decimal.
    QDoubleSpinBox* altitude_spin;
    /// Seed heading, `core::model::Navigation::heading_true_deg`; [0, 359.9] degrees true
    /// with one decimal.
    QDoubleSpinBox* heading_spin;
    /// Seed speed over ground, `core::model::Navigation::speed_over_ground_kn`; [0, 999.9]
    /// knots with one decimal.
    QDoubleSpinBox* speed_spin;
    /// Magnetic variation, positive east, `core::model::Navigation::magnetic_variation_deg`;
    /// [-180, 180] degrees with one decimal.
    QDoubleSpinBox* variation_spin;
    /// Compass deviation, positive east, `core::model::Navigation::magnetic_deviation_deg`;
    /// [-180, 180] degrees with one decimal.
    QDoubleSpinBox* deviation_spin;
    /// Seed depth below the transducer, `core::model::Water::depth_below_transducer_m`;
    /// [0, 99999.9] m with one decimal.
    QDoubleSpinBox* depth_spin;
    /// Transducer offset, `core::model::Water::transducer_offset_m`, positive up to the water
    /// line and negative down to the keel; [-50, 50] m with two decimals.
    QDoubleSpinBox* transducer_offset_spin;
    /// Seed water temperature, `core::model::Water::temperature_c`; [-5, 60] degrees Celsius
    /// with one decimal.
    QDoubleSpinBox* water_temperature_spin;
    /// Direction the true wind blows from, `core::model::Wind::true_direction_deg`;
    /// [0, 359.9] degrees true with one decimal.
    QDoubleSpinBox* wind_direction_spin;
    /// True wind speed, `core::model::Wind::true_speed_kn`; [0, 200] knots with one decimal.
    QDoubleSpinBox* wind_speed_spin;

    /// Whether the receiver has a position fix, `core::model::GnssFix::has_fix`.
    QCheckBox* fix_check;
    /// Fix quality, `core::model::GnssFix::quality`.
    ///
    /// The items are invalid, GPS and differential, in the order of the
    /// `core::model::FixQuality` enumerators, so the current index is the enumerator's value.
    QComboBox* quality_combo;
    /// Satellites used in the solution, `core::model::GnssFix::satellites_in_use`; [0, 12],
    /// the range the NMEA encoders report.
    QSpinBox* satellites_in_use_spin;
    /// Satellites in view, `core::model::GnssFix::satellites_in_view`; [0, 12], the size of
    /// the synthetic constellation.
    ///
    /// The page does not keep it at or above the satellites in use; the encoders do.
    QSpinBox* satellites_in_view_spin;
    /// Horizontal dilution of precision, `core::model::GnssFix::hdop`; [0, 99.9] with one
    /// decimal.
    QDoubleSpinBox* hdop_spin;
    /// Position dilution of precision, `core::model::GnssFix::pdop`; [0, 99.9] with one
    /// decimal.
    QDoubleSpinBox* pdop_spin;
    /// Vertical dilution of precision, `core::model::GnssFix::vdop`; [0, 99.9] with one
    /// decimal.
    QDoubleSpinBox* vdop_spin;
    /// Height of the geoid above the WGS 84 ellipsoid,
    /// `core::model::GnssFix::geoid_separation_m`; [-200, 200] m with one decimal.
    QDoubleSpinBox* geoid_spin;

    /// Drift amplitudes, `core::simulation::Variation::amplitude` of the heading, speed,
    /// depth, water temperature, wind direction and wind speed variations of
    /// `core::simulation::DeltaConfig`, in that order.
    ///
    /// Each is in the unit of its value (degrees, knots, metres, degrees Celsius, degrees,
    /// knots), [0, 1000] with two decimals; 0 freezes the value. The group is enabled only in
    /// delta mode, the only mode that uses the variations.
    std::array<QDoubleSpinBox*, 6> amplitude_spins{};
    /// Largest random change per second, `core::simulation::Variation::step_per_second`, of
    /// the same six variations in the same order as `amplitude_spins`.
    ///
    /// Each is in the unit of its value per second, [0, 1000] with three decimals; 0 freezes
    /// the value.
    std::array<QDoubleSpinBox*, 6> step_spins{};

    /// Gain of the steering model, `core::simulation::DeltaConfig::turn_rate_per_rudder_deg`;
    /// [0, 100] degrees per minute of rate of turn per degree of rudder, with two decimals.
    QDoubleSpinBox* turn_rate_spin;
    /// Largest rudder angle either side, `core::simulation::DeltaConfig::max_rudder_angle_deg`;
    /// [1, 90] whole degrees.
    QDoubleSpinBox* max_rudder_spin;

    /// Whether the vessel has a destination; unticked means the seed destination
    /// `core::model::VesselState::destination` is `std::nullopt`.
    ///
    /// Toggling it enables or disables the other four destination widgets.
    QCheckBox* destination_check;
    /// Waypoint id, `core::model::Destination::name`; at most 16 characters, the length
    /// `core::model::kMaxWaypointNameLength` that the encoders send. Empty is stored as `WPT`.
    QLineEdit* destination_name_edit;
    /// Latitude of the waypoint, positive north; [-90, 90] degrees with six decimals.
    QDoubleSpinBox* destination_latitude_spin;
    /// Longitude of the waypoint, positive east; [-180, 180] degrees with six decimals.
    QDoubleSpinBox* destination_longitude_spin;
    /// Radius of the arrival circle, `core::model::Destination::arrival_radius_m`;
    /// [1, 100000] whole metres.
    QDoubleSpinBox* arrival_radius_spin;

private:
    /// Enables the track group in track mode, the replay group in replay mode and the drift
    /// group in delta mode, after the current index of `mode_combo`.
    ///
    /// Connected to `QComboBox::currentIndexChanged` of `mode_combo`, and called by the
    /// constructor and by `load`.
    void update_mode_widgets();
    /// Asks the operator for a track file and puts the chosen path into `track_path_edit`.
    ///
    /// The file dialog starts in the directory of the current path, or in the documents
    /// folder when the field is empty, and offers `*.gpx` and `*.kml` files. Cancelling leaves
    /// the field unchanged. Connected to `QPushButton::clicked` of `track_browse_button`.
    void browse_track();
    /// Asks the operator for a log file and puts the chosen path into `replay_path_edit`.
    ///
    /// The file dialog starts in the directory of the current path, or in the documents
    /// folder when the field is empty, and offers `*.log`, `*.nmea` and `*.txt` files.
    /// Cancelling leaves the field unchanged. Connected to `QPushButton::clicked` of
    /// `replay_browse_button`.
    void browse_log();

    /// The *Track* group, enabled only in track mode; owned by the page.
    QGroupBox* track_box_;
    /// The *Log replay* group, enabled only in replay mode; owned by the page.
    QGroupBox* replay_box_;
    /// The *Drift around the initial values* group, enabled only in delta mode; owned by the
    /// page.
    QGroupBox* drift_box_;
};

}  // namespace nmeasim::app
