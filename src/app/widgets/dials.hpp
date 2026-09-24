// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The round instruments of the dashboard: a north-up compass rose and a wind dial relative to
/// the bow, painted in code in the colours of the current theme.
///
/// `DialWidget` holds the geometry and drawing helpers they share; `CompassDial` and
/// `WindDial` are the two instruments `DashboardWidget` shows in its top row. The free
/// functions fold and format wind angles for the readouts. Angles on the dials run clockwise
/// from the top of the dial, in degrees.

#pragma once

#include <QString>
#include <QWidget>

#include <optional>

class QPainter;

namespace nmeasim::app {

/// Folds an angle relative to the bow into (-180, 180], negative to port.
///
/// @param relative_deg Angle clockwise from the bow, in degrees; any finite value, including
///   negative values and values of 360 or more.
/// @return The same direction in (-180, 180]: 0 dead ahead, positive to starboard, negative
///   to port, 180 dead astern.
[[nodiscard]] double signed_relative_angle(double relative_deg) noexcept;

/// Formats a wind angle relative to the bow as sailors say it: `104°P`, `30°S`, `0°` ahead or
/// `180°` astern.
///
/// The angle is folded with `signed_relative_angle` and rounded to whole degrees; the side
/// letter is `P` for port and `S` for starboard, and is left out when the rounded angle is 0
/// or 180. For example 255.8 gives `104°P` and 359.6 gives `0°`.
///
/// @param relative_deg Angle clockwise from the bow, in degrees; any finite value.
/// @return The rounded magnitude with a degree sign and, off the centreline, the side letter.
[[nodiscard]] QString format_wind_angle(double relative_deg);

/// Common look of the round instruments: a square widget with a bezel, a ring of ticks and a
/// caption, painted in the colours of the current theme.
///
/// Subclasses paint in their `paintEvent` with the protected helpers, which place everything
/// by an angle in degrees clockwise from the top of the dial and a fraction of `radius`. The
/// widget repaints whenever `theme::Theme::changed` is emitted, and prefers to stay square
/// (`heightForWidth` returns the width).
class DialWidget : public QWidget {
    Q_OBJECT

public:
    /// Creates a dial that expands in both directions and follows theme changes.
    ///
    /// @param parent Parent widget, which owns the dial; may be null, in which case the caller
    ///   owns it.
    explicit DialWidget(QWidget* parent = nullptr);

