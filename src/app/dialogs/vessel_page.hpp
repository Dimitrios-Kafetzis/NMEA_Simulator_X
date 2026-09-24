// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The *Vessel* tab of the settings dialog, `VesselPage`.
///
/// The page edits the engines and the AIS static data of the seed vessel, the
/// `simulation.seed.engines` and `simulation.seed.ais` keys of a profile.
/// `docs/reference/desktop-app.md` describes the tab and `docs/reference/ais.md` the AIS
/// fields.

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
///
/// `SettingsDialog` calls `load` once with its copy of the profile, `validate` when the
/// operator presses *OK*, and `store` into the same copy when every page is valid. The widgets
/// are the only state of the page. The ranges of the numeric AIS widgets stay within those of
/// the fields of ITU-R M.1371-5 messages 1 and 5, so the encoder never has to clamp a number the
/// page stores; `load` clamps a profile value outside a widget's range to the nearest limit.
///
/// Every widget, cell widget and table item is owned through Qt parents by the page, and the
/// public widget pointers are never null and stay valid for the lifetime of the page. The
/// class declares no signals or slots of its own.
///
/// @see SettingsDialog
/// @see ITU-R M.1371-5, Annex 8, messages 1 and 5.
class VesselPage : public QWidget {
    Q_OBJECT

public:
    /// Columns of `engines_table`.
    enum EngineColumn {
        Label = 0,         ///< Editable item with the engine label, `core::model::Engine::label`.
        Running,           ///< Checkable item, `core::model::Engine::running`.
        Rpm,               ///< `QDoubleSpinBox` cell widget with the shaft revolutions per minute.
        Temperature,       ///< `QDoubleSpinBox` cell widget with the coolant temperature in °C.
        EngineColumnCount  ///< Number of columns, not a column.
    };

    /// Creates the page with an empty engine table and the AIS widgets at their minimum or
    /// first item.
    ///
    /// The widgets show no profile until `load` is called. Connects the *Add engine* and
    /// *Remove* buttons.
    ///
    /// @param parent The owner of the page, normally the `SettingsDialog`; when null, the
    ///     caller owns the page.
    explicit VesselPage(QWidget* parent = nullptr);

    /// Shows the engines and AIS static data of a profile's seed in the widgets.
    ///
    /// The engine table is rebuilt from `core::model::VesselState::engines`, in order, and
    /// every AIS widget is set from the field named at its member. A position report type
    /// other than 1, 2 or 3 is shown as type 1, the type the encoder sends for it.
    ///
    /// @param profile The profile to show; only read during the call.
    void load(const io::Profile& profile);
    /// Writes the engines and AIS static data into the seed of a profile.
    ///
    /// Replaces the seed engines with the rows of the engine table as `engine_at` returns
    /// them, and writes every field of the seed's `core::model::AisStatic`. The vessel name,
    /// call sign and voyage destination are trimmed and upper-cased. The rest of `profile` is
    /// left as it is.
    ///
    /// @param profile The profile to update, normally the dialog's copy.
    /// @note `store` does not validate the MMSI. `SettingsDialog` calls it only after
    ///     `validate` returned an empty string.
    void store(io::Profile& profile) const;
    /// Checks the AIS data.
    ///
    /// Only the MMSI is checked: it must be positive. The upper limit of nine digits is
    /// enforced by `mmsi_spin` itself.
    ///
    /// @return A message for the operator naming the MMSI when it is 0; an empty string when
    ///     the data is valid.
    [[nodiscard]] QString validate() const;

    /// Appends an engine row and selects it.
    ///
    /// The new engine is labelled `Engine n`, where `n` is the new row count, and runs at
    /// 1500 rpm with a coolant temperature of 80 °C. Connected to the *Add engine* button.
    void add_engine();
    /// Removes the current row of the engine table; does nothing when there is none.
    ///
    /// Connected to the *Remove* button.
    void remove_current_engine();
    /// Returns the number of rows in the engine table.
    ///
    /// @return The number of engines, running or not.
    [[nodiscard]] int engine_count() const;
    /// Returns the engine of a row as `store` writes it.
    ///
    /// The label is trimmed; an empty label becomes `Engine n`, where `n` is `row` plus one.
    ///
    /// @param row The row index in the engine table.
    /// @return The engine with its label, running flag, revolutions and coolant temperature.
    /// @pre `row` is in [0, `engine_count()`).
    [[nodiscard]] core::model::Engine engine_at(int row) const;
    /// Replaces the values shown in an engine row.
    ///
    /// The label is shown as given; the revolutions and temperature are clamped to the ranges
    /// of their spin boxes.
    ///
    /// @param row The row index in the engine table.
    /// @param engine The values to show; only read during the call.
    /// @pre `row` is in [0, `engine_count()`).
    void set_engine(int row, const core::model::Engine& engine);

