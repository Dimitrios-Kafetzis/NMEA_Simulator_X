// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Painting, view arithmetic and input handling of `MapWidget`, and the scale bar choice.
///
/// Positions are converted to world pixels at the fractional zoom by scaling the pixel
/// coordinates of the nearest whole level, the level the tiles are taken from, so the tiles,
/// the vessel and the overlays always agree.

#include "map_widget.hpp"

#include "theme/icons.hpp"
#include "theme/theme.hpp"
#include "tile_cache.hpp"
#include "widgets/dashboard_widget.hpp"

#include <nmeasim/core/units.hpp>

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QToolButton>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace nmeasim::app::map {

namespace {

/// Largest number of points in all track segments together; beyond it the oldest points are
/// dropped, which bounds memory and painting cost.
constexpr int kMaxTrackPoints{5000};
/// Number of zoom levels `MapWidget::draw_tile` looks up for a cached ancestor of a missing
/// tile; four levels up, one ancestor pixel is stretched over 16 by 16 screen pixels.
constexpr int kMaxParentLevels{4};
/// Smallest distance in pixels, at the zoom of the moment, between a new vessel position and
/// the last point of the track for the position to be added.
constexpr double kTrackMinPixelDistance{2.0};
/// Routes with more points than this are drawn without point markers.
constexpr qsizetype kMaxRouteMarkers{500};
/// The course vector ends where the vessel will be after this many hours: six minutes, a
/// tenth of an hour, so that the vector in nautical miles is a tenth of the speed in knots.
constexpr double kVectorHours{0.1};
/// Longest course vector in pixels; it caps the vector of a fast vessel at high zoom.
constexpr double kMaxVectorPx{400.0};
/// Width and height of the on-map buttons in pixels.
constexpr int kButtonSize{28};
/// Distance in pixels of the on-map buttons from the top and right edges.
constexpr int kButtonMargin{8};
/// Distance in pixels of the overlay boxes from the edges of the widget.
constexpr int kOverlayMargin{6};

/// Creates one of the square on-map buttons.
///
/// The button never takes keyboard focus, so that the map keeps it for its keys, and is
/// styled through its object name `map_button`.
///
/// @param parent Widget that owns the button.
/// @param text Caption; empty for a button that shows an icon.
/// @param tip Tooltip, which names the keyboard shortcut.
/// @return The new button, owned by `parent`.
QToolButton* make_button(QWidget* parent, const QString& text, const QString& tip) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("map_button"));
    button->setText(text);
    button->setToolTip(tip);
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAutoRaise(false);
    return button;
}

/// Returns the rectangle of a box that fits overlay text, placed with one corner on an
/// anchor.
///
/// The box is the text's bounding box with 6 pixels of padding left and right and 3 above
/// and below; the caller fills it translucently behind the text so that the text reads on
/// any chart.
///
/// @param painter Painter whose current font measures the text.
/// @param text Text the box must hold.
/// @param anchor Widget pixel where the chosen corner of the box goes.
/// @param corner Corner placed on `anchor`: `Qt::AlignRight` or left, combined with
///     `Qt::AlignBottom` or top.
/// @return The box in widget pixels.
QRect overlay_box(QPainter& painter, const QString& text, QPoint anchor, Qt::Alignment corner) {
    const QRect bounds = painter.fontMetrics().boundingRect(text).adjusted(-6, -3, 6, 3);
    QRect box(QPoint(0, 0), bounds.size());
    if (corner.testFlag(Qt::AlignRight)) {
        box.moveRight(anchor.x());
    } else {
        box.moveLeft(anchor.x());
    }
    if (corner.testFlag(Qt::AlignBottom)) {
        box.moveBottom(anchor.y());
    } else {
        box.moveTop(anchor.y());
    }
    return box;
}

}  // namespace

QString default_attribution(const QString& url_template) {
    // Braces are not allowed in a URL, and a placeholder may sit in the host name, as in
    // {s}.tile.example.org; replacing every placeholder first keeps the URL parsable.
    QString url = url_template;
    url.replace(QRegularExpression(QStringLiteral("\\{[^}]*\\}")), QStringLiteral("0"));
    const QString host = QUrl(url).host().toLower();
    const auto osm = QStringLiteral("openstreetmap.org");
    if (host == osm || host.endsWith(QLatin1Char('.') + osm)) {
        return QStringLiteral("© OpenStreetMap contributors");
    }
    return {};
}

