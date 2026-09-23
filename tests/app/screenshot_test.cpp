// Captures the screenshots used by the documentation and the AppStream metadata: the main
// window in the night theme (main-window.png) and in the daylight theme
// (main-window-day.png). The test is hidden, so ctest never runs it; generate the images with
//
//   export NMEASIM_SCREENSHOT_DIR=docs/assets/screenshots
//   build/<preset>/tests/nmeasim_app_tests "[.screenshot]"
#include "io/event_loop.hpp"
#include "main_window.hpp"
#include "theme/theme.hpp"

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/io/profile/profile.hpp>

#include <QByteArray>
#include <QDir>
#include <QPixmap>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <utility>

TEST_CASE("capture the main window for the documentation", "[.screenshot]") {
    const QByteArray directory = qgetenv("NMEASIM_SCREENSHOT_DIR");
    if (directory.isEmpty()) {
        SKIP("Set NMEASIM_SCREENSHOT_DIR to write the screenshots");
    }

    using nmeasim::app::theme::Mode;
    for (const auto& [mode, name] :
         {std::pair{Mode::Night, "main-window.png"}, std::pair{Mode::Day, "main-window-day.png"}}) {
        nmeasim::app::theme::Theme::instance().apply(mode);
        nmeasim::app::MainWindow window;
        auto profile = nmeasim::io::Profile::default_profile();
        nmeasim::core::model::Destination destination;
        destination.name = "AEGINA";
        destination.position = {37.7466, 23.4275};
        destination.origin = profile.delta.seed.navigation.position;
        profile.delta.seed.destination = destination;
        REQUIRE(window.set_profile(profile));
        window.resize(1440, 1000);
        window.show();
        window.start();
        // Let the dashboard, the console and the map tiles fill in.
        nmeasim::test::wait_until([] { return false; }, 6000);

        const QString path =
            QDir(QString::fromLocal8Bit(directory)).filePath(QString::fromLatin1(name));
        REQUIRE(window.grab().save(path));
        window.stop();
    }
}
