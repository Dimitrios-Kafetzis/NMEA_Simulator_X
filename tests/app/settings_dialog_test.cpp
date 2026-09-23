#include "dialogs/settings_dialog.hpp"

#include "dialogs/outputs_page.hpp"
#include "dialogs/sentences_page.hpp"
#include "dialogs/simulation_page.hpp"
#include "dialogs/vessel_page.hpp"
#include "main_window.hpp"

#include <QSignalSpy>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

using Catch::Approx;
using nmeasim::io::OutputConfig;

TEST_CASE("the settings dialog loads and stores simulation values", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    profile.name = QStringLiteral("Harbour");
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.simulation_page();
    CHECK(page->name_edit->text() == QStringLiteral("Harbour"));
    CHECK(page->latitude_spin->value() == Approx(37.9838));
    CHECK(page->tick_spin->value() == profile.tick_ms);
    CHECK_FALSE(page->fixed_start_check->isChecked());

    page->name_edit->setText(QStringLiteral("Edited"));
    page->tick_spin->setValue(250);
    page->latitude_spin->setValue(-33.85);
    page->longitude_spin->setValue(151.2);
    page->heading_spin->setValue(123.4);
    page->fix_check->setChecked(false);
    page->quality_combo->setCurrentIndex(2);
    page->amplitude_spins[0]->setValue(0.0);
    page->step_spins[5]->setValue(0.75);
    page->turn_rate_spin->setValue(1.2);
    page->fixed_start_check->setChecked(true);
    page->start_time_edit->setDateTime(
        QDateTime(QDate(2026, 9, 23), QTime(12, 0, 0), QTimeZone::utc()));
    dialog.accept();

    const auto& result = dialog.profile();
    CHECK(result.name == QStringLiteral("Edited"));
    CHECK(result.tick_ms == 250);
    CHECK(result.delta.seed.navigation.position.latitude_deg == Approx(-33.85));
    CHECK(result.delta.seed.navigation.position.longitude_deg == Approx(151.2));
    CHECK(result.delta.seed.navigation.heading_true_deg == Approx(123.4));
    CHECK_FALSE(result.delta.seed.gnss.has_fix);
    CHECK(result.delta.seed.gnss.quality == nmeasim::core::model::FixQuality::Differential);
    CHECK(result.delta.heading.amplitude == 0.0);
    CHECK(result.delta.wind_speed.step_per_second == Approx(0.75));
    CHECK(result.delta.turn_rate_per_rudder_deg == Approx(1.2));
    REQUIRE(result.start_time.has_value());
    CHECK(result.start_time->toString(Qt::ISODate) == QStringLiteral("2026-09-23T12:00:00Z"));
    CHECK(result.delta.speed.amplitude == Approx(profile.delta.speed.amplitude));
}

TEST_CASE("the sentences tab writes only settings that differ from the registry",
          "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    profile.sentences.clear();
    profile.sentences["GSV"] = {false, "", std::chrono::milliseconds{1000}};
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.sentences_page();
    const int gsv = page->row_of(QStringLiteral("GSV"));
    const int rmc = page->row_of(QStringLiteral("RMC"));
    REQUIRE(gsv >= 0);
    REQUIRE(rmc >= 0);
    CHECK_FALSE(page->is_enabled(gsv));
    CHECK(page->is_enabled(rmc));

    page->set_enabled(gsv, true);
    static_cast<QLineEdit*>(page->table->cellWidget(rmc, nmeasim::app::SentencesPage::Talker))
        ->setText(QStringLiteral("gn"));
    static_cast<QSpinBox*>(page->table->cellWidget(rmc, nmeasim::app::SentencesPage::Period))
        ->setValue(500);
    page->position_decimals_spin->setValue(3);
    dialog.accept();

    const auto& result = dialog.profile();
    CHECK(result.encoder.position_decimals == 3);
    CHECK_FALSE(result.sentences.contains("GSV"));
    REQUIRE(result.sentences.contains("RMC"));
    CHECK(result.sentences.at("RMC").talker == "GN");
    CHECK(result.sentences.at("RMC").period == std::chrono::milliseconds{500});
    CHECK(result.sentences.at("RMC").enabled);
    // A setting equal to the registry default is not stored.
    CHECK_FALSE(result.sentences.contains("GGA"));
}

