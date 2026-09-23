#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

namespace nmeasim::app {

/// Settings tab for the engines and the AIS static data of the own vessel.
class VesselPage : public QWidget {
    Q_OBJECT

public:
    enum EngineColumn { Label = 0, Running, Rpm, Temperature, EngineColumnCount };

    explicit VesselPage(QWidget* parent = nullptr);

    void load(const io::Profile& profile);
    void store(io::Profile& profile) const;
    /// A problem with the AIS data; empty when valid.
    [[nodiscard]] QString validate() const;

    void add_engine();
    void remove_current_engine();
    [[nodiscard]] int engine_count() const;
    [[nodiscard]] core::model::Engine engine_at(int row) const;
    void set_engine(int row, const core::model::Engine& engine);

    QTableWidget* engines_table;
    QPushButton* add_engine_button;
    QPushButton* remove_engine_button;

    QSpinBox* mmsi_spin;
    QSpinBox* imo_spin;
    QLineEdit* name_edit;
    QLineEdit* call_sign_edit;
    QSpinBox* ship_type_spin;
    QDoubleSpinBox* to_bow_spin;
    QDoubleSpinBox* to_stern_spin;
    QDoubleSpinBox* to_port_spin;
    QDoubleSpinBox* to_starboard_spin;
    QDoubleSpinBox* draught_spin;
    QLineEdit* ais_destination_edit;
    QSpinBox* navigation_status_spin;
    /// Position report message type 1, 2 or 3, as index 0 to 2.
    QComboBox* report_type_combo;

private:
    void append_engine_row(const core::model::Engine& engine);
};

}  // namespace nmeasim::app
