#pragma once

#include <QColor>
#include <QString>
#include <QTimer>
#include <QWidget>

namespace nmeasim::app {

/// A coloured indicator light with a short upper-case caption, as on an instrument panel:
/// the run state, recording and the outputs in the status bar. The light can blink.
class StatusLed : public QWidget {
    Q_OBJECT

public:
    explicit StatusLed(QWidget* parent = nullptr);

    /// Sets the light colour and the caption; an invalid colour shows an unlit light.
    void set_state(const QColor& color, const QString& text);
    /// Blinks the light about once a second, for example while recording.
    void set_blinking(bool blinking);

    [[nodiscard]] QColor color() const noexcept { return color_; }
    [[nodiscard]] QString text() const noexcept { return text_; }
    [[nodiscard]] bool blinking() const noexcept { return blink_timer_.isActive(); }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor color_;
    QString text_;
    QTimer blink_timer_;
    bool lit_{true};
};

}  // namespace nmeasim::app
