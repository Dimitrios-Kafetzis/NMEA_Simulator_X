#include <nmeasim/io/profile/profile.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <chrono>

namespace nmeasim::io {

namespace {

using core::model::FixQuality;
using core::simulation::Variation;

// ---------------------------------------------------------------------------------------------
// Small JSON helpers

double number(const QJsonObject& object, const char* key, double fallback) {
    return object.value(QLatin1String(key)).toDouble(fallback);
}

int integer(const QJsonObject& object, const char* key, int fallback) {
    return object.value(QLatin1String(key)).toInt(fallback);
}

bool boolean(const QJsonObject& object, const char* key, bool fallback) {
    return object.value(QLatin1String(key)).toBool(fallback);
}

QString text(const QJsonObject& object, const char* key, const QString& fallback = {}) {
    return object.value(QLatin1String(key)).toString(fallback);
}

QJsonObject variation_to_json(const Variation& variation) {
    return {{QStringLiteral("amplitude"), variation.amplitude},
            {QStringLiteral("step_per_second"), variation.step_per_second}};
}

Variation variation_from_json(const QJsonValue& value, const Variation& fallback) {
    if (!value.isObject()) {
        return fallback;
    }
    const auto object = value.toObject();
    return {number(object, "amplitude", fallback.amplitude),
            number(object, "step_per_second", fallback.step_per_second)};
}

QString fix_quality_to_string(FixQuality quality) {
    switch (quality) {
        case FixQuality::Invalid:
            return QStringLiteral("invalid");
        case FixQuality::Gps:
            return QStringLiteral("gps");
        case FixQuality::Differential:
            return QStringLiteral("differential");
    }
    return QStringLiteral("gps");
}

FixQuality fix_quality_from_string(const QString& value) {
    if (value == QLatin1String("invalid")) {
        return FixQuality::Invalid;
    }
    if (value == QLatin1String("differential")) {
        return FixQuality::Differential;
    }
    return FixQuality::Gps;
}

QString parity_to_string(QSerialPort::Parity parity) {
    switch (parity) {
        case QSerialPort::EvenParity:
            return QStringLiteral("even");
        case QSerialPort::OddParity:
            return QStringLiteral("odd");
        case QSerialPort::MarkParity:
            return QStringLiteral("mark");
        case QSerialPort::SpaceParity:
            return QStringLiteral("space");
        case QSerialPort::NoParity:
            break;
    }
    return QStringLiteral("none");
}

QSerialPort::Parity parity_from_string(const QString& value) {
    if (value == QLatin1String("even")) {
        return QSerialPort::EvenParity;
    }
    if (value == QLatin1String("odd")) {
        return QSerialPort::OddParity;
    }
    if (value == QLatin1String("mark")) {
        return QSerialPort::MarkParity;
    }
    if (value == QLatin1String("space")) {
        return QSerialPort::SpaceParity;
    }
    return QSerialPort::NoParity;
}

QString stop_bits_to_string(QSerialPort::StopBits bits) {
    switch (bits) {
        case QSerialPort::OneAndHalfStop:
            return QStringLiteral("1.5");
        case QSerialPort::TwoStop:
            return QStringLiteral("2");
        case QSerialPort::OneStop:
            break;
    }
    return QStringLiteral("1");
}

QSerialPort::StopBits stop_bits_from_string(const QString& value) {
    if (value == QLatin1String("1.5")) {
        return QSerialPort::OneAndHalfStop;
    }
    if (value == QLatin1String("2")) {
        return QSerialPort::TwoStop;
    }
    return QSerialPort::OneStop;
}

QString flow_control_to_string(QSerialPort::FlowControl flow) {
    switch (flow) {
        case QSerialPort::HardwareControl:
            return QStringLiteral("hardware");
        case QSerialPort::SoftwareControl:
            return QStringLiteral("software");
        case QSerialPort::NoFlowControl:
            break;
    }
    return QStringLiteral("none");
}

QSerialPort::FlowControl flow_control_from_string(const QString& value) {
    if (value == QLatin1String("hardware")) {
        return QSerialPort::HardwareControl;
    }
    if (value == QLatin1String("software")) {
        return QSerialPort::SoftwareControl;
    }
    return QSerialPort::NoFlowControl;
}

QSerialPort::DataBits data_bits_from_int(int bits) {
    switch (bits) {
        case 5:
            return QSerialPort::Data5;
        case 6:
            return QSerialPort::Data6;
        case 7:
            return QSerialPort::Data7;
        default:
            return QSerialPort::Data8;
    }
}

QString udp_mode_to_string(UdpConfig::Mode mode) {
    return to_string(mode);
}

std::optional<UdpConfig::Mode> udp_mode_from_string(const QString& value) {
    if (value == QLatin1String("unicast")) {
        return UdpConfig::Mode::Unicast;
    }
    if (value == QLatin1String("broadcast")) {
        return UdpConfig::Mode::Broadcast;
    }
    if (value == QLatin1String("multicast")) {
        return UdpConfig::Mode::Multicast;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------------------------
// Seed state

QJsonObject seed_to_json(const core::model::VesselState& seed) {
    const auto& navigation = seed.navigation;
    QJsonObject gnss{
        {QStringLiteral("fix"), seed.gnss.has_fix},
        {QStringLiteral("quality"), fix_quality_to_string(seed.gnss.quality)},
        {QStringLiteral("satellites_in_use"), seed.gnss.satellites_in_use},
        {QStringLiteral("satellites_in_view"), seed.gnss.satellites_in_view},
        {QStringLiteral("hdop"), seed.gnss.hdop},
        {QStringLiteral("pdop"), seed.gnss.pdop},
        {QStringLiteral("vdop"), seed.gnss.vdop},
        {QStringLiteral("geoid_separation_m"), seed.gnss.geoid_separation_m},
    };
    QJsonArray engines;
    for (const auto& engine : seed.engines) {
        engines.append(QJsonObject{
            {QStringLiteral("label"), QString::fromStdString(engine.label)},
            {QStringLiteral("running"), engine.running},
            {QStringLiteral("rpm"), engine.revolutions_rpm},
            {QStringLiteral("coolant_temperature_c"), engine.coolant_temperature_c},
        });
    }
    return {
        {QStringLiteral("position"),
         QJsonObject{{QStringLiteral("latitude"), navigation.position.latitude_deg},
                     {QStringLiteral("longitude"), navigation.position.longitude_deg}}},
        {QStringLiteral("altitude_m"), navigation.altitude_m},
        {QStringLiteral("heading_true_deg"), navigation.heading_true_deg},
        {QStringLiteral("speed_over_ground_kn"), navigation.speed_over_ground_kn},
        {QStringLiteral("magnetic_variation_deg"), navigation.magnetic_variation_deg},
        {QStringLiteral("magnetic_deviation_deg"), navigation.magnetic_deviation_deg},
        {QStringLiteral("rudder_angle_deg"), seed.steering.rudder_angle_deg},
        {QStringLiteral("depth_m"), seed.water.depth_below_transducer_m},
        {QStringLiteral("transducer_offset_m"), seed.water.transducer_offset_m},
        {QStringLiteral("water_temperature_c"), seed.water.temperature_c},
        {QStringLiteral("wind_true_direction_deg"), seed.wind.true_direction_deg},
        {QStringLiteral("wind_true_speed_kn"), seed.wind.true_speed_kn},
        {QStringLiteral("gnss"), gnss},
        {QStringLiteral("engines"), engines},
    };
}

core::model::VesselState seed_from_json(const QJsonObject& object,
                                        const core::model::VesselState& fallback) {
    core::model::VesselState seed = fallback;
    auto& navigation = seed.navigation;
    const auto position = object.value(QStringLiteral("position")).toObject();
    navigation.position.latitude_deg =
        number(position, "latitude", fallback.navigation.position.latitude_deg);
    navigation.position.longitude_deg =
        number(position, "longitude", fallback.navigation.position.longitude_deg);
    navigation.altitude_m = number(object, "altitude_m", fallback.navigation.altitude_m);
    navigation.heading_true_deg =
        number(object, "heading_true_deg", fallback.navigation.heading_true_deg);
    navigation.speed_over_ground_kn =
        number(object, "speed_over_ground_kn", fallback.navigation.speed_over_ground_kn);
    navigation.speed_through_water_kn = navigation.speed_over_ground_kn;
    navigation.course_over_ground_deg = navigation.heading_true_deg;
    navigation.magnetic_variation_deg =
        number(object, "magnetic_variation_deg", fallback.navigation.magnetic_variation_deg);
    navigation.magnetic_deviation_deg =
        number(object, "magnetic_deviation_deg", fallback.navigation.magnetic_deviation_deg);
    seed.steering.rudder_angle_deg =
        number(object, "rudder_angle_deg", fallback.steering.rudder_angle_deg);
    seed.water.depth_below_transducer_m =
        number(object, "depth_m", fallback.water.depth_below_transducer_m);
    seed.water.transducer_offset_m =
        number(object, "transducer_offset_m", fallback.water.transducer_offset_m);
    seed.water.temperature_c = number(object, "water_temperature_c", fallback.water.temperature_c);
    seed.wind.true_direction_deg =
        number(object, "wind_true_direction_deg", fallback.wind.true_direction_deg);
    seed.wind.true_speed_kn = number(object, "wind_true_speed_kn", fallback.wind.true_speed_kn);

    const auto gnss = object.value(QStringLiteral("gnss")).toObject();
    seed.gnss.has_fix = boolean(gnss, "fix", fallback.gnss.has_fix);
    seed.gnss.quality = fix_quality_from_string(
        text(gnss, "quality", fix_quality_to_string(fallback.gnss.quality)));
    seed.gnss.satellites_in_use =
        integer(gnss, "satellites_in_use", fallback.gnss.satellites_in_use);
    seed.gnss.satellites_in_view =
        integer(gnss, "satellites_in_view", fallback.gnss.satellites_in_view);
    seed.gnss.hdop = number(gnss, "hdop", fallback.gnss.hdop);
    seed.gnss.pdop = number(gnss, "pdop", fallback.gnss.pdop);
    seed.gnss.vdop = number(gnss, "vdop", fallback.gnss.vdop);
    seed.gnss.geoid_separation_m =
        number(gnss, "geoid_separation_m", fallback.gnss.geoid_separation_m);

    if (object.contains(QStringLiteral("engines"))) {
        seed.engines.clear();
        const auto engines = object.value(QStringLiteral("engines")).toArray();
        for (const auto& value : engines) {
            const auto engine = value.toObject();
            seed.engines.push_back({text(engine, "label", QStringLiteral("Engine")).toStdString(),
                                    boolean(engine, "running", false), number(engine, "rpm", 0.0),
                                    number(engine, "coolant_temperature_c", 20.0)});
        }
    }
    return seed;
}

// ---------------------------------------------------------------------------------------------
// Outputs

QJsonObject output_to_json(const OutputConfig& output) {
    QJsonObject object{
        {QStringLiteral("type"), to_string(output.type)},
        {QStringLiteral("enabled"), output.enabled},
        {QStringLiteral("encoding"), QStringLiteral("nmea0183")},
        {QStringLiteral("filter"), QJsonArray::fromStringList(output.filter)},
    };
    switch (output.type) {
        case OutputConfig::Type::TcpServer:
        case OutputConfig::Type::WebSocketServer:
            object.insert(QStringLiteral("bind_address"), output.bind_address);
            object.insert(QStringLiteral("port"), output.port);
            break;
        case OutputConfig::Type::TcpClient:
            object.insert(QStringLiteral("host"), output.host);
            object.insert(QStringLiteral("port"), output.port);
            object.insert(QStringLiteral("reconnect_ms"), output.reconnect_ms);
            break;
        case OutputConfig::Type::Udp:
            object.insert(QStringLiteral("mode"), udp_mode_to_string(output.udp.mode));
            object.insert(QStringLiteral("address"), output.udp.address);
            object.insert(QStringLiteral("port"), output.udp.port);
            object.insert(QStringLiteral("interface"), output.udp.interface_name);
            object.insert(QStringLiteral("multicast_ttl"), output.udp.multicast_ttl);
            break;
        case OutputConfig::Type::Serial:
            object.insert(QStringLiteral("port_name"), output.serial.port_name);
            object.insert(QStringLiteral("baud_rate"), output.serial.baud_rate);
            object.insert(QStringLiteral("data_bits"), static_cast<int>(output.serial.data_bits));
            object.insert(QStringLiteral("parity"), parity_to_string(output.serial.parity));
            object.insert(QStringLiteral("stop_bits"),
                          stop_bits_to_string(output.serial.stop_bits));
            object.insert(QStringLiteral("flow_control"),
                          flow_control_to_string(output.serial.flow_control));
            break;
        case OutputConfig::Type::File:
            object.insert(QStringLiteral("path"), output.path);
            object.insert(QStringLiteral("append"), output.append);
            break;
        case OutputConfig::Type::Stdout:
            break;
    }
    return object;
}

std::optional<OutputConfig> output_from_json(const QJsonObject& object, int index, QString* error) {
    OutputConfig output;
    const auto type = output_type_from_string(text(object, "type"));
    if (!type) {
        *error =
            QStringLiteral("outputs[%1]: unknown type '%2'").arg(index).arg(text(object, "type"));
        return std::nullopt;
    }
    output.type = *type;
    output.enabled = boolean(object, "enabled", true);
    const auto encoding = text(object, "encoding", QStringLiteral("nmea0183"));
    if (encoding != QLatin1String("nmea0183")) {
        *error = QStringLiteral("outputs[%1]: unsupported encoding '%2'").arg(index).arg(encoding);
        return std::nullopt;
    }
    const auto filter = object.value(QStringLiteral("filter")).toArray();
    for (const auto& value : filter) {
        output.filter.append(value.toString());
    }
    const int port = integer(object, "port", 10110);
    if (port < 0 || port > 65535) {
        *error = QStringLiteral("outputs[%1]: port %2 is out of range").arg(index).arg(port);
        return std::nullopt;
    }
    output.port = static_cast<quint16>(port);
    output.bind_address = text(object, "bind_address", QStringLiteral("0.0.0.0"));
    output.host = text(object, "host", QStringLiteral("127.0.0.1"));
    output.reconnect_ms = integer(object, "reconnect_ms", 2000);

    const auto mode = udp_mode_from_string(text(object, "mode", QStringLiteral("unicast")));
    if (!mode) {
        *error = QStringLiteral("outputs[%1]: unknown UDP mode '%2'")
                     .arg(index)
                     .arg(text(object, "mode"));
        return std::nullopt;
    }
    output.udp.mode = *mode;
    output.udp.address = text(object, "address", QStringLiteral("127.0.0.1"));
    output.udp.port = output.port;
    output.udp.interface_name = text(object, "interface");
    output.udp.multicast_ttl = integer(object, "multicast_ttl", 1);

    output.serial.port_name = text(object, "port_name");
    output.serial.baud_rate = integer(object, "baud_rate", 4800);
    output.serial.data_bits = data_bits_from_int(integer(object, "data_bits", 8));
    output.serial.parity = parity_from_string(text(object, "parity", QStringLiteral("none")));
    output.serial.stop_bits = stop_bits_from_string(text(object, "stop_bits", QStringLiteral("1")));
    output.serial.flow_control =
        flow_control_from_string(text(object, "flow_control", QStringLiteral("none")));
    if (output.type == OutputConfig::Type::Serial && output.serial.port_name.isEmpty()) {
        *error = QStringLiteral("outputs[%1]: serial output needs a port_name").arg(index);
        return std::nullopt;
    }

    output.path = text(object, "path");
    output.append = boolean(object, "append", true);
    if (output.type == OutputConfig::Type::File && output.path.isEmpty()) {
        *error = QStringLiteral("outputs[%1]: file output needs a path").arg(index);
        return std::nullopt;
    }
    return output;
}

// ---------------------------------------------------------------------------------------------
// Migrations. Each function upgrades a document by exactly one schema version.

QJsonObject migrate(QJsonObject document, int from_version) {
    // Version 1 is the first schema; migrations are added here as `if (from_version < N)`.
    Q_UNUSED(from_version);
    document.insert(QStringLiteral("schema_version"), Profile::kCurrentSchemaVersion);
    return document;
}

}  // namespace

QString to_string(OutputConfig::Type type) {
    switch (type) {
        case OutputConfig::Type::TcpServer:
            return QStringLiteral("tcp-server");
        case OutputConfig::Type::TcpClient:
            return QStringLiteral("tcp-client");
        case OutputConfig::Type::Udp:
            return QStringLiteral("udp");
        case OutputConfig::Type::WebSocketServer:
            return QStringLiteral("websocket-server");
        case OutputConfig::Type::Serial:
            return QStringLiteral("serial");
        case OutputConfig::Type::File:
            return QStringLiteral("file");
        case OutputConfig::Type::Stdout:
            return QStringLiteral("stdout");
    }
    return QStringLiteral("unknown");
}

std::optional<OutputConfig::Type> output_type_from_string(const QString& value) {
    for (const auto type :
         {OutputConfig::Type::TcpServer, OutputConfig::Type::TcpClient, OutputConfig::Type::Udp,
          OutputConfig::Type::WebSocketServer, OutputConfig::Type::Serial, OutputConfig::Type::File,
          OutputConfig::Type::Stdout}) {
        if (to_string(type) == value) {
            return type;
        }
    }
    return std::nullopt;
}

Profile Profile::default_profile() {
    Profile profile;
    auto& seed = profile.delta.seed;
    seed.navigation.position = {37.9838, 23.7275};
    seed.navigation.heading_true_deg = 45.0;
    seed.navigation.course_over_ground_deg = 45.0;
    seed.navigation.speed_over_ground_kn = 6.5;
    seed.navigation.speed_through_water_kn = 6.5;
    seed.navigation.magnetic_variation_deg = 4.6;
    seed.water.depth_below_transducer_m = 12.4;
    seed.water.transducer_offset_m = 0.5;
    seed.water.temperature_c = 21.5;
    seed.wind.true_direction_deg = 270.0;
    seed.wind.true_speed_kn = 12.0;
    seed.engines = {{"Port engine", true, 1800.0, 82.0}, {"Starboard engine", true, 1800.0, 83.0}};

    OutputConfig tcp;
    tcp.type = OutputConfig::Type::TcpServer;
    tcp.port = 10110;
    profile.outputs.append(tcp);
    return profile;
}

QJsonObject Profile::to_json() const {
    QJsonObject sentence_settings;
    for (const auto& [id, setting] : sentences) {
        sentence_settings.insert(
            QString::fromStdString(id),
            QJsonObject{
                {QStringLiteral("enabled"), setting.enabled},
                {QStringLiteral("talker"), QString::fromStdString(setting.talker)},
                {QStringLiteral("period_ms"), static_cast<qint64>(setting.period.count())}});
    }
    QJsonArray output_array;
    for (const auto& output : outputs) {
        output_array.append(output_to_json(output));
    }
    return {
        {QStringLiteral("schema_version"), kCurrentSchemaVersion},
        {QStringLiteral("name"), name},
        {QStringLiteral("simulation"),
         QJsonObject{
             {QStringLiteral("mode"), QStringLiteral("delta")},
             {QStringLiteral("tick_ms"), tick_ms},
             {QStringLiteral("start_time"),
              start_time ? start_time->toUTC().toString(Qt::ISODateWithMs) : QStringLiteral("now")},
             {QStringLiteral("random_seed"), static_cast<qint64>(delta.random_seed)},
             {QStringLiteral("seed"), seed_to_json(delta.seed)},
             {QStringLiteral("variation"),
              QJsonObject{
                  {QStringLiteral("heading"), variation_to_json(delta.heading)},
                  {QStringLiteral("speed"), variation_to_json(delta.speed)},
                  {QStringLiteral("depth"), variation_to_json(delta.depth)},
                  {QStringLiteral("water_temperature"), variation_to_json(delta.water_temperature)},
                  {QStringLiteral("wind_direction"), variation_to_json(delta.wind_direction)},
                  {QStringLiteral("wind_speed"), variation_to_json(delta.wind_speed)},
              }},
             {QStringLiteral("steering"),
              QJsonObject{
                  {QStringLiteral("turn_rate_per_rudder_deg"), delta.turn_rate_per_rudder_deg},
                  {QStringLiteral("max_rudder_angle_deg"), delta.max_rudder_angle_deg}}},
         }},
        {QStringLiteral("sentences"),
         QJsonObject{{QStringLiteral("position_decimals"), encoder.position_decimals},
                     {QStringLiteral("settings"), sentence_settings}}},
        {QStringLiteral("outputs"), output_array},
    };
}

std::optional<Profile> Profile::from_json(const QJsonObject& input, QString* error) {
    QString local_error;
    QString* err = error ? error : &local_error;

    const int version = integer(input, "schema_version", 0);
    if (version < 1) {
        *err = QStringLiteral("Missing or invalid schema_version");
        return std::nullopt;
    }
    if (version > kCurrentSchemaVersion) {
        *err = QStringLiteral("Profile schema version %1 is newer than the supported version %2")
                   .arg(version)
                   .arg(kCurrentSchemaVersion);
        return std::nullopt;
    }
    const QJsonObject json = version < kCurrentSchemaVersion ? migrate(input, version) : input;

    Profile profile = default_profile();
    profile.outputs.clear();
    profile.name = text(json, "name", profile.name);

    const auto simulation = json.value(QStringLiteral("simulation")).toObject();
    const auto mode = text(simulation, "mode", QStringLiteral("delta"));
    if (mode != QLatin1String("delta")) {
        *err = QStringLiteral("Unsupported simulation mode '%1'").arg(mode);
        return std::nullopt;
    }
    profile.tick_ms = integer(simulation, "tick_ms", profile.tick_ms);
    if (profile.tick_ms < 10 || profile.tick_ms > 10'000) {
        *err = QStringLiteral("tick_ms must be between 10 and 10000");
        return std::nullopt;
    }
    const auto start = text(simulation, "start_time", QStringLiteral("now"));
    if (start != QLatin1String("now") && !start.isEmpty()) {
        const auto parsed = QDateTime::fromString(start, Qt::ISODateWithMs);
        if (!parsed.isValid()) {
            *err = QStringLiteral("start_time '%1' is not an ISO 8601 date-time").arg(start);
            return std::nullopt;
        }
        profile.start_time = parsed.toUTC();
    }
    const auto random_seed = simulation.value(QStringLiteral("random_seed")).toDouble(-1.0);
    if (random_seed >= 0.0) {
        profile.delta.random_seed = static_cast<unsigned int>(random_seed);
    }
    profile.delta.seed =
        seed_from_json(simulation.value(QStringLiteral("seed")).toObject(), profile.delta.seed);
    const auto variation = simulation.value(QStringLiteral("variation")).toObject();
    profile.delta.heading =
        variation_from_json(variation.value(QStringLiteral("heading")), profile.delta.heading);
    profile.delta.speed =
        variation_from_json(variation.value(QStringLiteral("speed")), profile.delta.speed);
    profile.delta.depth =
        variation_from_json(variation.value(QStringLiteral("depth")), profile.delta.depth);
    profile.delta.water_temperature = variation_from_json(
        variation.value(QStringLiteral("water_temperature")), profile.delta.water_temperature);
    profile.delta.wind_direction = variation_from_json(
        variation.value(QStringLiteral("wind_direction")), profile.delta.wind_direction);
    profile.delta.wind_speed = variation_from_json(variation.value(QStringLiteral("wind_speed")),
                                                   profile.delta.wind_speed);
    const auto steering = simulation.value(QStringLiteral("steering")).toObject();
    profile.delta.turn_rate_per_rudder_deg =
        number(steering, "turn_rate_per_rudder_deg", profile.delta.turn_rate_per_rudder_deg);
    profile.delta.max_rudder_angle_deg =
        number(steering, "max_rudder_angle_deg", profile.delta.max_rudder_angle_deg);

    const auto sentences = json.value(QStringLiteral("sentences")).toObject();
    profile.encoder.position_decimals = integer(sentences, "position_decimals", 4);
    if (profile.encoder.position_decimals < 2 || profile.encoder.position_decimals > 8) {
        *err = QStringLiteral("position_decimals must be between 2 and 8");
        return std::nullopt;
    }
    const auto settings = sentences.value(QStringLiteral("settings")).toObject();
    const auto& registry = core::nmea0183::SentenceRegistry::standard();
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        const auto id = it.key().toStdString();
        const auto* descriptor = registry.find(id);
        if (descriptor == nullptr) {
            *err = QStringLiteral("Unknown sentence id '%1' in sentences.settings").arg(it.key());
            return std::nullopt;
        }
        const auto object = it.value().toObject();
        core::simulation::SentenceSetting setting;
        setting.enabled = boolean(object, "enabled", descriptor->enabled_by_default);
        setting.talker = text(object, "talker").toStdString();
        setting.period = std::chrono::milliseconds{
            integer(object, "period_ms", static_cast<int>(descriptor->default_period.count()))};
        profile.sentences[id] = setting;
    }

    const auto outputs = json.value(QStringLiteral("outputs")).toArray();
    for (qsizetype i = 0; i < outputs.size(); ++i) {
        auto output = output_from_json(outputs.at(i).toObject(), static_cast<int>(i), err);
        if (!output) {
            return std::nullopt;
        }
        profile.outputs.append(*output);
    }
    return profile;
}

std::optional<Profile> Profile::load(const QString& path, QString* error) {
    QString local_error;
    QString* err = error ? error : &local_error;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *err = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    QJsonParseError parse_error{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (document.isNull() || !document.isObject()) {
        *err = QStringLiteral("%1 is not a JSON object: %2").arg(path, parse_error.errorString());
        return std::nullopt;
    }
    return from_json(document.object(), err);
}

bool Profile::save(const QString& path, QString* error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(to_json()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

core::simulation::SentenceScheduler Profile::make_scheduler() const {
    core::simulation::SentenceScheduler scheduler;
    scheduler.set_encoder_options(encoder);
    for (const auto& [id, setting] : sentences) {
        scheduler.configure(id, setting);
    }
    return scheduler;
}

}  // namespace nmeasim::io
