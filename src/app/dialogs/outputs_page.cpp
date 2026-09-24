// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `OutputsPage`, the *Outputs* tab of the settings dialog.
///
/// Builds the list and the editor widgets, moves values between the widgets and the page's
/// copy of `io::Profile::outputs`, and checks the outputs before the dialog accepts them.

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

#include <algorithm>
#include <array>
#include <string>

namespace nmeasim::app {

namespace {

/// Short name for the output transport type, used throughout this file.
using Type = io::OutputConfig::Type;

/// Every output type, in the order the *Add* menu offers them.
///
/// Holds every enumerator of `io::OutputConfig::Type`; a new type must be added here to be
/// offered.
constexpr std::array<Type, 8> kTypes{
    Type::TcpServer, Type::TcpClient, Type::Udp,    Type::WebSocketServer,
    Type::Serial,    Type::File,      Type::Stdout, Type::Log,
};

/// Returns the translated name of an output type, as shown in the *Add* menu and the list.
///
/// @param type The output type.
/// @return The name, such as `TCP server` or `Log (timestamped)`; empty for a value outside
///     the enumeration.
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
        case Type::Log:
            return OutputsPage::tr("Log (timestamped)");
    }
    return {};
}

/// Creates a spin box for a TCP or UDP port number, limited to [0, 65535].
///
/// Keyboard tracking is off, so typing a port does not emit a value for every digit.
///
/// @param parent Qt parent that owns the spin box.
/// @return The new spin box, owned by `parent`.
QSpinBox* make_port(QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(0, 65535);
    spin->setKeyboardTracking(false);
    return spin;
}

/// Line speeds offered in the baud rate list, in bit/s: the common rates from 1200 to 115200,
/// including 4800 for NMEA 0183 and 38400 for IEC 61162-2 (AIS). Other rates can be typed.
constexpr std::array<int, 8> kBaudRates{1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

}  // namespace

