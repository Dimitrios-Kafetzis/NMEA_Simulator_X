// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Painting of the compass rose and the wind dial, and the wind angle helpers.

#include "dials.hpp"

#include "theme/theme.hpp"

#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nmeasim::app {

namespace {

/// Space between the tick ring and the widget edge, in pixels; the bezel takes 6 of them.
constexpr double kMargin{8.0};
/// Factor that converts degrees to radians.
constexpr double kDegreesToRadians{std::numbers::pi / 180.0};

/// Returns a copy of a font sized in pixels, so that dial text scales with the dial.
///
/// @param base Font to copy the family and style from.
/// @param pixels Text height in pixels; rounded, and raised to 1 when smaller.
/// @param bold True for a bold font.
/// @return The resized font.
QFont pixel_font(const QFont& base, double pixels, bool bold) {
    QFont font = base;
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(pixels))));
    font.setBold(bold);
    return font;
}

/// Formats an angle for a compass readout with three integer digits and one decimal.
///
/// @param value Angle in degrees, expected in [0, 360).
/// @return The angle zero-padded to five characters with a degree sign, such as `045.0°`.
QString degrees(double value) {
    return QStringLiteral("%1°").arg(value, 5, 'f', 1, QLatin1Char('0'));
}

}  // namespace

double signed_relative_angle(double relative_deg) noexcept {
    double angle = std::fmod(relative_deg, 360.0);
    if (angle <= -180.0) {
        angle += 360.0;
    } else if (angle > 180.0) {
        angle -= 360.0;
    }
    return angle;
}

QString format_wind_angle(double relative_deg) {
    const double angle = signed_relative_angle(relative_deg);
    const auto rounded = static_cast<int>(std::lround(std::fabs(angle)));
    if (rounded == 0 || rounded == 180) {
        return QStringLiteral("%1°").arg(rounded);
    }
    return QStringLiteral("%1°%2").arg(rounded).arg(angle < 0.0 ? QLatin1Char('P')
                                                                : QLatin1Char('S'));
}

// DialWidget --------------------------------------------------------------------------------

DialWidget::DialWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(&theme::Theme::instance(), &theme::Theme::changed, this, qOverload<>(&QWidget::update));
}

double DialWidget::radius() const {
    return std::max(10.0, std::min(width(), height()) / 2.0 - kMargin);
}

QPointF DialWidget::centre() const {
    return {width() / 2.0, height() / 2.0};
}

QPointF DialWidget::at(double angle_deg, double fraction) const {
    const double a = angle_deg * kDegreesToRadians;
    return centre() + QPointF(std::sin(a), -std::cos(a)) * (radius() * fraction);
}

void DialWidget::draw_face(QPainter& painter) const {
    const auto& colors = theme::Theme::instance().colors();
    const double r = radius();
    painter.setPen(QPen(colors.border_strong, 2.0));
    painter.setBrush(colors.panel_raised);
    painter.drawEllipse(centre(), r + 6.0, r + 6.0);
    painter.setPen(QPen(colors.border, 1.0));
    painter.setBrush(colors.inset);
    painter.drawEllipse(centre(), r, r);
}

void DialWidget::draw_ticks(QPainter& painter, int minor, int major) const {
    const auto& colors = theme::Theme::instance().colors();
    for (int angle = 0; angle < 360; angle += minor) {
        const bool is_major = angle % major == 0;
        painter.setPen(QPen(is_major ? colors.text : colors.text_dim, is_major ? 2.0 : 1.0));
        painter.drawLine(at(angle, 0.98), at(angle, is_major ? 0.87 : 0.93));
    }
}

void DialWidget::draw_ring_marker(QPainter& painter, double angle_deg, const QColor& color,
                                  bool filled) const {
    const double r = radius();
    const double a = angle_deg * kDegreesToRadians;
    const QPointF tangent(std::cos(a), std::sin(a));
    const QPointF tip = at(angle_deg, 0.84);
    const QPointF base = at(angle_deg, 1.0);
    const double half = r * 0.06;
    const QPolygonF marker{tip, base + tangent * half, base - tangent * half};
    painter.setPen(QPen(color, filled ? 1.0 : 2.0));
    painter.setBrush(filled ? QBrush(color) : Qt::NoBrush);
    painter.drawPolygon(marker);
}

