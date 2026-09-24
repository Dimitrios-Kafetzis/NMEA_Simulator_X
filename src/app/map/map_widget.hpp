// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The map view of the desktop application, `MapWidget`, and the scale bar it draws.
///
/// `MapWidget` paints OpenStreetMap-style raster tiles from a `TileCache` and draws the
/// vessel, its track, a loaded route, the destination and the overlays over them in the same
/// pass, as ADR 0010 decides. `scale_bar` picks the round distance the scale bar shows.

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

/// The slippy-map view of the desktop application, part of `nmeasim::app`.
///
/// It holds `MapWidget`, which paints raster tiles with the vessel, its track and the
/// navigation overlays; `TileCache`, which serves the tiles from memory, from a disk cache or
/// from a tile server over the network; and the Web Mercator (EPSG:3857) tile arithmetic both
/// rely on. Positions are WGS 84 latitudes and longitudes in degrees; widget and world
/// coordinates are pixels with x to the right and y downwards; zoom levels follow the slippy
/// map scheme, in which each level doubles the scale and a tile is 256 pixels square.
///
/// @see https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
namespace nmeasim::app::map {

class TileCache;

/// Length and caption of the scale bar.
///
/// The default, a zero length and an empty label, means that no bar is drawn.
struct ScaleBar {
    /// Length of the bar in pixels.
    double length_px{0.0};
    /// Distance the bar stands for, for example `0.5 nm` or `50 m`.
    QString label;
};

/// Returns the longest round distance whose bar fits in a maximum length.
///
/// Nautical miles are preferred, in steps of 1, 2 and 5 from 0.1 nm to 5000 nm; when not even
/// 0.1 nm fits, the bar shows 10, 20, 50 or 100 m instead.
///
/// @param metres_per_pixel Ground distance one pixel covers at the map centre, in metres;
///     must be positive.
/// @param max_length_px Longest bar that fits, in pixels; must be positive.
/// @return The bar, with its length in pixels and its caption. It is empty (zero length, no
///     label) when an argument is not positive or NaN, or when not even 10 m fits.
[[nodiscard]] ScaleBar scale_bar(double metres_per_pixel, double max_length_px);

/// A slippy map: raster tiles from a `TileCache`, the vessel with its heading and a vector
/// of its course and speed, the track it has sailed, and mouse, touchpad and keyboard
/// navigation.
///
/// Overlays show the zoom level and view state with a north arrow, zoom buttons and a follow
/// button, a scale bar, the position under the pointer and the OpenStreetMap attribution. The
/// chart is always north up. The zoom level is fractional in [`kMinZoom`, `kMaxZoom`], so
/// the wheel, the touchpad and pinch gestures zoom smoothly: tiles come from the nearest
/// whole level and are scaled by the remaining factor. The keyboard and the buttons step to
/// whole levels. In follow mode the centre tracks the vessel; dragging the map switches
/// follow mode off.
///
/// The sailed track is a list of segments, at most 5000 points in all, the oldest dropped
/// first. Moving the vessel by hand or seeking in a track or log starts a new segment
/// (`break_track`), so that no line joins the old and the new position.
///
/// The widget does not move the vessel or set the destination itself: it reports the
/// operator's choices through `position_picked`, `destination_picked` and
/// `destination_cleared`, and the owner feeds the resulting state back with `set_vessel` and
/// `set_destination`.
class MapWidget : public QWidget {
    Q_OBJECT

public:
    /// Creates the map centred on central Athens (37.9838 N, 23.7275 E, the default vessel
    /// position of a profile) at zoom 12, in follow mode and without a vessel.
    ///
    /// The widget repaints whenever `cache` reports a downloaded tile and whenever the theme
    /// changes.
    ///
    /// @param cache Source of the tiles; not owned, must not be null and must outlive the
    ///     widget. The widget queues downloads through it while painting.
    /// @param parent Qt parent that owns the widget; null leaves ownership with the caller.
    explicit MapWidget(TileCache* cache, QWidget* parent = nullptr);

