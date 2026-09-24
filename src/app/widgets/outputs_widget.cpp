// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Table and status lights of the *Outputs* panel.

#include "outputs_widget.hpp"

#include "theme/theme.hpp"

#include <QHeaderView>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

namespace nmeasim::app {

namespace {

/// Period of the table refresh, in milliseconds: twice a second, independent of the
/// simulation step.
constexpr int kRefreshIntervalMs{500};

/// Columns of the outputs table, in display order.
enum Column {
    /// The transport's description, naming the output.
    Description = 0,
    /// The transport state with a coloured light.
    Status,
    /// Connected clients, as `io::Transport::client_count` reports them.
    Clients,
    /// Lines handed to the open transport, sentences or state messages as the encoding
    /// decides; `io::OutputChannel::lines_sent`.
    Lines,
    /// Bytes written, summed over every consumer.
    Bytes,
    /// Message of the most recent failure; empty when none occurred.
    LastError,
    /// Number of columns, not a column.
    ColumnCount
};

/// Draws the status light of an output.
///
/// @param color Fill colour; the outline is a darker shade of it.
/// @return A 12 by 12 pixel icon with a disc of radius 4 on a transparent background.
QIcon dot_icon(const QColor& color) {
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color.darker(150), 1.0));
    painter.setBrush(color);
    painter.drawEllipse(QPointF(6, 6), 4.0, 4.0);
    return QIcon(pixmap);
}

}  // namespace

QColor state_color(io::Transport::State state) {
    const auto& colors = theme::Theme::instance().colors();
    switch (state) {
        case io::Transport::State::Open:
            return colors.ok;
        case io::Transport::State::Opening:
            return colors.warning;
        case io::Transport::State::Failed:
            return colors.danger;
        case io::Transport::State::Closed:
            break;
    }
    return colors.inactive;
}

OutputsWidget::OutputsWidget(QWidget* parent) : QWidget(parent), table_(new QTableWidget(this)) {
    table_->setColumnCount(ColumnCount);
    table_->setHorizontalHeaderLabels(
        {tr("Output"), tr("Status"), tr("Clients"), tr("Lines"), tr("Bytes"), tr("Last error")});
    table_->horizontalHeader()->setSectionResizeMode(Description, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(LastError, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setHighlightSections(false);
    connect(&theme::Theme::instance(), &theme::Theme::changed, this, &OutputsWidget::refresh);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(table_);

    refresh_timer_.setInterval(kRefreshIntervalMs);
    connect(&refresh_timer_, &QTimer::timeout, this, &OutputsWidget::refresh);
}

void OutputsWidget::set_runner(io::SimulationRunner* runner) {
    if (runner_ != nullptr) {
        disconnect(runner_, nullptr, this, nullptr);
    }
    runner_ = runner;
    if (runner_ != nullptr) {
        connect(runner_, &io::SimulationRunner::started, this, &OutputsWidget::refresh);
        connect(runner_, &io::SimulationRunner::stopped, this, &OutputsWidget::refresh);
        connect(runner_, &io::SimulationRunner::output_error, this, &OutputsWidget::refresh);
        refresh_timer_.start();
    } else {
        refresh_timer_.stop();
    }
    refresh();
}

void OutputsWidget::refresh() {
    if (runner_ == nullptr) {
        table_->setRowCount(0);
        return;
    }
    const auto& outputs = runner_->outputs();
    table_->setRowCount(static_cast<int>(outputs.size()));
    int row = 0;
    for (const auto& channel : outputs) {
        const auto* transport = channel.transport.get();
        const QStringList cells{
            transport->description(),
            io::to_string(transport->state()),
            QString::number(transport->client_count()),
            QString::number(channel.lines_sent),
            QString::number(transport->bytes_written()),
            transport->last_error(),
        };
        for (int column = 0; column < cells.size(); ++column) {
            auto* item = table_->item(row, column);
            if (item == nullptr) {
                item = new QTableWidgetItem;
                table_->setItem(row, column, item);
            }
            item->setText(cells.at(column));
        }
        // The state column is a coloured badge: green open, amber opening, red failed, grey
        // closed. The foreground colour is also what status_color reads back.
        const QColor color = state_color(transport->state());
        auto* status = table_->item(row, Status);
        status->setIcon(dot_icon(color));
        status->setForeground(color);
        ++row;
    }
}

int OutputsWidget::row_count() const {
    return table_->rowCount();
}

QColor OutputsWidget::status_color(int row) const {
    const auto* item = table_->item(row, Status);
    return item != nullptr ? item->foreground().color() : QColor{};
}

QString OutputsWidget::status_text(int row) const {
    const auto* item = table_->item(row, Status);
    return item != nullptr ? item->text() : QString{};
}

}  // namespace nmeasim::app
