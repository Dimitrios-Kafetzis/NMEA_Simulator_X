// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `StatusLed` indicator light with a caption, used in the status bar.
///
/// @see docs/reference/desktop-app.md, the status bar in section "Window layout".

#pragma once

#include <QColor>
#include <QString>
#include <QTimer>
#include <QWidget>

namespace nmeasim::app {

/// A coloured indicator light with a short upper-case caption, as on an instrument panel:
/// the run state, recording and the outputs in the status bar. The light can blink.
///
/// The light is a disc 9 pixels across with a soft glow when lit; an unlit light is drawn in
/// `theme::Colors::inactive` without glow. The caption is drawn in the widget font, bold, at
/// 85 % of its size and slightly spaced, in `theme::Colors::text` when lit and
/// `theme::Colors::text_dim` when not; the widget shows it as given and does not change its
/// case. The widget repaints when `theme::Theme::changed` is emitted. It starts unlit, with
/// no caption and not blinking.
class StatusLed : public QWidget {
    Q_OBJECT

public:
    /// Creates an unlit light with no caption.
    ///
    /// @param parent Qt parent that owns the widget; null leaves ownership to the caller.
    explicit StatusLed(QWidget* parent = nullptr);

    /// Sets the light colour and the caption.
    ///
    /// Does nothing when both are unchanged. A new caption also updates the size hint, so the
    /// layout makes room for it.
    ///
    /// @param color Colour of the lit light, usually one of the state colours of
    ///   `theme::Colors` such as `ok`; an invalid `QColor` shows an unlit light.
    /// @param text Caption, short and upper case by convention, such as `RUNNING`; may be
    ///   empty.
    void set_state(const QColor& color, const QString& text);
    /// Starts or stops blinking, for example while recording.
    ///
    /// A blinking light is on and off for 500 ms each, so it flashes once a second, and the
    /// caption dims with it. Starting or stopping restarts from the lit phase; a call that does
    /// not change the blinking state does nothing.
    ///
    /// @param blinking True to blink, false to light steadily.
    void set_blinking(bool blinking);

    /// Returns the colour set by `set_state`.
    ///
    /// @return The colour of the lit light; an invalid `QColor` for an unlit light.
    [[nodiscard]] QColor color() const noexcept { return color_; }
    /// Returns the caption set by `set_state`.
    ///
    /// @return The caption; empty before the first `set_state`.
    [[nodiscard]] QString text() const noexcept { return text_; }
    /// Returns whether the light is blinking.
    ///
    /// @return True between `set_blinking(true)` and `set_blinking(false)`.
    [[nodiscard]] bool blinking() const noexcept { return blink_timer_.isActive(); }

    /// Returns the size that fits the light and the whole caption.
    ///
    /// @return 6 pixels of margin, the 9-pixel light, a 6-pixel gap, the caption width and
    ///   6 pixels of margin wide; the caption height or 9 pixels, whichever is larger, plus 4
    ///   pixels high.
    [[nodiscard]] QSize sizeHint() const override;
    /// Returns the smallest size, which is `sizeHint`, so that layouts do not cut the caption
    /// off.
    ///
    /// @return The same size as `sizeHint`.
    [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    /// Paints the light, with its glow when lit, centred vertically at the left, and the
    /// caption to its right, in the colours of the current theme.
    ///
    /// @param event Paint event; not used, the whole widget is repainted.
    void paintEvent(QPaintEvent* event) override;

private:
    /// Colour of the lit light; invalid for an unlit light.
    QColor color_;
    /// Caption shown to the right of the light.
    QString text_;
    /// Toggles `lit_` every 500 ms while the light blinks; stopped otherwise.
    QTimer blink_timer_;
    /// Whether the blink cycle is in its lit phase; always true when not blinking.
    bool lit_{true};
};

}  // namespace nmeasim::app
