// Captures the screenshot used by the documentation and the AppStream metadata. The test is
// hidden, so ctest never runs it; generate the image with
//
//   NMEASIM_SCREENSHOT_DIR=docs/assets/screenshots build/<preset>/tests/nmeasim_app_tests
//   "[.screenshot]"
#include "io/event_loop.hpp"
#include "main_window.hpp"

#include <nmeasim/io/profile/profile.hpp>

#include <QByteArray>
#include <QDir>
#include <QPixmap>
#include <QString>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("capture the main window for the documentation", "[.screenshot]") {
    const QByteArray directory = qgetenv("NMEASIM_SCREENSHOT_DIR");
    if (directory.isEmpty()) {
        SKIP("Set NMEASIM_SCREENSHOT_DIR to write the screenshot");
    }

    nmeasim::app::MainWindow window;
    REQUIRE(window.set_profile(nmeasim::io::Profile::default_profile()));
    window.resize(1440, 1000);
    window.show();
    window.start();
    // Let the dashboard, the console and the map tiles fill in.
    nmeasim::test::wait_until([] { return false; }, 6000);

    const QString path =
        QDir(QString::fromLocal8Bit(directory)).filePath(QStringLiteral("main-window.png"));
    REQUIRE(window.grab().save(path));
    window.stop();
}
