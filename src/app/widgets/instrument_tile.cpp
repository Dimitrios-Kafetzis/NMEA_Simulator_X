#include "instrument_tile.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace nmeasim::app {

InstrumentTile::InstrumentTile(const QString& title, const QString& unit, QWidget* parent)
    : QFrame(parent), value_label_(new QLabel(this)), unit_label_(new QLabel(unit, this)) {
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setMinimumWidth(180);

    auto* title_label = new QLabel(title, this);
    QFont title_font = title_label->font();
    title_font.setPointSizeF(title_font.pointSizeF() * 0.9);
    title_font.setBold(true);
    title_label->setFont(title_font);

    QFont value_font = value_label_->font();
    value_font.setPointSizeF(value_font.pointSizeF() * 1.8);
    value_label_->setFont(value_font);
    value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value_label_->setText(QStringLiteral("--"));

    auto* value_row = new QHBoxLayout;
    value_row->addWidget(value_label_, 1);
    value_row->addWidget(unit_label_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->addWidget(title_label);
    layout->addLayout(value_row);
}

void InstrumentTile::set_value(double value, int decimals) {
    value_label_->setText(QString::number(value, 'f', decimals));
}

void InstrumentTile::set_text(const QString& text) {
    value_label_->setText(text);
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