OutputsPage::OutputsPage(QWidget* parent)
    : QWidget(parent),
      list(new QListWidget(this)),
      enabled_check(new QCheckBox(tr("Enabled"), this)),
      filter_edit(new QLineEdit(this)),
      encoding_combo(new QComboBox(this)),
      period_spin(new QSpinBox(this)),
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

    // Right: the common fields, the per-type editor and the encoding option groups.
    auto* common = new QFormLayout;
    common->addRow(enabled_check);
    filter_edit->setPlaceholderText(tr("All sentences"));
    filter_edit->setToolTip(
        tr("Comma-separated sentence ids to send on this output, for "
           "example RMC, GGA, VTG. Empty sends everything."));
    common->addRow(tr("Sentence filter"), filter_edit);
    encoding_combo->addItems(
        {tr("NMEA 0183 sentences"), tr("Signal K deltas"), tr("ViewSync packets")});
    encoding_combo->setToolTip(tr("What this output carries"));
    common->addRow(tr("Encoding"), encoding_combo);
    period_spin->setRange(50, 3600000);
    period_spin->setSingleStep(100);
    period_spin->setSuffix(tr(" ms"));
    period_spin->setKeyboardTracking(false);
    period_spin->setToolTip(tr("How often a Signal K delta or a ViewSync packet is sent"));
    common->addRow(tr("Period"), period_spin);
    connect(encoding_combo, &QComboBox::currentIndexChanged, this,
            &OutputsPage::update_encoding_widgets);

    tag_block_box = new QGroupBox(tr("IEC 61162-450 TAG block"), this);
    auto* tag = new QFormLayout(tag_block_box);
    tag_block_check = new QCheckBox(tr("Prefix every sentence with a TAG block"), tag_block_box);
    tag->addRow(tag_block_check);
    tag_source_edit = new QLineEdit(tag_block_box);
    tag_source_edit->setMaxLength(15);
    tag_source_edit->setPlaceholderText(QStringLiteral("SIM0001"));
    tag->addRow(tr("Source (s:)"), tag_source_edit);
    tag_time_check = new QCheckBox(tr("Include the time (c:)"), tag_block_box);
    tag->addRow(tag_time_check);
    tag_milliseconds_check = new QCheckBox(tr("Time in milliseconds"), tag_block_box);
    tag->addRow(tag_milliseconds_check);

    signalk_box = new QGroupBox(tr("Signal K"), this);
    auto* signalk = new QFormLayout(signalk_box);
    signalk_context_combo = new QComboBox(signalk_box);
    signalk_context_combo->addItems(
        {tr("Vessel with the AIS MMSI"), tr("Aircraft"), tr("Custom context")});
    signalk->addRow(tr("Context"), signalk_context_combo);
    signalk_context_edit = new QLineEdit(signalk_box);
    signalk_context_edit->setPlaceholderText(QStringLiteral("vessels.urn:mrn:imo:mmsi:239000001"));
    signalk->addRow(tr("Context string"), signalk_context_edit);
    signalk_source_edit = new QLineEdit(signalk_box);
    signalk_source_edit->setPlaceholderText(QStringLiteral("nmeasim"));
    signalk->addRow(tr("Source label"), signalk_source_edit);
    connect(signalk_context_combo, &QComboBox::currentIndexChanged, this,
            [this](int index) { signalk_context_edit->setEnabled(index == 2); });

    viewsync_box = new QGroupBox(tr("ViewSync camera"), this);
    auto* viewsync = new QFormLayout(viewsync_box);
    camera_altitude_spin = new QDoubleSpinBox(viewsync_box);
    camera_altitude_spin->setRange(0.0, 100000.0);
    camera_altitude_spin->setDecimals(0);
    camera_altitude_spin->setSingleStep(100.0);
    camera_altitude_spin->setSuffix(tr(" m"));
    viewsync->addRow(tr("Height above the vessel"), camera_altitude_spin);
    tilt_spin = new QDoubleSpinBox(viewsync_box);
    tilt_spin->setRange(0.0, 90.0);
    tilt_spin->setDecimals(0);
    tilt_spin->setSuffix(tr("°"));
    viewsync->addRow(tr("Tilt"), tilt_spin);
    roll_spin = new QDoubleSpinBox(viewsync_box);
    roll_spin->setRange(-180.0, 180.0);
    roll_spin->setDecimals(0);
    roll_spin->setSuffix(tr("°"));
    viewsync->addRow(tr("Roll"), roll_spin);
    planet_combo = new QComboBox(viewsync_box);
    planet_combo->addItems(
        {tr("Earth"), QStringLiteral("sky"), QStringLiteral("mars"), QStringLiteral("moon")});
    viewsync->addRow(tr("Planet"), planet_combo);

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
    right->addWidget(tag_block_box);
    right->addWidget(signalk_box);
    right->addWidget(viewsync_box);
    update_encoding_widgets();

    auto* columns = new QHBoxLayout(this);
    columns->addLayout(left, 1);
    columns->addLayout(right, 2);

    connect(list, &QListWidget::currentRowChanged, this, &OutputsPage::show_output);
    show_output(-1);
}