    /// Returns the tile cache the map paints from.
    ///
    /// @return The cache given to the constructor, not owned by the widget.
    [[nodiscard]] TileCache* cache() const noexcept { return cache_; }

    /// Returns the position at the centre of the widget.
    ///
    /// @return The centre. Its latitude lies within `kMaxLatitudeDeg` of the equator. Its
    ///     longitude is in [-180, 180) after `set_center`, but panning or zooming across the
    ///     antimeridian can leave it outside that range.
    [[nodiscard]] core::geo::Position center() const noexcept { return center_; }
    /// Moves the centre of the view to a position without changing the zoom.
    ///
    /// Follow mode is not changed; while it is on, the next `set_vessel` recentres on the
    /// vessel. Emits `view_changed`, even when the centre did not move.
    ///
    /// @param center New centre. The latitude is clamped to [-`kMaxLatitudeDeg`,
    ///     `kMaxLatitudeDeg`] and the longitude wrapped into [-180, 180).
    void set_center(core::geo::Position center);
    /// Returns the zoom level rounded to the nearest whole level.
    ///
    /// @return The whole zoom level, in [`kMinZoom`, `kMaxZoom`].
    [[nodiscard]] int zoom() const noexcept;
    /// Returns the fractional zoom level.
    ///
    /// @return The zoom level, in [`kMinZoom`, `kMaxZoom`]; each whole level doubles the
    ///     scale.
    [[nodiscard]] double zoom_level() const noexcept { return zoom_; }
    /// Sets a whole zoom level, keeping the centre.
    ///
    /// Emits `view_changed` when the level changes.
    ///
    /// @param zoom New zoom level; clamped to [`kMinZoom`, `kMaxZoom`].
    void set_zoom(int zoom);
    /// Zooms by whole levels from the nearest whole level, keeping the position under an
    /// anchor fixed.
    ///
    /// The result is a whole level even when the zoom was fractional. Behaves as `zoom_to`
    /// otherwise, emitting `view_changed` when the level changes.
    ///
    /// @param steps Number of levels, positive to zoom in and negative to zoom out; the
    ///     result is clamped to [`kMinZoom`, `kMaxZoom`].
    /// @param anchor Widget pixel whose position stays in place; ignored in follow mode.
    void zoom_by(int steps, QPoint anchor);
    /// Zooms to a fractional level, keeping the position under an anchor fixed.
    ///
    /// In follow mode the centre stays where it is, on the vessel once one is placed, and the
    /// anchor is ignored. A change of less than 1e-9 levels is ignored; otherwise the view repaints
    /// and `view_changed` is emitted.
    ///
    /// @param level New zoom level; clamped to [`kMinZoom`, `kMaxZoom`].
    /// @param anchor Widget pixel, from the top-left corner, whose position stays in place.
    void zoom_to(double level, QPointF anchor);

    /// Returns whether the map follows the vessel.
    ///
    /// @return `true` while the centre tracks the vessel.
    [[nodiscard]] bool follows_vessel() const noexcept { return follow_; }
    /// Switches follow mode on or off.
    ///
    /// Switching it on recentres on the vessel, if one has been placed, through `set_center`,
    /// which emits `view_changed`. The *Follow the vessel* button is updated to match
    /// without re-emitting its own signal. Emits `follow_changed` when the mode changes and
    /// does nothing otherwise. Connected to that button and to the *Follow vessel on the
    /// map* action of the main window.
    ///
    /// @param follow `true` to keep the vessel in the centre, `false` for a free view.
    void set_follow_vessel(bool follow);