TEST_CASE("the outputs tab adds, edits, validates and removes outputs", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.outputs_page();
    REQUIRE(page->count() == 1);
    CHECK(page->current_index() == 0);
    CHECK(page->server_port_spin->value() == 10110);

    page->server_port_spin->setValue(20000);
    page->filter_edit->setText(QStringLiteral("rmc, gga"));
    page->add_output(OutputConfig::Type::Udp);
    REQUIRE(page->count() == 2);
    CHECK(page->current_index() == 1);
    page->udp_mode_combo->setCurrentIndex(1);
    page->udp_address_edit->clear();
    page->udp_port_spin->setValue(10111);
    page->udp_interface_edit->setText(QStringLiteral("eth0"));
    page->add_output(OutputConfig::Type::Serial);
    REQUIRE(page->count() == 3);

    // The serial output has no port yet, so the dialog refuses to close.
    dialog.accept();
    CHECK_FALSE(dialog.error_text().isEmpty());
    CHECK(dialog.result() != QDialog::Accepted);

    page->serial_port_combo->setCurrentText(QStringLiteral("/dev/ttyUSB9"));
    page->baud_combo->setCurrentText(QStringLiteral("38400"));
    page->enabled_check->setChecked(false);
    dialog.accept();
    CHECK(dialog.error_text().isEmpty());

    const auto& outputs = dialog.profile().outputs;
    REQUIRE(outputs.size() == 3);
    CHECK(outputs[0].type == OutputConfig::Type::TcpServer);
    CHECK(outputs[0].port == 20000);
    CHECK(outputs[0].filter == QStringList{QStringLiteral("RMC"), QStringLiteral("GGA")});
    CHECK(outputs[1].type == OutputConfig::Type::Udp);
    CHECK(outputs[1].udp.mode == nmeasim::io::UdpConfig::Mode::Broadcast);
    CHECK(outputs[1].udp.address.isEmpty());
    CHECK(outputs[1].udp.port == 10111);
    CHECK(outputs[1].udp.interface_name == QStringLiteral("eth0"));
    CHECK(outputs[2].type == OutputConfig::Type::Serial);
    CHECK(outputs[2].serial.port_name == QStringLiteral("/dev/ttyUSB9"));
    CHECK(outputs[2].serial.baud_rate == 38400);
    CHECK_FALSE(outputs[2].enabled);

    page->select(1);
    page->remove_current();
    CHECK(page->count() == 2);
    CHECK(page->outputs().at(1).type == OutputConfig::Type::Serial);
}

TEST_CASE("an edited profile round-trips through JSON unchanged", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    nmeasim::app::SettingsDialog dialog(profile);
    dialog.outputs_page()->add_output(OutputConfig::Type::File);
    dialog.outputs_page()->file_path_edit->setText(QStringLiteral("out.nmea"));
    dialog.sentences_page()->set_enabled(dialog.sentences_page()->row_of(QStringLiteral("ZDA")),
                                         false);
    dialog.accept();
    const auto json = dialog.profile().to_json();
    QString error;
    const auto reloaded = nmeasim::io::Profile::from_json(json, &error);
    REQUIRE(reloaded.has_value());
    CHECK(reloaded->to_json() == json);
    CHECK(reloaded->outputs.size() == 2);
    CHECK_FALSE(reloaded->sentences.at("ZDA").enabled);
}

TEST_CASE("the main window applies a dialog result as the current profile", "[app][settings]") {
    nmeasim::app::MainWindow window;
    nmeasim::app::SettingsDialog dialog(window.profile(), &window);
    dialog.simulation_page()->name_edit->setText(QStringLiteral("From dialog"));
    dialog.accept();
    window.set_profile(dialog.profile());
    CHECK(window.profile().name == QStringLiteral("From dialog"));
    CHECK(window.windowTitle().startsWith(QStringLiteral("From dialog")));
}

