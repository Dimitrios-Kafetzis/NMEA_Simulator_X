// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Layout, readouts and override wiring of the dashboard and its engine tiles.

#include "dashboard_widget.hpp"

#include "dials.hpp"
#include "instrument_tile.hpp"

#include <nmeasim/core/geo/route.hpp>
#include <nmeasim/core/units.hpp>

#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimeZone>
#include <QVBoxLayout>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>

namespace nmeasim::app {

using core::simulation::Parameter;

namespace {

/// Formats one coordinate as degrees and decimal minutes with a hemisphere letter.
///
/// @param degrees Coordinate in degrees; the sign selects the letter.
/// @param degree_digits Width of the zero-padded degrees: 2 for latitude, 3 for longitude.
/// @param positive Letter for zero and positive values, `N` or `E`.
/// @param negative Letter for negative values, `S` or `W`.
/// @return The coordinate, such as `37°59.028'N`, with minutes to three decimals.
QString format_coordinate(double degrees, int degree_digits, char positive, char negative) {
    const double magnitude = std::fabs(degrees);
    const int whole = static_cast<int>(magnitude);
    const double minutes = (magnitude - whole) * 60.0;
    return QStringLiteral("%1°%2'%3")
        .arg(whole, degree_digits, 10, QLatin1Char('0'))
        .arg(minutes, 6, 'f', 3, QLatin1Char('0'))
        .arg(QLatin1Char(degrees < 0.0 ? negative : positive));
}

/// Formats a time point for the *Time (UTC)* tile.
///
/// @param time The time point, UTC; sub-millisecond parts are dropped.
/// @return The time as `yyyy-MM-dd HH:mm:ss` in UTC.
QString format_time(std::chrono::system_clock::time_point time) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    return QDateTime::fromMSecsSinceEpoch(ms.count(), QTimeZone::utc())
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

}  // namespace

EngineTile::EngineTile(const QString& text, QWidget* parent)
    : QFrame(parent),
      label(new QLabel(text, this)),
      running_check(new QCheckBox(tr("Running"), this)),
      rpm_spin(new QDoubleSpinBox(this)),
      temperature_spin(new QDoubleSpinBox(this)) {
    setObjectName(QStringLiteral("engine_tile"));
    label->setObjectName(QStringLiteral("tile_title"));
    QFont title_font = label->font();
    title_font.setBold(true);
    title_font.setPointSizeF(title_font.pointSizeF() * 0.82);
    title_font.setLetterSpacing(QFont::PercentageSpacing, 110);
    title_font.setCapitalization(QFont::AllUppercase);
    label->setFont(title_font);
    rpm_spin->setRange(0.0, 99999.9);
    rpm_spin->setDecimals(0);
    rpm_spin->setSingleStep(100.0);
    rpm_spin->setSuffix(tr(" rpm"));
    rpm_spin->setKeyboardTracking(false);
    temperature_spin->setRange(-50.0, 500.0);
    temperature_spin->setDecimals(1);
    temperature_spin->setSuffix(tr(" °C"));
    temperature_spin->setKeyboardTracking(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->addWidget(label);
    layout->addWidget(running_check);
    layout->addWidget(rpm_spin);
    layout->addWidget(temperature_spin);
    const auto notify = [this] {
        if (!suppress_signals_) {
            emit changed();
        }
    };
    connect(running_check, &QCheckBox::toggled, this, notify);
    connect(rpm_spin, &QDoubleSpinBox::valueChanged, this, notify);
    connect(temperature_spin, &QDoubleSpinBox::valueChanged, this, notify);
}

void EngineTile::show_engine(const core::model::Engine& engine) {
    suppress_signals_ = true;
    label->setText(QString::fromStdString(engine.label));
    running_check->setChecked(engine.running);
    if (!rpm_spin->hasFocus()) {
        rpm_spin->setValue(engine.revolutions_rpm);
    }
    if (!temperature_spin->hasFocus()) {
        temperature_spin->setValue(engine.coolant_temperature_c);
    }
    suppress_signals_ = false;
}

core::model::Engine EngineTile::engine() const {
    return {label->text().toStdString(), running_check->isChecked(), rpm_spin->value(),
            temperature_spin->value()};
}

void EngineTile::set_editable(bool editable) {
    running_check->setEnabled(editable);
    rpm_spin->setEnabled(editable);
    temperature_spin->setEnabled(editable);
}

QString format_position(const core::geo::Position& position) {
    return format_coordinate(position.latitude_deg, 2, 'N', 'S') + QStringLiteral("  ") +
           format_coordinate(position.longitude_deg, 3, 'E', 'W');
}

DashboardWidget::DashboardWidget(QWidget* parent)
    : QWidget(parent),
      content_(new QWidget),
      grid_(new QGridLayout),
      compass_(new CompassDial),
      wind_(new WindDial) {
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("dashboard_scroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    content_->setObjectName(QStringLiteral("dashboard_content"));
    scroll->setWidget(content_);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    auto* column = new QVBoxLayout(content_);
    column->setContentsMargins(8, 8, 8, 8);
    column->setSpacing(8);
    grid_->setSpacing(8);

    // Top row: the two dials and a column with position, time and GNSS.
    auto* top = new QHBoxLayout;
    top->setSpacing(8);
    top->addWidget(dial_panel(tr("Compass"), compass_), 1);
    top->addWidget(dial_panel(tr("Wind"), wind_), 1);
    auto* facts = new QVBoxLayout;
    facts->setSpacing(8);
    position_tile_ = new InstrumentTile(tr("Position"), QString{}, content_);
    time_tile_ = new InstrumentTile(tr("Time (UTC)"), QString{}, content_);
    gnss_tile_ = new InstrumentTile(tr("GNSS"), QString{}, content_);
    facts->addWidget(position_tile_);
    facts->addWidget(time_tile_);
    facts->addWidget(gnss_tile_);
    top->addLayout(facts, 1);
    column->addLayout(top);
    column->addLayout(grid_);
    fix_check_ = new QCheckBox(tr("Fix"), gnss_tile_);
    fix_check_->setChecked(true);
    satellites_spin_ = new QSpinBox(gnss_tile_);
    satellites_spin_->setRange(0, 12);
    satellites_spin_->setPrefix(tr("Satellites "));
    auto* gnss_row = new QHBoxLayout;
    gnss_row->addWidget(fix_check_);
    gnss_row->addWidget(satellites_spin_, 1);
    static_cast<QVBoxLayout*>(gnss_tile_->layout())->addLayout(gnss_row);
    connect(fix_check_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!suppress_signals_) {
            emit fix_changed(checked);
        }
    });
    connect(satellites_spin_, &QSpinBox::valueChanged, this, [this](int value) {
        if (!suppress_signals_) {
            emit satellites_changed(value);
        }
    });

    add_controllable(Parameter::HeadingTrue, tr("Heading"), tr("°T"), 0, 0, 0.0, 359.9, 1.0, 1);
    course_tile_ = add_tile(tr("Course over ground"), tr("°T"), 0, 1);
    rate_of_turn_tile_ = add_tile(tr("Rate of turn"), tr("°/min"), 0, 2);

    add_controllable(Parameter::SpeedOverGround, tr("Speed over ground"), tr("kn"), 1, 0, 0.0,
                     999.9, 0.1, 1);
    add_controllable(Parameter::SpeedThroughWater, tr("Speed through water"), tr("kn"), 1, 1, 0.0,
                     999.9, 0.1, 1);
    add_controllable(Parameter::RudderAngle, tr("Rudder"), tr("°"), 1, 2, -45.0, 45.0, 1.0, 1);

    add_controllable(Parameter::Depth, tr("Depth"), tr("m"), 2, 0, 0.0, 99999.9, 0.1, 1);
    add_controllable(Parameter::WaterTemperature, tr("Water temperature"), tr("°C"), 2, 1, -5.0,
                     60.0, 0.1, 1);
    add_controllable(Parameter::Altitude, tr("Altitude"), tr("m"), 2, 2, -500.0, 20000.0, 1.0, 1);

    add_controllable(Parameter::WindDirectionTrue, tr("True wind direction"), tr("°T"), 3, 0, 0.0,
                     359.9, 1.0, 1);
    add_controllable(Parameter::WindSpeedTrue, tr("True wind speed"), tr("kn"), 3, 1, 0.0, 200.0,
                     0.5, 1);
    apparent_wind_tile_ = add_tile(tr("Apparent wind"), QString{}, 3, 2);

    destination_tile_ = add_tile(tr("Destination"), QString{}, 4, 0);
    grid_->addWidget(destination_tile_, 4, 0, 1, 3);
    destination_tile_->set_text(tr("None"));

    auto* engines_box = new QGroupBox(tr("Engines"), content_);
    engines_row_ = new QHBoxLayout(engines_box);
    engines_row_->addStretch(1);
    grid_->addWidget(engines_box, 5, 0, 1, 3);
    column->addStretch(1);
    set_steering_mode(false);
}

void DashboardWidget::sync_engines(const std::vector<core::model::Engine>& engines) {
    bool same = engines.size() == engines_.size();
    for (std::size_t index = 0; same && index < engines.size(); ++index) {
        same = engines_[index]->label->text() == QString::fromStdString(engines[index].label);
    }
    if (same) {
        return;
    }
    for (auto* tile : engines_) {
        engines_row_->removeWidget(tile);
        tile->deleteLater();
    }
    engines_.clear();
    for (std::size_t index = 0; index < engines.size(); ++index) {
        auto* tile = new EngineTile(QString::fromStdString(engines[index].label), this);
        tile->set_editable(overrides_enabled_);
        const int position = static_cast<int>(index);
        connect(tile, &EngineTile::changed, this,
                [this, tile, position] { emit engine_changed(position, tile->engine()); });
        engines_row_->insertWidget(position, tile);
        engines_.push_back(tile);
    }
}

EngineTile* DashboardWidget::engine_tile(int index) const {
    return index >= 0 && index < engine_count() ? engines_[static_cast<std::size_t>(index)]
                                                : nullptr;
}

QString DashboardWidget::destination_text() const {
    return destination_tile_->text();
}

InstrumentTile* DashboardWidget::add_tile(const QString& title, const QString& unit, int row,
                                          int column) {
    auto* tile = new InstrumentTile(title, unit, content_);
    grid_->addWidget(tile, row, column);
    return tile;
}

QFrame* DashboardWidget::dial_panel(const QString& title, QWidget* dial) {
    auto* panel = new QFrame(content_);
    panel->setObjectName(QStringLiteral("dial_panel"));
    auto* caption = new QLabel(title, panel);
    caption->setObjectName(QStringLiteral("tile_title"));
    QFont font = caption->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 0.82);
    font.setLetterSpacing(QFont::PercentageSpacing, 110);
    font.setCapitalization(QFont::AllUppercase);
    caption->setFont(font);
    dial->setParent(panel);
    dial->setMinimumSize(200, 200);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->addWidget(caption);
    layout->addWidget(dial, 1);
    return panel;
}