    /// Places the vessel and extends the sailed track.
    ///
    /// The position is appended to the current track segment when it lies at least 2 pixels
    /// from the segment's last point at the current zoom, so that a vessel at rest adds
    /// nothing; after `break_track`, or with no track, it starts a new segment. Once the
    /// track exceeds 5000 points the oldest are dropped. In follow mode the map recentres
    /// on the vessel without emitting `view_changed`.
    ///
    /// @param position Position of the vessel.
    /// @param heading_true_deg Heading in degrees true, the direction the hull points.
    /// @param course_over_ground_deg Course over ground in degrees true, the direction of
    ///     the course vector.
    /// @param speed_over_ground_kn Speed over ground in knots. The course vector ends where
    ///     the vessel will be in six minutes at this speed; it is at most 400 pixels long
    ///     and not drawn below 2 pixels, so the default of 0 draws none.
    void set_vessel(core::geo::Position position, double heading_true_deg,
                    double course_over_ground_deg, double speed_over_ground_kn = 0.0);
    /// Returns where the vessel was last placed.
    ///
    /// @return The vessel position, or `std::nullopt` before the first `set_vessel`.
    [[nodiscard]] std::optional<core::geo::Position> vessel_position() const noexcept;
    /// Removes every track segment and repaints; the vessel stays.
    void clear_track();
    /// Ends the current track segment; the next vessel position starts a new one.
    ///
    /// Nothing changes on screen until that position arrives, and `track_segment_count`
    /// counts the new segment only then. Called when the vessel jumps: moved by hand or by a
    /// seek in a track or log.
    void break_track();
    /// Returns the number of points in all track segments.
    ///
    /// @return The point count, at most 5000.
    [[nodiscard]] int track_length() const noexcept;
    /// Returns the number of track segments.
    ///
    /// @return The segment count, including a segment of a single point, which is not drawn.
    [[nodiscard]] int track_segment_count() const noexcept {
        return static_cast<int>(track_.size());
    }

    /// Sets the track or route loaded from a file, drawn in green under the sailed track.
    ///
    /// A route of 500 points or fewer also gets a marker on every point.
    ///
    /// @param route Points in sailing order; empty draws nothing.
    void set_route(const QList<core::geo::Position>& route);
    /// Removes the loaded route and repaints.
    void clear_route();
    /// Returns the number of points of the loaded route.
    ///
    /// @return The point count; 0 without a route.
    [[nodiscard]] int route_length() const noexcept { return static_cast<int>(route_.size()); }

    /// Sets the destination waypoint and the origin of its leg.
    ///
    /// The destination is drawn as a magenta diamond with a dashed bearing line from the
    /// vessel and a dotted line for the leg from its origin, and the status overlay reads
    /// *destination set*. No signal is emitted.
    ///
    /// @param destination Waypoint position; `std::nullopt` removes the destination and the
    ///     leg.
    /// @param origin Start of the leg; `std::nullopt` draws no leg line. Ignored when
    ///     `destination` is empty.
    void set_destination(std::optional<core::geo::Position> destination,
                         std::optional<core::geo::Position> origin);
    /// Returns the destination waypoint drawn on the map.
    ///
    /// @return The destination, or `std::nullopt` when none is set.
    [[nodiscard]] std::optional<core::geo::Position> destination() const noexcept {
        return destination_;
    }

    /// Returns the widget pixel of a position at the current view.
    ///
    /// @param position Position to locate.
    /// @return Fractional widget coordinates from the top-left corner, x to the right and y
    ///     downwards; outside the widget when the position is not in view. The world is not
    ///     repeated, so a position one world width away gives a point one world width off.
    [[nodiscard]] QPointF point_of(core::geo::Position position) const;
    /// Returns the position under a widget pixel at the current view, the inverse of
    /// `point_of`.
    ///
    /// @param point Widget coordinates from the top-left corner; may lie outside the widget.
    /// @return The position. Its latitude is clamped to [-`kMaxLatitudeDeg`,
    ///     `kMaxLatitudeDeg`]; its longitude is not wrapped and lies outside [-180, 180] when
    ///     the point is beyond the antimeridian.
    [[nodiscard]] core::geo::Position position_at(QPointF point) const;

    /// Returns the ground distance one pixel covers at the centre latitude.
    ///
    /// @return Metres per pixel at the current fractional zoom.
    [[nodiscard]] double scale_m_per_px() const;
    /// Returns the length of the course vector drawn ahead of the vessel.
    ///
    /// @return The distance the vessel covers in six minutes at its speed over ground, in
    ///     pixels at the current view, at most 400; 0 without a vessel.
    [[nodiscard]] double course_vector_length_px() const;
    /// Returns the position under the pointer, as the bottom-right readout shows it.
    ///
    /// @return The position, or `std::nullopt` before the pointer first moves over the map
    ///     and after it leaves.
    [[nodiscard]] std::optional<core::geo::Position> pointer_position() const noexcept {
        return pointer_;
    }

