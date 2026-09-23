#pragma once

#include <QString>
#include <QWidget>

#include <optional>

class QPainter;

namespace nmeasim::app {

/// An angle relative to the bow in degrees, folded to (-180, 180]: negative to port.
[[nodiscard]] double signed_relative_angle(double relative_deg) noexcept;

/// A wind angle relative to the bow as sailors say it: `104°P`, `30°S`, `0°` ahead or
/// `180°` astern.
[[nodiscard]] QString format_wind_angle(double relative_deg);

/// Common look of the round instruments: a square widget with a bezel, a ring of ticks and a
/// caption, painted in the colours of the current theme.
class DialWidget : public QWidget {
    Q_OBJECT

public:
    explicit DialWidget(QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override { return {240, 240}; }
    [[nodiscard]] QSize minimumSizeHint() const override { return {170, 170}; }
    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    [[nodiscard]] int heightForWidth(int width) const override { return width; }

protected:
    /// Radius of the tick ring and the centre of the dial in widget pixels.
    [[nodiscard]] double radius() const;
    [[nodiscard]] QPointF centre() const;
    /// Point on a circle of `fraction` times the radius at `angle_deg`, clockwise from the top.
    [[nodiscard]] QPointF at(double angle_deg, double fraction) const;
    /// Bezel and background disc.
    void draw_face(QPainter& painter) const;
    /// Ticks every `minor` degrees, longer every `major` degrees.
    void draw_ticks(QPainter& painter, int minor, int major) const;
    /// A marker on the ring: a filled or hollow triangle pointing at the centre.
    void draw_ring_marker(QPainter& painter, double angle_deg, const QColor& color,
                          bool filled) const;
    /// Large readout with a small caption above it, centred at `y_fraction` of the radius
    /// below (positive) or above (negative) the centre.
    void draw_readout(QPainter& painter, const QString& caption, const QString& value,
                      double y_fraction, double size_fraction, const QColor& color) const;
};

/// North-up compass rose: the heading as a vessel-shaped pointer, the course over ground and
/// the bearing to the destination as markers on the ring, and the three values as readouts.
class CompassDial : public DialWidget {
    Q_OBJECT

public:
    explicit CompassDial(QWidget* parent = nullptr);

    /// Sets the heading and course, and the bearing to the destination (nullopt without one).
    void set_values(double heading_deg, double course_deg, std::optional<double> bearing_deg);

    [[nodiscard]] double heading() const noexcept { return heading_deg_; }
    [[nodiscard]] double course() const noexcept { return course_deg_; }
    [[nodiscard]] std::optional<double> bearing() const noexcept { return bearing_deg_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double heading_deg_{0.0};
    double course_deg_{0.0};
    std::optional<double> bearing_deg_;
};

/// Wind instrument relative to the bow: port and starboard close-hauled sectors, the apparent
/// wind as an arrow and the true wind as a hollow marker, with angles and speeds as readouts.
class WindDial : public DialWidget {
    Q_OBJECT

public:
    explicit WindDial(QWidget* parent = nullptr);

    /// Angles relative to the bow, clockwise in [0, 360), and speeds in knots.
    void set_values(double apparent_angle_deg, double apparent_speed_kn, double true_angle_deg,
                    double true_speed_kn);

    [[nodiscard]] double apparent_angle() const noexcept { return apparent_angle_deg_; }
    [[nodiscard]] double true_angle() const noexcept { return true_angle_deg_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double apparent_angle_deg_{0.0};
    double apparent_speed_kn_{0.0};
    double true_angle_deg_{0.0};
    double true_speed_kn_{0.0};
};

}  // namespace nmeasim::app
