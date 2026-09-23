#include "map_widget.hpp"

#include "theme/theme.hpp"
#include "tile_cache.hpp"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace nmeasim::app::map {

namespace {

constexpr int kMaxTrackPoints{5000};
constexpr int kMaxParentLevels{4};
constexpr double kTrackMinPixelDistance{2.0};
/// Routes with more points than this are drawn without point markers.
constexpr qsizetype kMaxRouteMarkers{500};

}  // namespace

MapWidget::MapWidget(TileCache* cache, QWidget* parent) : QWidget(parent), cache_(cache) {
    setMinimumSize(200, 150);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
    setAutoFillBackground(false);
    connect(cache_, &TileCache::tile_ready, this, [this](const TileKey&) { update(); });
    connect(&theme::Theme::instance(), &theme::Theme::changed, this, qOverload<>(&QWidget::update));
}

void MapWidget::set_center(core::geo::Position center) {
    center_ = {std::clamp(center.latitude_deg, -kMaxLatitudeDeg, kMaxLatitudeDeg),
               std::fmod(center.longitude_deg + 540.0, 360.0) - 180.0};
    update();
    emit view_changed();
}

void MapWidget::set_zoom(int zoom) {
    const int clamped = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (clamped == zoom_) {
        return;
    }
    zoom_ = clamped;
    update();
    emit view_changed();
}

void MapWidget::zoom_by(int steps, QPoint anchor) {
    const int target = std::clamp(zoom_ + steps, kMinZoom, kMaxZoom);
    if (target == zoom_) {
        return;
    }
    const auto anchored = position_at(anchor);
    zoom_ = target;
    if (!follow_) {
        // Keep the position under the cursor where it was.
        const QPointF anchor_px = pixel_coordinates(anchored, zoom_);
        const QPointF offset = QPointF(anchor) - QPointF(width(), height()) / 2.0;
        center_ = position_of_pixel(anchor_px - offset, zoom_);
    }
    update();
    emit view_changed();
}

void MapWidget::set_follow_vessel(bool follow) {
    if (follow_ == follow) {
        return;
    }
    follow_ = follow;
    if (follow_ && vessel_) {
        set_center(vessel_->position);
    }
    update();
    emit follow_changed(follow_);
}

void MapWidget::set_vessel(core::geo::Position position, double heading_true_deg,
                           double course_over_ground_deg) {
    vessel_ = Vessel{position, heading_true_deg, course_over_ground_deg};
    if (track_.isEmpty() || track_broken_) {
        track_.append(QList<core::geo::Position>{position});
        track_broken_ = false;
    } else {
        auto& segment = track_.last();
        const QPointF last = pixel_coordinates(segment.last(), zoom_);
        const QPointF now = pixel_coordinates(position, zoom_);
        const QPointF delta = now - last;
        if (std::hypot(delta.x(), delta.y()) >= kTrackMinPixelDistance) {
            segment.append(position);
        }
    }
    // Drop the oldest points once the whole track grows beyond its limit.
    int excess = track_length() - kMaxTrackPoints;
    while (excess > 0 && !track_.isEmpty()) {
        auto& oldest = track_.first();
        const auto removed = std::min<qsizetype>(excess, oldest.size());
        oldest.remove(0, removed);
        excess -= static_cast<int>(removed);
        if (oldest.isEmpty()) {
            track_.removeFirst();
        }
    }
    if (follow_) {
        center_ = position;
    }
    update();
}

std::optional<core::geo::Position> MapWidget::vessel_position() const noexcept {
    if (!vessel_) {
        return std::nullopt;
    }
    return vessel_->position;
}

void MapWidget::clear_track() {
    track_.clear();
    track_broken_ = false;
    update();
}

void MapWidget::break_track() {
    track_broken_ = true;
}

int MapWidget::track_length() const noexcept {
    qsizetype points = 0;
    for (const auto& segment : track_) {
        points += segment.size();
    }
    return static_cast<int>(points);
}

void MapWidget::set_route(const QList<core::geo::Position>& route) {
    route_ = route;
    update();
}

void MapWidget::clear_route() {
    route_.clear();
    update();
}

void MapWidget::set_destination(std::optional<core::geo::Position> destination,
                                std::optional<core::geo::Position> origin) {
    destination_ = destination;
    leg_origin_ = destination ? origin : std::nullopt;
    update();
}

QPointF MapWidget::center_pixel() const {
    return pixel_coordinates(center_, zoom_);
}

QPointF MapWidget::point_of(core::geo::Position position) const {
    const QPointF origin = center_pixel() - QPointF(width(), height()) / 2.0;
    return pixel_coordinates(position, zoom_) - origin;
}

core::geo::Position MapWidget::position_at(QPointF point) const {
    const QPointF origin = center_pixel() - QPointF(width(), height()) / 2.0;
    return position_of_pixel(origin + point, zoom_);
}

double MapWidget::scale_m_per_px() const {
    return metres_per_pixel(center_.latitude_deg, zoom_);
}

