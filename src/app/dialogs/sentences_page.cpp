#include "sentences_page.hpp"

#include <nmeasim/core/nmea0183/registry.hpp>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include <string>

namespace nmeasim::app {

namespace {

QString from_view(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

SentencesPage::SentencesPage(QWidget* parent)
    : QWidget(parent), position_decimals_spin(new QSpinBox(this)), table(new QTableWidget(this)) {
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

    auto* layout_ = new QVBoxLayout(this);
    layout_->addLayout(top);
    layout_->addWidget(table, 1);
}

void SentencesPage::load(const io::Profile& profile) {
    position_decimals_spin->setValue(profile.encoder.position_decimals);
    const auto scheduler = profile.make_scheduler();
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
    profile.sentences.clear();
    const auto descriptors = core::nmea0183::SentenceRegistry::standard().descriptors();
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& descriptor = descriptors[index];
        const int row = static_cast<int>(index);
        core::simulation::SentenceSetting setting;
        setting.enabled = is_enabled(row);
        setting.talker =
            static_cast<QLineEdit*>(table->cellWidget(row, Talker))->text().toUpper().toStdString();
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
    load(defaults);
}

}  // namespace nmeasim::app
