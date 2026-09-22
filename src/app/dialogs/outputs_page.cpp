#include "outputs_page.hpp"

#include <nmeasim/io/serial_ports.hpp>

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace nmeasim::app {

namespace {

using Type = io::OutputConfig::Type;

constexpr std::array<Type, 7> kTypes{
    Type::TcpServer, Type::TcpClient, Type::Udp,    Type::WebSocketServer,
    Type::Serial,    Type::File,      Type::Stdout,
};

QString type_label(Type type) {
    switch (type) {
        case Type::TcpServer:
            return OutputsPage::tr("TCP server");
        case Type::TcpClient:
            return OutputsPage::tr("TCP client");
        case Type::Udp:
            return OutputsPage::tr("UDP");
        case Type::WebSocketServer:
            return OutputsPage::tr("WebSocket server");
        case Type::Serial:
            return OutputsPage::tr("Serial port");
        case Type::File:
            return OutputsPage::tr("File");
        case Type::Stdout:
            return OutputsPage::tr("Standard output");
    }
    return {};
}

QSpinBox* make_port(QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(0, 65535);
    spin->setKeyboardTracking(false);
    return spin;
}

constexpr std::array<int, 8> kBaudRates{1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

}  // namespace

OutputsPage::OutputsPage(QWidget* parent)
    : QWidget(parent),
      list(new QListWidget(this)),
      enabled_check(new QCheckBox(tr("Enabled"), this)),
      filter_edit(new QLineEdit(this)),
      editor_stack(new QStackedWidget(this)) {
    // Left: the list with add and remove buttons.
    auto* add_button = new QToolButton(this);
    add_button->setText(tr("Add"));
    add_button->setPopupMode(QToolButton::InstantPopup);
    auto* add_menu = new QMenu(add_button);
    for (const auto type : kTypes) {
        add_menu->addAction(type_label(type), this, [this, type] { add_output(type); });
    }
    add_button->setMenu(add_menu);
    auto* remove_button = new QPushButton(tr("Remove"), this);
    connect(remove_button, &QPushButton::clicked, this, &OutputsPage::remove_current);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(add_button);
    buttons->addWidget(remove_button);
    buttons->addStretch(1);
    auto* left = new QVBoxLayout;
    left->addWidget(list, 1);
    left->addLayout(buttons);

    // Right: common fields and the per-type editor.
    auto* common = new QFormLayout;
    common->addRow(enabled_check);
    filter_edit->setPlaceholderText(tr("All sentences"));
    filter_edit->setToolTip(
        tr("Comma-separated sentence ids to send on this output, for "
           "example RMC, GGA, VTG. Empty sends everything."));
    common->addRow(tr("Sentence filter"), filter_edit);

    auto* server_page = new QWidget(editor_stack);
    auto* server = new QFormLayout(server_page);
    bind_address_edit = new QLineEdit(server_page);
    server->addRow(tr("Bind address"), bind_address_edit);
    server_port_spin = make_port(server_page);
    server->addRow(tr("Port"), server_port_spin);
    editor_stack->addWidget(server_page);  // 0: TCP and WebSocket servers

    auto* client_page = new QWidget(editor_stack);
    auto* client = new QFormLayout(client_page);
    host_edit = new QLineEdit(client_page);
    client->addRow(tr("Host"), host_edit);
    client_port_spin = make_port(client_page);
    client->addRow(tr("Port"), client_port_spin);
    reconnect_spin = new QSpinBox(client_page);
    reconnect_spin->setRange(100, 600000);
    reconnect_spin->setSuffix(tr(" ms"));
    client->addRow(tr("Reconnect after"), reconnect_spin);
    editor_stack->addWidget(client_page);  // 1: TCP client

    auto* udp_page = new QWidget(editor_stack);
    auto* udp = new QFormLayout(udp_page);
    udp_mode_combo = new QComboBox(udp_page);
    udp_mode_combo->addItems({tr("Unicast"), tr("Broadcast"), tr("Multicast")});
    udp->addRow(tr("Mode"), udp_mode_combo);
    udp_address_edit = new QLineEdit(udp_page);
    udp_address_edit->setToolTip(
        tr("Destination address. For broadcast, leave empty to use the subnet broadcast "
           "address of the interface."));
    udp->addRow(tr("Address"), udp_address_edit);
    udp_port_spin = make_port(udp_page);
    udp->addRow(tr("Port"), udp_port_spin);
    udp_interface_edit = new QLineEdit(udp_page);
    udp_interface_edit->setPlaceholderText(tr("Any"));
    udp->addRow(tr("Interface"), udp_interface_edit);
    udp_ttl_spin = new QSpinBox(udp_page);
    udp_ttl_spin->setRange(1, 255);
    udp->addRow(tr("Multicast TTL"), udp_ttl_spin);
    editor_stack->addWidget(udp_page);  // 2: UDP

    auto* serial_page = new QWidget(editor_stack);
    auto* serial = new QFormLayout(serial_page);
    serial_port_combo = new QComboBox(serial_page);
    serial_port_combo->setEditable(true);
    for (const auto& port : io::available_serial_ports()) {
        serial_port_combo->addItem(QString::fromStdString(port.system_location));
    }
    serial->addRow(tr("Port"), serial_port_combo);
    baud_combo = new QComboBox(serial_page);
    baud_combo->setEditable(true);
    baud_combo->setValidator(new QIntValidator(1, 10000000, baud_combo));
    for (const int rate : kBaudRates) {
        baud_combo->addItem(QString::number(rate));
    }
    serial->addRow(tr("Baud rate"), baud_combo);
    data_bits_combo = new QComboBox(serial_page);
    data_bits_combo->addItems(
        {QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")});
    serial->addRow(tr("Data bits"), data_bits_combo);
    parity_combo = new QComboBox(serial_page);
    parity_combo->addItems({tr("None"), tr("Even"), tr("Odd"), tr("Space"), tr("Mark")});
    serial->addRow(tr("Parity"), parity_combo);
    stop_bits_combo = new QComboBox(serial_page);
    stop_bits_combo->addItems({QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2")});
    serial->addRow(tr("Stop bits"), stop_bits_combo);
    flow_control_combo = new QComboBox(serial_page);
    flow_control_combo->addItems({tr("None"), tr("Hardware (RTS/CTS)"), tr("Software (XON/XOFF)")});
    serial->addRow(tr("Flow control"), flow_control_combo);
    editor_stack->addWidget(serial_page);  // 3: serial

    auto* file_page = new QWidget(editor_stack);
    auto* file = new QFormLayout(file_page);
    file_path_edit = new QLineEdit(file_page);
    auto* browse = new QPushButton(tr("Browse..."), file_page);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Log file"), file_path_edit->text(), tr("NMEA logs (*.nmea *.log *.txt)"),
            nullptr, QFileDialog::DontConfirmOverwrite);
        if (!path.isEmpty()) {
            file_path_edit->setText(path);
        }
    });
    auto* path_row = new QHBoxLayout;
    path_row->addWidget(file_path_edit, 1);
    path_row->addWidget(browse);
    file->addRow(tr("Path"), path_row);
    append_check = new QCheckBox(tr("Append to an existing file"), file_page);
    file->addRow(append_check);
    editor_stack->addWidget(file_page);  // 4: file

    auto* stdout_page = new QLabel(tr("Sentences are written to the standard output of the "
                                      "process. Useful with the command-line tool."),
                                   editor_stack);
    stdout_page->setWordWrap(true);
    editor_stack->addWidget(stdout_page);  // 5: stdout

    auto* empty_page = new QLabel(tr("Add an output to send sentences somewhere."), editor_stack);
    empty_page->setAlignment(Qt::AlignCenter);
    editor_stack->addWidget(empty_page);  // 6: nothing selected

    auto* right = new QVBoxLayout;
    right->addLayout(common);
    right->addWidget(editor_stack, 1);

    auto* columns = new QHBoxLayout(this);
    columns->addLayout(left, 1);
    columns->addLayout(right, 2);

    connect(list, &QListWidget::currentRowChanged, this, &OutputsPage::show_output);
    show_output(-1);
}

void OutputsPage::load(const io::Profile& profile) {
    outputs_ = profile.outputs;
    editing_ = -1;
    loading_ = true;
    list->clear();
    for (const auto& output : outputs_) {
        list->addItem(title_of(output));
    }
    loading_ = false;
    if (!outputs_.isEmpty()) {
        list->setCurrentRow(0);
    } else {
        show_output(-1);
    }
}

void OutputsPage::store(io::Profile& profile) const {
    profile.outputs = outputs();
}

QList<io::OutputConfig> OutputsPage::outputs() const {
    auto result = outputs_;
    if (editing_ >= 0 && editing_ < result.size()) {
        const_cast<OutputsPage*>(this)->commit_editor();
        result = outputs_;
    }
    return result;
}

QString OutputsPage::validate() const {
    const auto all = outputs();
    for (int index = 0; index < all.size(); ++index) {
        const auto& output = all.at(index);
        if (output.type == Type::Serial && output.serial.port_name.trimmed().isEmpty()) {
            return tr("Output %1: the serial port name is missing.").arg(index + 1);
        }
        if (output.type == Type::File && output.path.trimmed().isEmpty()) {
            return tr("Output %1: the file path is missing.").arg(index + 1);
        }
        if (output.type == Type::TcpClient && output.host.trimmed().isEmpty()) {
            return tr("Output %1: the host is missing.").arg(index + 1);
        }
    }
    return {};
}

void OutputsPage::add_output(Type type) {
    commit_editor();
    io::OutputConfig output;
    output.type = type;
    if (type == Type::WebSocketServer) {
        output.port = 3000;
    }
    outputs_.append(output);
    list->addItem(title_of(output));
    list->setCurrentRow(static_cast<int>(outputs_.size()) - 1);
}

void OutputsPage::remove_current() {
    const int index = list->currentRow();
    if (index < 0 || index >= outputs_.size()) {
        return;
    }
    editing_ = -1;
    outputs_.removeAt(index);
    delete list->takeItem(index);
    if (list->currentRow() >= 0) {
        show_output(list->currentRow());
    } else {
        show_output(-1);
    }
}

void OutputsPage::select(int index) {
    list->setCurrentRow(index);
}

int OutputsPage::count() const {
    return static_cast<int>(outputs_.size());
}

int OutputsPage::current_index() const {
    return list->currentRow();
}

int OutputsPage::page_of(Type type) {
    switch (type) {
        case Type::TcpServer:
        case Type::WebSocketServer:
            return 0;
        case Type::TcpClient:
            return 1;
        case Type::Udp:
            return 2;
        case Type::Serial:
            return 3;
        case Type::File:
            return 4;
        case Type::Stdout:
            return 5;
    }
    return 6;
}

void OutputsPage::show_output(int index) {
    if (loading_) {
        return;
    }
    commit_editor();
    if (index < 0 || index >= outputs_.size()) {
        editing_ = -1;
        editor_stack->setCurrentIndex(6);
        enabled_check->setEnabled(false);
        filter_edit->setEnabled(false);
        return;
    }
    const auto& output = outputs_.at(index);
    enabled_check->setEnabled(true);
    filter_edit->setEnabled(true);
    enabled_check->setChecked(output.enabled);
    filter_edit->setText(output.filter.join(QStringLiteral(", ")));
    editor_stack->setCurrentIndex(page_of(output.type));
    switch (output.type) {
        case Type::TcpServer:
        case Type::WebSocketServer:
            bind_address_edit->setText(output.bind_address);
            server_port_spin->setValue(output.port);
            break;
        case Type::TcpClient:
            host_edit->setText(output.host);
            client_port_spin->setValue(output.port);
            reconnect_spin->setValue(output.reconnect_ms);
            break;
        case Type::Udp:
            udp_mode_combo->setCurrentIndex(static_cast<int>(output.udp.mode));
            udp_address_edit->setText(output.udp.address);
            udp_port_spin->setValue(output.udp.port);
            udp_interface_edit->setText(output.udp.interface_name);
            udp_ttl_spin->setValue(output.udp.multicast_ttl);
            break;
        case Type::Serial: {
            serial_port_combo->setCurrentText(output.serial.port_name);
            baud_combo->setCurrentText(QString::number(output.serial.baud_rate));
            data_bits_combo->setCurrentIndex(static_cast<int>(output.serial.data_bits) - 5);
            int parity = 0;
            switch (output.serial.parity) {
                case QSerialPort::EvenParity:
                    parity = 1;
                    break;
                case QSerialPort::OddParity:
                    parity = 2;
                    break;
                case QSerialPort::SpaceParity:
                    parity = 3;
                    break;
                case QSerialPort::MarkParity:
                    parity = 4;
                    break;
                default:
                    break;
            }
            parity_combo->setCurrentIndex(parity);
            stop_bits_combo->setCurrentIndex(output.serial.stop_bits == QSerialPort::OneAndHalfStop
                                                 ? 1
                                             : output.serial.stop_bits == QSerialPort::TwoStop ? 2
                                                                                               : 0);
            flow_control_combo->setCurrentIndex(static_cast<int>(output.serial.flow_control));
            break;
        }
        case Type::File:
            file_path_edit->setText(output.path);
            append_check->setChecked(output.append);
            break;
        case Type::Stdout:
            break;
    }
    editing_ = index;
}

void OutputsPage::commit_editor() {
    if (editing_ < 0 || editing_ >= outputs_.size()) {
        return;
    }
    auto& output = outputs_[editing_];
    output.enabled = enabled_check->isChecked();
    output.filter.clear();
    for (const auto& part : filter_edit->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString id = part.trimmed().toUpper();
        if (!id.isEmpty()) {
            output.filter.append(id);
        }
    }
    switch (output.type) {
        case Type::TcpServer:
        case Type::WebSocketServer:
            output.bind_address = bind_address_edit->text().trimmed();
            output.port = static_cast<quint16>(server_port_spin->value());
            break;
        case Type::TcpClient:
            output.host = host_edit->text().trimmed();
            output.port = static_cast<quint16>(client_port_spin->value());
            output.reconnect_ms = reconnect_spin->value();
            break;
        case Type::Udp:
            output.udp.mode = static_cast<io::UdpConfig::Mode>(udp_mode_combo->currentIndex());
            output.udp.address = udp_address_edit->text().trimmed();
            output.udp.port = static_cast<quint16>(udp_port_spin->value());
            output.udp.interface_name = udp_interface_edit->text().trimmed();
            output.udp.multicast_ttl = udp_ttl_spin->value();
            break;
        case Type::Serial: {
            output.serial.port_name = serial_port_combo->currentText().trimmed();
            output.serial.baud_rate = baud_combo->currentText().toInt();
            output.serial.data_bits =
                static_cast<QSerialPort::DataBits>(data_bits_combo->currentIndex() + 5);
            constexpr std::array<QSerialPort::Parity, 5> parities{
                QSerialPort::NoParity, QSerialPort::EvenParity, QSerialPort::OddParity,
                QSerialPort::SpaceParity, QSerialPort::MarkParity};
            output.serial.parity =
                parities.at(static_cast<std::size_t>(parity_combo->currentIndex()));
            constexpr std::array<QSerialPort::StopBits, 3> stop_bits{
                QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop};
            output.serial.stop_bits =
                stop_bits.at(static_cast<std::size_t>(stop_bits_combo->currentIndex()));
            output.serial.flow_control =
                static_cast<QSerialPort::FlowControl>(flow_control_combo->currentIndex());
            break;
        }
        case Type::File:
            output.path = file_path_edit->text().trimmed();
            output.append = append_check->isChecked();
            break;
        case Type::Stdout:
            break;
    }
    refresh_titles();
}

void OutputsPage::refresh_titles() {
    for (int index = 0; index < outputs_.size() && index < list->count(); ++index) {
        list->item(index)->setText(title_of(outputs_.at(index)));
    }
}

QString OutputsPage::title_of(const io::OutputConfig& output) {
    QString detail;
    switch (output.type) {
        case Type::TcpServer:
        case Type::WebSocketServer:
            detail = QStringLiteral("%1:%2").arg(output.bind_address).arg(output.port);
            break;
        case Type::TcpClient:
            detail = QStringLiteral("%1:%2").arg(output.host).arg(output.port);
            break;
        case Type::Udp:
            detail = QStringLiteral("%1 %2:%3")
                         .arg(io::to_string(output.udp.mode), output.udp.address)
                         .arg(output.udp.port);
            break;
        case Type::Serial:
            detail =
                QStringLiteral("%1 @ %2").arg(output.serial.port_name).arg(output.serial.baud_rate);
            break;
        case Type::File:
            detail = output.path;
            break;
        case Type::Stdout:
            break;
    }
    QString title = type_label(output.type);
    if (!detail.trimmed().isEmpty()) {
        title += QStringLiteral(" - ") + detail;
    }
    if (!output.enabled) {
        title += tr(" (disabled)");
    }
    return title;
}

}  // namespace nmeasim::app
