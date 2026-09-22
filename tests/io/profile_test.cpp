#include <nmeasim/io/profile/profile.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimeZone>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

using Catch::Approx;
using namespace std::chrono_literals;
using nmeasim::io::OutputConfig;
using nmeasim::io::Profile;

TEST_CASE("the default profile is valid and round-trips through JSON", "[io][profile]") {
    const auto original = Profile::default_profile();
    QString error;
    const auto parsed = Profile::from_json(original.to_json(), &error);
    REQUIRE(parsed.has_value());
    CHECK(error.isEmpty());
    CHECK(parsed->name == original.name);
    CHECK(parsed->tick_ms == original.tick_ms);
    CHECK_FALSE(parsed->start_time.has_value());
    CHECK(parsed->delta.seed.navigation.position.latitude_deg ==
          Approx(original.delta.seed.navigation.position.latitude_deg));
    CHECK(parsed->delta.seed.engines.size() == 2);
    CHECK(parsed->delta.seed.engines[1].label == "Starboard engine");
    CHECK(parsed->delta.heading.amplitude == Approx(original.delta.heading.amplitude));
    CHECK(parsed->delta.random_seed == original.delta.random_seed);
    REQUIRE(parsed->outputs.size() == 1);
    CHECK(parsed->outputs.first().type == OutputConfig::Type::TcpServer);
    CHECK(parsed->outputs.first().port == 10110);
    CHECK(parsed->to_json() == original.to_json());
}

TEST_CASE("sentence settings, start time and every output type round-trip", "[io][profile]") {
    Profile profile = Profile::default_profile();
    profile.name = QStringLiteral("Everything");
    profile.start_time = QDateTime(QDate(2026, 9, 22), QTime(12, 34, 56, 780), QTimeZone::utc());
    profile.encoder.position_decimals = 5;
    profile.sentences["RMC"] = {true, "GN", 500ms};
    profile.sentences["GSV"] = {false, "", 1000ms};
    profile.outputs.clear();

    OutputConfig tcp_client;
    tcp_client.type = OutputConfig::Type::TcpClient;
    tcp_client.host = QStringLiteral("192.168.1.10");
    tcp_client.port = 2000;
    tcp_client.reconnect_ms = 500;
    tcp_client.filter = {QStringLiteral("RMC"), QStringLiteral("GGA")};
    profile.outputs.append(tcp_client);

    OutputConfig udp;
    udp.type = OutputConfig::Type::Udp;
    udp.udp.mode = nmeasim::io::UdpConfig::Mode::Multicast;
    udp.udp.address = QStringLiteral("239.2.1.1");
    udp.udp.port = 10110;
    udp.udp.interface_name = QStringLiteral("eth0");
    udp.udp.multicast_ttl = 4;
    profile.outputs.append(udp);

    OutputConfig websocket;
    websocket.type = OutputConfig::Type::WebSocketServer;
    websocket.port = 3000;
    websocket.bind_address = QStringLiteral("127.0.0.1");
    profile.outputs.append(websocket);

    OutputConfig serial;
    serial.type = OutputConfig::Type::Serial;
    serial.serial.port_name = QStringLiteral("COM3");
    serial.serial.baud_rate = 38400;
    serial.serial.data_bits = QSerialPort::Data7;
    serial.serial.parity = QSerialPort::EvenParity;
    serial.serial.stop_bits = QSerialPort::TwoStop;
    serial.serial.flow_control = QSerialPort::HardwareControl;
    serial.enabled = false;
    profile.outputs.append(serial);

    OutputConfig file;
    file.type = OutputConfig::Type::File;
    file.path = QStringLiteral("out.log");
    file.append = false;
    profile.outputs.append(file);

    OutputConfig standard_output;
    standard_output.type = OutputConfig::Type::Stdout;
    profile.outputs.append(standard_output);

    QString error;
    const auto parsed = Profile::from_json(profile.to_json(), &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->start_time == profile.start_time);
    CHECK(parsed->encoder.position_decimals == 5);
    REQUIRE(parsed->sentences.count("RMC") == 1);
    CHECK(parsed->sentences.at("RMC").talker == "GN");
    CHECK(parsed->sentences.at("RMC").period == 500ms);
    CHECK_FALSE(parsed->sentences.at("GSV").enabled);
    REQUIRE(parsed->outputs.size() == 6);
    CHECK(parsed->outputs[0].host == QStringLiteral("192.168.1.10"));
    CHECK(parsed->outputs[0].reconnect_ms == 500);
    CHECK(parsed->outputs[0].filter == QStringList{QStringLiteral("RMC"), QStringLiteral("GGA")});
    CHECK(parsed->outputs[1].udp.mode == nmeasim::io::UdpConfig::Mode::Multicast);
    CHECK(parsed->outputs[1].udp.interface_name == QStringLiteral("eth0"));
    CHECK(parsed->outputs[1].udp.multicast_ttl == 4);
    CHECK(parsed->outputs[2].bind_address == QStringLiteral("127.0.0.1"));
    CHECK(parsed->outputs[3].serial.baud_rate == 38400);
    CHECK(parsed->outputs[3].serial.data_bits == QSerialPort::Data7);
    CHECK(parsed->outputs[3].serial.parity == QSerialPort::EvenParity);
    CHECK(parsed->outputs[3].serial.stop_bits == QSerialPort::TwoStop);
    CHECK(parsed->outputs[3].serial.flow_control == QSerialPort::HardwareControl);
    CHECK_FALSE(parsed->outputs[3].enabled);
    CHECK(parsed->outputs[4].path == QStringLiteral("out.log"));
    CHECK_FALSE(parsed->outputs[4].append);
    CHECK(parsed->outputs[5].type == OutputConfig::Type::Stdout);
    CHECK(parsed->to_json() == profile.to_json());

    const auto scheduler = parsed->make_scheduler();
    CHECK(scheduler.setting("RMC").talker == "GN");
    CHECK_FALSE(scheduler.setting("GSV").enabled);
    CHECK(scheduler.encoder_options().position_decimals == 5);
}

