// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::app::MainWindow`, the main window of the desktop application.
///
/// Covers the position and duration formatting, running a profile against a local TCP server,
/// dashboard overrides and keyboard nudges reaching the delta simulation, loading and saving
/// profiles, the about text, following a GPX track with step and seek, replaying a log to its
/// end, recording a session to a log file, and the engine tiles. The window is created on the
/// offscreen platform (see `main.cpp`). The file reads the fixtures `tracks/timestamped.gpx`,
/// `tracks/malformed.gpx` and `logs/plain.nmea` from `tests/fixtures`.

#include "main_window.hpp"

#include "io/event_loop.hpp"
#include "map/map_widget.hpp"
#include "map/tile_cache.hpp"
#include "widgets/console_widget.hpp"
#include "widgets/dashboard_widget.hpp"
#include "widgets/outputs_widget.hpp"

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/version.hpp>

#include <QFile>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

using Catch::Approx;
using namespace std::chrono_literals;
using nmeasim::test::wait_until;
namespace sim = nmeasim::core::simulation;

namespace {

/// Returns the default profile made fast and self-contained for tests.
///
/// The tick is 20 ms instead of the default 100 ms, and the only output is a TCP server on
/// `127.0.0.1` with port 0, so that the operating system picks a free port and the test never
/// collides with a port already in use. Every sentence of the standard registry is listed with
/// its default enabled state, the default talker (the empty string) and a period of 100 ms, so
/// that the console fills within a fraction of a second.
///
/// @return The profile, ready for `nmeasim::app::MainWindow::set_profile`.
nmeasim::io::Profile quick_profile() {
    auto profile = nmeasim::io::Profile::default_profile();
    profile.tick_ms = 20;
    profile.outputs.clear();
    nmeasim::io::OutputConfig tcp;
    tcp.type = nmeasim::io::OutputConfig::Type::TcpServer;
    tcp.port = 0;
    tcp.bind_address = QStringLiteral("127.0.0.1");
    profile.outputs.append(tcp);
    for (const auto& descriptor :
         nmeasim::core::nmea0183::SentenceRegistry::standard().descriptors()) {
        profile.sentences[std::string{descriptor.id}] = {descriptor.enabled_by_default, "", 100ms};
    }
    return profile;
}

/// Returns the absolute path of a file in the test fixtures directory.
///
/// @param relative Path below `tests/fixtures`, for example `"tracks/timestamped.gpx"`.
/// @return The path, built from the `NMEASIM_FIXTURES_DIR` compile definition.
QString fixture(const char* relative) {
    return QStringLiteral(NMEASIM_FIXTURES_DIR "/") + QLatin1String(relative);
}

}  // namespace

TEST_CASE("positions are formatted as degrees and decimal minutes", "[app]") {
    // 0.9838 degrees is 59.028 minutes and 0.7275 degrees 43.650 minutes.
    CHECK(nmeasim::app::format_position({37.9838, 23.7275}) ==
          QStringLiteral("37°59.028'N  023°43.650'E"));
    CHECK(nmeasim::app::format_position({-38.9997, -151.5001}) ==
          QStringLiteral("38°59.982'S  151°30.006'W"));
}

TEST_CASE("the main window runs a profile and shows sentences and outputs", "[app][integration]") {
    nmeasim::app::MainWindow window;
    window.set_profile(quick_profile());
    CHECK_FALSE(window.is_running());
    CHECK(window.outputs()->row_count() == 1);
    CHECK(window.outputs()->status_text(0) == QStringLiteral("closed"));

    window.start();
    CHECK(window.is_running());
    REQUIRE(wait_until([&] { return window.console()->line_count() > 10; }, 5000));
    window.outputs()->refresh();
    CHECK(window.outputs()->status_text(0) == QStringLiteral("open"));
    CHECK(window.runner().sentences_emitted() > 0);

    window.stop();
    CHECK_FALSE(window.is_running());
    window.outputs()->refresh();
    CHECK(window.outputs()->status_text(0) == QStringLiteral("closed"));
}