void MapWidget::pan_by(QPointF delta_px) {
    center_ = position_of_pixel(center_pixel() - delta_px, zoom_);
    center_.latitude_deg = std::clamp(center_.latitude_deg, -kMaxLatitudeDeg, kMaxLatitudeDeg);
    update();
    emit view_changed();
}

// Painting -----------------------------------------------------------------------------------

void MapWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    const auto& colors = theme::Theme::instance().colors();
    painter.fillRect(rect(), colors.dark ? colors.inset : QColor(0xe8, 0xe8, 0xe8));
    draw_tiles(painter);
    if (colors.map_dimming > 0.0) {
        // Darken the chart at night so that it does not dazzle next to the dark panels.
        QColor shade = colors.window;
        shade.setAlphaF(static_cast<float>(colors.map_dimming));
        painter.fillRect(rect(), shade);
    }
    painter.setRenderHint(QPainter::Antialiasing);
    draw_route(painter);
    draw_track(painter);
    draw_destination(painter);
    draw_vessel(painter);
    draw_overlay(painter);
}

void MapWidget::draw_tiles(QPainter& painter) {
    const QPointF origin = center_pixel() - QPointF(width(), height()) / 2.0;
    const int n = tiles_at(zoom_);
    const int first_x = static_cast<int>(std::floor(origin.x() / kTileSize));
    const int first_y = static_cast<int>(std::floor(origin.y() / kTileSize));
    const int last_x = static_cast<int>(std::floor((origin.x() + width()) / kTileSize));
    const int last_y = static_cast<int>(std::floor((origin.y() + height()) / kTileSize));
    for (int ty = std::max(first_y, 0); ty <= std::min(last_y, n - 1); ++ty) {
        for (int tx = first_x; tx <= last_x; ++tx) {
            const QRect target(QPoint(static_cast<int>(std::lround(tx * kTileSize - origin.x())),
                                      static_cast<int>(std::lround(ty * kTileSize - origin.y()))),
                               QSize(kTileSize, kTileSize));
            int wrapped_x = tx % n;
            if (wrapped_x < 0) {
                wrapped_x += n;
            }
            draw_tile(painter, TileKey{zoom_, wrapped_x, ty}, target);
        }
    }
}

void MapWidget::draw_tile(QPainter& painter, TileKey key, const QRect& target) {
    if (const auto pixmap = cache_->tile(key)) {
        painter.drawPixmap(target, *pixmap);
        return;
    }
    // Fall back to the part of the nearest cached ancestor that covers this tile.
    TileKey ancestor = key;
    int levels = 0;
    while (ancestor.zoom > kMinZoom && levels < kMaxParentLevels) {
        ancestor = parent_of(ancestor);
        ++levels;
        if (!cache_->is_cached(ancestor)) {
            continue;
        }
        if (const auto pixmap = cache_->tile(ancestor)) {
            const int scale = 1 << levels;
            const int part = kTileSize / scale;
            const QRect source((key.x % scale) * part, (key.y % scale) * part, part, part);
            painter.drawPixmap(target, *pixmap, source);
            return;
        }
    }
    painter.fillRect(target, QColor(0xdd, 0xdd, 0xdd));
    painter.setPen(QColor(0xc8, 0xc8, 0xc8));
    painter.drawRect(target.adjusted(0, 0, -1, -1));
}

