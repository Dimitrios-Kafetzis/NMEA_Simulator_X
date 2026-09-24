// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Hidden test that captures `nmeasim::app::MainWindow` for the documentation screenshots.
///
/// The test runs the default profile with a destination set (Aegina, so that the map and the
/// dashboard show a leg), waits six seconds for the dashboard, the console and the map tiles
/// to fill in, and saves the window at 1440 by 1000 pixels twice: in the night bridge theme as
/// `main-window.png` and in the daylight theme as `main-window-day.png`. The documentation site
/// and the AppStream metadata show both images from `docs/assets/screenshots/`.
///
/// The tag `[.screenshot]` starts with a dot, which hides the test: Catch2 runs it only when
/// the tag is named on the command line, so `ctest` never runs it. Without the environment
/// variable `NMEASIM_SCREENSHOT_DIR` the test skips itself. To regenerate the images from the
/// repository root, with the build directory of a CMake preset such as `dev`:
///
/// ```sh
/// export NMEASIM_SCREENSHOT_DIR=docs/assets/screenshots
/// build/dev/tests/nmeasim_app_tests "[.screenshot]"
/// ```
///
/// The window is rendered by the offscreen platform set up in `main.cpp`. `main.cpp` also
/// switches tile downloads off and gives the run an empty temporary tile cache, so this test
/// switches its window's downloads back on: the map tiles are fetched from the tile server on
/// every run, and the chart only appears with network access.

#include "io/event_loop.hpp"
#include "main_window.hpp"
#include "map/map_widget.hpp"
#include "map/tile_cache.hpp"
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
        window.map_view()->cache()->set_online(true);
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
        // The condition never holds: this only pumps the event loop for six seconds so that the
        // dashboard, the console and the map tile downloads fill in.
        nmeasim::test::wait_until([] { return false; }, 6000);

        const QString path =
            QDir(QString::fromLocal8Bit(directory)).filePath(QString::fromLatin1(name));
        REQUIRE(window.grab().save(path));
        window.stop();
    }
}