InstrumentTile* DashboardWidget::add_controllable(Parameter parameter, const QString& title,
                                                  const QString& unit, int row, int column,
                                                  double minimum, double maximum, double step,
                                                  int decimals) {
    auto* tile = add_tile(title, unit, row, column);
    tile->enable_override(minimum, maximum, step, decimals);
    connect(tile, &InstrumentTile::override_changed, this,
            [this, parameter](bool active, double value) {
                emit override_changed(parameter, active, value);
            });
    controls_[parameter] = tile;
    return tile;
}

void DashboardWidget::update_state(const core::model::VesselState& state) {
    const auto& navigation = state.navigation;
    // Latitude and longitude on separate lines keep the readout narrow.
    position_tile_->set_text(
        format_position(navigation.position).replace(QStringLiteral("  "), QStringLiteral("\n")));
    time_tile_->set_text(format_time(state.time_utc));
    gnss_tile_->set_text(state.gnss.has_fix ? tr("Fix, %1 sat.\nHDOP %2")
                                                  .arg(state.gnss.satellites_in_use)
                                                  .arg(state.gnss.hdop, 0, 'f', 1)
                                            : tr("No fix"));
    course_tile_->set_value(navigation.course_over_ground_deg, 1);
    std::optional<double> bearing;
    rate_of_turn_tile_->set_value(navigation.rate_of_turn_deg_per_min, 1);
    apparent_wind_tile_->set_text(tr("%1° at %2 kn")
                                      .arg(state.wind.apparent_angle_deg, 0, 'f', 1)
                                      .arg(state.wind.apparent_speed_kn, 0, 'f', 1));
    if (state.destination) {
        const auto leg = core::geo::solve_leg(state.destination->origin,
                                              state.destination->position, navigation.position);
        bearing = leg.bearing_deg;
        destination_tile_->set_text(
            tr("%1: bearing %2°, %3 nm, XTE %4 nm %5")
                .arg(QString::fromStdString(state.destination->name))
                .arg(leg.bearing_deg, 0, 'f', 1)
                .arg(leg.distance_m / core::units::kMetresPerNauticalMile, 0, 'f', 2)
                .arg(std::fabs(leg.cross_track_m) / core::units::kMetresPerNauticalMile, 0, 'f', 2)
                .arg(leg.cross_track_m > 0.0 ? tr("steer left") : tr("steer right")));
    } else {
        destination_tile_->set_text(tr("None"));
    }
    compass_->set_values(navigation.heading_true_deg, navigation.course_over_ground_deg, bearing);
    wind_->set_values(state.wind.apparent_angle_deg, state.wind.apparent_speed_kn,
                      state.wind.true_angle_relative_deg(navigation.heading_true_deg),
                      state.wind.true_speed_kn);
    sync_engines(state.engines);
    for (std::size_t index = 0; index < state.engines.size(); ++index) {
        engines_[index]->show_engine(state.engines[index]);
    }

    const std::map<Parameter, double> values{
        {Parameter::HeadingTrue, navigation.heading_true_deg},
        {Parameter::SpeedOverGround, navigation.speed_over_ground_kn},
        {Parameter::SpeedThroughWater, navigation.speed_through_water_kn},
        {Parameter::Altitude, navigation.altitude_m},
        {Parameter::Depth, state.water.depth_below_transducer_m},
        {Parameter::WaterTemperature, state.water.temperature_c},
        {Parameter::WindDirectionTrue, state.wind.true_direction_deg},
        {Parameter::WindSpeedTrue, state.wind.true_speed_kn},
        {Parameter::RudderAngle, state.steering.rudder_angle_deg},
    };
    for (const auto& [parameter, value] : values) {
        controls_.at(parameter)->set_value(value, 1);
    }

    suppress_signals_ = true;
    fix_check_->setChecked(state.gnss.has_fix);
    if (!satellites_spin_->hasFocus()) {
        satellites_spin_->setValue(state.gnss.satellites_in_use);
    }
    suppress_signals_ = false;
}