    /// Returns the on-map *+* button, which zooms in one level around the centre; for tests.
    ///
    /// @return The button, a child of and owned by the widget.
    [[nodiscard]] QToolButton* zoom_in_button() const noexcept { return zoom_in_button_; }
    /// Returns the on-map *−* button, which zooms out one level around the centre; for tests.
    ///
    /// @return The button, a child of and owned by the widget.
    [[nodiscard]] QToolButton* zoom_out_button() const noexcept { return zoom_out_button_; }
    /// Returns the on-map *Follow the vessel* button, checked while following; for tests.
    ///
    /// @return The button, a child of and owned by the widget.
    [[nodiscard]] QToolButton* follow_button() const noexcept { return follow_button_; }

signals:
    /// Emitted when the operator asks to move the vessel to a point of the map.
    ///
    /// The request comes from a left double-click, a Ctrl+click or *Move vessel here* in
    /// the context menu. The widget itself does not move the vessel; the main window does
    /// and calls `set_vessel`.
    ///
    /// @param position The position under the pointer, as `position_at` returns it.
    void position_picked(nmeasim::core::geo::Position position);
    /// Emitted when the operator asks to steer for a point of the map.
    ///
    /// The request comes from a Shift+click or *Set destination here* in the context menu.
    /// The widget itself does not set the destination; see `set_destination`.
    ///
    /// @param position The position under the pointer, as `position_at` returns it.
    void destination_picked(nmeasim::core::geo::Position position);
    /// Emitted when the operator chooses *Clear destination* in the context menu, which is
    /// enabled only while a destination is set.
    void destination_cleared();
    /// Emitted when the centre or the zoom level changes through the view functions or a
    /// drag.
    ///
    /// Emitted by `set_center` (always), `set_zoom`, `zoom_by` and `zoom_to` (when the level
    /// changes) and by every step of a drag; not emitted when follow mode recentres on a new
    /// vessel position. The main window saves the zoom level on it.
    void view_changed();
    /// Emitted when follow mode is switched on or off, and only then.
    ///
    /// @param follow The new mode, `true` while the map follows the vessel.
    void follow_changed(bool follow);

protected:
    /// Handles pinch gestures on a touchpad and passes every other event to `QWidget`.
    ///
    /// A native zoom gesture zooms smoothly by the magnification it reports, around the
    /// fingers (or keeping the vessel centred in follow mode), as `zoom_to` does. Platforms
    /// report such gestures on macOS and Wayland.
    ///
    /// @param event Event to handle.
    /// @return `true` for a zoom gesture, which is consumed; otherwise the result of
    ///     `QWidget::event`.
    bool event(QEvent* event) override;
    /// Paints the whole map in one pass.
    ///
    /// Paints, from bottom to top: the background, the tiles of the visible area (a missing
    /// tile replaced by the covering part of a cached ancestor up to four levels up, or a
    /// grey square), a translucent shade that dims the chart in the night theme, the loaded
    /// route, the sailed track, the destination, the vessel and the overlays. Missing tiles
    /// are queued for download when the cache is online, so painting drives downloading.
    ///
    /// @param event Paint event; unused, as the whole widget is repainted.
    void paintEvent(QPaintEvent* event) override;
    /// Starts a drag, or picks a position with a modifier.
    ///
    /// A left press with Shift emits `destination_picked`, with Ctrl emits
    /// `position_picked`, and without either starts a drag with a closed-hand cursor. A
    /// press that does not pick a position gives the widget keyboard focus.
    ///
    /// @param event Press to handle.
    void mousePressEvent(QMouseEvent* event) override;
    /// Updates the pointer readout and, during a drag, pans the map.
    ///
    /// The first movement of a drag switches follow mode off (emitting `follow_changed`);
    /// every movement pans the map by the pointer's travel and emits `view_changed`.
    ///
    /// @param event Movement to handle.
    void mouseMoveEvent(QMouseEvent* event) override;
    /// Ends a drag when the left button is released and restores the cursor.
    ///
    /// @param event Release to handle.
    void mouseReleaseEvent(QMouseEvent* event) override;
    /// Emits `position_picked` for a left double-click, to move the vessel there.
    ///
    /// @param event Double-click to handle.
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    /// Hides the pointer readout when the pointer leaves the map.
    ///
    /// @param event Leave event, passed on to `QWidget`.
    void leaveEvent(QEvent* event) override;
    /// Shows the context menu for the position under the pointer.
    ///
    /// The menu offers *Move vessel here* (emits `position_picked`), *Set destination here*
    /// (emits `destination_picked`) and *Clear destination* (emits `destination_cleared`,
    /// enabled only while a destination is set). It runs modally and returns once closed.
    ///
    /// @param event Context menu request, which gives the position.
    void contextMenuEvent(QContextMenuEvent* event) override;
    /// Zooms smoothly around the pointer, or keeping the vessel centred in follow mode.
    ///
    /// One wheel notch, a vertical angle delta of 120, zooms one level; touchpads and
    /// high-resolution wheels send smaller deltas, which zoom by the matching fraction of a
    /// level. Horizontal scrolling is ignored. The event is always accepted.
    ///
    /// @param event Wheel event to handle.
    void wheelEvent(QWheelEvent* event) override;
    /// Handles the map keys and passes every other key to `QWidget`.
    ///
    /// Plus (or equals) and minus zoom one whole level around the centre; Home switches
    /// follow mode on and recentres.
    ///
    /// @param event Key press to handle.
    void keyPressEvent(QKeyEvent* event) override;
    /// Keeps the buttons in the top-right corner after a resize and repaints.
    ///
    /// @param event Resize event, passed on to `QWidget`.
    void resizeEvent(QResizeEvent* event) override;

private:
    /// The vessel as last placed with `set_vessel`.
    struct Vessel {
        /// Position of the vessel.
        core::geo::Position position;
        /// Heading in degrees true, the direction the hull is drawn pointing.
        double heading_true_deg{0.0};
        /// Course over ground in degrees true, the direction of the course vector.
        double course_over_ground_deg{0.0};
        /// Speed over ground in knots, which sets the length of the course vector.
        double speed_over_ground_kn{0.0};
    };

