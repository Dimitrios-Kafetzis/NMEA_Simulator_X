#pragma once

#include "tile_math.hpp"

#include <nmeasim/core/geo/geodesic.hpp>

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QWidget>

#include <optional>

class QToolButton;

namespace nmeasim::app::map {

class TileCache;

/// Length and caption of the scale bar.
struct ScaleBar {
    /// Length of the bar in pixels.
    double length_px{0.0};
    /// Distance it stands for, e.g. `0.5 nm` or `50 m`.
    QString label;
};

/// The longest round distance that fits in `max_length_px` at `metres_per_pixel`: nautical
/// miles (0.1 to 2000 nm in 1, 2, 5 steps), or metres (10 to 100 m) below 0.1 nm.
[[nodiscard]] ScaleBar scale_bar(double metres_per_pixel, double max_length_px);

/// A slippy map: raster tiles from a `TileCache`, the vessel with its heading and a vector
/// of its course and speed, the track it has sailed, and mouse, touchpad and keyboard
/// navigation. Overlays show zoom buttons, a follow button, a scale bar, a north arrow and
/// the position under the pointer.
///
/// The zoom level is fractional, so the wheel, the touchpad and pinch gestures zoom smoothly:
/// tiles come from the nearest whole level and are scaled by the remaining factor. The
/// keyboard and the buttons step to whole levels. In follow mode the centre tracks the
/// vessel; dragging the map switches follow mode off.
///
/// The sailed track is a list of segments: moving the vessel by hand or seeking in a track or
/// log starts a new segment, so that no line joins the old and the new position.
class MapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MapWidget(TileCache* cache, QWidget* parent = nullptr);

    [[nodiscard]] TileCache* cache() const noexcept { return cache_; }

    [[nodiscard]] core::geo::Position center() const noexcept { return center_; }
    void set_center(core::geo::Position center);
    /// The zoom level rounded to a whole level.
    [[nodiscard]] int zoom() const noexcept;
    /// The fractional zoom level.
    [[nodiscard]] double zoom_level() const noexcept { return zoom_; }
    void set_zoom(int zoom);
    /// Zooms by `steps` whole levels keeping the position under `anchor` (widget pixels)
    /// fixed; the result is a whole level.
    void zoom_by(int steps, QPoint anchor);
    /// Zooms to a fractional `level` keeping the position under `anchor` fixed, or keeping
    /// the vessel in the centre in follow mode.
    void zoom_to(double level, QPointF anchor);

    [[nodiscard]] bool follows_vessel() const noexcept { return follow_; }
    void set_follow_vessel(bool follow);

    /// Places the vessel. In follow mode the map recentres on it. The speed over ground sets
    /// the length of the course vector, which ends where the vessel will be in six minutes.
    void set_vessel(core::geo::Position position, double heading_true_deg,
                    double course_over_ground_deg, double speed_over_ground_kn = 0.0);
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
    /// Length in pixels of the course vector drawn ahead of the vessel.
    [[nodiscard]] double course_vector_length_px() const;
    /// Position under the pointer while it is over the map.
    [[nodiscard]] std::optional<core::geo::Position> pointer_position() const noexcept {
        return pointer_;
    }

    /// The on-map buttons, for tests.
    [[nodiscard]] QToolButton* zoom_in_button() const noexcept { return zoom_in_button_; }
    [[nodiscard]] QToolButton* zoom_out_button() const noexcept { return zoom_out_button_; }
    [[nodiscard]] QToolButton* follow_button() const noexcept { return follow_button_; }

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
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Vessel {
        core::geo::Position position;
        double heading_true_deg{0.0};
        double course_over_ground_deg{0.0};
        double speed_over_ground_kn{0.0};
    };

    void draw_tiles(class QPainter& painter);
    void draw_route(QPainter& painter);
    void draw_track(QPainter& painter);
    void draw_destination(QPainter& painter);
    void draw_vessel(QPainter& painter);
    void draw_overlay(QPainter& painter);
    /// Draws a tile, falling back to a scaled part of an ancestor when it is not cached.
    void draw_tile(QPainter& painter, TileKey key, const QRect& target);
    /// The whole zoom level tiles are taken from, and the factor they are scaled by.
    [[nodiscard]] int tile_zoom() const noexcept;
    [[nodiscard]] double tile_scale() const noexcept;
    /// Pixel coordinates of the whole map at the current fractional zoom, and back.
    [[nodiscard]] QPointF world_pixel(core::geo::Position position) const;
    [[nodiscard]] core::geo::Position position_of_world(QPointF pixel) const;
    [[nodiscard]] QPointF center_pixel() const;
    void pan_by(QPointF delta_px);
    void place_buttons();

    TileCache* cache_;
    core::geo::Position center_{37.9838, 23.7275};
    double zoom_{12.0};
    bool follow_{true};
    std::optional<Vessel> vessel_;
    QList<QList<core::geo::Position>> track_;
    bool track_broken_{false};
    QList<core::geo::Position> route_;
    std::optional<core::geo::Position> destination_;
    std::optional<core::geo::Position> leg_origin_;
    std::optional<core::geo::Position> pointer_;
    std::optional<QPoint> drag_last_;
    bool dragged_{false};
    QToolButton* zoom_in_button_;
    QToolButton* zoom_out_button_;
    QToolButton* follow_button_;
};

}  // namespace nmeasim::app::map
