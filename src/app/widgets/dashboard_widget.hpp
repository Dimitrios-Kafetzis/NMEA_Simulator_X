// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The central dashboard of the main window: the compass and wind dials, the instrument
/// tiles with their override controls and one tile per engine.
///
/// `DashboardWidget` displays a `core::model::VesselState` and reports what the operator
/// edits through signals; the main window applies them to the `core::simulation::DeltaSource`
/// in delta mode. `EngineTile` is the editable tile of one engine and `format_position` the
/// position format of the readouts.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QWidget>

#include <map>
#include <vector>

namespace nmeasim::app {

class CompassDial;
class InstrumentTile;
class WindDial;

/// One engine on the dashboard: its label, a running switch and editable revolutions and
/// coolant temperature.
///
/// The controls are public so that tests can drive them. Every child widget is owned by the
/// tile. The object name is `engine_tile`.
class EngineTile : public QFrame {
    Q_OBJECT

public:
    /// Creates a tile for one engine, not running, at 0 rpm and 0 °C until `show_engine`.
    ///
    /// @param label Engine label shown as the caption.
    /// @param parent Parent widget, which owns the tile; may be null, in which case the caller
    ///   owns it.
    explicit EngineTile(const QString& label, QWidget* parent = nullptr);

    /// Shows an engine without emitting `changed`.
    ///
    /// A spin box that has the keyboard focus keeps its value, so that a periodic refresh does
    /// not overwrite what the operator is typing.
    ///
    /// @param engine The engine to show; values outside the spin box ranges are clamped.
    void show_engine(const core::model::Engine& engine);
    /// Returns the engine as the controls describe it.
    ///
    /// @return The label, running state, revolutions and coolant temperature shown.
    [[nodiscard]] core::model::Engine engine() const;
    /// Enables or disables the running switch and the spin boxes.
    ///
    /// @param editable False to grey them out, as in track and replay mode.
    void set_editable(bool editable);

    /// Caption with the engine label.
    QLabel* label;
    /// *Running* switch.
    QCheckBox* running_check;
    /// Shaft revolutions in rpm, [0, 99999.9] without decimals, in steps of 100.
    QDoubleSpinBox* rpm_spin;
    /// Coolant temperature in degrees Celsius, [-50, 500] with one decimal.
    QDoubleSpinBox* temperature_spin;

signals:
    /// Emitted when the operator changes the running switch, the revolutions or the
    /// temperature.
    ///
    /// Not emitted from `show_engine`. A typed spin box value is reported when editing
    /// finishes, not on every keystroke.
    void changed();

private:
    /// True inside `show_engine`, so that programmatic changes emit no `changed`.
    bool suppress_signals_{false};
};

/// Formats a position as degrees and decimal minutes with hemisphere letters.
///
/// Latitude has two degree digits and longitude three, minutes have two integer digits and
/// three decimals, and the two are separated by two spaces, for example
/// `37°59.028'N  023°43.650'E`.
///
/// @param position Position in degrees, latitude positive north, longitude positive east.
/// @return The formatted position.
[[nodiscard]] QString format_position(const core::geo::Position& position);

/// The instruments: a compass rose and a wind dial with the position, time and GNSS tiles on
/// top, the grid of digital tiles below. Displays the vessel state and lets the operator
/// override values. The whole dashboard scrolls when the window is too small for it.
///
/// Tiles with an override control, their units and ranges, and the parameter each drives:
///
/// - *Heading*, °T, [0, 359.9], `core::simulation::Parameter::HeadingTrue`;
/// - *Speed over ground*, kn, [0, 999.9], `Parameter::SpeedOverGround`;
/// - *Speed through water*, kn, [0, 999.9], `Parameter::SpeedThroughWater`;
/// - *Rudder*, degrees positive to starboard, within the profile's rudder limit (see
///   `set_rudder_limit`, [-35, 35] by default), `Parameter::RudderAngle`;
/// - *Depth*, m below the transducer, [0, 99999.9], `Parameter::Depth`;
/// - *Water temperature*, °C, [-5, 60], `Parameter::WaterTemperature`;
/// - *Altitude*, m, [-500, 20000], `Parameter::Altitude`;
/// - *True wind direction*, °T the wind comes from, [0, 359.9], `Parameter::WindDirectionTrue`;
/// - *True wind speed*, kn, [0, 200], `Parameter::WindSpeedTrue`.
///
/// The display-only tiles are *Position*, *Time (UTC)*, *Course over ground* (°T), *Rate of
/// turn* (°/min, positive to starboard), *Apparent wind* (angle off the bow to port or
/// starboard, as `format_wind_angle` writes it, and speed) and *Destination*. The *GNSS* tile
/// carries a *Fix* check box and a *Satellites* spin box in [0, 12]; the *Engines* group holds one
/// `EngineTile` per engine. Every child widget is owned by the dashboard through Qt parents.
///
/// @see docs/reference/desktop-app.md, sections "Dashboard instruments" and "Dashboard tiles".
class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    /// Builds the dashboard with no engines, steering mode off and overrides enabled.
    ///
    /// @param parent Parent widget, which owns the dashboard; may be null, in which case the
    ///   caller owns it.
    explicit DashboardWidget(QWidget* parent = nullptr);