    /// Returns the preferred size, 240 by 240 pixels.
    ///
    /// @return `QSize(240, 240)`.
    [[nodiscard]] QSize sizeHint() const override { return {240, 240}; }
    /// Returns the smallest size at which the labels stay legible, 170 by 170 pixels.
    ///
    /// @return `QSize(170, 170)`.
    [[nodiscard]] QSize minimumSizeHint() const override { return {170, 170}; }
    /// Tells layouts that the height depends on the width, so that the dial stays square.
    ///
    /// @return Always true.
    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    /// Returns the preferred height for a width: the width itself, keeping the dial square.
    ///
    /// @param width Available width in pixels.
    /// @return `width`.
    [[nodiscard]] int heightForWidth(int width) const override { return width; }

protected:
    /// Returns the radius of the tick ring in widget pixels.
    ///
    /// @return Half the shorter side of the widget less an 8-pixel margin that leaves room for
    ///   the bezel, and never less than 10.
    [[nodiscard]] double radius() const;
    /// Returns the centre of the dial.
    ///
    /// @return The centre of the widget, in widget pixels.
    [[nodiscard]] QPointF centre() const;
    /// Returns a point at an angle and a distance from the centre.
    ///
    /// @param angle_deg Angle in degrees clockwise from the top of the dial; any finite value.
    /// @param fraction Distance from the centre as a fraction of `radius`; 1 is the outer
    ///   edge of the tick ring.
    /// @return The point in widget pixels.
    [[nodiscard]] QPointF at(double angle_deg, double fraction) const;
    /// Draws the bezel and the background disc.
    ///
    /// The bezel is a disc 6 pixels larger than `radius` in the raised panel colour; the face
    /// inside it has the inset colour of the theme.
    ///
    /// @param painter Painter open on this widget.
    void draw_face(QPainter& painter) const;
    /// Draws the ring of ticks just inside the edge of the face.
    ///
    /// Major ticks are longer and drawn in the text colour, minor ones in the dimmed text
    /// colour. Ticks start at the top of the dial.
    ///
    /// @param painter Painter open on this widget.
    /// @param minor Spacing of all ticks, in degrees; must be positive.
    /// @param major Spacing of the long ticks, in degrees; a tick is major when its angle is a
    ///   multiple of `major`, which must be positive.
    void draw_ticks(QPainter& painter, int minor, int major) const;
    /// Draws a marker on the ring: a filled or hollow triangle pointing at the centre.
    ///
    /// The triangle's base lies on the edge of the ring and its tip at 0.84 of the radius.
    ///
    /// @param painter Painter open on this widget.
    /// @param angle_deg Direction of the marker, in degrees clockwise from the top of the dial.
    /// @param color Colour of the outline and, when `filled`, of the fill.
    /// @param filled True for a solid triangle, false for an outline only.
    void draw_ring_marker(QPainter& painter, double angle_deg, const QColor& color,
                          bool filled) const;
    /// Draws a large readout with a small caption above it.
    ///
    /// The caption is bold, in the dimmed text colour; the value uses
    /// `theme::Theme::readout_font`. Both are centred horizontally across the dial.
    ///
    /// @param painter Painter open on this widget.
    /// @param caption Caption text, such as `HDG °T`.
    /// @param value Value text.
    /// @param y_fraction Vertical position of the value's centre as a fraction of the radius:
    ///   positive below the centre of the dial, negative above.
    /// @param size_fraction Pixel height of the value text as a fraction of the radius.
    /// @param color Colour of the value text.
    void draw_readout(QPainter& painter, const QString& caption, const QString& value,
                      double y_fraction, double size_fraction, const QColor& color) const;
};

/// North-up compass rose: the heading as a vessel-shaped pointer, the course over ground and
/// the bearing to the destination as markers on the ring, and the three values as readouts.
///
/// North is always at the top; the rose has ticks every 5 degrees, longer every 30, with the
/// cardinal letters and the tens of degrees (3, 6, 12 and so on) inside the ring and `N` in
/// the danger colour. The heading is the accent-coloured pointer, the course over ground a
/// filled triangle in the ok colour and the bearing a diamond in the map's destination colour.
/// The centre reads the heading (`HDG °T`), and below it `COG` and, with a destination, `BRG`.
/// All angles are degrees true. The object name is `compass_dial`.
class CompassDial : public DialWidget {
    Q_OBJECT

public:
    /// Creates a compass showing heading and course 0 without a destination.
    ///
    /// @param parent Parent widget, which owns the dial; may be null, in which case the caller
    ///   owns it.
    explicit CompassDial(QWidget* parent = nullptr);

    /// Sets the values to show and schedules a repaint.
    ///
    /// Values are drawn as given, without normalising; the readouts show them with one decimal
    /// and three integer digits, so they are expected in [0, 360).
    ///
    /// @param heading_deg Heading of the bow, degrees true.
    /// @param course_deg Course over ground, degrees true.
    /// @param bearing_deg Bearing from the vessel to the destination, degrees true;
    ///   `std::nullopt` hides the diamond and the `BRG` readout.
    void set_values(double heading_deg, double course_deg, std::optional<double> bearing_deg);

