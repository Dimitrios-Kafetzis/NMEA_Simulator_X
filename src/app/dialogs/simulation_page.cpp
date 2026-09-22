#include "simulation_page.hpp"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimeZone>
#include <QVBoxLayout>

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

QSpinBox* make_int(QWidget* parent, int minimum, int maximum, const QString& suffix = {}) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

}  // namespace

SimulationPage::SimulationPage(QWidget* parent) : QWidget(parent) {
    auto* content = new QWidget;
    auto* columns = new QHBoxLayout(content);

    // Left column: profile, clock, seed values.
    auto* left = new QVBoxLayout;
    auto* general_box = new QGroupBox(tr("Profile and clock"), content);
    auto* general = new QFormLayout(general_box);
    name_edit = new QLineEdit(general_box);
    general->addRow(tr("Name"), name_edit);
    tick_spin = make_int(general_box, 10, 10000, tr(" ms"));
    general->addRow(tr("Simulation step"), tick_spin);
    fixed_start_check = new QCheckBox(tr("Fixed start time (UTC)"), general_box);
    start_time_edit = new QDateTimeEdit(general_box);
    start_time_edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    start_time_edit->setTimeZone(QTimeZone::utc());
    start_time_edit->setCalendarPopup(true);
    start_time_edit->setEnabled(false);
    connect(fixed_start_check, &QCheckBox::toggled, start_time_edit, &QWidget::setEnabled);
    general->addRow(fixed_start_check, start_time_edit);
    random_seed_spin = make_int(general_box, 0, 1000000000);
    general->addRow(tr("Random seed"), random_seed_spin);
    left->addWidget(general_box);

    auto* seed_box = new QGroupBox(tr("Initial vessel values"), content);
    auto* seed = new QFormLayout(seed_box);
    latitude_spin = make_double(seed_box, -90.0, 90.0, 0.001, 6, tr("°"));
    seed->addRow(tr("Latitude"), latitude_spin);
    longitude_spin = make_double(seed_box, -180.0, 180.0, 0.001, 6, tr("°"));
    seed->addRow(tr("Longitude"), longitude_spin);
    altitude_spin = make_double(seed_box, -500.0, 20000.0, 1.0, 1, tr(" m"));
    seed->addRow(tr("Altitude"), altitude_spin);
    heading_spin = make_double(seed_box, 0.0, 359.9, 1.0, 1, tr("°T"));
    seed->addRow(tr("Heading"), heading_spin);
    speed_spin = make_double(seed_box, 0.0, 999.9, 0.1, 1, tr(" kn"));
    seed->addRow(tr("Speed over ground"), speed_spin);
    variation_spin = make_double(seed_box, -180.0, 180.0, 0.1, 1, tr("°"));
    seed->addRow(tr("Magnetic variation (east +)"), variation_spin);
    deviation_spin = make_double(seed_box, -180.0, 180.0, 0.1, 1, tr("°"));
    seed->addRow(tr("Compass deviation (east +)"), deviation_spin);
    depth_spin = make_double(seed_box, 0.0, 99999.9, 0.1, 1, tr(" m"));
    seed->addRow(tr("Depth below transducer"), depth_spin);
    transducer_offset_spin = make_double(seed_box, -50.0, 50.0, 0.1, 2, tr(" m"));
    seed->addRow(tr("Transducer offset"), transducer_offset_spin);
    water_temperature_spin = make_double(seed_box, -5.0, 60.0, 0.1, 1, tr(" °C"));
    seed->addRow(tr("Water temperature"), water_temperature_spin);
    wind_direction_spin = make_double(seed_box, 0.0, 359.9, 1.0, 1, tr("°T"));
    seed->addRow(tr("True wind direction"), wind_direction_spin);
    wind_speed_spin = make_double(seed_box, 0.0, 200.0, 0.5, 1, tr(" kn"));
    seed->addRow(tr("True wind speed"), wind_speed_spin);
    left->addWidget(seed_box);
    left->addStretch(1);
    columns->addLayout(left, 1);

    // Right column: GNSS, drift, steering.
    auto* right = new QVBoxLayout;
    auto* gnss_box = new QGroupBox(tr("GNSS receiver"), content);
    auto* gnss = new QFormLayout(gnss_box);
    fix_check = new QCheckBox(tr("Receiver has a fix"), gnss_box);
    gnss->addRow(fix_check);
    quality_combo = new QComboBox(gnss_box);
    quality_combo->addItems({tr("Invalid"), tr("GPS"), tr("Differential")});
    gnss->addRow(tr("Fix quality"), quality_combo);
    satellites_in_use_spin = make_int(gnss_box, 0, 12);
    gnss->addRow(tr("Satellites in use"), satellites_in_use_spin);
    satellites_in_view_spin = make_int(gnss_box, 0, 12);
    gnss->addRow(tr("Satellites in view"), satellites_in_view_spin);
    hdop_spin = make_double(gnss_box, 0.0, 99.9, 0.1, 1);
    gnss->addRow(tr("HDOP"), hdop_spin);
    pdop_spin = make_double(gnss_box, 0.0, 99.9, 0.1, 1);
    gnss->addRow(tr("PDOP"), pdop_spin);
    vdop_spin = make_double(gnss_box, 0.0, 99.9, 0.1, 1);
    gnss->addRow(tr("VDOP"), vdop_spin);
    geoid_spin = make_double(gnss_box, -200.0, 200.0, 0.1, 1, tr(" m"));
    gnss->addRow(tr("Geoid separation"), geoid_spin);
    right->addWidget(gnss_box);

    auto* drift_box = new QGroupBox(tr("Drift around the initial values"), content);
    auto* drift = new QGridLayout(drift_box);
    drift->addWidget(new QLabel(tr("Amplitude"), drift_box), 0, 1);
    drift->addWidget(new QLabel(tr("Step per second"), drift_box), 0, 2);
    const std::array<QString, 6> labels{tr("Heading (°)"),        tr("Speed (kn)"),
                                        tr("Depth (m)"),          tr("Water temperature (°C)"),
                                        tr("Wind direction (°)"), tr("Wind speed (kn)")};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const int row = static_cast<int>(index) + 1;
        drift->addWidget(new QLabel(labels.at(index), drift_box), row, 0);
        amplitude_spins.at(index) = make_double(drift_box, 0.0, 1000.0, 0.1, 2);
        drift->addWidget(amplitude_spins.at(index), row, 1);
        step_spins.at(index) = make_double(drift_box, 0.0, 1000.0, 0.05, 3);
        drift->addWidget(step_spins.at(index), row, 2);
    }
    right->addWidget(drift_box);

    auto* steering_box = new QGroupBox(tr("Steering"), content);
    auto* steering = new QFormLayout(steering_box);
    turn_rate_spin = make_double(steering_box, 0.0, 100.0, 0.1, 2, tr(" °/min per °"));
    steering->addRow(tr("Turn rate per degree of rudder"), turn_rate_spin);
    max_rudder_spin = make_double(steering_box, 1.0, 90.0, 1.0, 0, tr("°"));
    steering->addRow(tr("Maximum rudder angle"), max_rudder_spin);
    right->addWidget(steering_box);
    right->addStretch(1);
    columns->addLayout(right, 1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

void SimulationPage::load(const io::Profile& profile) {
    name_edit->setText(profile.name);
    tick_spin->setValue(profile.tick_ms);
    fixed_start_check->setChecked(profile.start_time.has_value());
    start_time_edit->setDateTime(profile.start_time.value_or(
        QDateTime::currentDateTimeUtc().addMSecs(-QDateTime::currentDateTimeUtc().time().msec())));
    random_seed_spin->setValue(static_cast<int>(profile.delta.random_seed));

    const auto& seed = profile.delta.seed;
    latitude_spin->setValue(seed.navigation.position.latitude_deg);
    longitude_spin->setValue(seed.navigation.position.longitude_deg);
    altitude_spin->setValue(seed.navigation.altitude_m);
    heading_spin->setValue(seed.navigation.heading_true_deg);
    speed_spin->setValue(seed.navigation.speed_over_ground_kn);
    variation_spin->setValue(seed.navigation.magnetic_variation_deg);
    deviation_spin->setValue(seed.navigation.magnetic_deviation_deg);
    depth_spin->setValue(seed.water.depth_below_transducer_m);
    transducer_offset_spin->setValue(seed.water.transducer_offset_m);
    water_temperature_spin->setValue(seed.water.temperature_c);
    wind_direction_spin->setValue(seed.wind.true_direction_deg);
    wind_speed_spin->setValue(seed.wind.true_speed_kn);

    fix_check->setChecked(seed.gnss.has_fix);
    quality_combo->setCurrentIndex(static_cast<int>(seed.gnss.quality));
    satellites_in_use_spin->setValue(seed.gnss.satellites_in_use);
    satellites_in_view_spin->setValue(seed.gnss.satellites_in_view);
    hdop_spin->setValue(seed.gnss.hdop);
    pdop_spin->setValue(seed.gnss.pdop);
    vdop_spin->setValue(seed.gnss.vdop);
    geoid_spin->setValue(seed.gnss.geoid_separation_m);

    const auto& delta = profile.delta;
    const std::array<const core::simulation::Variation*, 6> variations{
        &delta.heading,           &delta.speed,          &delta.depth,
        &delta.water_temperature, &delta.wind_direction, &delta.wind_speed,
    };
    for (std::size_t index = 0; index < variations.size(); ++index) {
        amplitude_spins.at(index)->setValue(variations.at(index)->amplitude);
        step_spins.at(index)->setValue(variations.at(index)->step_per_second);
    }
    turn_rate_spin->setValue(delta.turn_rate_per_rudder_deg);
    max_rudder_spin->setValue(delta.max_rudder_angle_deg);
}

void SimulationPage::store(io::Profile& profile) const {
    profile.name = name_edit->text().trimmed().isEmpty() ? QStringLiteral("Untitled")
                                                         : name_edit->text().trimmed();
    profile.tick_ms = tick_spin->value();
    if (fixed_start_check->isChecked()) {
        profile.start_time = start_time_edit->dateTime().toUTC();
    } else {
        profile.start_time.reset();
    }
    profile.delta.random_seed = static_cast<unsigned int>(random_seed_spin->value());

    auto& seed = profile.delta.seed;
    seed.navigation.position.latitude_deg = latitude_spin->value();
    seed.navigation.position.longitude_deg = longitude_spin->value();
    seed.navigation.altitude_m = altitude_spin->value();
    seed.navigation.heading_true_deg = heading_spin->value();
    seed.navigation.speed_over_ground_kn = speed_spin->value();
    seed.navigation.magnetic_variation_deg = variation_spin->value();
    seed.navigation.magnetic_deviation_deg = deviation_spin->value();
    seed.water.depth_below_transducer_m = depth_spin->value();
    seed.water.transducer_offset_m = transducer_offset_spin->value();
    seed.water.temperature_c = water_temperature_spin->value();
    seed.wind.true_direction_deg = wind_direction_spin->value();
    seed.wind.true_speed_kn = wind_speed_spin->value();

    seed.gnss.has_fix = fix_check->isChecked();
    seed.gnss.quality = static_cast<core::model::FixQuality>(quality_combo->currentIndex());
    seed.gnss.satellites_in_use = satellites_in_use_spin->value();
    seed.gnss.satellites_in_view = satellites_in_view_spin->value();
    seed.gnss.hdop = hdop_spin->value();
    seed.gnss.pdop = pdop_spin->value();
    seed.gnss.vdop = vdop_spin->value();
    seed.gnss.geoid_separation_m = geoid_spin->value();

    auto& delta = profile.delta;
    const std::array<core::simulation::Variation*, 6> variations{
        &delta.heading,           &delta.speed,          &delta.depth,
        &delta.water_temperature, &delta.wind_direction, &delta.wind_speed,
    };
    for (std::size_t index = 0; index < variations.size(); ++index) {
        variations.at(index)->amplitude = amplitude_spins.at(index)->value();
        variations.at(index)->step_per_second = step_spins.at(index)->value();
    }
    delta.turn_rate_per_rudder_deg = turn_rate_spin->value();
    delta.max_rudder_angle_deg = max_rudder_spin->value();
}

}  // namespace nmeasim::app