    /// Paints the tiles covering the widget at the current view.
    ///
    /// Rows are limited to the world; columns wrap around the antimeridian, so the world
    /// repeats east and west. Scaled tiles are smoothed.
    ///
    /// @param painter Painter on the widget.
    void draw_tiles(class QPainter& painter);
    /// Draws the loaded route as a green line, with a marker on every point when it has 500
    /// points or fewer.
    ///
    /// @param painter Painter on the widget.
    void draw_route(QPainter& painter);
    /// Draws the sailed track as a red line per segment; single-point segments draw nothing.
    ///
    /// @param painter Painter on the widget.
    void draw_track(QPainter& painter);
    /// Draws the destination diamond, the dashed bearing line from the vessel and the dotted
    /// leg line from the origin; nothing without a destination.
    ///
    /// @param painter Painter on the widget.
    void draw_destination(QPainter& painter);
    /// Draws the vessel: the six-minute course vector, a heading line past the bow and the
    /// hull outline rotated to the heading; nothing before the first `set_vessel`.
    ///
    /// @param painter Painter on the widget.
    void draw_vessel(QPainter& painter);
    /// Draws the overlays: the zoom level and view state with the north arrow (top left),
    /// the scale bar (bottom left), and the pointer position and the OpenStreetMap
    /// attribution (bottom right).
    ///
    /// @param painter Painter on the widget.
    void draw_overlay(QPainter& painter);
    /// Draws a tile, falling back to a scaled part of an ancestor when it is not cached.
    ///
    /// Asking the cache for the tile queues its download when online. The fallback looks up
    /// to four levels up and takes only ancestors already cached, so it queues no further
    /// downloads; without one the target becomes a grey square with a faint border.
    ///
    /// @param painter Painter on the widget.
    /// @param key Tile to draw, with x already wrapped into the level.
    /// @param target Widget rectangle the tile fills, in pixels.
    void draw_tile(QPainter& painter, TileKey key, const QRect& target);
    /// Returns the whole zoom level the tiles are taken from.
    ///
    /// @return The fractional zoom rounded to the nearest level, in [`kMinZoom`,
    ///     `kMaxZoom`].
    [[nodiscard]] int tile_zoom() const noexcept;
    /// Returns the factor the tiles of `tile_zoom` are scaled by to reach the fractional
    /// zoom.
    ///
    /// @return 2 to the power of the difference between the fractional and the tile zoom,
    ///     in about [0.71, 1.41].
    [[nodiscard]] double tile_scale() const noexcept;
    /// Returns the pixel coordinates of a position in the world image at the current
    /// fractional zoom.
    ///
    /// @param position Position to project, treated as `pixel_coordinates` treats it.
    /// @return World pixel coordinates, 2 to the power of the zoom times 256 pixels across.
    [[nodiscard]] QPointF world_pixel(core::geo::Position position) const;
    /// Returns the position at world pixel coordinates at the current fractional zoom, the
    /// inverse of `world_pixel`.
    ///
    /// @param pixel World pixel coordinates.
    /// @return The position, with the latitude clamped to [-`kMaxLatitudeDeg`,
    ///     `kMaxLatitudeDeg`] and the longitude not wrapped.
    [[nodiscard]] core::geo::Position position_of_world(QPointF pixel) const;
    /// Returns the world pixel coordinates of the centre at the current fractional zoom.
    ///
    /// @return `world_pixel` of the centre.
    [[nodiscard]] QPointF center_pixel() const;
    /// Moves the map content by a distance on screen and emits `view_changed`.
    ///
    /// @param delta_px Movement of the content in pixels, x to the right and y downwards;
    ///     the centre moves the opposite way.
    void pan_by(QPointF delta_px);
    /// Stacks the zoom and follow buttons in the top-right corner.
    void place_buttons();

