// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `SentencesPage`, the *Sentences* tab of the settings dialog.
///
/// Builds the registry and custom sentence tables and moves values between them and the
/// `sentences` fields of an `io::Profile`.

#include "sentences_page.hpp"

#include <nmeasim/core/nmea0183/registry.hpp>
#include <nmeasim/core/simulation/custom_sentence.hpp>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include <chrono>
#include <string>

namespace nmeasim::app {

namespace {

/// Converts registry text to a `QString`.
///
/// The registry holds its ids, descriptions, group names and talkers as `std::string_view`
/// values, which need not be null-terminated, so the length is passed explicitly.
///
/// @param text UTF-8 text; only read during the call.
/// @return A copy of `text`.
QString from_view(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

SentencesPage::SentencesPage(QWidget* parent)
    : QWidget(parent),
      position_decimals_spin(new QSpinBox(this)),
      table(new QTableWidget(this)),
      custom_table(new QTableWidget(this)),
      add_custom_button(new QPushButton(tr("Add custom sentence"), this)),
      remove_custom_button(new QPushButton(tr("Remove"), this)) {
    position_decimals_spin->setRange(2, 8);
    position_decimals_spin->setToolTip(
        tr("Decimal minutes in latitude and longitude. Lowered automatically when a sentence "
           "would exceed 82 characters."));

    auto* enable_all = new QPushButton(tr("Enable all"), this);
    connect(enable_all, &QPushButton::clicked, this, [this] { set_all(true); });
    auto* disable_all = new QPushButton(tr("Disable all"), this);
    connect(disable_all, &QPushButton::clicked, this, [this] { set_all(false); });
    auto* defaults = new QPushButton(tr("Reset to defaults"), this);
    connect(defaults, &QPushButton::clicked, this, &SentencesPage::reset_defaults);

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Position decimals"), this));
    top->addWidget(position_decimals_spin);
    top->addStretch(1);
    top->addWidget(enable_all);
    top->addWidget(disable_all);
    top->addWidget(defaults);

    table->setColumnCount(ColumnCount);
    table->setHorizontalHeaderLabels(
        {tr("On"), tr("Id"), tr("Description"), tr("Group"), tr("Talker"), tr("Period (ms)")});
    table->horizontalHeader()->setSectionResizeMode(Description, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    const auto& registry = core::nmea0183::SentenceRegistry::standard();
    const auto descriptors = registry.descriptors();
    table->setRowCount(static_cast<int>(descriptors.size()));
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& descriptor = descriptors[index];
        const int row = static_cast<int>(index);
        auto* enabled = new QTableWidgetItem;
        enabled->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        enabled->setCheckState(Qt::Unchecked);
        table->setItem(row, Enabled, enabled);
        auto* id = new QTableWidgetItem(from_view(descriptor.id));
        id->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, Id, id);
        auto* description = new QTableWidgetItem(from_view(descriptor.description));
        description->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, Description, description);
        auto* group = new QTableWidgetItem(from_view(to_string(descriptor.group)));
        group->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, Group, group);

        auto* talker = new QLineEdit(table);
        talker->setMaxLength(2);
        talker->setPlaceholderText(from_view(descriptor.default_talker));
        talker->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("[A-Za-z]{0,2}")), talker));
        talker->setToolTip(tr("Two-letter talker; empty uses %1").arg(talker->placeholderText()));
        table->setCellWidget(row, Talker, talker);

        auto* period = new QSpinBox(table);
        period->setRange(50, 3600000);
        period->setSingleStep(100);
        period->setKeyboardTracking(false);
        table->setCellWidget(row, Period, period);
    }
    table->resizeColumnsToContents();

    custom_table->setColumnCount(CustomColumnCount);
    custom_table->setHorizontalHeaderLabels(
        {tr("On"), tr("Id"), tr("Sentence"), tr("Period (ms)")});
    custom_table->horizontalHeader()->setSectionResizeMode(CustomBody, QHeaderView::Stretch);
    custom_table->verticalHeader()->setVisible(false);
    custom_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    custom_table->setSelectionMode(QAbstractItemView::SingleSelection);
    custom_table->setToolTip(
        tr("Sentences of your own, sent with a computed checksum. Write the sentence without "
           "checksum, for example $PXYZ,1,2,3"));
    connect(add_custom_button, &QPushButton::clicked, this, [this] { add_custom(); });
    connect(remove_custom_button, &QPushButton::clicked, this,
            &SentencesPage::remove_current_custom);
    auto* custom_buttons = new QHBoxLayout;
    custom_buttons->addWidget(new QLabel(tr("Custom sentences"), this));
    custom_buttons->addStretch(1);
    custom_buttons->addWidget(add_custom_button);
    custom_buttons->addWidget(remove_custom_button);

    auto* layout_ = new QVBoxLayout(this);
    layout_->addLayout(top);
    layout_->addWidget(table, 3);
    layout_->addLayout(custom_buttons);
    layout_->addWidget(custom_table, 1);
}

void SentencesPage::add_custom(const QString& body) {
    const int row = custom_table->rowCount();
    custom_table->insertRow(row);
    auto* enabled = new QTableWidgetItem;
    enabled->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    enabled->setCheckState(Qt::Checked);
    custom_table->setItem(row, CustomEnabled, enabled);
    auto* id = new QLineEdit(custom_table);
    id->setMaxLength(12);
    id->setPlaceholderText(QStringLiteral("CUSTOM-%1").arg(row + 1));
    id->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9-]{0,12}")), id));
    custom_table->setCellWidget(row, CustomId, id);
    auto* text = new QLineEdit(body, custom_table);
    text->setMaxLength(80);
    text->setPlaceholderText(QStringLiteral("$PXYZ,1,2,3"));
    custom_table->setCellWidget(row, CustomBody, text);
    auto* period = new QSpinBox(custom_table);
    period->setRange(50, 3600000);
    period->setSingleStep(100);
    period->setValue(1000);
    period->setKeyboardTracking(false);
    custom_table->setCellWidget(row, CustomPeriod, period);
    custom_table->selectRow(row);
}