    /// Refreshes every tile from a state snapshot. Tiles with an active override keep their
    /// control value.
    ///
    /// The readouts, the dials, the *Fix* check box and the engine tiles follow the state; the
    /// *Satellites* spin box does too unless it has the keyboard focus. The engine tiles are
    /// rebuilt when the number or the labels of the engines changed. With a destination the
    /// compass shows the bearing to it and the *Destination* tile its name, bearing, distance
    /// and cross-track error in nautical miles with the side to steer. Emits no signal.
    ///
    /// @param state The state to show; not kept.
    void update_state(const core::model::VesselState& state);
    /// Reflects the current overrides of a source in the controls.
    ///
    /// Each override control is ticked when its parameter is overridden and shows the pinned
    /// value, or the current simulated value when it is not. Emits no signal. The main window
    /// calls it after keyboard nudges, steering changes and loading a profile.
    ///
    /// @param source The delta source whose overrides to show; not kept.
    void sync_overrides(const core::simulation::DeltaSource& source);
    /// Switches the controls between heading and rudder steering.
    ///
    /// In steering mode the *Rudder* override control is enabled and the *Heading* one
    /// disabled; outside it the reverse. Neither is enabled while overrides are disabled.
    ///
    /// @param enabled True for steering mode.
    void set_steering_mode(bool enabled);
    /// Enables or disables every override control, for example while a track or a log
    /// drives the vessel and overrides would have no effect.
    ///
    /// Covers the override controls, the *Fix* and *Satellites* controls and the engine tiles,
    /// and then applies the steering mode again.
    ///
    /// @param enabled False to grey out every control.
    void set_overrides_enabled(bool enabled);
    /// Limits the *Rudder* override to the rudder limit of a profile.
    ///
    /// The simulation clamps the rudder to the same limit, so the control never holds an
    /// angle the simulation does not use; its tooltip names the limit. The main window calls
    /// it when it applies a profile.
    ///
    /// @param max_rudder_angle_deg Largest rudder angle either side, in degrees, as
    ///   `core::simulation::DeltaConfig::max_rudder_angle_deg`; positive.
    void set_rudder_limit(double max_rudder_angle_deg);
    /// Returns whether the override controls are enabled.
    ///
    /// @return The value last passed to `set_overrides_enabled`, true initially.
    [[nodiscard]] bool overrides_enabled() const noexcept { return overrides_enabled_; }