void OutputsPage::update_encoding_widgets() {
    const auto encoding = static_cast<io::OutputConfig::Encoding>(encoding_combo->currentIndex());
    const bool nmea = encoding == io::OutputConfig::Encoding::Nmea0183;
    tag_block_box->setVisible(nmea);
    signalk_box->setVisible(encoding == io::OutputConfig::Encoding::SignalK);
    viewsync_box->setVisible(encoding == io::OutputConfig::Encoding::ViewSync);
    period_spin->setEnabled(!nmea);
    filter_edit->setPlaceholderText(nmea ? tr("All sentences") : tr("All paths"));
    filter_edit->setEnabled(encoding != io::OutputConfig::Encoding::ViewSync);
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
        // The widgets hold the latest values of the output being edited, so the copy is
        // refreshed from them first; `validate` and `store` rely on that, hence the cast.
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
        if ((output.type == Type::File || output.type == Type::Log) &&
            output.path.trimmed().isEmpty()) {
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
    // Forget the editor before removing, so the removed output's widgets are not committed
    // into whichever output takes its index.
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
        case Type::Log:
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
        encoding_combo->setEnabled(false);
        period_spin->setEnabled(false);
        tag_block_box->hide();
        signalk_box->hide();
        viewsync_box->hide();
        return;
    }
    const auto& output = outputs_.at(index);
    enabled_check->setEnabled(true);
    filter_edit->setEnabled(true);
    encoding_combo->setEnabled(true);
    enabled_check->setChecked(output.enabled);
    filter_edit->setText(output.filter.join(QStringLiteral(", ")));
    encoding_combo->setCurrentIndex(static_cast<int>(output.encoding));
    period_spin->setValue(output.period_ms);
    tag_block_check->setChecked(output.tag_block.enabled);
    tag_source_edit->setText(QString::fromStdString(output.tag_block.options.source));
    tag_time_check->setChecked(output.tag_block.options.include_time);
    tag_milliseconds_check->setChecked(output.tag_block.options.milliseconds);
    const QString context = QString::fromStdString(output.signalk.context);
    signalk_context_combo->setCurrentIndex(context.isEmpty()                      ? 0
                                           : context == QLatin1String("aircraft") ? 1
                                                                                  : 2);
    signalk_context_edit->setText(context == QLatin1String("aircraft") ? QString{} : context);
    signalk_context_edit->setEnabled(signalk_context_combo->currentIndex() == 2);
    signalk_source_edit->setText(QString::fromStdString(output.signalk.source_label));
    camera_altitude_spin->setValue(output.viewsync.camera_altitude_m);
    tilt_spin->setValue(output.viewsync.tilt_deg);
    roll_spin->setValue(output.viewsync.roll_deg);
    planet_combo->setCurrentIndex(
        std::max(0, planet_combo->findText(QString::fromStdString(output.viewsync.planet))));
    if (output.viewsync.planet.empty()) {
        planet_combo->setCurrentIndex(0);
    }
    update_encoding_widgets();
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
        case Type::Log:
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
    output.encoding = static_cast<io::OutputConfig::Encoding>(encoding_combo->currentIndex());
    output.filter.clear();
    const bool nmea = output.encoding == io::OutputConfig::Encoding::Nmea0183;
    for (const auto& part : filter_edit->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString id = nmea ? part.trimmed().toUpper() : part.trimmed();
        if (!id.isEmpty()) {
            output.filter.append(id);
        }
    }
    output.period_ms = period_spin->value();
    output.tag_block.enabled = tag_block_check->isChecked();
    // An empty source falls back to the profile default rather than an empty `s:` parameter.
    const QString tag_source = tag_source_edit->text().trimmed();
    output.tag_block.options.source =
        tag_source.isEmpty() ? std::string{"SIM0001"} : tag_source.toStdString();
    output.tag_block.options.include_time = tag_time_check->isChecked();
    output.tag_block.options.milliseconds = tag_milliseconds_check->isChecked();
    switch (signalk_context_combo->currentIndex()) {
        case 1:
            output.signalk.context = "aircraft";
            break;
        case 2:
            output.signalk.context = signalk_context_edit->text().trimmed().toStdString();
            break;
        default:
            output.signalk.context.clear();
            break;
    }
    const QString source_label = signalk_source_edit->text().trimmed();
    output.signalk.source_label =
        source_label.isEmpty() ? std::string{"nmeasim"} : source_label.toStdString();
    output.viewsync.camera_altitude_m = camera_altitude_spin->value();
    output.viewsync.tilt_deg = tilt_spin->value();
    output.viewsync.roll_deg = roll_spin->value();
    output.viewsync.planet = planet_combo->currentIndex() == 0
                                 ? std::string{}
                                 : planet_combo->currentText().toStdString();
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
        case Type::Log:
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
        case Type::Log:
            detail = output.path;
            break;
        case Type::Stdout:
            break;
    }
    QString title = type_label(output.type);
    if (!detail.trimmed().isEmpty()) {
        title += QStringLiteral(" - ") + detail;
    }
    if (output.encoding == io::OutputConfig::Encoding::SignalK) {
        title += tr(" (Signal K)");
    } else if (output.encoding == io::OutputConfig::Encoding::ViewSync) {
        title += tr(" (ViewSync)");
    }
    if (!output.enabled) {
        title += tr(" (disabled)");
    }
    return title;
}

}  // namespace nmeasim::app
