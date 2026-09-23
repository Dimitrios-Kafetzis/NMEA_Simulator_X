#include "vessel_page.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include <string>

namespace nmeasim::app {

namespace {

QDoubleSpinBox* make_double(QWidget* parent, double minimum, double maximum, double step,
                            int decimals, const QString& suffix = {}) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

QSpinBox* make_int(QWidget* parent, int minimum, int maximum) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setKeyboardTracking(false);
    return spin;
}

}  // namespace

VesselPage::VesselPage(QWidget* parent) : QWidget(parent), engines_table(new QTableWidget(this)) {
    auto* engines_box = new QGroupBox(tr("Engines"), this);
    engines_table->setColumnCount(EngineColumnCount);
    engines_table->setHorizontalHeaderLabels(
        {tr("Label"), tr("Running"), tr("Revolutions (rpm)"), tr("Coolant (°C)")});
    engines_table->horizontalHeader()->setSectionResizeMode(Label, QHeaderView::Stretch);
    engines_table->verticalHeader()->setVisible(false);
    engines_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    engines_table->setSelectionMode(QAbstractItemView::SingleSelection);
    engines_table->setToolTip(
        tr("The first engine is number 1 in RPM and ENGINE#0 in XDR; the Signal K id comes "
           "from the label"));
    add_engine_button = new QPushButton(tr("Add engine"), engines_box);
    connect(add_engine_button, &QPushButton::clicked, this, &VesselPage::add_engine);
    remove_engine_button = new QPushButton(tr("Remove"), engines_box);
    connect(remove_engine_button, &QPushButton::clicked, this, &VesselPage::remove_current_engine);
    auto* engine_buttons = new QHBoxLayout;
    engine_buttons->addWidget(add_engine_button);
    engine_buttons->addWidget(remove_engine_button);
    engine_buttons->addStretch(1);
    auto* engines_layout = new QVBoxLayout(engines_box);
    engines_layout->addWidget(engines_table, 1);
    engines_layout->addLayout(engine_buttons);

    auto* ais_box = new QGroupBox(tr("AIS static data"), this);
    auto* ais = new QFormLayout(ais_box);
    mmsi_spin = make_int(ais_box, 0, 999999999);
    ais->addRow(tr("MMSI"), mmsi_spin);
    imo_spin = make_int(ais_box, 0, 999999999);
    imo_spin->setSpecialValueText(tr("None"));
    ais->addRow(tr("IMO number"), imo_spin);
    name_edit = new QLineEdit(ais_box);
    name_edit->setMaxLength(20);
    ais->addRow(tr("Vessel name"), name_edit);
    call_sign_edit = new QLineEdit(ais_box);
    call_sign_edit->setMaxLength(7);
    ais->addRow(tr("Call sign"), call_sign_edit);
    ship_type_spin = make_int(ais_box, 0, 255);
    ship_type_spin->setToolTip(
        tr("ITU-R M.1371 type of ship and cargo: 36 sailing, 37 pleasure "
           "craft, 30 fishing, 70 cargo, 80 tanker"));
    ais->addRow(tr("Ship type code"), ship_type_spin);
    to_bow_spin = make_double(ais_box, 0.0, 511.0, 1.0, 0, tr(" m"));
    ais->addRow(tr("Antenna to bow"), to_bow_spin);
    to_stern_spin = make_double(ais_box, 0.0, 511.0, 1.0, 0, tr(" m"));
    ais->addRow(tr("Antenna to stern"), to_stern_spin);
    to_port_spin = make_double(ais_box, 0.0, 63.0, 1.0, 0, tr(" m"));
    ais->addRow(tr("Antenna to port side"), to_port_spin);
    to_starboard_spin = make_double(ais_box, 0.0, 63.0, 1.0, 0, tr(" m"));
    ais->addRow(tr("Antenna to starboard side"), to_starboard_spin);
    draught_spin = make_double(ais_box, 0.0, 25.5, 0.1, 1, tr(" m"));
    ais->addRow(tr("Draught"), draught_spin);
    ais_destination_edit = new QLineEdit(ais_box);
    ais_destination_edit->setMaxLength(20);
    ais_destination_edit->setPlaceholderText(tr("Not set"));
    ais->addRow(tr("Voyage destination"), ais_destination_edit);
    navigation_status_spin = make_int(ais_box, 0, 15);
    navigation_status_spin->setToolTip(
        tr("0 under way using engine, 1 at anchor, 5 moored, 8 under way sailing"));
    ais->addRow(tr("Navigational status"), navigation_status_spin);
    report_type_combo = new QComboBox(ais_box);
    report_type_combo->addItems(
        {tr("Type 1 (scheduled)"), tr("Type 2 (assigned)"), tr("Type 3 (interrogated)")});
    ais->addRow(tr("Position report"), report_type_combo);

    auto* columns = new QHBoxLayout(this);
    columns->addWidget(engines_box, 1);
    columns->addWidget(ais_box, 1);
}