    /// Returns the heading last set, degrees true.
    ///
    /// @return The `heading_deg` of the last `set_values`, 0 before the first.
    [[nodiscard]] double heading() const noexcept { return heading_deg_; }
    /// Returns the course over ground last set, degrees true.
    ///
    /// @return The `course_deg` of the last `set_values`, 0 before the first.
    [[nodiscard]] double course() const noexcept { return course_deg_; }
    /// Returns the bearing to the destination last set, degrees true.
    ///
    /// @return The `bearing_deg` of the last `set_values`; `std::nullopt` without a
    ///   destination.
    [[nodiscard]] std::optional<double> bearing() const noexcept { return bearing_deg_; }

protected:
    /// Paints the face, the ticks and labels, the bearing, course and heading markers in that
    /// order, so that the heading pointer lies on top, and then the readouts.
    ///
    /// @param event Unused; the whole dial is repainted.
    void paintEvent(QPaintEvent* event) override;

private:
    /// Heading of the bow, degrees true.
    double heading_deg_{0.0};
    /// Course over ground, degrees true.
    double course_deg_{0.0};
    /// Bearing to the destination, degrees true; `std::nullopt` without a destination.
    std::optional<double> bearing_deg_;
};

/// Wind instrument relative to the bow: port and starboard close-hauled sectors, the apparent
/// wind as an arrow and the true wind as a hollow marker, with angles and speeds as readouts.
///
/// The bow is always at the top. Ticks run every 10 degrees, longer every 30, labelled 0 to
/// 180 on each side. The close-hauled sectors span 20 to 60 degrees off the bow, in the danger
/// colour to port and the ok colour to starboard. The apparent wind is an accent-coloured
/// arrow from the ring towards the centre and the true wind a hollow triangle on the ring;
/// both sit on the side the wind comes from. The centre reads the apparent wind speed
/// (`AWS KN`) and below it `AWA`, `TWA` and `TWS`, with angles written by
/// `format_wind_angle`. The object name is `wind_dial`.
class WindDial : public DialWidget {
    Q_OBJECT

public:
    /// Creates a wind dial showing calm air from ahead.
    ///
    /// @param parent Parent widget, which owns the dial; may be null, in which case the caller
    ///   owns it.
    explicit WindDial(QWidget* parent = nullptr);

    /// Sets the values to show and schedules a repaint.
    ///
    /// @param apparent_angle_deg Angle the apparent wind comes from, degrees clockwise from
    ///   the bow, normally in [0, 360); other values are drawn and folded the same way.
    /// @param apparent_speed_kn Apparent wind speed.
    /// @param true_angle_deg Angle the true wind comes from, degrees clockwise from the bow,
    ///   as `core::model::Wind::true_angle_relative_deg` returns it.
    /// @param true_speed_kn True wind speed.
    void set_values(double apparent_angle_deg, double apparent_speed_kn, double true_angle_deg,
                    double true_speed_kn);

    /// Returns the apparent wind angle last set, degrees clockwise from the bow.
    ///
    /// @return The `apparent_angle_deg` of the last `set_values`, 0 before the first.
    [[nodiscard]] double apparent_angle() const noexcept { return apparent_angle_deg_; }
    /// Returns the true wind angle last set, degrees clockwise from the bow.
    ///
    /// @return The `true_angle_deg` of the last `set_values`, 0 before the first.
    [[nodiscard]] double true_angle() const noexcept { return true_angle_deg_; }

protected:
    /// Paints the face, the close-hauled sectors, the ticks and labels, the true wind marker,
    /// the apparent wind arrow on top of it, and then the readouts.
    ///
    /// @param event Unused; the whole dial is repainted.
    void paintEvent(QPaintEvent* event) override;

private:
    /// Angle the apparent wind comes from, degrees clockwise from the bow.
    double apparent_angle_deg_{0.0};
    /// Apparent wind speed.
    double apparent_speed_kn_{0.0};
    /// Angle the true wind comes from, degrees clockwise from the bow.
    double true_angle_deg_{0.0};
    /// True wind speed.
    double true_speed_kn_{0.0};
};

}  // namespace nmeasim::app