    /// Tile source; not owned, outlives the widget.
    TileCache* cache_;
    /// Position at the centre of the widget; defaults to central Athens, the default vessel
    /// position of a profile.
    core::geo::Position center_{37.9838, 23.7275};
    /// Fractional zoom level, in [`kMinZoom`, `kMaxZoom`]; the default of 12 matches the
    /// default of the saved `map/zoom` preference.
    double zoom_{12.0};
    /// Whether the centre tracks the vessel.
    bool follow_{true};
    /// The vessel, or `std::nullopt` before the first `set_vessel`.
    std::optional<Vessel> vessel_;
    /// Sailed track segments, oldest first, at most 5000 points in all.
    QList<QList<core::geo::Position>> track_;
    /// Whether the next vessel position starts a new segment, set by `break_track`.
    bool track_broken_{false};
    /// Track or route loaded from a file; empty when none is loaded.
    QList<core::geo::Position> route_;
    /// Destination waypoint, or `std::nullopt` when none is set.
    std::optional<core::geo::Position> destination_;
    /// Start of the leg to the destination, or `std::nullopt` for no leg line; always empty
    /// without a destination.
    std::optional<core::geo::Position> leg_origin_;
    /// Position under the pointer, or `std::nullopt` while the pointer is not over the map.
    std::optional<core::geo::Position> pointer_;
    /// Pointer position in widget pixels at the last drag step, or `std::nullopt` when no
    /// drag is in progress.
    std::optional<QPoint> drag_last_;
    /// Whether the current drag has moved the map; set but not read.
    bool dragged_{false};
    /// The *+* button; a child of the widget.
    QToolButton* zoom_in_button_;
    /// The *−* button; a child of the widget.
    QToolButton* zoom_out_button_;
    /// The checkable *Follow the vessel* button; a child of the widget.
    QToolButton* follow_button_;
};

}  // namespace nmeasim::app::map
