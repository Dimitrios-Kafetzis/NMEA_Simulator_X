// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::io::Profile`: its JSON form, validation, schema migration and files.
///
/// Covers `Profile::default_profile`, the round trip through `Profile::to_json` and
/// `Profile::from_json` for every setting and output type, the defaults that fill in missing
/// keys, the rejection of invalid documents with a reason that names the offending key, the
/// migration of schema versions 1 and 2 to the current version 3, `Profile::save` and
/// `Profile::load` with relative track and log paths, `Profile::make_scheduler`, and the
/// string names of output types, simulation modes and encodings.
///
/// The profiles are built in code or saved to temporary directories; the file reads no
/// fixture. The track and log paths it sets are never opened.

#include <nmeasim/io/profile/profile.hpp>

#include <QDir>
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
using nmeasim::io::SimulationMode;

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
    // `Profile::default_profile` seeds two engines, port and starboard, and has one output: a
    // TCP server on port 10110, the port registered with IANA for NMEA 0183.
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
    // The name and tick come from the `Profile` member defaults, the seed from
    // `Profile::default_profile`; its TCP server output is not added to a parsed profile.
    CHECK(parsed->name == QStringLiteral("Default"));
    CHECK(parsed->tick_ms == 100);
    CHECK(parsed->outputs.isEmpty());
    CHECK(parsed->delta.seed.navigation.heading_true_deg == Approx(45.0));
}