void SentencesPage::remove_current_custom() {
    const int row = custom_table->currentRow();
    if (row >= 0) {
        custom_table->removeRow(row);
    }
}

int SentencesPage::custom_count() const {
    return custom_table->rowCount();
}

core::simulation::CustomSentence SentencesPage::custom_at(int row) const {
    core::simulation::CustomSentence sentence;
    sentence.enabled = custom_table->item(row, CustomEnabled)->checkState() == Qt::Checked;
    sentence.id = static_cast<QLineEdit*>(custom_table->cellWidget(row, CustomId))
                      ->text()
                      .trimmed()
                      .toUpper()
                      .toStdString();
    sentence.body =
        static_cast<QLineEdit*>(custom_table->cellWidget(row, CustomBody))->text().toStdString();
    sentence.period = std::chrono::milliseconds{
        static_cast<QSpinBox*>(custom_table->cellWidget(row, CustomPeriod))->value()};
    return sentence;
}

QString SentencesPage::validate() const {
    const auto& registry = core::nmea0183::SentenceRegistry::standard();
    for (int row = 0; row < custom_table->rowCount(); ++row) {
        const auto sentence = custom_at(row);
        if (const auto problem = core::simulation::validate_custom_sentence(sentence.body)) {
            return tr("Custom sentence %1: %2").arg(row + 1).arg(QString::fromStdString(*problem));
        }
        if (registry.find(sentence.id) != nullptr) {
            return tr("Custom sentence %1: the id %2 belongs to a registry sentence")
                .arg(row + 1)
                .arg(QString::fromStdString(sentence.id));
        }
    }
    return {};
}

void SentencesPage::load(const io::Profile& profile) {
    position_decimals_spin->setValue(profile.encoder.position_decimals);
    custom_table->setRowCount(0);
    for (const auto& sentence : profile.custom_sentences) {
        add_custom(QString::fromStdString(sentence.body));
        const int row = custom_table->rowCount() - 1;
        custom_table->item(row, CustomEnabled)
            ->setCheckState(sentence.enabled ? Qt::Checked : Qt::Unchecked);
        static_cast<QLineEdit*>(custom_table->cellWidget(row, CustomId))
            ->setText(QString::fromStdString(sentence.id));
        static_cast<QSpinBox*>(custom_table->cellWidget(row, CustomPeriod))
            ->setValue(static_cast<int>(sentence.period.count()));
    }
    // The scheduler fills in the registry default of every sentence the profile leaves out.
    const auto scheduler = profile.make_scheduler();
    // The scheduler uses the standard registry, so its descriptors are in table row order.
    const auto descriptors = scheduler.registry().descriptors();
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const int row = static_cast<int>(index);
        const auto setting = scheduler.setting(descriptors[index].id);
        table->item(row, Enabled)->setCheckState(setting.enabled ? Qt::Checked : Qt::Unchecked);
        static_cast<QLineEdit*>(table->cellWidget(row, Talker))
            ->setText(QString::fromStdString(setting.talker).toUpper());
        static_cast<QSpinBox*>(table->cellWidget(row, Period))
            ->setValue(static_cast<int>(setting.period.count()));
    }
}

void SentencesPage::store(io::Profile& profile) const {
    profile.encoder.position_decimals = position_decimals_spin->value();
    profile.custom_sentences.clear();
    for (int row = 0; row < custom_table->rowCount(); ++row) {
        profile.custom_sentences.push_back(custom_at(row));
    }
    profile.sentences.clear();
    const auto descriptors = core::nmea0183::SentenceRegistry::standard().descriptors();
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& descriptor = descriptors[index];
        const int row = static_cast<int>(index);
        core::simulation::SentenceSetting setting;
        setting.enabled = is_enabled(row);
        setting.talker =
            static_cast<QLineEdit*>(table->cellWidget(row, Talker))->text().toUpper().toStdString();
        // Spelling out the default talker is not a change from the default.
        if (setting.talker == descriptor.default_talker) {
            setting.talker.clear();
        }
        setting.period = std::chrono::milliseconds{
            static_cast<QSpinBox*>(table->cellWidget(row, Period))->value()};
        const bool is_default = setting.enabled == descriptor.enabled_by_default &&
                                setting.talker.empty() &&
                                setting.period == descriptor.default_period;
        if (!is_default) {
            profile.sentences[std::string{descriptor.id}] = setting;
        }
    }
}

int SentencesPage::row_of(const QString& id) const {
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, Id)->text() == id) {
            return row;
        }
    }
    return -1;
}

void SentencesPage::set_enabled(int row, bool enabled) {
    table->item(row, Enabled)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

bool SentencesPage::is_enabled(int row) const {
    return table->item(row, Enabled)->checkState() == Qt::Checked;
}

void SentencesPage::set_all(bool enabled) {
    for (int row = 0; row < table->rowCount(); ++row) {
        set_enabled(row, enabled);
    }
}

void SentencesPage::reset_defaults() {
    io::Profile defaults;
    defaults.sentences.clear();
    for (int row = 0; row < custom_table->rowCount(); ++row) {
        defaults.custom_sentences.push_back(custom_at(row));
    }
    load(defaults);
}

}  // namespace nmeasim::app