void DialWidget::draw_readout(QPainter& painter, const QString& caption, const QString& value,
                              double y_fraction, double size_fraction, const QColor& color) const {
    const auto& colors = theme::Theme::instance().colors();
    const double r = radius();
    const QPointF c = centre() + QPointF(0.0, r * y_fraction);
    const double value_px = r * size_fraction;
    const double caption_px = std::max(8.0, r * 0.085);
    QFont caption_font = pixel_font(font(), caption_px, true);
    caption_font.setLetterSpacing(QFont::PercentageSpacing, 110);
    painter.setFont(caption_font);
    painter.setPen(colors.text_dim);
    painter.drawText(
        QRectF(c.x() - r, c.y() - value_px / 2.0 - caption_px * 1.4, 2 * r, caption_px * 1.3),
        Qt::AlignHCenter | Qt::AlignBottom, caption);
    painter.setFont(pixel_font(theme::Theme::readout_font(10.0), value_px, false));
    painter.setPen(color);
    painter.drawText(QRectF(c.x() - r, c.y() - value_px / 2.0, 2 * r, value_px * 1.2),
                     Qt::AlignHCenter | Qt::AlignTop, value);
}

// CompassDial -------------------------------------------------------------------------------

CompassDial::CompassDial(QWidget* parent) : DialWidget(parent) {
    setObjectName(QStringLiteral("compass_dial"));
    setToolTip(
        tr("Heading (pointer), course over ground (triangle) and bearing to the "
           "destination (diamond), north up"));
}

void CompassDial::set_values(double heading_deg, double course_deg,
                             std::optional<double> bearing_deg) {
    heading_deg_ = heading_deg;
    course_deg_ = course_deg;
    bearing_deg_ = bearing_deg;
    update();
}

void CompassDial::paintEvent(QPaintEvent* /*event*/) {
    const auto& colors = theme::Theme::instance().colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    draw_face(painter);
    draw_ticks(painter, 5, 30);

    // Every 30 degrees a cardinal letter or the tens of degrees, as compass cards print them;
    // north stands out in the danger colour.
    const double r = radius();
    for (int angle = 0; angle < 360; angle += 30) {
        const bool cardinal = angle % 90 == 0;
        QString label = QString::number(angle / 10);
        if (cardinal) {
            label = QStringLiteral("NESW").at(angle / 90);
        }
        painter.setFont(pixel_font(font(), r * (cardinal ? 0.13 : 0.09), cardinal));
        painter.setPen(angle == 0 ? colors.danger : (cardinal ? colors.text : colors.text_dim));
        const QPointF p = at(angle, 0.74);
        painter.drawText(QRectF(p.x() - r * 0.15, p.y() - r * 0.1, r * 0.3, r * 0.2),
                         Qt::AlignCenter, label);
    }

    // Bearing to the destination, course over ground, then the heading pointer on top.
    if (bearing_deg_) {
        const QPointF p = at(*bearing_deg_, 0.93);
        const double s = r * 0.055;
        painter.setPen(QPen(colors.map_vessel_outline, 1.0));
        painter.setBrush(colors.map_destination);
        painter.drawPolygon(QPolygonF{p + QPointF(0, -s), p + QPointF(s, 0), p + QPointF(0, s),
                                      p + QPointF(-s, 0)});
    }
    draw_ring_marker(painter, course_deg_, colors.ok, true);

    const double h = heading_deg_ * kDegreesToRadians;
    const QPointF side(std::cos(h), std::sin(h));
    const QPointF tip = at(heading_deg_, 0.97);
    const QPointF shoulder = at(heading_deg_, 0.62);
    const QPointF tail = at(heading_deg_, 0.46);
    QPolygonF pointer{tip, shoulder + side * (r * 0.07), tail, shoulder - side * (r * 0.07)};
    painter.setPen(QPen(colors.map_vessel_outline, 1.0));
    painter.setBrush(colors.accent);
    painter.drawPolygon(pointer);

    draw_readout(painter, tr("HDG °T"), degrees(heading_deg_), -0.08, 0.2, colors.text_value);
    QString secondary = tr("COG %1").arg(degrees(course_deg_));
    if (bearing_deg_) {
        secondary += QStringLiteral("   ") + tr("BRG %1").arg(degrees(*bearing_deg_));
    }
    painter.setFont(pixel_font(theme::Theme::readout_font(10.0), r * 0.09, false));
    painter.setPen(colors.text_dim);
    painter.drawText(QRectF(centre().x() - r, centre().y() + r * 0.2, 2 * r, r * 0.14),
                     Qt::AlignCenter, secondary);
}

