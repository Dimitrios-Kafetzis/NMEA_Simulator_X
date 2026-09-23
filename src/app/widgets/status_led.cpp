#include "status_led.hpp"

#include "theme/theme.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QRadialGradient>

#include <algorithm>

namespace nmeasim::app {

namespace {

constexpr int kLedDiameter{9};
constexpr int kGap{6};
constexpr int kMargin{6};
constexpr int kBlinkIntervalMs{500};

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
        // Soft glow around a lit light.
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
