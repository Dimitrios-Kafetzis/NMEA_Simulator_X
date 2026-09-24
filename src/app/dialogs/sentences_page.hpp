// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The *Sentences* tab of the settings dialog, `SentencesPage`.
///
/// The page edits the `sentences` object of a profile: the enabled flag, talker and period of
/// every NMEA 0183 sentence in the registry, the position precision, and the operator's custom
/// sentences. `docs/reference/desktop-app.md` describes the tab and
/// `docs/reference/nmea0183-sentences.md` the sentences.

#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

namespace nmeasim::app {

/// Settings tab that lists every sentence in the registry with its enabled flag, talker and
/// period, plus the position precision.
///
/// Below the registry table, a second table holds the custom sentences. `SettingsDialog`
/// calls `load` once with its copy of the profile, `validate` when the operator presses
/// *OK*, and `store` into the same copy when every page is valid. The widgets are the only
/// state of the page.
///
/// The registry table has one row per descriptor of
/// `core::nmea0183::SentenceRegistry::standard`, in catalogue order, created once by the
/// constructor; its rows are never added or removed. The *Talker* and *Period* cells hold a
/// `QLineEdit` and a `QSpinBox` as cell widgets, and the *Id*, *Description* and *Group* cells
/// are read-only. The custom table has one row per custom sentence, with a `QLineEdit` in the
/// *Id* and *Sentence* cells and a `QSpinBox` in the *Period* cell.
///
/// Every widget, cell widget and table item is owned through Qt parents by the page, and the
/// public widget pointers are never null and stay valid for the lifetime of the page. The
/// class declares no signals or slots of its own.
///
/// @see SettingsDialog
class SentencesPage : public QWidget {
    Q_OBJECT

public:
    /// Columns of `table`, the registry sentences.
    enum Column {
        Enabled = 0,  ///< Checkable item: whether the sentence is sent.
        Id,           ///< Read-only registry id, such as `RMC` or `MWV-T`.
        Description,  ///< Read-only one-line summary of the sentence.
        Group,        ///< Read-only name of the sentence group, such as `GNSS`.
        Talker,       ///< `QLineEdit` cell widget with the talker; empty uses the default.
        Period,       ///< `QSpinBox` cell widget with the period in milliseconds.
        ColumnCount   ///< Number of columns, not a column.
    };
    /// Columns of `custom_table`, the custom sentences.
    enum CustomColumn {
        CustomEnabled = 0,  ///< Checkable item: whether the sentence is sent.
        CustomId,           ///< `QLineEdit` cell widget with the id; empty gives `CUSTOM-n`.
        CustomBody,         ///< `QLineEdit` cell widget with the sentence without checksum.
        CustomPeriod,       ///< `QSpinBox` cell widget with the period in milliseconds.
        CustomColumnCount   ///< Number of columns, not a column.
    };

    /// Creates the page with one row per registry sentence, every row unticked, and an empty
    /// custom table.
    ///
    /// The rows show no profile until `load` is called. Connects the *Enable all*, *Disable
    /// all*, *Reset to defaults*, *Add custom sentence* and *Remove* buttons.
    ///
    /// @param parent The owner of the page, normally the `SettingsDialog`; when null, the
    ///     caller owns the page.
    explicit SentencesPage(QWidget* parent = nullptr);

    /// Shows the sentence settings of a profile in the widgets.
    ///
    /// The custom table is rebuilt from `io::Profile::custom_sentences`, in order. Each
    /// registry row shows the setting that `io::Profile::make_scheduler` gives the sentence:
    /// the profile's setting when it has one, the registry default otherwise. The talker is
    /// shown upper-cased, and empty, with the default talker as placeholder, when the setting
    /// has none. A period outside the range of
    /// the period spin box, which a profile file can hold, is clamped to it.
    ///
    /// @param profile The profile to show; only read during the call.
    void load(const io::Profile& profile);
    /// Writes the widget values into the sentence fields of a profile.
    ///
    /// Writes `io::Profile::encoder` position decimals, replaces
    /// `io::Profile::custom_sentences` with the rows of the custom table as `custom_at`
    /// returns them, and replaces `io::Profile::sentences` with the registry rows that differ
    /// from the registry defaults. Talkers are upper-cased, and a talker equal to the default
    /// is stored as empty, so it does not make a row differ. Writing only the differences
    /// keeps a saved profile small and lets it follow registry changes in later versions.
    ///
    /// @param profile The profile to update, normally the dialog's copy.
    /// @note `store` does not validate the custom sentences. `SettingsDialog` calls it only
    ///     after `validate` returned an empty string.
    void store(io::Profile& profile) const;
    /// Checks the custom sentences, row by row.
    ///
    /// A row is refused when `core::simulation::validate_custom_sentence` refuses its body or
    /// when its id, as `custom_at` returns it, is a registry id. Duplicate custom ids are not
    /// checked.
    ///
    /// @return For the first refused row, `Custom sentence n:` followed by the reason, where
    ///     `n` is the 1-based row number; an empty string when every row is valid.
    [[nodiscard]] QString validate() const;

