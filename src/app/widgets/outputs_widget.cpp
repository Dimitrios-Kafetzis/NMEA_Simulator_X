#include "outputs_widget.hpp"

#include <QHeaderView>
#include <QVBoxLayout>

namespace nmeasim::app {

namespace {

constexpr int kRefreshIntervalMs{500};

enum Column { Description = 0, Status, Clients, Sentences, Bytes, LastError, ColumnCount };

}  // namespace

OutputsWidget::OutputsWidget(QWidget* parent) : QWidget(parent), table_(new QTableWidget(this)) {
    table_->setColumnCount(ColumnCount);
    table_->setHorizontalHeaderLabels({tr("Output"), tr("Status"), tr("Clients"), tr("Sentences"),
                                       tr("Bytes"), tr("Last error")});
    table_->horizontalHeader()->setSectionResizeMode(Description, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(LastError, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);

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
            QString::number(channel.sentences_sent),
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
        ++row;
    }
}

int OutputsWidget::row_count() const {
    return table_->rowCount();
}

QString OutputsWidget::status_text(int row) const {
    const auto* item = table_->item(row, Status);
    return item != nullptr ? item->text() : QString{};
}

}  // namespace nmeasim::app