ScaleBar scale_bar(double metres_per_pixel, double max_length_px) {
    if (!(metres_per_pixel > 0.0) || !(max_length_px > 0.0)) {
        return {};
    }
    constexpr std::array<double, 15> kNauticalMiles{0.1,   0.2,   0.5,    1.0,    2.0,
                                                    5.0,   10.0,  20.0,   50.0,   100.0,
                                                    200.0, 500.0, 1000.0, 2000.0, 5000.0};
    constexpr std::array<double, 4> kMetres{10.0, 20.0, 50.0, 100.0};
    ScaleBar best;
    for (const double metres : kMetres) {
        const double length = metres / metres_per_pixel;
        if (length <= max_length_px) {
            best = {length, QStringLiteral("%1 m").arg(metres, 0, 'f', 0)};
        }
    }
    for (const double miles : kNauticalMiles) {
        const double length = miles * core::units::kMetresPerNauticalMile / metres_per_pixel;
        if (length <= max_length_px) {
            best = {length, QStringLiteral("%1 nm").arg(QString::number(miles, 'g', 4))};
        }
    }
    return best;
}

MapWidget::MapWidget(TileCache* cache, QWidget* parent)
    : QWidget(parent),
      cache_(cache),
      zoom_in_button_(make_button(this, QStringLiteral("+"), tr("Zoom in (+)"))),
      zoom_out_button_(make_button(this, QStringLiteral("−"), tr("Zoom out (-)"))),
      follow_button_(make_button(this, QString{}, tr("Follow the vessel (Home)"))) {
    setMinimumSize(200, 150);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_AcceptTouchEvents);
    follow_button_->setCheckable(true);
    follow_button_->setChecked(follow_);
    follow_button_->setIcon(theme::themed_icon(theme::Icon::Follow));
    connect(zoom_in_button_, &QToolButton::clicked, this,
            [this] { zoom_by(1, QPoint(width() / 2, height() / 2)); });
    connect(zoom_out_button_, &QToolButton::clicked, this,
            [this] { zoom_by(-1, QPoint(width() / 2, height() / 2)); });
    connect(follow_button_, &QToolButton::toggled, this, &MapWidget::set_follow_vessel);
    connect(cache_, &TileCache::tile_ready, this, [this](const TileKey&) { update(); });
    connect(&theme::Theme::instance(), &theme::Theme::changed, this, [this] {
        follow_button_->setIcon(theme::themed_icon(theme::Icon::Follow));
        update();
    });
    place_buttons();
}

QString MapWidget::attribution() const {
    return attribution_.value_or(default_attribution(cache_->url_template()));
}

void MapWidget::set_attribution(std::optional<QString> attribution) {
    attribution_ = std::move(attribution);
    update();
}

void MapWidget::set_center(core::geo::Position center) {
    center_ = {std::clamp(center.latitude_deg, -kMaxLatitudeDeg, kMaxLatitudeDeg),
               wrap_longitude(center.longitude_deg)};
    update();
    emit view_changed();
}

int MapWidget::zoom() const noexcept {
    return static_cast<int>(std::lround(zoom_));
}

void MapWidget::set_zoom(int zoom) {
    const double clamped = std::clamp(static_cast<double>(zoom), static_cast<double>(kMinZoom),
                                      static_cast<double>(kMaxZoom));
    if (clamped == zoom_) {
        return;
    }
    zoom_ = clamped;
    update();
    emit view_changed();
}

void MapWidget::zoom_by(int steps, QPoint anchor) {
    zoom_to(std::round(zoom_) + steps, QPointF(anchor));
}

void MapWidget::zoom_to(double level, QPointF anchor) {
    const double target =
        std::clamp(level, static_cast<double>(kMinZoom), static_cast<double>(kMaxZoom));
    if (std::fabs(target - zoom_) < 1e-9) {
        return;
    }
    const auto anchored = position_at(anchor);
    zoom_ = target;
    if (!follow_) {
        // Move the centre so that the anchor still shows the same position; in follow mode
        // the vessel must stay centred instead.
        const QPointF offset = anchor - QPointF(width(), height()) / 2.0;
        center_ = position_of_world(world_pixel(anchored) - offset);
    }
    update();
    emit view_changed();
}

