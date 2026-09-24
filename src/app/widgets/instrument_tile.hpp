// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The digital instrument tile of the dashboard: a caption, a large readout with its unit and
/// an optional override control.

#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>

namespace nmeasim::app {

/// One instrument on the dashboard: a caption, a large digital readout with its unit and,
/// for controllable values, an override checkbox with a spin box. An active override draws
/// the tile with a warning-coloured border.
///
/// The tile only displays and edits numbers; it knows nothing of the simulation.
/// `DashboardWidget` maps its `override_changed` signal to a `core::simulation::Parameter`.
/// The object name `instrument_tile` and the dynamic property `overridden` select the
/// style-sheet rules of `theme::Theme`; the caption, readout and unit labels are named
/// `tile_title`, `tile_value` and `tile_unit`. Every child widget is owned by the tile.
class InstrumentTile : public QFrame {
    Q_OBJECT

public:
    /// Creates a display-only tile that reads `--` until a value is set.
    ///
    /// @param title Caption, shown in bold capitals.
    /// @param unit Unit shown to the right of the readout, such as `kn`; empty for none.
    /// @param parent Parent widget, which owns the tile; may be null, in which case the caller
    ///   owns it.
    InstrumentTile(const QString& title, const QString& unit, QWidget* parent = nullptr);

    /// Shows a numeric value with the given number of decimals.
    ///
    /// Numeric readouts use a larger font than text readouts. The override control is not
    /// touched, so an overridden tile shows the simulated value while keeping its control
    /// value.
    ///
    /// @param value Value in the tile's unit.
    /// @param decimals Number of digits after the decimal point.
    void set_value(double value, int decimals);
    /// Shows arbitrary text, for values such as positions or times.
    ///
    /// @param text Text to show; may span several lines.
    void set_text(const QString& text);
    /// Returns the text of the readout.
    ///
    /// @return The readout as shown, `--` before the first `set_value` or `set_text`.
    [[nodiscard]] QString text() const { return value_label_->text(); }
    /// Returns whether the tile is drawn as overridden.
    ///
    /// @return True while the override is active, which the style sheet shows as an amber
    ///   border.
    [[nodiscard]] bool overridden() const { return property("overridden").toBool(); }

    /// Adds the override control. Until this is called the tile is display-only.
    ///
    /// The control is an *Override* check box and a spin box, which is enabled only while the
    /// box is ticked and applies a typed value when editing finishes rather than on every
    /// keystroke. A second call does nothing.
    ///
    /// @param minimum Smallest value the spin box accepts, in the tile's unit.
    /// @param maximum Largest value the spin box accepts, in the tile's unit.
    /// @param step Change per arrow click or key press, in the tile's unit.
    /// @param decimals Number of decimals the spin box shows and accepts.
    void enable_override(double minimum, double maximum, double step, int decimals);
    /// Returns whether the override check box is ticked.
    ///
    /// @return True while the override is active; false for a display-only tile.
    [[nodiscard]] bool override_active() const;
    /// Returns the value of the override spin box.
    ///
    /// @return The spin box value in the tile's unit, also when the override is not active;
    ///   0 for a display-only tile.
    [[nodiscard]] double override_value() const;
    /// Sets the control without emitting `override_changed`.
    ///
    /// Used to reflect overrides set elsewhere, such as keyboard nudges. The value is clamped
    /// to the range given to `enable_override`. Does nothing on a display-only tile.
    ///
    /// @param active True to tick the check box and draw the tile as overridden.
    /// @param value Value for the spin box, in the tile's unit.
    void set_override(bool active, double value);
    /// Enables or disables the override control, for example the rudder outside steering
    /// mode.
    ///
    /// Does nothing on a display-only tile. The spin box is enabled only when `enabled` is
    /// true and the check box is ticked.
    ///
    /// @param enabled False to grey out the check box and the spin box.
    void set_override_enabled(bool enabled);

signals:
    /// Emitted when the operator ticks or unticks the override, or changes the spin box
    /// while the override is active.
    ///
    /// Emitted synchronously from the control's own signal; not emitted from
    /// `set_override`, nor for spin box changes while the override is inactive.
    ///
    /// @param active True when the override is active, false when it was released.
    /// @param value Value of the spin box, in the tile's unit.
    void override_changed(bool active, double value);

private:
    /// Switches the readout font to `scale` times the label's original point size, unless
    /// that scale is already in use.
    ///
    /// @param scale Factor applied to `base_point_size_`.
    void set_readout_scale(double scale);
    /// Sets the `overridden` property and re-polishes the tile so that the border follows it.
    ///
    /// @param overridden New value of the property; nothing happens when it is unchanged.
    void mark_overridden(bool overridden);

    /// Large digital readout, owned by the tile.
    QLabel* value_label_;
    /// Unit next to the readout, owned by the tile.
    QLabel* unit_label_;
    /// Override check box, owned by the tile; null until `enable_override`.
    QCheckBox* override_check_{nullptr};
    /// Override value, owned by the tile; null until `enable_override`.
    QDoubleSpinBox* override_spin_{nullptr};
    /// True inside `set_override`, so that programmatic changes emit no `override_changed`.
    bool suppress_signals_{false};
    /// Point size of the readout label's default font, the base of the readout scales.
    double base_point_size_{10.0};
    /// Scale of the current readout font; 0 until the first `set_value` or `set_text`.
    double readout_scale_{0.0};
};

}  // namespace nmeasim::app