TEST_CASE("the simulation tab selects the mode and validates its files", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.simulation_page();
    CHECK(page->mode_combo->currentIndex() == 0);
    CHECK(page->track_speed_spin->value() == Approx(6.0));
    CHECK(page->track_timestamps_check->isChecked());
    CHECK(page->replay_interval_spin->value() == 100);

    // Track mode without a file is refused on the simulation tab.
    page->mode_combo->setCurrentIndex(1);
    dialog.accept();
    CHECK(dialog.error_text().contains(QStringLiteral("track")));
    CHECK(dialog.tabs()->currentWidget() == page);
    CHECK(dialog.result() != QDialog::Accepted);

    page->track_path_edit->setText(QStringLiteral("/tracks/harbour.gpx"));
    page->track_speed_spin->setValue(8.5);
    page->track_timestamps_check->setChecked(false);
    page->track_loop_check->setChecked(true);
    dialog.accept();
    CHECK(dialog.error_text().isEmpty());
    const auto& result = dialog.profile();
    CHECK(result.mode == nmeasim::io::SimulationMode::Track);
    CHECK(result.track.path == QStringLiteral("/tracks/harbour.gpx"));
    CHECK(result.track.speed_kn == Approx(8.5));
    CHECK_FALSE(result.track.use_timestamps);
    CHECK(result.track.loop);

    // Replay settings load back into the widgets.
    profile.mode = nmeasim::io::SimulationMode::Replay;
    profile.replay.path = QStringLiteral("/logs/monday.log");
    profile.replay.loop = true;
    profile.replay.fixed_interval_ms = 250;
    nmeasim::app::SettingsDialog replay_dialog(profile);
    auto* replay_page = replay_dialog.simulation_page();
    CHECK(replay_page->mode_combo->currentIndex() == 2);
    CHECK(replay_page->replay_path_edit->text() == QStringLiteral("/logs/monday.log"));
    CHECK(replay_page->replay_loop_check->isChecked());
    CHECK(replay_page->replay_interval_spin->value() == 250);
    replay_page->replay_path_edit->clear();
    replay_dialog.accept();
    CHECK(replay_dialog.error_text().contains(QStringLiteral("log")));
    replay_page->replay_path_edit->setText(QStringLiteral("/logs/tuesday.log"));
    replay_dialog.accept();
    CHECK(replay_dialog.profile().replay.path == QStringLiteral("/logs/tuesday.log"));
    CHECK(replay_dialog.profile().mode == nmeasim::io::SimulationMode::Replay);
}

TEST_CASE("the simulation tab edits the destination", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.simulation_page();
    CHECK_FALSE(page->destination_check->isChecked());
    CHECK_FALSE(page->destination_name_edit->isEnabled());
    page->destination_check->setChecked(true);
    CHECK(page->destination_name_edit->isEnabled());
    page->destination_name_edit->setText(QStringLiteral("AEGINA"));
    page->destination_latitude_spin->setValue(37.7466);
    page->destination_longitude_spin->setValue(23.4275);
    page->arrival_radius_spin->setValue(250.0);
    dialog.accept();
    const auto& result = dialog.profile();
    REQUIRE(result.delta.seed.destination.has_value());
    CHECK(result.delta.seed.destination->name == "AEGINA");
    CHECK(result.delta.seed.destination->position.latitude_deg == Approx(37.7466));
    CHECK(result.delta.seed.destination->origin.latitude_deg == Approx(37.9838));
    CHECK(result.delta.seed.destination->arrival_radius_m == Approx(250.0));

    // Reopening keeps the leg origin; unticking clears the destination.
    nmeasim::app::SettingsDialog again(result);
    CHECK(again.simulation_page()->destination_check->isChecked());
    CHECK(again.simulation_page()->destination_name_edit->text() == QStringLiteral("AEGINA"));
    again.simulation_page()->arrival_radius_spin->setValue(50.0);
    again.accept();
    CHECK(again.profile().delta.seed.destination->origin.latitude_deg == Approx(37.9838));
    CHECK(again.profile().delta.seed.destination->arrival_radius_m == Approx(50.0));
    nmeasim::app::SettingsDialog cleared(result);
    cleared.simulation_page()->destination_check->setChecked(false);
    cleared.accept();
    CHECK_FALSE(cleared.profile().delta.seed.destination.has_value());
}

