#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>

#include <QCheckBox>
#include <QSpinBox>
#include <QWidget>

#include <map>

namespace nmeasim::app {

class InstrumentTile;

/// Formats a position as degrees and decimal minutes with hemisphere letters.
[[nodiscard]] QString format_position(const core::geo::Position& position);

/// The instrument grid. Displays the vessel state and lets the operator override values.
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

signals:
    void override_changed(nmeasim::core::simulation::Parameter parameter, bool active,
                          double value);
    void fix_changed(bool has_fix);
    void satellites_changed(int in_use);

private:
    InstrumentTile* add_tile(const QString& title, const QString& unit, int row, int column);
    InstrumentTile* add_controllable(core::simulation::Parameter parameter, const QString& title,
                                     const QString& unit, int row, int column, double minimum,
                                     double maximum, double step, int decimals);

    std::map<core::simulation::Parameter, InstrumentTile*> controls_;
    InstrumentTile* position_tile_;
    InstrumentTile* time_tile_;
    InstrumentTile* course_tile_;
    InstrumentTile* rate_of_turn_tile_;
    InstrumentTile* apparent_wind_tile_;
    InstrumentTile* gnss_tile_;
    QCheckBox* fix_check_;
    QSpinBox* satellites_spin_;
    bool suppress_signals_{false};
};

}  // namespace nmeasim::app
