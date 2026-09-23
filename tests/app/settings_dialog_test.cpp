#include "dialogs/settings_dialog.hpp"

#include "dialogs/outputs_page.hpp"
#include "dialogs/sentences_page.hpp"
#include "dialogs/simulation_page.hpp"
#include "main_window.hpp"

#include <QSignalSpy>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
