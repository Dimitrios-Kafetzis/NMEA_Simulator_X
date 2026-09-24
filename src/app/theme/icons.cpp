// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The drawing code of the toolbar and menu icons and their rendering into pixmaps.

#include "icons.hpp"

#include "theme.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include <cmath>
#include <numbers>

namespace nmeasim::app::theme {

namespace {

/// Draws the outline and fills of one symbol.
///
/// Coordinates are in a 24 by 24 unit design grid with the origin at the top left. Sets the
/// painter's pen to a 1.8-unit round-capped stroke and changes its brush.
///
/// @param p Painter already scaled so that 24 units span the target size.
/// @param icon Symbol to draw.
/// @param color Colour of strokes and fills.
void draw(QPainter& p, Icon icon, const QColor& color) {
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (icon) {
        case Icon::NewProfile: {
            QPainterPath page;
            page.moveTo(6, 3);
            page.lineTo(14, 3);
            page.lineTo(19, 8);
            page.lineTo(19, 21);
            page.lineTo(6, 21);
            page.closeSubpath();
            p.drawPath(page);
            p.drawLine(QPointF(14, 3), QPointF(14, 8));
            p.drawLine(QPointF(14, 8), QPointF(19, 8));
            p.drawLine(QPointF(12.5, 11.5), QPointF(12.5, 17.5));
            p.drawLine(QPointF(9.5, 14.5), QPointF(15.5, 14.5));
            break;
        }
        case Icon::Open: {
            QPainterPath folder;
            folder.moveTo(3, 6);
            folder.lineTo(9, 6);
            folder.lineTo(11, 8);
            folder.lineTo(21, 8);
            folder.lineTo(21, 19);
            folder.lineTo(3, 19);
            folder.closeSubpath();
            p.drawPath(folder);
            p.drawLine(QPointF(3, 11), QPointF(21, 11));
            break;
        }
        case Icon::Save: {
            p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
            p.drawRect(QRectF(8, 4, 8, 5));
            p.drawRoundedRect(QRectF(7.5, 13, 9, 7), 1, 1);
            break;
        }
        case Icon::Settings: {
            const QPointF c(12, 12);
            for (int tooth = 0; tooth < 8; ++tooth) {
                const double a = tooth * std::numbers::pi / 4.0;
                p.drawLine(c + QPointF(std::cos(a) * 6.5, std::sin(a) * 6.5),
                           c + QPointF(std::cos(a) * 9.5, std::sin(a) * 9.5));
            }
            p.drawEllipse(c, 6.5, 6.5);
            p.drawEllipse(c, 2.5, 2.5);
            break;
        }
        case Icon::Start: {
            p.setBrush(color);
            p.drawPolygon(QPolygonF{QPointF(8, 5), QPointF(19, 12), QPointF(8, 19)});
            break;
        }
        case Icon::Stop: {
            p.setBrush(color);
            p.drawRoundedRect(QRectF(6.5, 6.5, 11, 11), 1.5, 1.5);
            break;
        }
        case Icon::Pause: {
            p.setBrush(color);
            p.drawRoundedRect(QRectF(7, 5.5, 3.5, 13), 1, 1);
            p.drawRoundedRect(QRectF(13.5, 5.5, 3.5, 13), 1, 1);
            break;
        }
        case Icon::Step: {
            p.setBrush(color);
            p.drawPolygon(QPolygonF{QPointF(6, 5.5), QPointF(15, 12), QPointF(6, 18.5)});
            p.drawRoundedRect(QRectF(16, 5.5, 3, 13), 1, 1);
            break;
        }
        case Icon::Steering: {
            const QPointF c(12, 12);
            p.drawEllipse(c, 7, 7);
            p.drawEllipse(c, 2, 2);
            for (int spoke = 0; spoke < 6; ++spoke) {
                const double a = spoke * std::numbers::pi / 3.0;
                p.drawLine(c + QPointF(std::cos(a) * 2, std::sin(a) * 2),
                           c + QPointF(std::cos(a) * 10, std::sin(a) * 10));
            }
            break;
        }
        case Icon::Record: {
            p.setBrush(color);
            p.drawEllipse(QPointF(12, 12), 6.5, 6.5);
            break;
        }
        case Icon::Track: {
            QPainterPath route;
            route.moveTo(4, 19);
            route.lineTo(9, 9);
            route.lineTo(15, 14);
            route.lineTo(20, 5);
            p.drawPath(route);
            p.setBrush(color);
            for (const QPointF point :
                 {QPointF(4, 19), QPointF(9, 9), QPointF(15, 14), QPointF(20, 5)}) {
                p.drawEllipse(point, 1.6, 1.6);
            }
            break;
        }
        case Icon::Log: {
            for (int line = 0; line < 4; ++line) {
                const double y = 6 + line * 4;
                p.drawLine(QPointF(4, y), QPointF(5, y));
                p.drawLine(QPointF(8, y), QPointF(20, y));
            }
            break;
        }
        case Icon::Follow: {
            const QPointF c(12, 12);
            p.drawEllipse(c, 6, 6);
            p.drawLine(QPointF(12, 2.5), QPointF(12, 7));
            p.drawLine(QPointF(12, 17), QPointF(12, 21.5));
            p.drawLine(QPointF(2.5, 12), QPointF(7, 12));
            p.drawLine(QPointF(17, 12), QPointF(21.5, 12));
            p.setBrush(color);
            p.drawEllipse(c, 1.5, 1.5);
            break;
        }
        case Icon::Destination: {
            p.drawPolygon(QPolygonF{QPointF(12, 3.5), QPointF(20.5, 12), QPointF(12, 20.5),
                                    QPointF(3.5, 12)});
            p.setBrush(color);
            p.drawEllipse(QPointF(12, 12), 2.2, 2.2);
            break;
        }
    }
}

/// Renders one symbol into a square pixmap.
///
/// @param icon Symbol to draw.
/// @param color Colour of strokes and fills.
/// @param size Edge length in device-independent pixels, greater than 0.
/// @param ratio Device pixel ratio: 1 for normal screens, 2 for HiDPI.
/// @return A transparent pixmap of `size` times `ratio` physical pixels, rounded, with that
///   device pixel ratio set.
QPixmap render(Icon icon, const QColor& color, int size, double ratio) {
    QPixmap pixmap(static_cast<int>(std::lround(size * ratio)),
                   static_cast<int>(std::lround(size * ratio)));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);
    draw(painter, icon, color);
    return pixmap;
}

}  // namespace

QIcon make_icon(Icon icon, const QColor& color, const QColor& active, const QColor& disabled) {
    QIcon result;
    for (const int size : {16, 20, 24, 32}) {
        for (const double ratio : {1.0, 2.0}) {
            result.addPixmap(render(icon, color, size, ratio), QIcon::Normal, QIcon::Off);
            result.addPixmap(render(icon, active, size, ratio), QIcon::Normal, QIcon::On);
            result.addPixmap(render(icon, disabled, size, ratio), QIcon::Disabled, QIcon::Off);
            result.addPixmap(render(icon, disabled, size, ratio), QIcon::Disabled, QIcon::On);
        }
    }
    return result;
}

QIcon themed_icon(Icon icon) {
    const Colors& c = Theme::instance().colors();
    if (icon == Icon::Record) {
        return make_icon(icon, c.danger, c.danger, c.inactive);
    }
    return make_icon(icon, c.text, c.accent, c.inactive);
}

}  // namespace nmeasim::app::theme