void MapWidget::set_follow_vessel(bool follow) {
    if (follow_ == follow) {
        return;
    }
    follow_ = follow;
    {
        const QSignalBlocker blocker(follow_button_);
        follow_button_->setChecked(follow_);
    }
    if (follow_ && vessel_) {
        set_center(vessel_->position);
    }
    update();
    emit follow_changed(follow_);
}

void MapWidget::set_vessel(core::geo::Position position, double heading_true_deg,
                           double course_over_ground_deg, double speed_over_ground_kn) {
    vessel_ = Vessel{position, heading_true_deg, course_over_ground_deg, speed_over_ground_kn};
    if (track_.isEmpty() || track_broken_) {
        track_.append(QList<core::geo::Position>{position});
        track_broken_ = false;
    } else {
        auto& segment = track_.last();
        const QPointF delta = world_pixel(position) - world_pixel(segment.last());
        if (std::hypot(delta.x(), delta.y()) >= kTrackMinPixelDistance) {
            segment.append(position);
        }
    }
    // Trim from the oldest segment, which may disappear, so that a long run stays bounded.
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
        const core::geo::Position center{
            std::clamp(position.latitude_deg, -kMaxLatitudeDeg, kMaxLatitudeDeg),
            wrap_longitude(position.longitude_deg)};
        if (center.latitude_deg != center_.latitude_deg ||
            center.longitude_deg != center_.longitude_deg) {
            center_ = center;
            emit view_changed();
        }
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

int MapWidget::tile_zoom() const noexcept {
    return std::clamp(static_cast<int>(std::lround(zoom_)), kMinZoom, kMaxZoom);
}

double MapWidget::tile_scale() const noexcept {
    return std::exp2(zoom_ - tile_zoom());
}

QPointF MapWidget::world_pixel(core::geo::Position position) const {
    return pixel_coordinates(position, tile_zoom()) * tile_scale();
}

core::geo::Position MapWidget::position_of_world(QPointF pixel) const {
    auto position = position_of_pixel(pixel / tile_scale(), tile_zoom());
    position.latitude_deg = std::clamp(position.latitude_deg, -kMaxLatitudeDeg, kMaxLatitudeDeg);
    position.longitude_deg = wrap_longitude(position.longitude_deg);
    return position;
}

QPointF MapWidget::center_pixel() const {
    return world_pixel(center_);
}

QPointF MapWidget::point_of(core::geo::Position position) const {
    const QPointF center = center_pixel();
    QPointF pixel = world_pixel(position);
    // The world repeats east and west: take the copy of the position nearest to the centre,
    // so that the vessel and a track across the antimeridian stay in view.
    const double world_width = tiles_at(tile_zoom()) * kTileSize * tile_scale();
    pixel.rx() -= world_width * std::round((pixel.x() - center.x()) / world_width);
    return pixel - (center - QPointF(width(), height()) / 2.0);
}

core::geo::Position MapWidget::position_at(QPointF point) const {
    const QPointF origin = center_pixel() - QPointF(width(), height()) / 2.0;
    return position_of_world(origin + point);
}

double MapWidget::scale_m_per_px() const {
    return metres_per_pixel(center_.latitude_deg, tile_zoom()) / tile_scale();
}

double MapWidget::course_vector_length_px() const {
    if (!vessel_) {
        return 0.0;
    }
    const double metres =
        vessel_->speed_over_ground_kn * kVectorHours * core::units::kMetresPerNauticalMile;
    return std::min(metres / scale_m_per_px(), kMaxVectorPx);
}

void MapWidget::pan_by(QPointF delta_px) {
    center_ = position_of_world(center_pixel() - delta_px);
    update();
    emit view_changed();
}

void MapWidget::place_buttons() {
    const int x = width() - kButtonMargin - kButtonSize;
    zoom_in_button_->move(x, kButtonMargin);
    zoom_out_button_->move(x, kButtonMargin + kButtonSize + 4);
    follow_button_->move(x, kButtonMargin + 2 * (kButtonSize + 4) + 6);
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
    const int level = tile_zoom();
    const double size = kTileSize * tile_scale();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, std::fabs(size - kTileSize) > 0.01);
    const QPointF origin = center_pixel() - QPointF(width(), height()) / 2.0;
    const int n = tiles_at(level);
    const auto first_x = static_cast<int>(std::floor(origin.x() / size));
    const auto first_y = static_cast<int>(std::floor(origin.y() / size));
    const auto last_x = static_cast<int>(std::floor((origin.x() + width()) / size));
    const auto last_y = static_cast<int>(std::floor((origin.y() + height()) / size));
    for (int ty = std::max(first_y, 0); ty <= std::min(last_y, n - 1); ++ty) {
        for (int tx = first_x; tx <= last_x; ++tx) {
            // Edges are rounded independently so that scaled tiles meet without seams.
            const auto left = static_cast<int>(std::lround(tx * size - origin.x()));
            const auto top = static_cast<int>(std::lround(ty * size - origin.y()));
            const auto right = static_cast<int>(std::lround((tx + 1) * size - origin.x()));
            const auto bottom = static_cast<int>(std::lround((ty + 1) * size - origin.y()));
            int wrapped_x = tx % n;
            if (wrapped_x < 0) {
                wrapped_x += n;
            }
            draw_tile(painter, TileKey{level, wrapped_x, ty},
                      QRect(QPoint(left, top), QPoint(right - 1, bottom - 1)));
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
    painter.setPen(QPen(track, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
    const auto& colors = theme::Theme::instance().colors();
    const QPointF at = point_of(vessel_->position);
    painter.save();
    painter.translate(at);

    // Course and speed vector to the position six minutes ahead.
    const double vector = course_vector_length_px();
    if (vector > 2.0) {
        painter.save();
        painter.rotate(vessel_->course_over_ground_deg);
        painter.setPen(QPen(colors.accent, 1.8, Qt::DashLine, Qt::RoundCap));
        painter.drawLine(QPointF(0, 0), QPointF(0, -vector));
        painter.setPen(QPen(colors.accent, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(0, -vector), 3.0, 3.0);
        painter.restore();
    }

    painter.rotate(vessel_->heading_true_deg);
    // Heading line well past the bow.
    QColor heading_line = colors.text_value;
    heading_line.setAlpha(0xb0);
    painter.setPen(QPen(heading_line, 1.0));
    painter.drawLine(QPointF(0, -16), QPointF(0, -48));

    // Hull: a pointed bow, rounded shoulders and a square stern.
    QPainterPath hull;
    hull.moveTo(0, -17);
    hull.cubicTo(5, -10, 7.5, -2, 7, 11);
    hull.lineTo(-7, 11);
    hull.cubicTo(-7.5, -2, -5, -10, 0, -17);
    // A soft halo keeps the hull visible on busy charts.
    QColor halo = colors.map_vessel_outline;
    halo.setAlpha(0x60);
    painter.setPen(QPen(halo, 4.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(hull);
    painter.setPen(QPen(colors.map_vessel_outline, 1.2));
    painter.setBrush(colors.map_vessel);
    painter.drawPath(hull);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colors.map_vessel_outline);
    painter.drawEllipse(QPointF(0, 0), 1.8, 1.8);
    painter.restore();
}

void MapWidget::draw_overlay(QPainter& painter) {
    const auto& colors = theme::Theme::instance().colors();
    const QFont base_font = painter.font();
    QColor box_color = colors.panel;
    box_color.setAlpha(0xd8);
    const auto draw_box = [&](const QRect& box, const QString& text) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(box_color);
        painter.drawRoundedRect(box, 4, 4);
        painter.setPen(colors.text);
        painter.drawText(box, Qt::AlignCenter, text);
    };

    // Attribution of the tile server, bottom right; the position under the pointer above it,
    // or in the corner when there is no attribution.
    int readout_bottom = height() - 4;
    if (const QString text = attribution(); !text.isEmpty()) {
        const QRect attribution_box =
            overlay_box(painter, text, QPoint(width() - kOverlayMargin, readout_bottom),
                        Qt::AlignRight | Qt::AlignBottom);
        draw_box(attribution_box, text);
        readout_bottom = attribution_box.top() - 4;
    }
    if (pointer_) {
        painter.setFont(theme::Theme::mono_font());
        const QString text = format_position(*pointer_);
        draw_box(overlay_box(painter, text, QPoint(width() - kOverlayMargin, readout_bottom),
                             Qt::AlignRight | Qt::AlignBottom),
                 text);
        painter.setFont(base_font);
    }

    // Zoom level and view state, top left.
    QStringList status;
    status << tr("z%1").arg(zoom_, 0, 'f', std::fabs(zoom_ - std::round(zoom_)) < 0.05 ? 0 : 1);
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
    const QRect status_box = overlay_box(painter, text, QPoint(kOverlayMargin, kOverlayMargin),
                                         Qt::AlignLeft | Qt::AlignTop);
    draw_box(status_box, text);

    // North arrow under the status line: the chart is always north up.
    const QPointF north(kOverlayMargin + 14.0, status_box.bottom() + 26.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(box_color);
    painter.drawEllipse(north, 13.0, 13.0);
    painter.setBrush(colors.danger);
    painter.drawPolygon(
        QPolygonF{north + QPointF(0, -10), north + QPointF(5, 2), north + QPointF(-5, 2)});
    painter.setBrush(colors.text_dim);
    painter.drawPolygon(
        QPolygonF{north + QPointF(0, 10), north + QPointF(5, 2), north + QPointF(-5, 2)});
    QFont small = painter.font();
    small.setBold(true);
    small.setPixelSize(9);
    painter.setFont(small);
    painter.setPen(colors.text);
    painter.drawText(QRectF(north.x() + 12, north.y() - 16, 14, 12), Qt::AlignCenter,
                     QStringLiteral("N"));
    painter.setFont(base_font);

    // Scale bar, bottom left.
    const ScaleBar bar = scale_bar(scale_m_per_px(), std::min(160.0, width() * 0.35));
    if (bar.length_px > 0.0) {
        const QRect label = overlay_box(painter, bar.label, QPoint(kOverlayMargin, height() - 4),
                                        Qt::AlignLeft | Qt::AlignBottom);
        const double x0 = kOverlayMargin + 2.0;
        const double y = label.top() - 8.0;
        const QRectF backing(x0 - 4, y - 7, bar.length_px + 8, 12);
        painter.setPen(Qt::NoPen);
        painter.setBrush(box_color);
        painter.drawRoundedRect(backing, 3, 3);
        painter.setPen(QPen(colors.text, 2.0, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(QPointF(x0, y), QPointF(x0 + bar.length_px, y));
        painter.drawLine(QPointF(x0, y - 4), QPointF(x0, y + 3));
        painter.drawLine(QPointF(x0 + bar.length_px, y - 4), QPointF(x0 + bar.length_px, y + 3));
        draw_box(label, bar.label);
    }
}

// Interaction --------------------------------------------------------------------------------

bool MapWidget::event(QEvent* event) {
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            // Pinch on a touchpad: the value is the relative change of the magnification.
            zoom_to(zoom_ + std::log2(1.0 + gesture->value()), gesture->position());
            return true;
        }
    }
    return QWidget::event(event);
}

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
        setCursor(Qt::ClosedHandCursor);
    }
    setFocus();
}

void MapWidget::mouseMoveEvent(QMouseEvent* event) {
    pointer_ = position_at(event->position());
    if (!drag_last_) {
        update();
        return;
    }
    const QPoint now = event->position().toPoint();
    const QPoint delta = now - *drag_last_;
    if (!delta.isNull()) {
        if (follow_) {
            set_follow_vessel(false);
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

void MapWidget::leaveEvent(QEvent* event) {
    pointer_.reset();
    update();
    QWidget::leaveEvent(event);
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
    // A mouse wheel notch (120) zooms one level; touchpads and high-resolution wheels send
    // smaller deltas, which zoom by the matching fraction of a level.
    const int delta = event->angleDelta().y();
    if (delta != 0) {
        zoom_to(zoom_ + delta / 120.0, event->position());
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
    place_buttons();
    update();
}

}  // namespace nmeasim::app::map
