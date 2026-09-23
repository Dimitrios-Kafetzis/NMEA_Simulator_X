#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

namespace nmeasim::app {

/// Settings tab that lists every sentence in the registry with its enabled flag, talker and
/// period, plus the position precision.
class SentencesPage : public QWidget {
    Q_OBJECT

public:
    enum Column { Enabled = 0, Id, Description, Group, Talker, Period, ColumnCount };
    enum CustomColumn { CustomEnabled = 0, CustomId, CustomBody, CustomPeriod, CustomColumnCount };

    explicit SentencesPage(QWidget* parent = nullptr);

    void load(const io::Profile& profile);
    void store(io::Profile& profile) const;
    /// A problem with the custom sentences, for example an invalid body; empty when valid.
    [[nodiscard]] QString validate() const;

    /// Row of a registry id in the table, or -1.
    [[nodiscard]] int row_of(const QString& id) const;
    void set_enabled(int row, bool enabled);
    [[nodiscard]] bool is_enabled(int row) const;

    void add_custom(const QString& body = {});
    void remove_current_custom();
    [[nodiscard]] int custom_count() const;
    [[nodiscard]] core::simulation::CustomSentence custom_at(int row) const;

    QSpinBox* position_decimals_spin;
    QTableWidget* table;
    QTableWidget* custom_table;
    QPushButton* add_custom_button;
    QPushButton* remove_custom_button;

private:
    void set_all(bool enabled);
    void reset_defaults();
};

}  // namespace nmeasim::app