TEST_CASE("invalid profiles are rejected with a reason", "[io][profile]") {
    QString error;
    CHECK_FALSE(Profile::from_json(QJsonObject{}, &error).has_value());
    CHECK(error.contains(QStringLiteral("schema_version")));

    // Version 99 is newer than `Profile::kCurrentSchemaVersion`.
    CHECK_FALSE(Profile::from_json(QJsonObject{{QStringLiteral("schema_version"), 99}}, &error)
                    .has_value());
    CHECK(error.contains(QStringLiteral("newer")));

    QJsonObject bad_mode{{QStringLiteral("schema_version"), 1},
                         {QStringLiteral("simulation"),
                          QJsonObject{{QStringLiteral("mode"), QStringLiteral("warp")}}}};
    CHECK_FALSE(Profile::from_json(bad_mode, &error).has_value());
    CHECK(error.contains(QStringLiteral("mode")));

    // `tick_ms` must be in [10, 10000].
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

    // 70000 does not fit the 16-bit port range [0, 65535].
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

TEST_CASE("a destination needs a position and unknown GNSS qualities are rejected",
          "[io][profile]") {
    QString error;
    const auto seed = [](const QJsonObject& keys) {
        return QJsonObject{
            {QStringLiteral("schema_version"), 3},
            {QStringLiteral("simulation"), QJsonObject{{QStringLiteral("seed"), keys}}}};
    };
    const auto destination = [&seed](const QJsonObject& keys) {
        return seed({{QStringLiteral("destination"), keys}});
    };

    CHECK_FALSE(
        Profile::from_json(destination({{QStringLiteral("name"), QStringLiteral("X")}}), &error)
            .has_value());
    CHECK(error.contains(QStringLiteral("destination needs a numeric latitude")));
    CHECK_FALSE(
        Profile::from_json(destination({{QStringLiteral("latitude"), 37.7}}), &error).has_value());
    CHECK(error.contains(QStringLiteral("destination needs a numeric longitude")));
    // A position given as text is not a position.
    CHECK_FALSE(Profile::from_json(destination({{QStringLiteral("latitude"), QStringLiteral("37")},
                                                {QStringLiteral("longitude"), 23.4}}),
                                   &error)
                    .has_value());
    CHECK(error.contains(QStringLiteral("destination needs a numeric latitude")));
    const auto complete = Profile::from_json(destination({{QStringLiteral("latitude"), 37.7466},
                                                          {QStringLiteral("longitude"), 23.4275}}),
                                             &error);
    REQUIRE(complete.has_value());
    REQUIRE(complete->delta.seed.destination.has_value());
    CHECK(complete->delta.seed.destination->name == "WPT");
    // `null` still means no destination.
    const auto none =
        Profile::from_json(seed({{QStringLiteral("destination"), QJsonValue::Null}}), &error);
    REQUIRE(none.has_value());
    CHECK_FALSE(none->delta.seed.destination.has_value());

    const auto quality = [&seed](const QString& value) {
        return seed({{QStringLiteral("gnss"), QJsonObject{{QStringLiteral("quality"), value}}}});
    };
    CHECK_FALSE(Profile::from_json(quality(QStringLiteral("rtk")), &error).has_value());
    CHECK(error.contains(QStringLiteral("gnss.quality")));
    CHECK(error.contains(QStringLiteral("rtk")));
    const auto differential = Profile::from_json(quality(QStringLiteral("differential")), &error);
    REQUIRE(differential.has_value());
    CHECK(differential->delta.seed.gnss.quality == nmeasim::core::model::FixQuality::Differential);
}

TEST_CASE("sentence settings are validated", "[io][profile]") {
    QString error;
    const auto setting = [](const QJsonObject& keys) {
        return QJsonObject{{QStringLiteral("schema_version"), 3},
                           {QStringLiteral("sentences"),
                            QJsonObject{{QStringLiteral("settings"),
                                         QJsonObject{{QStringLiteral("RMC"), keys}}}}}};
    };
    // Periods lie in [50, 3600000] ms, as for custom sentences and outputs.
    for (const int period : {0, 49, 3'600'001}) {
        INFO(period);
        CHECK_FALSE(Profile::from_json(setting({{QStringLiteral("period_ms"), period}}), &error)
                        .has_value());
        CHECK(error.contains(QStringLiteral("RMC")));
        CHECK(error.contains(QStringLiteral("period_ms")));
    }
    // A talker is two upper-case letters, or empty for the registry default.
    for (const char* talker : {"G", "GPS", "gn", "G1", "$G"}) {
        INFO(talker);
        CHECK_FALSE(
            Profile::from_json(setting({{QStringLiteral("talker"), QLatin1String(talker)}}), &error)
                .has_value());
        CHECK(error.contains(QStringLiteral("talker")));
    }
    const auto valid = Profile::from_json(setting({{QStringLiteral("talker"), QStringLiteral("GN")},
                                                   {QStringLiteral("period_ms"), 50}}),
                                          &error);
    REQUIRE(valid.has_value());
    CHECK(valid->sentences.at("RMC").talker == "GN");
    CHECK(valid->sentences.at("RMC").period == 50ms);
    const auto empty = Profile::from_json(setting({{QStringLiteral("talker"), QString()}}), &error);
    REQUIRE(empty.has_value());
    CHECK(empty->sentences.at("RMC").talker.empty());
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
          OutputConfig::Type::Stdout, OutputConfig::Type::Log}) {
        CHECK(nmeasim::io::output_type_from_string(nmeasim::io::to_string(type)) == type);
    }
    CHECK_FALSE(nmeasim::io::output_type_from_string(QStringLiteral("smoke-signals")).has_value());
    for (const auto mode : {SimulationMode::Delta, SimulationMode::Track, SimulationMode::Replay}) {
        CHECK(nmeasim::io::simulation_mode_from_string(nmeasim::io::to_string(mode)) == mode);
    }
    CHECK_FALSE(nmeasim::io::simulation_mode_from_string(QStringLiteral("warp")).has_value());
}

TEST_CASE("track and replay modes round-trip with their settings", "[io][profile]") {
    Profile profile = Profile::default_profile();
    profile.mode = SimulationMode::Track;
    profile.track.path = QStringLiteral("/tracks/harbour.gpx");
    profile.track.speed_kn = 4.5;
    profile.track.use_timestamps = false;
    profile.track.loop = true;
    profile.replay.path = QStringLiteral("/logs/yesterday.log");
    profile.replay.loop = true;
    profile.replay.fixed_interval_ms = 250;
    OutputConfig log;
    log.type = OutputConfig::Type::Log;
    log.path = QStringLiteral("record.log");
    log.append = false;
    profile.outputs.append(log);

    const auto json = profile.to_json();
    // 3 is `Profile::kCurrentSchemaVersion`.
    CHECK(json.value(QStringLiteral("schema_version")).toInt() == 3);
    CHECK(json.value(QStringLiteral("simulation")).toObject().value(QStringLiteral("mode")) ==
          QStringLiteral("track"));
    QString error;
    const auto parsed = Profile::from_json(json, &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->mode == SimulationMode::Track);
    CHECK(parsed->track.path == QStringLiteral("/tracks/harbour.gpx"));
    CHECK(parsed->track.speed_kn == Approx(4.5));
    CHECK_FALSE(parsed->track.use_timestamps);
    CHECK(parsed->track.loop);
    CHECK(parsed->replay.path == QStringLiteral("/logs/yesterday.log"));
    CHECK(parsed->replay.loop);
    CHECK(parsed->replay.fixed_interval_ms == 250);
    REQUIRE(parsed->outputs.size() == 2);
    CHECK(parsed->outputs[1].type == OutputConfig::Type::Log);
    CHECK(parsed->outputs[1].path == QStringLiteral("record.log"));
    CHECK_FALSE(parsed->outputs[1].append);
    CHECK(parsed->to_json() == json);

    profile.mode = SimulationMode::Replay;
    const auto replay = Profile::from_json(profile.to_json(), &error);
    REQUIRE(replay.has_value());
    CHECK(replay->mode == SimulationMode::Replay);
}