    /// Returns the row of a registry id in the table.
    ///
    /// @param id The registry id, such as `RMC` or `MWV-T`; compared case-sensitively.
    /// @return The row index, or -1 when no row has that id.
    [[nodiscard]] int row_of(const QString& id) const;
    /// Ticks or unticks the enabled flag of a registry row.
    ///
    /// @param row The row index, as `row_of` returns it.
    /// @param enabled Whether the sentence is sent.
    /// @pre `row` is in [0, `table->rowCount()`).
    void set_enabled(int row, bool enabled);
    /// Returns whether the enabled flag of a registry row is ticked.
    ///
    /// @param row The row index, as `row_of` returns it.
    /// @return True when the sentence is enabled.
    /// @pre `row` is in [0, `table->rowCount()`).
    [[nodiscard]] bool is_enabled(int row) const;

    /// Appends a custom sentence row and selects it.
    ///
    /// The new row is enabled, has an empty id with `CUSTOM-n` as placeholder, where `n` is
    /// the new row count, and a period of 1000 ms. The id field accepts at most 12 letters,
    /// digits and hyphens; the sentence field accepts at most 80 characters, and `validate`
    /// applies the exact length limit to the framed sentence. The period spin box accepts [50,
    /// 3600000] ms, the range the profile accepts for custom sentences. The *Add custom sentence*
    /// button calls it with an empty body.
    ///
    /// @param body The sentence text to start with, unchecked; empty for none.
    void add_custom(const QString& body = {});
    /// Removes the current row of the custom table; does nothing when there is none.
    ///
    /// Connected to the *Remove* button.
    void remove_current_custom();
    /// Returns the number of rows in the custom table.
    ///
    /// @return The number of custom sentences, enabled or not.
    [[nodiscard]] int custom_count() const;
    /// Returns the custom sentence of a row as `store` writes it.
    ///
    /// The id is trimmed and upper-cased. The body is taken as typed, surrounding white
    /// space included; `core::simulation::validate_custom_sentence` and the scheduler ignore
    /// it.
    ///
    /// @param row The row index in the custom table.
    /// @return The sentence with its enabled flag, id, body and period.
    /// @pre `row` is in [0, `custom_count()`).
    [[nodiscard]] core::simulation::CustomSentence custom_at(int row) const;

    /// Number of decimals of the minutes in latitudes and longitudes,
    /// `core::nmea0183::EncoderOptions::position_decimals`; [2, 8], the range the profile
    /// accepts.
    ///
    /// The encoders lower it when a sentence would exceed 82 characters, as its tooltip says.
    QSpinBox* position_decimals_spin;
    /// The registry sentences, one row per descriptor in catalogue order, with the columns of
    /// `Column`.
    ///
    /// The talker cell accepts zero to two letters. The period cell accepts [50, 3600000] ms,
    /// the range the profile accepts for custom sentences, in steps of 100 ms.
    QTableWidget* table;
    /// The custom sentences in emission order, with the columns of `CustomColumn`; rows are
    /// added by `add_custom` and removed by `remove_current_custom`.
    QTableWidget* custom_table;
    /// The *Add custom sentence* button, which calls `add_custom` with an empty body.
    QPushButton* add_custom_button;
    /// The *Remove* button, which calls `remove_current_custom`.
    QPushButton* remove_custom_button;

private:
    /// Ticks or unticks the enabled flag of every registry row.
    ///
    /// Connected to the *Enable all* button with `true` and the *Disable all* button with
    /// `false`.
    ///
    /// @param enabled Whether every registry sentence is sent.
    void set_all(bool enabled);
    /// Returns every registry row and the position decimals to their defaults.
    ///
    /// Loads a default-constructed `io::Profile`, which has no sentence settings, with the
    /// current custom sentences copied into it, so the custom table keeps its rows; their ids
    /// come back trimmed and upper-cased. Connected to the *Reset to defaults* button.
    void reset_defaults();
};

}  // namespace nmeasim::app
