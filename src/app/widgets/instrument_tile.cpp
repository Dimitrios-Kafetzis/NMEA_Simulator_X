#include "instrument_tile.hpp"

#include "theme/theme.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

namespace nmeasim::app {

namespace {

/// Point size of numeric readouts and of text readouts (positions, times), relative to the
/// application font.
constexpr double kNumberScale{2.1};
constexpr double kTextScale{1.4};

QFont caption_font(const QFont& base) {
    QFont font = base;
    font.setPointSizeF(base.pointSizeF() * 0.82);
    font.setBold(true);
    font.setLetterSpacing(QFont::PercentageSpacing, 110);
    font.setCapitalization(QFont::AllUppercase);
    return font;
}

}  // namespace

InstrumentTile::InstrumentTile(const QString& title, const QString& unit, QWidget* parent)
    : QFrame(parent), value_label_(new QLabel(this)), unit_label_(new QLabel(unit, this)) {
    setObjectName(QStringLiteral("instrument_tile"));
    setProperty("overridden", false);
    setMinimumWidth(180);

    auto* title_label = new QLabel(title, this);
    title_label->setObjectName(QStringLiteral("tile_title"));
    title_label->setFont(caption_font(title_label->font()));

    value_label_->setObjectName(QStringLiteral("tile_value"));
    value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value_label_->setText(QStringLiteral("--"));
    base_point_size_ = value_label_->font().pointSizeF();
    value_label_->setFont(theme::Theme::readout_font(base_point_size_ * kNumberScale));
    unit_label_->setObjectName(QStringLiteral("tile_unit"));
    unit_label_->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

    auto* value_row = new QHBoxLayout;
    value_row->setSpacing(6);
    value_row->addWidget(value_label_, 1);
    value_row->addWidget(unit_label_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);
    layout->addWidget(title_label);
    layout->addLayout(value_row);
}

void InstrumentTile::set_value(double value, int decimals) {
    set_readout_scale(kNumberScale);
    value_label_->setText(QString::number(value, 'f', decimals));
}

void InstrumentTile::set_text(const QString& text) {
    set_readout_scale(kTextScale);
    value_label_->setText(text);
}

void InstrumentTile::set_readout_scale(double scale) {
    if (scale == readout_scale_) {
        return;
    }
    readout_scale_ = scale;
    value_label_->setFont(theme::Theme::readout_font(base_point_size_ * scale));
}

void InstrumentTile::mark_overridden(bool overridden) {
    if (property("overridden").toBool() == overridden) {
        return;
    }
    setProperty("overridden", overridden);
    // Dynamic properties only take effect in the style sheet after a re-polish.
    style()->unpolish(this);
    style()->polish(this);
}

void InstrumentTile::enable_override(double minimum, double maximum, double step, int decimals) {
    if (override_check_ != nullptr) {
        return;
    }
    override_check_ = new QCheckBox(tr("Override"), this);
    override_spin_ = new QDoubleSpinBox(this);
    override_spin_->setRange(minimum, maximum);
    override_spin_->setSingleStep(step);
    override_spin_->setDecimals(decimals);
    override_spin_->setEnabled(false);
    override_spin_->setKeyboardTracking(false);

    auto* row = new QHBoxLayout;
    row->addWidget(override_check_);
    row->addWidget(override_spin_, 1);
    static_cast<QVBoxLayout*>(layout())->addLayout(row);

    connect(override_check_, &QCheckBox::toggled, this, [this](bool checked) {
        override_spin_->setEnabled(checked);
        mark_overridden(checked);
        if (!suppress_signals_) {
            emit override_changed(checked, override_spin_->value());
        }
    });
    connect(override_spin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!suppress_signals_ && override_check_->isChecked()) {
            emit override_changed(true, value);
        }
    });
}

bool InstrumentTile::override_active() const {
    return override_check_ != nullptr && override_check_->isChecked();
}

double InstrumentTile::override_value() const {
    return override_spin_ != nullptr ? override_spin_->value() : 0.0;
}

void InstrumentTile::set_override(bool active, double value) {
    if (override_check_ == nullptr) {
        return;
    }
    suppress_signals_ = true;
    override_spin_->setValue(value);
    override_check_->setChecked(active);
    mark_overridden(active);
    suppress_signals_ = false;
}

void InstrumentTile::set_override_enabled(bool enabled) {
    if (override_check_ == nullptr) {
        return;
    }
    override_check_->setEnabled(enabled);
    override_spin_->setEnabled(enabled && override_check_->isChecked());
}

}  // namespace nmeasim::app