TEST_CASE("schema version 1 profiles are migrated to the current version", "[io][profile]") {
    QJsonObject v1{{QStringLiteral("schema_version"), 1},
                   {QStringLiteral("name"), QStringLiteral("Old")},
                   {QStringLiteral("simulation"), QJsonObject{{QStringLiteral("tick_ms"), 200}}}};
    QString error;
    const auto parsed = Profile::from_json(v1, &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->name == QStringLiteral("Old"));
    CHECK(parsed->tick_ms == 200);
    CHECK(parsed->mode == SimulationMode::Delta);
    // Keys that version 1 did not have take the member defaults of the track and replay
    // settings.
    CHECK(parsed->track.speed_kn == Approx(6.0));
    CHECK(parsed->track.use_timestamps);
    CHECK(parsed->replay.fixed_interval_ms == 100);
    CHECK(parsed->to_json().value(QStringLiteral("schema_version")).toInt() == 3);
}

TEST_CASE("mode settings are validated", "[io][profile]") {
    QString error;
    QJsonObject track_without_path{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("simulation"),
         QJsonObject{{QStringLiteral("mode"), QStringLiteral("track")}}}};
    CHECK_FALSE(Profile::from_json(track_without_path, &error).has_value());
    CHECK(error.contains(QStringLiteral("track.path")));

    QJsonObject replay_without_path{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("simulation"),
         QJsonObject{{QStringLiteral("mode"), QStringLiteral("replay")}}}};
    CHECK_FALSE(Profile::from_json(replay_without_path, &error).has_value());
    CHECK(error.contains(QStringLiteral("replay.path")));

    QJsonObject bad_speed{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("simulation"),
         QJsonObject{{QStringLiteral("track"), QJsonObject{{QStringLiteral("speed_kn"), 0.0}}}}}};
    CHECK_FALSE(Profile::from_json(bad_speed, &error).has_value());
    CHECK(error.contains(QStringLiteral("speed_kn")));

    QJsonObject bad_interval{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("simulation"),
         QJsonObject{
             {QStringLiteral("replay"), QJsonObject{{QStringLiteral("fixed_interval_ms"), 0}}}}}};
    CHECK_FALSE(Profile::from_json(bad_interval, &error).has_value());
    CHECK(error.contains(QStringLiteral("fixed_interval_ms")));

    QJsonObject log_without_path{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("outputs"),
         QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("log")}}}}};
    CHECK_FALSE(Profile::from_json(log_without_path, &error).has_value());
    CHECK(error.contains(QStringLiteral("log output needs a path")));
}

TEST_CASE("relative track and log paths are resolved against the profile file", "[io][profile]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    REQUIRE(QDir(directory.path()).mkpath(QStringLiteral("profiles")));
    const QString path = directory.filePath(QStringLiteral("profiles/track.json"));
    Profile profile = Profile::default_profile();
    profile.mode = SimulationMode::Track;
    profile.track.path = QStringLiteral("../tracks/harbour.gpx");
    profile.replay.path = QStringLiteral("logs/yesterday.log");
    QString error;
    REQUIRE(profile.save(path, &error));

    const auto loaded = Profile::load(path, &error);
    REQUIRE(loaded.has_value());
    CHECK(loaded->track.path ==
          QDir::cleanPath(directory.filePath(QStringLiteral("tracks/harbour.gpx"))));
    CHECK(loaded->replay.path ==
          QDir::cleanPath(directory.filePath(QStringLiteral("profiles/logs/yesterday.log"))));
    // Absolute paths are left alone.
    profile.track.path = QDir::cleanPath(directory.filePath(QStringLiteral("abs.gpx")));
    REQUIRE(profile.save(path, &error));
    CHECK(Profile::load(path, &error)->track.path == profile.track.path);
}

