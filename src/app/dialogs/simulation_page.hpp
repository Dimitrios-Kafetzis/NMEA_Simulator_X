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
class SimulationPage : public QWidget {
    Q_OBJECT

public:
    explicit SimulationPage(QWidget* parent = nullptr);

    void load(const io::Profile& profile);
    void store(io::Profile& profile) const;
    /// A problem with the mode settings, for example a missing track file; empty when valid.
    [[nodiscard]] QString validate() const;

    // Widgets are public so that tests can drive them like an operator would.
    /// Delta simulation, track or replay, in `io::SimulationMode` order.
    QComboBox* mode_combo;
    QLineEdit* track_path_edit;
    QPushButton* track_browse_button;
    QDoubleSpinBox* track_speed_spin;
    QCheckBox* track_timestamps_check;
    QCheckBox* track_loop_check;
    QLineEdit* replay_path_edit;
    QPushButton* replay_browse_button;
    QCheckBox* replay_loop_check;
    QSpinBox* replay_interval_spin;

    QLineEdit* name_edit;
    QSpinBox* tick_spin;
    QCheckBox* fixed_start_check;
    QDateTimeEdit* start_time_edit;
    QSpinBox* random_seed_spin;

    QDoubleSpinBox* latitude_spin;
    QDoubleSpinBox* longitude_spin;
    QDoubleSpinBox* altitude_spin;
    QDoubleSpinBox* heading_spin;
    QDoubleSpinBox* speed_spin;
    QDoubleSpinBox* variation_spin;
    QDoubleSpinBox* deviation_spin;
    QDoubleSpinBox* depth_spin;
    QDoubleSpinBox* transducer_offset_spin;
    QDoubleSpinBox* water_temperature_spin;
    QDoubleSpinBox* wind_direction_spin;
    QDoubleSpinBox* wind_speed_spin;

    QCheckBox* fix_check;
    QComboBox* quality_combo;
    QSpinBox* satellites_in_use_spin;
    QSpinBox* satellites_in_view_spin;
    QDoubleSpinBox* hdop_spin;
    QDoubleSpinBox* pdop_spin;
    QDoubleSpinBox* vdop_spin;
    QDoubleSpinBox* geoid_spin;

    /// Amplitude and step for heading, speed, depth, water temperature, wind direction and
    /// wind speed, in that order.
    std::array<QDoubleSpinBox*, 6> amplitude_spins{};
    std::array<QDoubleSpinBox*, 6> step_spins{};

    QDoubleSpinBox* turn_rate_spin;
    QDoubleSpinBox* max_rudder_spin;

private:
    void update_mode_widgets();
    void browse_track();
    void browse_log();

    QGroupBox* track_box_;
    QGroupBox* replay_box_;
    QGroupBox* drift_box_;
};

}  // namespace nmeasim::app
