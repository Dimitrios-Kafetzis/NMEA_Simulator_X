// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `SettingsDialog`: builds the tabs and buttons, and validates and stores
/// the pages when *OK* is pressed.

#include "settings_dialog.hpp"

#include "outputs_page.hpp"
#include "sentences_page.hpp"
#include "simulation_page.hpp"
#include "vessel_page.hpp"

#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace nmeasim::app {

SettingsDialog::SettingsDialog(const io::Profile& profile, QWidget* parent)
    : QDialog(parent),
      profile_(profile),
      tabs_(new QTabWidget(this)),
      simulation_(new SimulationPage(this)),
      vessel_(new VesselPage(this)),
      sentences_(new SentencesPage(this)),
      outputs_(new OutputsPage(this)),
      error_label_(new QLabel(this)) {
    setWindowTitle(tr("Settings - %1").arg(profile.name));
    setModal(true);
    resize(900, 640);

    tabs_->addTab(simulation_, tr("Simulation"));
    tabs_->addTab(vessel_, tr("Vessel"));
    tabs_->addTab(sentences_, tr("Sentences"));
    tabs_->addTab(outputs_, tr("Outputs"));

    error_label_->setStyleSheet(QStringLiteral("color: palette(highlight);"));
    error_label_->setWordWrap(true);
    error_label_->hide();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    auto* layout_ = new QVBoxLayout(this);
    layout_->addWidget(tabs_, 1);
    layout_->addWidget(error_label_);
    layout_->addWidget(buttons);

    simulation_->load(profile_);
    vessel_->load(profile_);
    sentences_->load(profile_);
    outputs_->load(profile_);
}

QString SettingsDialog::error_text() const {
    return error_label_->isHidden() ? QString{} : error_label_->text();
}

void SettingsDialog::accept() {
    const QString mode_problem = simulation_->validate();
    if (!mode_problem.isEmpty()) {
        error_label_->setText(mode_problem);
        error_label_->show();
        tabs_->setCurrentWidget(simulation_);
        return;
    }
    const QString vessel_problem = vessel_->validate();
    if (!vessel_problem.isEmpty()) {
        error_label_->setText(vessel_problem);
        error_label_->show();
        tabs_->setCurrentWidget(vessel_);
        return;
    }
    const QString sentence_problem = sentences_->validate();
    if (!sentence_problem.isEmpty()) {
        error_label_->setText(sentence_problem);
        error_label_->show();
        tabs_->setCurrentWidget(sentences_);
        return;
    }
    const QString problem = outputs_->validate();
    if (!problem.isEmpty()) {
        error_label_->setText(problem);
        error_label_->show();
        tabs_->setCurrentWidget(outputs_);
        return;
    }
    error_label_->hide();
    simulation_->store(profile_);
    vessel_->store(profile_);
    sentences_->store(profile_);
    outputs_->store(profile_);
    QDialog::accept();
}

}  // namespace nmeasim::app
