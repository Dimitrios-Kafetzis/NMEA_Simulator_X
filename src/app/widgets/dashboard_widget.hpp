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
class EngineTile : public QFrame {
    Q_OBJECT

public:
    explicit EngineTile(const QString& label, QWidget* parent = nullptr);

    /// Shows an engine without emitting `changed`.
    void show_engine(const core::model::Engine& engine);
    [[nodiscard]] core::model::Engine engine() const;
    void set_editable(bool editable);

    QLabel* label;
    QCheckBox* running_check;
    QDoubleSpinBox* rpm_spin;
    QDoubleSpinBox* temperature_spin;

signals:
    void changed();

private:
    bool suppress_signals_{false};
};

/// Formats a position as degrees and decimal minutes with hemisphere letters.
[[nodiscard]] QString format_position(const core::geo::Position& position);

/// The instruments: a compass rose and a wind dial with the position, time and GNSS tiles on
/// top, the grid of digital tiles below. Displays the vessel state and lets the operator
/// override values. The whole dashboard scrolls when the window is too small for it.
class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    /// Refreshes every tile from a state snapshot. Tiles with an active override keep their
    /// control value.
    void update_state(const core::model::VesselState& state);
    /// Reflects the current overrides of a source in the controls.
    void sync_overrides(const core::simulation::DeltaSource& source);
    void set_steering_mode(bool enabled);
    /// Enables or disables every override control, for example while a track or a log
    /// drives the vessel and overrides would have no effect.
    void set_overrides_enabled(bool enabled);
    [[nodiscard]] bool overrides_enabled() const noexcept { return overrides_enabled_; }

    [[nodiscard]] int engine_count() const noexcept { return static_cast<int>(engines_.size()); }
    [[nodiscard]] EngineTile* engine_tile(int index) const;
    [[nodiscard]] QString destination_text() const;
    [[nodiscard]] CompassDial* compass_dial() const noexcept { return compass_; }
    [[nodiscard]] WindDial* wind_dial() const noexcept { return wind_; }

signals:
    void override_changed(nmeasim::core::simulation::Parameter parameter, bool active,
                          double value);
    void fix_changed(bool has_fix);
    void satellites_changed(int in_use);
    /// The operator changed an engine on its tile.
    void engine_changed(int index, const nmeasim::core::model::Engine& engine);

private:
    InstrumentTile* add_tile(const QString& title, const QString& unit, int row, int column);
    /// A framed panel with a caption around a dial.
    QFrame* dial_panel(const QString& title, QWidget* dial);
    /// Rebuilds the engine tiles when the number or the labels of the engines change.
    void sync_engines(const std::vector<core::model::Engine>& engines);
    InstrumentTile* add_controllable(core::simulation::Parameter parameter, const QString& title,
                                     const QString& unit, int row, int column, double minimum,
                                     double maximum, double step, int decimals);

    QWidget* content_;
    QGridLayout* grid_;
    CompassDial* compass_;
    WindDial* wind_;
    std::map<core::simulation::Parameter, InstrumentTile*> controls_;
    InstrumentTile* position_tile_;
    InstrumentTile* time_tile_;
    InstrumentTile* course_tile_;
    InstrumentTile* rate_of_turn_tile_;
    InstrumentTile* apparent_wind_tile_;
    InstrumentTile* destination_tile_;
    InstrumentTile* gnss_tile_;
    QHBoxLayout* engines_row_;
    std::vector<EngineTile*> engines_;
    QCheckBox* fix_check_;
    QSpinBox* satellites_spin_;
    bool suppress_signals_{false};
    bool overrides_enabled_{true};
    bool steering_mode_{false};
};

}  // namespace nmeasim::app