TEST_CASE("destination, AIS data, custom sentences and encodings round-trip", "[io][profile]") {
    Profile profile = Profile::default_profile();
    profile.delta.seed.destination =
        nmeasim::core::model::Destination{"AEGINA", {37.7466, 23.4275}, {38.0, 23.7}, 250.0};
    profile.delta.seed.ais.mmsi = 211000123;
    profile.delta.seed.ais.name = "TEST VESSEL";
    profile.delta.seed.ais.call_sign = "DA1234";
    profile.delta.seed.ais.ship_type = 70;
    profile.delta.seed.ais.dimension_to_bow_m = 40.0;
    profile.delta.seed.ais.draught_m = 4.5;
    profile.delta.seed.ais.destination = "PIRAEUS";
    profile.delta.seed.ais.navigation_status = 8;
    profile.delta.seed.ais.position_report_type = 3;
    profile.custom_sentences = {{"BARO", "$IIXDR,P,1.013,B,BARO", 5000ms, true},
                                {"", "PXYZ,1,2,3", 1000ms, false}};
    profile.outputs.clear();
    OutputConfig tagged;
    tagged.type = OutputConfig::Type::TcpServer;
    tagged.tag_block.enabled = true;
    tagged.tag_block.options.source = "GP0001";
    tagged.tag_block.options.milliseconds = true;
    profile.outputs.append(tagged);
    OutputConfig signalk;
    signalk.type = OutputConfig::Type::WebSocketServer;
    signalk.port = 3000;
    signalk.encoding = OutputConfig::Encoding::SignalK;
    signalk.period_ms = 500;
    signalk.signalk.context = "aircraft.urn:mrn:signalk:uuid:1";
    signalk.signalk.source_label = "sim";
    signalk.filter = {QStringLiteral("navigation"), QStringLiteral("environment.wind")};
    profile.outputs.append(signalk);
    OutputConfig viewsync;
    viewsync.type = OutputConfig::Type::Udp;
    viewsync.encoding = OutputConfig::Encoding::ViewSync;
    viewsync.period_ms = 200;
    viewsync.viewsync.camera_altitude_m = 1500.0;
    viewsync.viewsync.tilt_deg = 45.0;
    viewsync.viewsync.planet = "moon";
    profile.outputs.append(viewsync);

    const auto json = profile.to_json();
    QString error;
    const auto parsed = Profile::from_json(json, &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->to_json() == json);
    REQUIRE(parsed->delta.seed.destination.has_value());
    CHECK(parsed->delta.seed.destination->name == "AEGINA");
    CHECK(parsed->delta.seed.destination->position.longitude_deg == Approx(23.4275));
    CHECK(parsed->delta.seed.destination->origin.latitude_deg == Approx(38.0));
    CHECK(parsed->delta.seed.destination->arrival_radius_m == Approx(250.0));
    CHECK(parsed->delta.seed.ais.mmsi == 211000123);
    CHECK(parsed->delta.seed.ais.name == "TEST VESSEL");
    CHECK(parsed->delta.seed.ais.call_sign == "DA1234");
    CHECK(parsed->delta.seed.ais.ship_type == 70);
    CHECK(parsed->delta.seed.ais.dimension_to_bow_m == Approx(40.0));
    CHECK(parsed->delta.seed.ais.draught_m == Approx(4.5));
    CHECK(parsed->delta.seed.ais.destination == "PIRAEUS");
    CHECK(parsed->delta.seed.ais.navigation_status == 8);
    CHECK(parsed->delta.seed.ais.position_report_type == 3);
    REQUIRE(parsed->custom_sentences.size() == 2);
    CHECK(parsed->custom_sentences[0].id == "BARO");
    CHECK(parsed->custom_sentences[0].period == 5000ms);
    CHECK(parsed->custom_sentences[1].id.empty());
    CHECK_FALSE(parsed->custom_sentences[1].enabled);
    REQUIRE(parsed->outputs.size() == 3);
    CHECK(parsed->outputs[0].encoding == OutputConfig::Encoding::Nmea0183);
    CHECK(parsed->outputs[0].tag_block.enabled);
    CHECK(parsed->outputs[0].tag_block.options.source == "GP0001");
    CHECK(parsed->outputs[0].tag_block.options.milliseconds);
    CHECK(parsed->outputs[1].encoding == OutputConfig::Encoding::SignalK);
    CHECK(parsed->outputs[1].period_ms == 500);
    CHECK(parsed->outputs[1].signalk.context == "aircraft.urn:mrn:signalk:uuid:1");
    CHECK(parsed->outputs[1].signalk.source_label == "sim");
    CHECK(parsed->outputs[1].filter.size() == 2);
    CHECK(parsed->outputs[2].encoding == OutputConfig::Encoding::ViewSync);
    CHECK(parsed->outputs[2].period_ms == 200);
    CHECK(parsed->outputs[2].viewsync.camera_altitude_m == Approx(1500.0));
    CHECK(parsed->outputs[2].viewsync.tilt_deg == Approx(45.0));
    CHECK(parsed->outputs[2].viewsync.planet == "moon");

    const auto scheduler = parsed->make_scheduler();
    REQUIRE(scheduler.custom_sentences().size() == 2);
    // The scheduler names a custom sentence without id `CUSTOM-n`, n being its 1-based position.
    CHECK(scheduler.custom_sentences()[1].id == "CUSTOM-2");

    // Clearing the destination writes null, which reads back as none.
    profile.delta.seed.destination.reset();
    const auto cleared = Profile::from_json(profile.to_json(), &error);
    REQUIRE(cleared.has_value());
    CHECK_FALSE(cleared->delta.seed.destination.has_value());
}