// WindDial ----------------------------------------------------------------------------------

WindDial::WindDial(QWidget* parent) : DialWidget(parent) {
    setObjectName(QStringLiteral("wind_dial"));
    setToolTip(tr("Apparent wind (arrow) and true wind (hollow triangle) relative to the bow"));
}

void WindDial::set_values(double apparent_angle_deg, double apparent_speed_kn,
                          double true_angle_deg, double true_speed_kn) {
    apparent_angle_deg_ = apparent_angle_deg;
    apparent_speed_kn_ = apparent_speed_kn;
    true_angle_deg_ = true_angle_deg;
    true_speed_kn_ = true_speed_kn;
    update();
}

void WindDial::paintEvent(QPaintEvent* /*event*/) {
    const auto& colors = theme::Theme::instance().colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    draw_face(painter);

    // Close-hauled sectors, 20 to 60 degrees off the bow: red to port, green to starboard.
    const double r = radius();
    const QRectF arc_box(centre() - QPointF(r * 0.955, r * 0.955),
                         centre() + QPointF(r * 0.955, r * 0.955));
    const auto sector = [&](const QColor& color, double from_deg, double to_deg) {
        QColor band = color;
        band.setAlpha(170);
        painter.setPen(QPen(band, r * 0.07, Qt::SolidLine, Qt::FlatCap));
        painter.setBrush(Qt::NoBrush);
        // Qt measures arcs counter-clockwise from three o'clock in sixteenths of a degree.
        const auto start = static_cast<int>(std::lround((90.0 - to_deg) * 16.0));
        const auto span = static_cast<int>(std::lround((to_deg - from_deg) * 16.0));
        painter.drawArc(arc_box, start, span);
    };
    sector(colors.danger, -60.0, -20.0);
    sector(colors.ok, 20.0, 60.0);
    draw_ticks(painter, 10, 30);

    for (int angle = 0; angle < 360; angle += 30) {
        const auto label = static_cast<int>(std::lround(std::fabs(signed_relative_angle(angle))));
        painter.setFont(pixel_font(font(), r * 0.085, false));
        painter.setPen(colors.text_dim);
        const QPointF p = at(angle, 0.74);
        painter.drawText(QRectF(p.x() - r * 0.15, p.y() - r * 0.1, r * 0.3, r * 0.2),
                         Qt::AlignCenter, QString::number(label));
    }

    draw_ring_marker(painter, true_angle_deg_, colors.text_dim, false);

    // Apparent wind: an arrow from the ring towards the centre.
    const double a = apparent_angle_deg_ * kDegreesToRadians;
    const QPointF side(std::cos(a), std::sin(a));
    const QPointF tail = at(apparent_angle_deg_, 0.98);
    const QPointF head = at(apparent_angle_deg_, 0.56);
    painter.setPen(QPen(colors.accent, r * 0.035, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(tail, at(apparent_angle_deg_, 0.66));
    const QPointF barb = at(apparent_angle_deg_, 0.7);
    painter.setPen(QPen(colors.map_vessel_outline, 1.0));
    painter.setBrush(colors.accent);
    painter.drawPolygon(QPolygonF{head, barb + side * (r * 0.07), barb - side * (r * 0.07)});

    draw_readout(painter, tr("AWS KN"), QString::number(apparent_speed_kn_, 'f', 1), -0.1, 0.2,
                 colors.text_value);
    painter.setFont(pixel_font(theme::Theme::readout_font(10.0), r * 0.085, false));
    painter.setPen(colors.text_dim);
    painter.drawText(QRectF(centre().x() - r, centre().y() + r * 0.16, 2 * r, r * 0.13),
                     Qt::AlignCenter, tr("AWA %1").arg(format_wind_angle(apparent_angle_deg_)));
    painter.drawText(
        QRectF(centre().x() - r, centre().y() + r * 0.29, 2 * r, r * 0.13), Qt::AlignCenter,
        tr("TWA %1  TWS %2")
            .arg(format_wind_angle(true_angle_deg_), QString::number(true_speed_kn_, 'f', 1)));
}

}  // namespace nmeasim::app
