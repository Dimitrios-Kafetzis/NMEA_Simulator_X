#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QStackedWidget>
#include <QWidget>

namespace nmeasim::app {

/// Settings tab with the list of outputs and an editor for the selected one.
class OutputsPage : public QWidget {
    Q_OBJECT

public:
    explicit OutputsPage(QWidget* parent = nullptr);

    void load(const io::Profile& profile);
    void store(io::Profile& profile) const;
    /// Returns an empty string when every output is complete, otherwise what is missing.
    [[nodiscard]] QString validate() const;

    void add_output(io::OutputConfig::Type type);
    void remove_current();
    void select(int index);
    [[nodiscard]] int count() const;
    [[nodiscard]] int current_index() const;
    /// The list as edited so far, including the output being edited.
    [[nodiscard]] QList<io::OutputConfig> outputs() const;

    QListWidget* list;
    QCheckBox* enabled_check;
    QLineEdit* filter_edit;
    QStackedWidget* editor_stack;

    // Per-type editors. Tests and the store function read them through these members.
    QLineEdit* bind_address_edit;
    QSpinBox* server_port_spin;
    QLineEdit* host_edit;
    QSpinBox* client_port_spin;
    QSpinBox* reconnect_spin;
    QComboBox* udp_mode_combo;
    QLineEdit* udp_address_edit;
    QSpinBox* udp_port_spin;
    QLineEdit* udp_interface_edit;
    QSpinBox* udp_ttl_spin;
    QComboBox* serial_port_combo;
    QComboBox* baud_combo;
    QComboBox* data_bits_combo;
    QComboBox* parity_combo;
    QComboBox* stop_bits_combo;
    QComboBox* flow_control_combo;
    QLineEdit* file_path_edit;
    QCheckBox* append_check;

private:
    void show_output(int index);
    void commit_editor();
    void refresh_titles();
    [[nodiscard]] static QString title_of(const io::OutputConfig& output);
    [[nodiscard]] static int page_of(io::OutputConfig::Type type);

    QList<io::OutputConfig> outputs_;
    int editing_{-1};
    bool loading_{false};
};

}  // namespace nmeasim::app