TEST_CASE("dashboard overrides and keyboard nudges reach the simulation", "[app][integration]") {
    nmeasim::app::MainWindow window;
    window.set_profile(quick_profile());
    window.start();
    auto* source = dynamic_cast<sim::DeltaSource*>(&window.runner().simulation()->source());
    REQUIRE(source != nullptr);

    emit window.dashboard()->override_changed(sim::Parameter::Depth, true, 42.0);
    CHECK(source->override_value(sim::Parameter::Depth) == 42.0);
    REQUIRE(wait_until(
        [&] { return source->current().water.depth_below_transducer_m == Approx(42.0); }));

    emit window.dashboard()->override_changed(sim::Parameter::Depth, false, 42.0);
    CHECK_FALSE(source->override_value(sim::Parameter::Depth).has_value());

    const double speed_before = source->current().navigation.speed_over_ground_kn;
    // Up without Shift is the fine speed nudge of 0.1 kn; with Shift it is 1 kn.
    QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
    QApplication::sendEvent(&window, &up);
    CHECK(source->override_value(sim::Parameter::SpeedOverGround) == Approx(speed_before + 0.1));

    QKeyEvent right_fast(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
    QApplication::sendEvent(&window, &right_fast);
    REQUIRE(source->override_value(sim::Parameter::HeadingTrue).has_value());

    emit window.dashboard()->fix_changed(false);
    CHECK_FALSE(source->current().gnss.has_fix);
    emit window.dashboard()->satellites_changed(5);
    CHECK(source->current().gnss.satellites_in_use == 5);
    window.stop();
}

TEST_CASE("profiles round-trip through the window", "[app]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("test.json"));
    auto profile = quick_profile();
    profile.name = QStringLiteral("Window test");
    QString error;
    REQUIRE(profile.save(path, &error));

    nmeasim::app::MainWindow window;
    REQUIRE(window.load_profile(path));
    CHECK(window.profile().name == QStringLiteral("Window test"));
    CHECK(window.windowTitle().startsWith(QStringLiteral("test.json")));
    CHECK_FALSE(window.load_profile(directory.filePath(QStringLiteral("missing.json"))));
}

TEST_CASE("durations are formatted for the transport label", "[app]") {
    CHECK(nmeasim::app::MainWindow::format_duration(0ms) == QStringLiteral("00:00"));
    CHECK(nmeasim::app::MainWindow::format_duration(65s) == QStringLiteral("01:05"));
    CHECK(nmeasim::app::MainWindow::format_duration(12min + 500ms) == QStringLiteral("12:00"));
    CHECK(nmeasim::app::MainWindow::format_duration(3h + 7min + 9s) == QStringLiteral("3:07:09"));
}

TEST_CASE("the about text names the version and the build", "[app]") {
    const std::string_view version = nmeasim::core::version_description();
    CHECK(nmeasim::app::MainWindow::about_text().contains(
        QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size()))));
}

TEST_CASE("the main window follows a track and offers step and seek", "[app][track]") {
    nmeasim::app::MainWindow window;
    window.map_view()->cache()->set_online(false);
    window.set_profile(quick_profile());
    CHECK_FALSE(window.seek_slider()->isEnabled());
    CHECK(window.position_label()->text().isEmpty());
    CHECK(window.dashboard()->overrides_enabled());

    REQUIRE(window.load_track(fixture("tracks/timestamped.gpx")));
    CHECK(window.profile().mode == nmeasim::io::SimulationMode::Track);
    CHECK(window.profile().track.path == fixture("tracks/timestamped.gpx"));
    CHECK(window.map_view()->route_length() == 5);
    CHECK(window.seek_slider()->isEnabled());
    // The fixture runs from 10:00:00 to 10:12:00.500, 720500 ms.
    CHECK(window.seek_slider()->maximum() == 720500);
    CHECK(window.position_label()->text() == QStringLiteral("00:00 / 12:00"));
    CHECK_FALSE(window.dashboard()->overrides_enabled());
    CHECK(window.runner().simulation()->state().navigation.position.latitude_deg == Approx(37.9));
    REQUIRE(window.map_view()->vessel_position().has_value());
    CHECK(window.map_view()->vessel_position()->latitude_deg == Approx(37.9));

    // Step starts the run paused and advances one tick.
    window.step_action()->trigger();
    CHECK(window.is_running());
    CHECK(window.runner().is_paused());
    CHECK(window.runner().position() == 20ms);
    CHECK(window.runner().sentences_emitted() > 0);

    // The slider seeks; the label and the map follow. At 3 minutes the track is at its second
    // point, 37.91 N.
    window.seek_slider()->setValue(180000);
    CHECK(window.runner().position() == 3min);
    CHECK(window.position_label()->text() == QStringLiteral("03:00 / 12:00"));
    CHECK(window.runner().simulation()->state().navigation.position.latitude_deg ==
          Approx(37.91).margin(1e-9));
    CHECK(window.map_view()->vessel_position()->latitude_deg == Approx(37.91).margin(1e-9));
    window.stop();
    CHECK_FALSE(window.runner().is_paused());

    // A bad file is reported and the previous profile stays current.
    QSignalSpy errors(&window, &nmeasim::app::MainWindow::error_reported);
    CHECK_FALSE(window.load_track(fixture("tracks/malformed.gpx")));
    CHECK(errors.count() == 1);
    CHECK(errors.first().at(1).toString().contains(QStringLiteral("Invalid XML")));
    CHECK(window.profile().track.path == fixture("tracks/timestamped.gpx"));
    CHECK(window.map_view()->route_length() == 5);

    // Back to the delta simulation through a profile.
    window.set_profile(quick_profile());
    CHECK(window.map_view()->route_length() == 0);
    CHECK(window.dashboard()->overrides_enabled());
    CHECK_FALSE(window.seek_slider()->isEnabled());
}