    /// One row per engine, `core::model::VesselState::engines` in order, with the columns of
    /// `EngineColumn`.
    ///
    /// The first row is engine 1 in RPM and `ENGINE#0` in XDR, and the Signal K id of an
    /// engine is derived from its label. The revolutions cell accepts [0, 99999.9] and shows
    /// whole revolutions per minute; the temperature cell accepts [-50, 500] °C with one
    /// decimal.
    QTableWidget* engines_table;
    /// The *Add engine* button, which calls `add_engine`.
    QPushButton* add_engine_button;
    /// The *Remove* button, which calls `remove_current_engine`.
    QPushButton* remove_engine_button;

    /// Maritime Mobile Service Identity, `core::model::AisStatic::mmsi`; [0, 999999999], at
    /// most nine digits. 0 is accepted by the widget but refused by `validate`.
    QSpinBox* mmsi_spin;
    /// IMO number, `core::model::AisStatic::imo_number`; [0, 999999999], shown as *None*
    /// when 0, the value for a vessel without one.
    QSpinBox* imo_spin;
    /// Vessel name, `core::model::AisStatic::name`; at most 20 characters, the length of the
    /// name field of message 5. Trimmed and upper-cased by `store`.
    QLineEdit* name_edit;
    /// Call sign, `core::model::AisStatic::call_sign`; at most 7 characters, the length of the
    /// call sign field of message 5. Trimmed and upper-cased by `store`.
    QLineEdit* call_sign_edit;
    /// Type of ship and cargo code, `core::model::AisStatic::ship_type`; [0, 255], the range
    /// of the 8-bit field of message 5.
    QSpinBox* ship_type_spin;
    /// Distance from the GNSS antenna to the bow, `core::model::AisStatic::dimension_to_bow_m`;
    /// [0, 511] whole metres, the range of the 9-bit field of message 5.
    QDoubleSpinBox* to_bow_spin;
    /// Distance from the GNSS antenna to the stern,
    /// `core::model::AisStatic::dimension_to_stern_m`; [0, 511] whole metres, the range of the
    /// 9-bit field of message 5.
    QDoubleSpinBox* to_stern_spin;
    /// Distance from the GNSS antenna to port, `core::model::AisStatic::dimension_to_port_m`;
    /// [0, 63] whole metres, the range of the 6-bit field of message 5.
    QDoubleSpinBox* to_port_spin;
    /// Distance from the GNSS antenna to starboard,
    /// `core::model::AisStatic::dimension_to_starboard_m`; [0, 63] whole metres, the range of
    /// the 6-bit field of message 5.
    QDoubleSpinBox* to_starboard_spin;
    /// Maximum present static draught, `core::model::AisStatic::draught_m`; [0, 25.5] m with
    /// one decimal, the tenths of a metre that message 5 carries.
    QDoubleSpinBox* draught_spin;
    /// Voyage destination, `core::model::AisStatic::destination`; at most 20 characters, the
    /// length of the destination field of message 5, and empty for none. Trimmed and
    /// upper-cased by `store`.
    QLineEdit* ais_destination_edit;
    /// Navigational status code, `core::model::AisStatic::navigation_status`; [0, 15], the
    /// range of the 4-bit field of the position report. 0 is under way using engine, 1 at
    /// anchor, 5 moored and 8 under way sailing.
    QSpinBox* navigation_status_spin;
    /// Position report message type 1, 2 or 3, as index 0 to 2.
    ///
    /// Edits `core::model::AisStatic::position_report_type`: type 1 is scheduled, 2 assigned
    /// and 3 sent in response to an interrogation.
    QComboBox* report_type_combo;

private:
    /// Appends a row that shows an engine, without selecting it.
    ///
    /// Creates the label and running items and the revolutions and temperature spin boxes,
    /// all owned by `engines_table`. Used by `load` and `add_engine`.
    ///
    /// @param engine The values to show; only read during the call.
    void append_engine_row(const core::model::Engine& engine);
};

}  // namespace nmeasim::app
