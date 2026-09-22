#include "main_window.hpp"

#include "io/event_loop.hpp"
#include "widgets/console_widget.hpp"
#include "widgets/dashboard_widget.hpp"
#include "widgets/outputs_widget.hpp"

#include <nmeasim/core/simulation/delta_source.hpp>

#include <QKeyEvent>
#include <QTemporaryDir>
#include <QTest>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

using Catch::Approx;
using namespace std::chrono_literals;
using nmeasim::test::wait_until;
namespace sim = nmeasim::core::simulation;

namespace {

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

}  // namespace

TEST_CASE("positions are formatted as degrees and decimal minutes", "[app]") {
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