void VesselPage::append_engine_row(const core::model::Engine& engine) {
    const int row = engines_table->rowCount();
    engines_table->insertRow(row);
    auto* label = new QTableWidgetItem(QString::fromStdString(engine.label));
    engines_table->setItem(row, Label, label);
    auto* running = new QTableWidgetItem;
    running->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    running->setCheckState(engine.running ? Qt::Checked : Qt::Unchecked);
    engines_table->setItem(row, Running, running);
    auto* rpm = make_double(engines_table, 0.0, 99999.9, 100.0, 0);
    rpm->setValue(engine.revolutions_rpm);
    engines_table->setCellWidget(row, Rpm, rpm);
    auto* temperature = make_double(engines_table, -50.0, 500.0, 1.0, 1);
    temperature->setValue(engine.coolant_temperature_c);
    engines_table->setCellWidget(row, Temperature, temperature);
}

void VesselPage::add_engine() {
    core::model::Engine engine;
    engine.label = "Engine " + std::to_string(engines_table->rowCount() + 1);
    engine.running = true;
    engine.revolutions_rpm = 1500.0;
    engine.coolant_temperature_c = 80.0;
    append_engine_row(engine);
    engines_table->selectRow(engines_table->rowCount() - 1);
}

void VesselPage::remove_current_engine() {
    const int row = engines_table->currentRow();
    if (row >= 0) {
        engines_table->removeRow(row);
    }
}

int VesselPage::engine_count() const {
    return engines_table->rowCount();
}

core::model::Engine VesselPage::engine_at(int row) const {
    core::model::Engine engine;
    engine.label = engines_table->item(row, Label)->text().trimmed().toStdString();
    if (engine.label.empty()) {
        engine.label = "Engine " + std::to_string(row + 1);
    }
    engine.running = engines_table->item(row, Running)->checkState() == Qt::Checked;
    engine.revolutions_rpm =
        static_cast<QDoubleSpinBox*>(engines_table->cellWidget(row, Rpm))->value();
    engine.coolant_temperature_c =
        static_cast<QDoubleSpinBox*>(engines_table->cellWidget(row, Temperature))->value();
    return engine;
}

void VesselPage::set_engine(int row, const core::model::Engine& engine) {
    engines_table->item(row, Label)->setText(QString::fromStdString(engine.label));
    engines_table->item(row, Running)->setCheckState(engine.running ? Qt::Checked : Qt::Unchecked);
    static_cast<QDoubleSpinBox*>(engines_table->cellWidget(row, Rpm))
        ->setValue(engine.revolutions_rpm);
    static_cast<QDoubleSpinBox*>(engines_table->cellWidget(row, Temperature))
        ->setValue(engine.coolant_temperature_c);
}

QString VesselPage::validate() const {
    if (mmsi_spin->value() <= 0) {
        return tr("The AIS MMSI must be a positive number of at most nine digits.");
    }
    return {};
}

void VesselPage::load(const io::Profile& profile) {
    engines_table->setRowCount(0);
    for (const auto& engine : profile.delta.seed.engines) {
        append_engine_row(engine);
    }
    const auto& ais = profile.delta.seed.ais;
    mmsi_spin->setValue(static_cast<int>(ais.mmsi));
    imo_spin->setValue(static_cast<int>(ais.imo_number));
    name_edit->setText(QString::fromStdString(ais.name));
    call_sign_edit->setText(QString::fromStdString(ais.call_sign));
    ship_type_spin->setValue(ais.ship_type);
    to_bow_spin->setValue(ais.dimension_to_bow_m);
    to_stern_spin->setValue(ais.dimension_to_stern_m);
    to_port_spin->setValue(ais.dimension_to_port_m);
    to_starboard_spin->setValue(ais.dimension_to_starboard_m);
    draught_spin->setValue(ais.draught_m);
    ais_destination_edit->setText(QString::fromStdString(ais.destination));
    navigation_status_spin->setValue(ais.navigation_status);
    report_type_combo->setCurrentIndex(ais.position_report_type >= 1 &&
                                               ais.position_report_type <= 3
                                           ? ais.position_report_type - 1
                                           : 0);
}

void VesselPage::store(io::Profile& profile) const {
    auto& seed = profile.delta.seed;
    seed.engines.clear();
    for (int row = 0; row < engines_table->rowCount(); ++row) {
        seed.engines.push_back(engine_at(row));
    }
    auto& ais = seed.ais;
    ais.mmsi = static_cast<std::uint32_t>(mmsi_spin->value());
    ais.imo_number = static_cast<std::uint32_t>(imo_spin->value());
    ais.name = name_edit->text().trimmed().toUpper().toStdString();
    ais.call_sign = call_sign_edit->text().trimmed().toUpper().toStdString();
    ais.ship_type = ship_type_spin->value();
    ais.dimension_to_bow_m = to_bow_spin->value();
    ais.dimension_to_stern_m = to_stern_spin->value();
    ais.dimension_to_port_m = to_port_spin->value();
    ais.dimension_to_starboard_m = to_starboard_spin->value();
    ais.draught_m = draught_spin->value();
    ais.destination = ais_destination_edit->text().trimmed().toUpper().toStdString();
    ais.navigation_status = navigation_status_spin->value();
    ais.position_report_type = report_type_combo->currentIndex() + 1;
}

}  // namespace nmeasim::app