TEST_CASE("schema version 2 profiles load with the new defaults", "[io][profile]") {
    QJsonObject v2{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("outputs"),
         QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("tcp-server")},
                                {QStringLiteral("encoding"), QStringLiteral("nmea0183")}}}}};
    QString error;
    const auto parsed = Profile::from_json(v2, &error);
    REQUIRE(parsed.has_value());
    CHECK_FALSE(parsed->delta.seed.destination.has_value());
    // 239000001 is the default MMSI of the core AIS model, and 1000 ms the default period of
    // an output.
    CHECK(parsed->delta.seed.ais.mmsi == 239000001);
    CHECK(parsed->custom_sentences.empty());
    REQUIRE(parsed->outputs.size() == 1);
    CHECK(parsed->outputs[0].encoding == OutputConfig::Encoding::Nmea0183);
    CHECK_FALSE(parsed->outputs[0].tag_block.enabled);
    CHECK(parsed->outputs[0].period_ms == 1000);
    CHECK(parsed->to_json().value(QStringLiteral("schema_version")).toInt() == 3);
    for (const auto encoding : {OutputConfig::Encoding::Nmea0183, OutputConfig::Encoding::SignalK,
                                OutputConfig::Encoding::ViewSync}) {
        CHECK(nmeasim::io::encoding_from_string(nmeasim::io::to_string(encoding)) == encoding);
    }
    CHECK_FALSE(nmeasim::io::encoding_from_string(QStringLiteral("morse")).has_value());
}