TEST_CASE("the main window replays a log to its end and records a session",
          "[app][replay][integration]") {
    nmeasim::app::MainWindow window;
    window.map_view()->cache()->set_online(false);
    window.set_profile(quick_profile());
    REQUIRE(window.load_log(fixture("logs/plain.nmea")));
    CHECK(window.profile().mode == nmeasim::io::SimulationMode::Replay);
    // The fixture holds 32 sentences timed from 10:00:00.00 to 10:00:01.50, 1500 ms.
    CHECK(window.seek_slider()->maximum() == 1500);
    CHECK(window.map_view()->route_length() == 0);

    window.step_action()->trigger();
    CHECK(window.runner().is_paused());
    CHECK(window.runner().sentences_emitted() == 1);
    // The first sentence of the fixture is an RMC with a speed of 6.5 kn.
    CHECK(window.runner().simulation()->state().navigation.speed_over_ground_kn == Approx(6.5));

    QSignalSpy stopped(&window.runner(), &nmeasim::io::SimulationRunner::stopped);
    window.runner().resume();
    REQUIRE(wait_until([&] { return stopped.count() == 1; }, 5000));
    CHECK(window.runner().sentences_emitted() == 32);
    CHECK_FALSE(window.is_running());

    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.log"));
    window.set_profile(quick_profile());
    CHECK_FALSE(window.record_action()->isChecked());
    REQUIRE(window.set_recording(path));
    CHECK(window.record_action()->isChecked());
    CHECK(window.runner().recording_path() == path);
    window.start();
    REQUIRE(wait_until([&] { return window.runner().sentences_emitted() >= 20; }, 5000));
    window.stop();
    const auto emitted = window.runner().sentences_emitted();
    // Unticking the action stops the recording.
    window.record_action()->setChecked(false);
    CHECK_FALSE(window.runner().is_recording());

    QFile reader(path);
    REQUIRE(reader.open(QIODevice::ReadOnly | QIODevice::Text));
    std::string error;
    const auto log = nmeasim::core::log::parse_log(reader.readAll().toStdString(), {}, &error);
    REQUIRE(log.has_value());
    CHECK(log->entries.size() == static_cast<std::size_t>(emitted));

    QSignalSpy errors(&window, &nmeasim::app::MainWindow::error_reported);
    window.start();
    CHECK_FALSE(window.set_recording(directory.filePath(QStringLiteral("no/dir/x.log"))));
    CHECK(errors.count() == 1);
    CHECK_FALSE(window.record_action()->isChecked());
    window.stop();
}

TEST_CASE("engine tiles on the dashboard drive the engines of the delta source",
          "[app][integration]") {
    nmeasim::app::MainWindow window;
    window.set_profile(quick_profile());
    // The default profile has a port and a starboard engine.
    REQUIRE(window.dashboard()->engine_count() == 2);
    auto* tile = window.dashboard()->engine_tile(1);
    REQUIRE(tile != nullptr);
    CHECK(tile->label->text() == QStringLiteral("Starboard engine"));
    CHECK(tile->running_check->isChecked());
    tile->rpm_spin->setValue(2500.0);
    tile->running_check->setChecked(false);
    auto* source = dynamic_cast<sim::DeltaSource*>(&window.runner().simulation()->source());
    REQUIRE(source != nullptr);
    CHECK(source->current().engines[1].revolutions_rpm == Approx(2500.0));
    CHECK_FALSE(source->current().engines[1].running);

    // A profile with one engine rebuilds the tiles; a track disables them.
    auto profile = quick_profile();
    profile.delta.seed.engines = {{"Main", true, 900.0, 60.0}};
    window.set_profile(profile);
    REQUIRE(window.dashboard()->engine_count() == 1);
    CHECK(window.dashboard()->engine_tile(0)->label->text() == QStringLiteral("Main"));
    CHECK(window.dashboard()->engine_tile(0)->rpm_spin->isEnabled());
    REQUIRE(window.load_track(fixture("tracks/timestamped.gpx")));
    CHECK_FALSE(window.dashboard()->engine_tile(0)->rpm_spin->isEnabled());
}