TEST_CASE("the vessel tab edits the engines and the AIS static data", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.vessel_page();
    REQUIRE(page->engine_count() == 2);
    CHECK(page->engine_at(0).label == "Port engine");
    CHECK(page->mmsi_spin->value() == 239000001);
    CHECK(page->name_edit->text() == QStringLiteral("NMEA SIMULATOR X"));

    page->set_engine(1, {"Starboard engine", false, 0.0, 40.0});
    page->add_engine();
    REQUIRE(page->engine_count() == 3);
    CHECK(page->engine_at(2).label == "Engine 3");
    page->engines_table->selectRow(0);
    page->remove_current_engine();
    REQUIRE(page->engine_count() == 2);
    page->mmsi_spin->setValue(211000123);
    page->name_edit->setText(QStringLiteral("test vessel"));
    page->call_sign_edit->setText(QStringLiteral("da1234"));
    page->ship_type_spin->setValue(70);
    page->to_bow_spin->setValue(40.0);
    page->draught_spin->setValue(4.5);
    page->ais_destination_edit->setText(QStringLiteral("Piraeus"));
    page->navigation_status_spin->setValue(8);
    page->report_type_combo->setCurrentIndex(2);
    dialog.accept();
    CHECK(dialog.error_text().isEmpty());

    const auto& seed = dialog.profile().delta.seed;
    REQUIRE(seed.engines.size() == 2);
    CHECK(seed.engines[0].label == "Starboard engine");
    CHECK_FALSE(seed.engines[0].running);
    CHECK(seed.engines[0].coolant_temperature_c == Approx(40.0));
    CHECK(seed.engines[1].label == "Engine 3");
    CHECK(seed.ais.mmsi == 211000123);
    CHECK(seed.ais.name == "TEST VESSEL");
    CHECK(seed.ais.call_sign == "DA1234");
    CHECK(seed.ais.ship_type == 70);
    CHECK(seed.ais.dimension_to_bow_m == Approx(40.0));
    CHECK(seed.ais.draught_m == Approx(4.5));
    CHECK(seed.ais.destination == "PIRAEUS");
    CHECK(seed.ais.navigation_status == 8);
    CHECK(seed.ais.position_report_type == 3);

    // An MMSI of zero is refused on the vessel tab.
    nmeasim::app::SettingsDialog invalid(profile);
    invalid.vessel_page()->mmsi_spin->setValue(0);
    invalid.accept();
    CHECK(invalid.error_text().contains(QStringLiteral("MMSI")));
    CHECK(invalid.tabs()->currentWidget() == invalid.vessel_page());
}

TEST_CASE("the sentences tab edits custom sentences and validates them", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    profile.custom_sentences = {
        {"BARO", "$IIXDR,P,1.013,B,BARO", std::chrono::milliseconds{5000}, false}};
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.sentences_page();
    REQUIRE(page->custom_count() == 1);
    CHECK(page->custom_at(0).id == "BARO");
    CHECK_FALSE(page->custom_at(0).enabled);
    CHECK(page->custom_at(0).period == std::chrono::milliseconds{5000});

    page->add_custom(QStringLiteral("not a sentence!"));
    dialog.accept();
    CHECK(dialog.error_text().contains(QStringLiteral("Custom sentence 2")));
    CHECK(dialog.tabs()->currentWidget() == page);

    static_cast<QLineEdit*>(
        page->custom_table->cellWidget(1, nmeasim::app::SentencesPage::CustomBody))
        ->setText(QStringLiteral("PXYZ,1,2,3"));
    static_cast<QLineEdit*>(
        page->custom_table->cellWidget(1, nmeasim::app::SentencesPage::CustomId))
        ->setText(QStringLiteral("rmc"));
    dialog.accept();
    CHECK(dialog.error_text().contains(QStringLiteral("registry")));
    static_cast<QLineEdit*>(
        page->custom_table->cellWidget(1, nmeasim::app::SentencesPage::CustomId))
        ->clear();
    dialog.accept();
    CHECK(dialog.error_text().isEmpty());
    REQUIRE(dialog.profile().custom_sentences.size() == 2);
    CHECK(dialog.profile().custom_sentences[1].id.empty());
    CHECK(dialog.profile().custom_sentences[1].body == "PXYZ,1,2,3");
    CHECK(dialog.profile().custom_sentences[1].enabled);

    page->custom_table->selectRow(0);
    page->remove_current_custom();
    CHECK(page->custom_count() == 1);
}