TEST_CASE("the new schema 3 keys are validated", "[io][profile]") {
    QString error;
    const auto with = [](const QString& key, const QJsonValue& value) {
        return QJsonObject{{QStringLiteral("schema_version"), 3}, {key, value}};
    };
    const auto seed = [](const QJsonObject& ais) {
        return QJsonObject{{QStringLiteral("seed"), QJsonObject{{QStringLiteral("ais"), ais}}}};
    };
    // An MMSI has at most nine digits, a ship type is in [0, 255] and a position report is
    // message 1, 2 or 3.
    CHECK_FALSE(Profile::from_json(with(QStringLiteral("simulation"),
                                        seed({{QStringLiteral("mmsi"), 1234567890.0}})),
                                   &error)
                    .has_value());
    CHECK(error.contains(QStringLiteral("mmsi")));
    CHECK_FALSE(
        Profile::from_json(
            with(QStringLiteral("simulation"), seed({{QStringLiteral("ship_type"), 300}})), &error)
            .has_value());
    CHECK(error.contains(QStringLiteral("ship_type")));
    CHECK_FALSE(Profile::from_json(with(QStringLiteral("simulation"),
                                        seed({{QStringLiteral("position_report_type"), 4}})),
                                   &error)
                    .has_value());
    CHECK(error.contains(QStringLiteral("position_report_type")));

    const auto custom = [](const QJsonObject& sentence) {
        return QJsonObject{{QStringLiteral("custom"), QJsonArray{sentence}}};
    };
    CHECK_FALSE(
        Profile::from_json(with(QStringLiteral("sentences"),
                                custom({{QStringLiteral("body"), QStringLiteral("not valid")}})),
                           &error)
            .has_value());
    CHECK(error.contains(QStringLiteral("custom[0]")));
    // Custom ids are upper-cased before the check, so `rmc` clashes with the registry's RMC.
    CHECK_FALSE(
        Profile::from_json(with(QStringLiteral("sentences"),
                                custom({{QStringLiteral("id"), QStringLiteral("rmc")},
                                        {QStringLiteral("body"), QStringLiteral("$PXYZ,1")}})),
                           &error)
            .has_value());
    CHECK(error.contains(QStringLiteral("registry")));
    // Periods of custom sentences and outputs must be in [50, 3600000] ms.
    CHECK_FALSE(Profile::from_json(with(QStringLiteral("sentences"),
                                        custom({{QStringLiteral("body"), QStringLiteral("$PXYZ,1")},
                                                {QStringLiteral("period_ms"), 10}})),
                                   &error)
                    .has_value());
    CHECK(error.contains(QStringLiteral("period_ms")));

    const auto output = [](const QJsonObject& extra) {
        QJsonObject object{{QStringLiteral("type"), QStringLiteral("stdout")}};
        for (auto it = extra.begin(); it != extra.end(); ++it) {
            object.insert(it.key(), it.value());
        }
        return QJsonArray{object};
    };
    CHECK_FALSE(
        Profile::from_json(with(QStringLiteral("outputs"),
                                output({{QStringLiteral("encoding"), QStringLiteral("n2k")}})),
                           &error)
            .has_value());
    CHECK(error.contains(QStringLiteral("encoding")));
    CHECK_FALSE(
        Profile::from_json(
            with(QStringLiteral("outputs"), output({{QStringLiteral("period_ms"), 10}})), &error)
            .has_value());
    CHECK(error.contains(QStringLiteral("period_ms")));
}

TEST_CASE("random seed, MMSI and IMO number outside their integer range are rejected",
          "[io][profile]") {
    QString error;
    const auto simulation = [](const QJsonObject& keys) {
        return QJsonObject{{QStringLiteral("schema_version"), 3},
                           {QStringLiteral("simulation"), keys}};
    };
    const auto ais = [&simulation](const char* key, double value) {
        return simulation(
            {{QStringLiteral("seed"),
              QJsonObject{{QStringLiteral("ais"), QJsonObject{{QLatin1String(key), value}}}}}});
    };

    // 4294967295 is the largest `unsigned int` of 32 bits, 999999999 the largest nine-digit
    // number.
    for (const double bad : {-1.0, 4294967296.0, 5e9, 1.5}) {
        INFO(bad);
        CHECK_FALSE(Profile::from_json(simulation({{QStringLiteral("random_seed"), bad}}), &error)
                        .has_value());
        CHECK(error.contains(QStringLiteral("random_seed")));
    }
    for (const double bad : {-1.0, 1e9, 5e9, 211000123.5}) {
        INFO(bad);
        CHECK_FALSE(Profile::from_json(ais("mmsi", bad), &error).has_value());
        CHECK(error.contains(QStringLiteral("mmsi")));
        CHECK_FALSE(Profile::from_json(ais("imo_number", bad), &error).has_value());
        CHECK(error.contains(QStringLiteral("imo_number")));
    }

    const auto seed =
        Profile::from_json(simulation({{QStringLiteral("random_seed"), 4294967295.0}}), &error);
    REQUIRE(seed.has_value());
    CHECK(seed->delta.random_seed == 4294967295U);
    const auto zero = Profile::from_json(simulation({{QStringLiteral("random_seed"), 0}}), &error);
    REQUIRE(zero.has_value());
    CHECK(zero->delta.random_seed == 0U);
    const auto mmsi = Profile::from_json(ais("mmsi", 999999999.0), &error);
    REQUIRE(mmsi.has_value());
    CHECK(mmsi->delta.seed.ais.mmsi == 999999999U);
    const auto imo = Profile::from_json(ais("imo_number", 9074729.0), &error);
    REQUIRE(imo.has_value());
    CHECK(imo->delta.seed.ais.imo_number == 9074729U);
}