void DashboardWidget::sync_overrides(const core::simulation::DeltaSource& source) {
    const auto& state = source.current();
    for (auto& [parameter, tile] : controls_) {
        const auto value = source.override_value(parameter);
        double fallback = 0.0;
        switch (parameter) {
            case Parameter::HeadingTrue:
                fallback = state.navigation.heading_true_deg;
                break;
            case Parameter::SpeedOverGround:
                fallback = state.navigation.speed_over_ground_kn;
                break;
            case Parameter::SpeedThroughWater:
                fallback = state.navigation.speed_through_water_kn;
                break;
            case Parameter::Altitude:
                fallback = state.navigation.altitude_m;
                break;
            case Parameter::Depth:
                fallback = state.water.depth_below_transducer_m;
                break;
            case Parameter::WaterTemperature:
                fallback = state.water.temperature_c;
                break;
            case Parameter::WindDirectionTrue:
                fallback = state.wind.true_direction_deg;
                break;
            case Parameter::WindSpeedTrue:
                fallback = state.wind.true_speed_kn;
                break;
            case Parameter::RudderAngle:
                fallback = state.steering.rudder_angle_deg;
                break;
        }
        tile->set_override(value.has_value(), value.value_or(fallback));
    }
}

void DashboardWidget::set_steering_mode(bool enabled) {
    steering_mode_ = enabled;
    controls_.at(Parameter::RudderAngle)->set_override_enabled(overrides_enabled_ && enabled);
    controls_.at(Parameter::HeadingTrue)->set_override_enabled(overrides_enabled_ && !enabled);
}

void DashboardWidget::set_overrides_enabled(bool enabled) {
    overrides_enabled_ = enabled;
    for (auto& [parameter, tile] : controls_) {
        tile->set_override_enabled(enabled);
    }
    fix_check_->setEnabled(enabled);
    satellites_spin_->setEnabled(enabled);
    for (auto* tile : engines_) {
        tile->set_editable(enabled);
    }
    set_steering_mode(steering_mode_);
}

}  // namespace nmeasim::app
