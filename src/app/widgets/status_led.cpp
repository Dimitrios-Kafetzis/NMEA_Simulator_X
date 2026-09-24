// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `StatusLed`: its size, blinking and painting.

#include "status_led.hpp"

#include "theme/theme.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QRadialGradient>

#include <algorithm>

namespace nmeasim::app {

namespace {

/// Diameter of the light, in pixels; its glow reaches one diameter from the centre. A
/// user-interface choice for a light that fits in the status bar.
constexpr int kLedDiameter{9};
/// Space between the light and the caption, in pixels.
constexpr int kGap{6};
/// Space left of the light and right of the caption, in pixels.
constexpr int kMargin{6};
/// Duration of each lit and unlit phase while blinking, in milliseconds: one flash a second.
constexpr int kBlinkIntervalMs{500};

/// Derives the caption font from the widget font.
///
/// @param base The widget font.
/// @return `base` in bold at 85 % of its point size, with letters spaced at 108 %.
QFont caption_font(const QFont& base) {
    QFont font = base;
    font.setBold(true);
    font.setPointSizeF(base.pointSizeF() * 0.85);
    font.setLetterSpacing(QFont::PercentageSpacing, 108);
    return font;
}

}  // namespace

StatusLed::StatusLed(QWidget* parent) : QWidget(parent) {
    blink_timer_.setInterval(kBlinkIntervalMs);
    connect(&blink_timer_, &QTimer::timeout, this, [this] {
        lit_ = !lit_;
        update();
    });
    connect(&theme::Theme::instance(), &theme::Theme::changed, this, qOverload<>(&QWidget::update));
}

void StatusLed::set_state(const QColor& color, const QString& text) {
    if (color_ == color && text_ == text) {
        return;
    }
    const bool resized = text_ != text;
    color_ = color;
    text_ = text;
    if (resized) {
        updateGeometry();
    }
    update();
}

void StatusLed::set_blinking(bool blinking) {
    if (blinking == blink_timer_.isActive()) {
        return;
    }
    lit_ = true;
    if (blinking) {
        blink_timer_.start();
    } else {
        blink_timer_.stop();
    }
    update();
}

QSize StatusLed::sizeHint() const {
    const QFontMetrics metrics(caption_font(font()));
    return {kMargin + kLedDiameter + kGap + metrics.horizontalAdvance(text_) + kMargin,
            std::max(metrics.height(), kLedDiameter) + 4};
}

void StatusLed::paintEvent(QPaintEvent* /*event*/) {
    const auto& colors = theme::Theme::instance().colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF centre(kMargin + kLedDiameter / 2.0, height() / 2.0);
    const bool on = color_.isValid() && lit_;
    const QColor fill = on ? color_ : colors.inactive;
    if (on) {
        // Soft glow around a lit light: 43 % opacity at the centre, fading to nothing at one
        // diameter.
        QRadialGradient glow(centre, kLedDiameter);
        QColor halo = fill;
        halo.setAlpha(110);
        glow.setColorAt(0.0, halo);
        halo.setAlpha(0);
        glow.setColorAt(1.0, halo);
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(centre, kLedDiameter, kLedDiameter);
    }
    painter.setPen(QPen(fill.darker(160), 1.0));
    painter.setBrush(fill);
    painter.drawEllipse(centre, kLedDiameter / 2.0, kLedDiameter / 2.0);

    painter.setFont(caption_font(font()));
    painter.setPen(on ? colors.text : colors.text_dim);
    const QRect text_rect(kMargin + kLedDiameter + kGap, 0, width(), height());
    painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, text_);
}

}  // namespace nmeasim::app
