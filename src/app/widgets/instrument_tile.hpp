#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>

namespace nmeasim::app {

/// One instrument on the dashboard: a title, a large value and, for controllable values, an
/// override checkbox with a spin box.
class InstrumentTile : public QFrame {
    Q_OBJECT

public:
    InstrumentTile(const QString& title, const QString& unit, QWidget* parent = nullptr);

    /// Shows a numeric value with the given number of decimals.
    void set_value(double value, int decimals);
    /// Shows arbitrary text, for values such as positions or times.
    void set_text(const QString& text);
    [[nodiscard]] QString text() const { return value_label_->text(); }

    /// Adds the override control. Until this is called the tile is display-only.
    void enable_override(double minimum, double maximum, double step, int decimals);
    [[nodiscard]] bool override_active() const;
    [[nodiscard]] double override_value() const;
    /// Sets the control without emitting `override_changed`.
    void set_override(bool active, double value);
    /// Enables or disables the override control, e.g. the rudder outside steering mode.
    void set_override_enabled(bool enabled);

signals:
    void override_changed(bool active, double value);

private:
    QLabel* value_label_;
    QLabel* unit_label_;
    QCheckBox* override_check_{nullptr};
    QDoubleSpinBox* override_spin_{nullptr};
    bool suppress_signals_{false};
};

}  // namespace nmeasim::app