TEST_CASE("the outputs tab selects the encoding and its options", "[app][settings]") {
    auto profile = nmeasim::io::Profile::default_profile();
    nmeasim::app::SettingsDialog dialog(profile);
    auto* page = dialog.outputs_page();
    CHECK(page->encoding_combo->currentIndex() == 0);
    CHECK(page->tag_block_box->isVisibleTo(page));
    CHECK_FALSE(page->signalk_box->isVisibleTo(page));
    page->tag_block_check->setChecked(true);
    page->tag_source_edit->setText(QStringLiteral("GP0001"));
    page->tag_milliseconds_check->setChecked(true);

    page->add_output(OutputConfig::Type::WebSocketServer);
    page->encoding_combo->setCurrentIndex(1);
    CHECK(page->signalk_box->isVisibleTo(page));
    CHECK_FALSE(page->tag_block_box->isVisibleTo(page));
    CHECK(page->period_spin->isEnabled());
    page->period_spin->setValue(500);
    page->filter_edit->setText(QStringLiteral("navigation, environment.wind"));
    page->signalk_context_combo->setCurrentIndex(2);
    page->signalk_context_edit->setText(QStringLiteral("aircraft.urn:mrn:signalk:uuid:1"));
    page->signalk_source_edit->setText(QStringLiteral("sim"));

    page->add_output(OutputConfig::Type::Udp);
    page->encoding_combo->setCurrentIndex(2);
    CHECK(page->viewsync_box->isVisibleTo(page));
    page->camera_altitude_spin->setValue(1500.0);
    page->tilt_spin->setValue(45.0);
    page->planet_combo->setCurrentIndex(2);
    dialog.accept();
    CHECK(dialog.error_text().isEmpty());

    const auto& outputs = dialog.profile().outputs;
    REQUIRE(outputs.size() == 3);
    CHECK(outputs[0].encoding == OutputConfig::Encoding::Nmea0183);
    CHECK(outputs[0].tag_block.enabled);
    CHECK(outputs[0].tag_block.options.source == "GP0001");
    CHECK(outputs[0].tag_block.options.milliseconds);
    CHECK(outputs[1].encoding == OutputConfig::Encoding::SignalK);
    CHECK(outputs[1].period_ms == 500);
    CHECK(outputs[1].filter ==
          QStringList{QStringLiteral("navigation"), QStringLiteral("environment.wind")});
    CHECK(outputs[1].signalk.context == "aircraft.urn:mrn:signalk:uuid:1");
    CHECK(outputs[1].signalk.source_label == "sim");
    CHECK(outputs[2].encoding == OutputConfig::Encoding::ViewSync);
    CHECK(outputs[2].viewsync.camera_altitude_m == Approx(1500.0));
    CHECK(outputs[2].viewsync.tilt_deg == Approx(45.0));
    CHECK(outputs[2].viewsync.planet == "mars");

    // The options load back into the widgets.
    nmeasim::app::SettingsDialog again(dialog.profile());
    again.outputs_page()->select(1);
    CHECK(again.outputs_page()->encoding_combo->currentIndex() == 1);
    CHECK(again.outputs_page()->signalk_context_combo->currentIndex() == 2);
    CHECK(again.outputs_page()->signalk_context_edit->text() ==
          QStringLiteral("aircraft.urn:mrn:signalk:uuid:1"));
    again.outputs_page()->select(2);
    CHECK(again.outputs_page()->planet_combo->currentText() == QStringLiteral("mars"));
}