void MapWidget::draw_route(QPainter& painter) {
    if (route_.isEmpty()) {
        return;
    }
    QPainterPath path;
    path.moveTo(point_of(route_.first()));
    for (qsizetype index = 1; index < route_.size(); ++index) {
        path.lineTo(point_of(route_.at(index)));
    }
    const auto& colors = theme::Theme::instance().colors();
    QColor line = colors.map_route;
    line.setAlpha(0xd0);
    painter.setPen(QPen(line, 2.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    if (route_.size() <= kMaxRouteMarkers) {
        painter.setPen(QPen(colors.map_route.darker(140), 1.0));
        painter.setBrush(colors.panel);
        for (const auto& position : route_) {
            painter.drawEllipse(point_of(position), 3.0, 3.0);
        }
    }
}

void MapWidget::draw_track(QPainter& painter) {
    QPainterPath path;
    for (const auto& segment : track_) {
        if (segment.size() < 2) {
            continue;
        }
        path.moveTo(point_of(segment.first()));
        for (qsizetype index = 1; index < segment.size(); ++index) {
            path.lineTo(point_of(segment.at(index)));
        }
    }
    if (path.isEmpty()) {
        return;
    }
    QColor track = theme::Theme::instance().colors().map_track;
    track.setAlpha(0xc0);
    painter.setPen(QPen(track, 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

void MapWidget::draw_destination(QPainter& painter) {
    if (!destination_) {
        return;
    }
    const QPointF at = point_of(*destination_);
    const auto& colors = theme::Theme::instance().colors();
    const QColor magenta = colors.map_destination;
    if (leg_origin_) {
        QColor leg = magenta;
        leg.setAlpha(0x90);
        painter.setPen(QPen(leg, 1.0, Qt::DotLine));
        painter.drawLine(point_of(*leg_origin_), at);
    }
    if (vessel_) {
        painter.setPen(QPen(magenta, 1.5, Qt::DashLine));
        painter.drawLine(point_of(vessel_->position), at);
    }
    QPolygonF diamond;
    diamond << at + QPointF(0, -8) << at + QPointF(8, 0) << at + QPointF(0, 8)
            << at + QPointF(-8, 0);
    painter.setPen(QPen(colors.map_vessel_outline, 1.0));
    painter.setBrush(magenta);
    painter.drawPolygon(diamond);
}

void MapWidget::draw_vessel(QPainter& painter) {
    if (!vessel_) {
        return;
    }
    const QPointF at = point_of(vessel_->position);
    painter.save();
    painter.translate(at);
    // Course over ground as a thin line ahead of the vessel.
    painter.rotate(vessel_->course_over_ground_deg);
    const auto& colors = theme::Theme::instance().colors();
    painter.setPen(QPen(colors.accent, 1.5, Qt::DashLine));
    painter.drawLine(QPointF(0, 0), QPointF(0, -40));
    painter.rotate(vessel_->heading_true_deg - vessel_->course_over_ground_deg);
    QPolygonF hull;
    hull << QPointF(0, -14) << QPointF(8, 10) << QPointF(0, 5) << QPointF(-8, 10);
    painter.setPen(QPen(colors.map_vessel_outline, 1.0));
    painter.setBrush(colors.map_vessel);
    painter.drawPolygon(hull);
    painter.restore();
}

void MapWidget::draw_overlay(QPainter& painter) {
    const auto& colors = theme::Theme::instance().colors();
    QColor box = colors.panel;
    box.setAlpha(0xd8);
    painter.setPen(colors.text);
    const QString attribution = QStringLiteral("© OpenStreetMap contributors");
    const QRect text_rect = painter.fontMetrics().boundingRect(attribution).adjusted(-4, -2, 4, 2);
    const QRect at(width() - text_rect.width() - 4, height() - text_rect.height() - 4,
                   text_rect.width(), text_rect.height());
    painter.fillRect(at, box);
    painter.drawText(at, Qt::AlignCenter, attribution);

    QStringList status;
    status << tr("z%1").arg(zoom_);
    if (!cache_->online()) {
        status << tr("offline");
    }
    if (!follow_) {
        status << tr("free view");
    }
    if (destination_) {
        status << tr("destination set");
    }
    const QString text = status.join(QStringLiteral("  "));
    const QRect status_rect = painter.fontMetrics().boundingRect(text).adjusted(-4, -2, 4, 2);
    const QRect status_at(4, 4, status_rect.width(), status_rect.height());
    painter.fillRect(status_at, box);
    painter.drawText(status_at, Qt::AlignCenter, text);
}

// Interaction --------------------------------------------------------------------------------

void MapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            emit destination_picked(position_at(event->position()));
            return;
        }
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit position_picked(position_at(event->position()));
            return;
        }
        drag_last_ = event->position().toPoint();
        dragged_ = false;
        setCursor(Qt::ClosedHandCursor);
    }
    setFocus();
}

void MapWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!drag_last_) {
        return;
    }
    const QPoint now = event->position().toPoint();
    const QPoint delta = now - *drag_last_;
    if (!delta.isNull()) {
        dragged_ = true;
        if (follow_) {
            follow_ = false;
            emit follow_changed(false);
        }
        drag_last_ = now;
        pan_by(QPointF(delta));
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        drag_last_.reset();
        unsetCursor();
    }
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit position_picked(position_at(event->position()));
    }
}

void MapWidget::contextMenuEvent(QContextMenuEvent* event) {
    const auto position = position_at(event->pos());
    QMenu menu(this);
    menu.addAction(tr("Move vessel here"), this,
                   [this, position] { emit position_picked(position); });
    menu.addAction(tr("Set destination here"), this,
                   [this, position] { emit destination_picked(position); });
    auto* clear =
        menu.addAction(tr("Clear destination"), this, [this] { emit destination_cleared(); });
    clear->setEnabled(destination_.has_value());
    menu.exec(event->globalPos());
}

void MapWidget::wheelEvent(QWheelEvent* event) {
    // A mouse wheel reports 120 per notch, but touchpads and high-resolution wheels report
    // many smaller deltas; they add up here until they amount to a whole zoom level.
    wheel_remainder_ += event->angleDelta().y();
    const int steps = wheel_remainder_ / 120;
    if (steps != 0) {
        wheel_remainder_ -= steps * 120;
        zoom_by(steps, event->position().toPoint());
    }
    event->accept();
}

void MapWidget::keyPressEvent(QKeyEvent* event) {
    const QPoint middle(width() / 2, height() / 2);
    switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoom_by(1, middle);
            return;
        case Qt::Key_Minus:
            zoom_by(-1, middle);
            return;
        case Qt::Key_Home:
            set_follow_vessel(true);
            return;
        default:
            QWidget::keyPressEvent(event);
    }
}

void MapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

}  // namespace nmeasim::app::map