    /// Returns the number of engine tiles.
    ///
    /// @return One per engine of the last state shown.
    [[nodiscard]] int engine_count() const noexcept { return static_cast<int>(engines_.size()); }
    /// Returns one engine tile.
    ///
    /// @param index Engine index, from 0.
    /// @return The tile, owned by the dashboard and valid until the engines are rebuilt by
    ///   `update_state`; null when `index` is out of range.
    [[nodiscard]] EngineTile* engine_tile(int index) const;
    /// Returns the text of the *Destination* tile.
    ///
    /// @return The destination summary, or `None` (translated) without a destination.
    [[nodiscard]] QString destination_text() const;
    /// Returns the text of the *Apparent wind* tile.
    ///
    /// @return The angle off the bow, as `format_wind_angle` writes it, and the speed, for
    ///   example `104°P at 8.7 kn`; `--` before the first `update_state`.
    [[nodiscard]] QString apparent_wind_text() const;
    /// Returns the tile with the override control of a parameter.
    ///
    /// @param parameter Parameter the tile drives.
    /// @return The tile, owned by the dashboard; never null, as every
    ///   `core::simulation::Parameter` has one.
    [[nodiscard]] InstrumentTile* control(core::simulation::Parameter parameter) const;
    /// Returns the compass rose.
    ///
    /// @return The dial, owned by the dashboard; never null.
    [[nodiscard]] CompassDial* compass_dial() const noexcept { return compass_; }
    /// Returns the wind dial.
    ///
    /// @return The dial, owned by the dashboard; never null.
    [[nodiscard]] WindDial* wind_dial() const noexcept { return wind_; }

signals:
    /// Emitted when the operator ticks or unticks the override of a tile, or changes its value
    /// while the override is active.
    ///
    /// The main window calls `core::simulation::DeltaSource::set_override` when `active` is
    /// true and `core::simulation::DeltaSource::clear_override` otherwise. Not emitted from
    /// `sync_overrides` or `update_state`.
    ///
    /// @param parameter The parameter the tile controls.
    /// @param active True to pin the parameter, false to release it.
    /// @param value Value of the control, in the parameter's unit.
    void override_changed(nmeasim::core::simulation::Parameter parameter, bool active,
                          double value);
    /// Emitted when the operator ticks or unticks the *Fix* check box.
    ///
    /// The main window passes it to `core::simulation::DeltaSource::set_fix`. Not emitted from
    /// `update_state`.
    ///
    /// @param has_fix True when the box is ticked.
    void fix_changed(bool has_fix);
    /// Emitted when the operator changes the *Satellites* spin box.
    ///
    /// The main window passes it to `core::simulation::DeltaSource::set_satellites`, keeping
    /// the satellites in view. Not emitted from `update_state`.
    ///
    /// @param in_use Satellites used in the fix, in [0, 12].
    void satellites_changed(int in_use);
    /// The operator changed an engine on its tile.
    ///
    /// Emitted whenever an `EngineTile` emits `EngineTile::changed`. The main window passes
    /// it to `core::simulation::DeltaSource::set_engine`.
    ///
    /// @param index Engine index, from 0.
    /// @param engine The engine as the tile now describes it.
    void engine_changed(int index, const nmeasim::core::model::Engine& engine);

private:
    /// Creates a display-only tile and places it in the grid.
    ///
    /// @param title Caption of the tile.
    /// @param unit Unit next to the readout; empty for none.
    /// @param row Grid row, from 0.
    /// @param column Grid column, from 0 to 2.
    /// @return The tile, owned by the dashboard's content widget.
    InstrumentTile* add_tile(const QString& title, const QString& unit, int row, int column);
    /// A framed panel with a caption around a dial.
    ///
    /// @param title Caption above the dial.
    /// @param dial The dial; reparented to the panel and given a minimum size of 200 by 200
    ///   pixels.
    /// @return The panel, owned by the dashboard's content widget.
    QFrame* dial_panel(const QString& title, QWidget* dial);
    /// Rebuilds the engine tiles when the number or the labels of the engines change.
    ///
    /// Old tiles are deleted later from the event loop; new ones start editable when overrides
    /// are enabled.
    ///
    /// @param engines The engines of the state being shown.
    void sync_engines(const std::vector<core::model::Engine>& engines);
    /// Creates a tile with an override control, connects it to `override_changed` and
    /// registers it in `controls_`.
    ///
    /// @param parameter The parameter the control drives.
    /// @param title Caption of the tile.
    /// @param unit Unit next to the readout.
    /// @param row Grid row, from 0.
    /// @param column Grid column, from 0 to 2.
    /// @param minimum Smallest override value, in the unit.
    /// @param maximum Largest override value, in the unit.
    /// @param step Spin box step, in the unit.
    /// @param decimals Decimals of the spin box.
    /// @return The tile, owned by the dashboard's content widget.
    InstrumentTile* add_controllable(core::simulation::Parameter parameter, const QString& title,
                                     const QString& unit, int row, int column, double minimum,
                                     double maximum, double step, int decimals);

    /// Widget inside the scroll area that holds everything else; owned by the scroll area.
    QWidget* content_;
    /// Grid of the digital tiles, three columns wide; owned by the content layout.
    QGridLayout* grid_;
    /// Compass rose; owned by its dial panel.
    CompassDial* compass_;
    /// Wind dial; owned by its dial panel.
    WindDial* wind_;
    /// Tiles with an override control, one per `core::simulation::Parameter`.
    std::map<core::simulation::Parameter, InstrumentTile*> controls_;
    /// Position in degrees and decimal minutes, on two lines.
    InstrumentTile* position_tile_;
    /// Simulated clock in UTC.
    InstrumentTile* time_tile_;
    /// Course over ground, °T.
    InstrumentTile* course_tile_;
    /// Rate of turn, °/min, positive to starboard.
    InstrumentTile* rate_of_turn_tile_;
    /// Apparent wind angle off the bow, to port or starboard, and speed in knots.
    InstrumentTile* apparent_wind_tile_;
    /// Destination summary, or `None` without a destination; spans the grid's width.
    InstrumentTile* destination_tile_;
    /// Fix state, satellites in use and HDOP, with the *Fix* and *Satellites* controls.
    InstrumentTile* gnss_tile_;
    /// Layout of the *Engines* group, ending in a stretch after the tiles.
    QHBoxLayout* engines_row_;
    /// Engine tiles in engine order, owned by the *Engines* group.
    std::vector<EngineTile*> engines_;
    /// *Fix* check box on the GNSS tile.
    QCheckBox* fix_check_;
    /// *Satellites* spin box on the GNSS tile, [0, 12].
    QSpinBox* satellites_spin_;
    /// True inside `update_state`, so that programmatic changes emit no signal.
    bool suppress_signals_{false};
    /// Whether the controls are enabled; see `set_overrides_enabled`.
    bool overrides_enabled_{true};
    /// Whether steering mode is on; see `set_steering_mode`.
    bool steering_mode_{false};
};

}  // namespace nmeasim::app
