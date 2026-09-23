#pragma once

#include "tile_math.hpp"

#include <nmeasim/core/geo/geodesic.hpp>

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <optional>

namespace nmeasim::app::map {

class TileCache;

/// A slippy map: raster tiles from a `TileCache`, the vessel with its heading, the track it
/// has sailed, and mouse and keyboard navigation.
///
/// The sailed track is a list of segments: moving the vessel by hand or seeking in a track or
/// log starts a new segment, so that no line joins the old and the new position.
///
/// The widget keeps a centre position and an integer zoom level. In follow mode the centre
/// tracks the vessel; dragging the map switches follow mode off.
class MapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MapWidget(TileCache* cache, QWidget* parent = nullptr);

    [[nodiscard]] TileCache* cache() const noexcept { return cache_; }

    [[nodiscard]] core::geo::Position center() const noexcept { return center_; }
    void set_center(core::geo::Position center);
    [[nodiscard]] int zoom() const noexcept { return zoom_; }
    void set_zoom(int zoom);
    /// Zooms by `steps` levels keeping the position under `anchor` (widget pixels) fixed.
    void zoom_by(int steps, QPoint anchor);

    [[nodiscard]] bool follows_vessel() const noexcept { return follow_; }
    void set_follow_vessel(bool follow);

    /// Places the vessel. In follow mode the map recentres on it.
    void set_vessel(core::geo::Position position, double heading_true_deg,
                    double course_over_ground_deg);
    [[nodiscard]] std::optional<core::geo::Position> vessel_position() const noexcept;
    void clear_track();
    /// Ends the current track segment; the next vessel position starts a new one.
    void break_track();
    /// Number of points in all track segments.
    [[nodiscard]] int track_length() const noexcept;
    [[nodiscard]] int track_segment_count() const noexcept {
        return static_cast<int>(track_.size());
    }

    /// The track or route loaded from a file, drawn under the sailed track.
    void set_route(const QList<core::geo::Position>& route);
    void clear_route();
    [[nodiscard]] int route_length() const noexcept { return static_cast<int>(route_.size()); }

    /// The destination waypoint and the origin of its leg, drawn with the bearing line from
    /// the vessel; nullopt removes them.
    void set_destination(std::optional<core::geo::Position> destination,
                         std::optional<core::geo::Position> origin);
    [[nodiscard]] std::optional<core::geo::Position> destination() const noexcept {
        return destination_;
    }

    /// Widget pixel of a position at the current view.
    [[nodiscard]] QPointF point_of(core::geo::Position position) const;
    /// Position under a widget pixel at the current view.
    [[nodiscard]] core::geo::Position position_at(QPointF point) const;

    /// Metres per pixel at the centre latitude.
    [[nodiscard]] double scale_m_per_px() const;

signals:
    /// The operator double-clicked (or Ctrl+clicked) the map to move the vessel there.
    void position_picked(nmeasim::core::geo::Position position);
    /// The operator Shift+clicked the map, or chose *Set destination here*, to steer for
    /// that point.
    void destination_picked(nmeasim::core::geo::Position position);
    /// The operator chose *Clear destination*.
    void destination_cleared();
    void view_changed();
    void follow_changed(bool follow);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Vessel {
        core::geo::Position position;
        double heading_true_deg{0.0};
        double course_over_ground_deg{0.0};
    };

    void draw_tiles(class QPainter& painter);
    void draw_route(QPainter& painter);
    void draw_track(QPainter& painter);
    void draw_destination(QPainter& painter);
    void draw_vessel(QPainter& painter);
    void draw_overlay(QPainter& painter);
    /// Draws a tile, falling back to a scaled part of an ancestor when it is not cached.
    void draw_tile(QPainter& painter, TileKey key, const QRect& target);
    [[nodiscard]] QPointF center_pixel() const;
    void pan_by(QPointF delta_px);

    TileCache* cache_;
    core::geo::Position center_{37.9838, 23.7275};
    int zoom_{12};
    bool follow_{true};
    std::optional<Vessel> vessel_;
    QList<QList<core::geo::Position>> track_;
    bool track_broken_{false};
    QList<core::geo::Position> route_;
    std::optional<core::geo::Position> destination_;
    std::optional<core::geo::Position> leg_origin_;
    std::optional<QPoint> drag_last_;
    bool dragged_{false};
    /// Wheel rotation not yet turned into whole zoom levels, in eighths of a degree.
    int wheel_remainder_{0};
};

}  // namespace nmeasim::app::map