TEST_CASE("missing fields fall back to defaults", "[io][profile]") {
    QString error;
    const auto parsed =
        Profile::from_json(QJsonObject{{QStringLiteral("schema_version"), 1}}, &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->name == QStringLiteral("Default"));
    CHECK(parsed->tick_ms == 100);
    CHECK(parsed->outputs.isEmpty());
    CHECK(parsed->delta.seed.navigation.heading_true_deg == Approx(45.0));
}

TEST_CASE("invalid profiles are rejected with a reason", "[io][profile]") {
    QString error;
    CHECK_FALSE(Profile::from_json(QJsonObject{}, &error).has_value());
    CHECK(error.contains(QStringLiteral("schema_version")));

    CHECK_FALSE(Profile::from_json(QJsonObject{{QStringLiteral("schema_version"), 99}}, &error)
                    .has_value());
    CHECK(error.contains(QStringLiteral("newer")));

    QJsonObject bad_mode{{QStringLiteral("schema_version"), 1},
                         {QStringLiteral("simulation"),
                          QJsonObject{{QStringLiteral("mode"), QStringLiteral("warp")}}}};
    CHECK_FALSE(Profile::from_json(bad_mode, &error).has_value());
    CHECK(error.contains(QStringLiteral("mode")));

    QJsonObject bad_tick{
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("simulation"), QJsonObject{{QStringLiteral("tick_ms"), 1}}}};
    CHECK_FALSE(Profile::from_json(bad_tick, &error).has_value());
    CHECK(error.contains(QStringLiteral("tick_ms")));

    QJsonObject bad_time{
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("simulation"),
         QJsonObject{{QStringLiteral("start_time"), QStringLiteral("yesterday")}}}};
    CHECK_FALSE(Profile::from_json(bad_time, &error).has_value());
    CHECK(error.contains(QStringLiteral("start_time")));

    QJsonObject bad_sentence{{QStringLiteral("schema_version"), 1},
                             {QStringLiteral("sentences"),
                              QJsonObject{{QStringLiteral("settings"),
                                           QJsonObject{{QStringLiteral("XYZ"), QJsonObject{}}}}}}};
    CHECK_FALSE(Profile::from_json(bad_sentence, &error).has_value());
    CHECK(error.contains(QStringLiteral("XYZ")));

    QJsonObject bad_output{
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("outputs"),
         QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("carrier-pigeon")}}}}};
    CHECK_FALSE(Profile::from_json(bad_output, &error).has_value());
    CHECK(error.contains(QStringLiteral("outputs[0]")));

    QJsonObject bad_port{
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("outputs"),
         QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("tcp-server")},
                                {QStringLiteral("port"), 70000}}}}};
    CHECK_FALSE(Profile::from_json(bad_port, &error).has_value());
    CHECK(error.contains(QStringLiteral("port")));

    QJsonObject serial_without_port{
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("outputs"),
         QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("serial")}}}}};
    CHECK_FALSE(Profile::from_json(serial_without_port, &error).has_value());
    CHECK(error.contains(QStringLiteral("port_name")));
}

TEST_CASE("profiles are saved to and loaded from disk", "[io][profile]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("profile.json"));

    auto profile = Profile::default_profile();
    profile.name = QStringLiteral("Saved");
    QString error;
    REQUIRE(profile.save(path, &error));

    const auto loaded = Profile::load(path, &error);
    REQUIRE(loaded.has_value());
    CHECK(loaded->name == QStringLiteral("Saved"));

    CHECK_FALSE(
        Profile::load(directory.filePath(QStringLiteral("missing.json")), &error).has_value());
    CHECK(error.contains(QStringLiteral("Cannot read")));

    QFile garbage(path);
    REQUIRE(garbage.open(QIODevice::WriteOnly | QIODevice::Truncate));
    garbage.write("not json");
    garbage.close();
    CHECK_FALSE(Profile::load(path, &error).has_value());
    CHECK(error.contains(QStringLiteral("not a JSON object")));
}

TEST_CASE("output types have stable names", "[io][profile]") {
    for (const auto type :
         {OutputConfig::Type::TcpServer, OutputConfig::Type::TcpClient, OutputConfig::Type::Udp,
          OutputConfig::Type::WebSocketServer, OutputConfig::Type::Serial, OutputConfig::Type::File,
          OutputConfig::Type::Stdout}) {
        CHECK(nmeasim::io::output_type_from_string(nmeasim::io::to_string(type)) == type);
    }
    CHECK_FALSE(nmeasim::io::output_type_from_string(QStringLiteral("smoke-signals")).has_value());
}
