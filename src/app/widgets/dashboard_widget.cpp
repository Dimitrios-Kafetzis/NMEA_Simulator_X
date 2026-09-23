#include "dashboard_widget.hpp"

#include "instrument_tile.hpp"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimeZone>
#include <QVBoxLayout>

#include <chrono>
#include <cmath>

namespace nmeasim::app {

using core::simulation::Parameter;

namespace {

QString format_coordinate(double degrees, int degree_digits, char positive, char negative) {
    const double magnitude = std::fabs(degrees);
    const int whole = static_cast<int>(magnitude);
    const double minutes = (magnitude - whole) * 60.0;
    return QStringLiteral("%1°%2'%3")
        .arg(whole, degree_digits, 10, QLatin1Char('0'))
        .arg(minutes, 6, 'f', 3, QLatin1Char('0'))
        .arg(QLatin1Char(degrees < 0.0 ? negative : positive));
}

QString format_time(std::chrono::system_clock::time_point time) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    return QDateTime::fromMSecsSinceEpoch(ms.count(), QTimeZone::utc())
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

}  // namespace

QString format_position(const core::geo::Position& position) {
    return format_coordinate(position.latitude_deg, 2, 'N', 'S') + QStringLiteral("  ") +
           format_coordinate(position.longitude_deg, 3, 'E', 'W');
}

DashboardWidget::DashboardWidget(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setSpacing(8);

    position_tile_ = add_tile(tr("Position"), QString{}, 0, 0);
    time_tile_ = add_tile(tr("Time (UTC)"), QString{}, 0, 1);
    gnss_tile_ = add_tile(tr("GNSS"), QString{}, 0, 2);
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

    add_controllable(Parameter::HeadingTrue, tr("Heading"), tr("°T"), 1, 0, 0.0, 359.9, 1.0, 1);
    course_tile_ = add_tile(tr("Course over ground"), tr("°T"), 1, 1);
    rate_of_turn_tile_ = add_tile(tr("Rate of turn"), tr("°/min"), 1, 2);

    add_controllable(Parameter::SpeedOverGround, tr("Speed over ground"), tr("kn"), 2, 0, 0.0,
                     999.9, 0.1, 1);
    add_controllable(Parameter::SpeedThroughWater, tr("Speed through water"), tr("kn"), 2, 1, 0.0,
                     999.9, 0.1, 1);
    add_controllable(Parameter::RudderAngle, tr("Rudder"), tr("°"), 2, 2, -45.0, 45.0, 1.0, 1);

    add_controllable(Parameter::Depth, tr("Depth"), tr("m"), 3, 0, 0.0, 99999.9, 0.1, 1);
    add_controllable(Parameter::WaterTemperature, tr("Water temperature"), tr("°C"), 3, 1, -5.0,
                     60.0, 0.1, 1);
    add_controllable(Parameter::Altitude, tr("Altitude"), tr("m"), 3, 2, -500.0, 20000.0, 1.0, 1);

    add_controllable(Parameter::WindDirectionTrue, tr("True wind direction"), tr("°T"), 4, 0, 0.0,
                     359.9, 1.0, 1);
    add_controllable(Parameter::WindSpeedTrue, tr("True wind speed"), tr("kn"), 4, 1, 0.0, 200.0,
                     0.5, 1);
    apparent_wind_tile_ = add_tile(tr("Apparent wind"), QString{}, 4, 2);

    grid->setRowStretch(5, 1);
    set_steering_mode(false);
}

InstrumentTile* DashboardWidget::add_tile(const QString& title, const QString& unit, int row,
                                          int column) {
    auto* tile = new InstrumentTile(title, unit, this);
    static_cast<QGridLayout*>(layout())->addWidget(tile, row, column);
    return tile;
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
    position_tile_->set_text(format_position(navigation.position));
    time_tile_->set_text(format_time(state.time_utc));
    gnss_tile_->set_text(state.gnss.has_fix ? tr("Fix, %1 satellites, HDOP %2")
                                                  .arg(state.gnss.satellites_in_use)
                                                  .arg(state.gnss.hdop, 0, 'f', 1)
                                            : tr("No fix"));
    course_tile_->set_value(navigation.course_over_ground_deg, 1);
    rate_of_turn_tile_->set_value(navigation.rate_of_turn_deg_per_min, 1);
    apparent_wind_tile_->set_text(tr("%1° at %2 kn")
                                      .arg(state.wind.apparent_angle_deg, 0, 'f', 1)
                                      .arg(state.wind.apparent_speed_kn, 0, 'f', 1));

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
    set_steering_mode(steering_mode_);
}

}  // namespace nmeasim::app
