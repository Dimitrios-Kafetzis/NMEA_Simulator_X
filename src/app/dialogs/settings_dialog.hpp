#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QDialog>
#include <QLabel>
#include <QTabWidget>

namespace nmeasim::app {

class OutputsPage;
class SentencesPage;
class SimulationPage;

/// Edits a copy of a profile in three tabs. `profile()` holds the result after `accept()`.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const io::Profile& profile, QWidget* parent = nullptr);

    [[nodiscard]] const io::Profile& profile() const noexcept { return profile_; }

    [[nodiscard]] SimulationPage* simulation_page() const noexcept { return simulation_; }
    [[nodiscard]] SentencesPage* sentences_page() const noexcept { return sentences_; }
    [[nodiscard]] OutputsPage* outputs_page() const noexcept { return outputs_; }
    [[nodiscard]] QTabWidget* tabs() const noexcept { return tabs_; }
    /// The validation message shown at the bottom, empty when the last accept succeeded.
    [[nodiscard]] QString error_text() const;

public slots:
    void accept() override;

private:
    io::Profile profile_;
    QTabWidget* tabs_;
    SimulationPage* simulation_;
    SentencesPage* sentences_;
    OutputsPage* outputs_;
    QLabel* error_label_;
};

}  // namespace nmeasim::app
