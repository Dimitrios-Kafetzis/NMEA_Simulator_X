#include "io/event_loop.hpp"
#include "main_window.hpp"
#include "map/map_widget.hpp"
#include "map/tile_cache.hpp"
#include "map/tile_math.hpp"
#include "widgets/dashboard_widget.hpp"

#include <nmeasim/core/simulation/delta_source.hpp>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWheelEvent>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using nmeasim::core::geo::Position;
using nmeasim::test::wait_until;
namespace map = nmeasim::app::map;

namespace {

void write_tile(const QString& directory, map::TileKey key, QColor color) {
    QImage image(map::kTileSize, map::kTileSize, QImage::Format_RGB32);
    image.fill(color);
    QDir().mkpath(QStringLiteral("%1/%2/%3").arg(directory).arg(key.zoom).arg(key.x));
    REQUIRE(image.save(
        QStringLiteral("%1/%2/%3/%4.png").arg(directory).arg(key.zoom).arg(key.x).arg(key.y)));
}

}  // namespace

TEST_CASE("tile arithmetic matches the slippy map convention", "[app][map]") {
    // Known reference values for the OSM scheme.
    const auto athens = map::tile_coordinates({37.9838, 23.7275}, 12);
    CHECK(static_cast<int>(athens.x()) == 2317);
    CHECK(static_cast<int>(athens.y()) == 1580);
    CHECK(map::tile_at(map::tile_coordinates({0.0, 0.0}, 1), 1) == map::TileKey{1, 1, 1});
    CHECK(map::tile_at(map::tile_coordinates({0.0, -180.0}, 1), 1) == map::TileKey{1, 0, 1});
    CHECK(map::tile_at(QPointF(-0.5, 0.5), 1) == map::TileKey{1, 1, 0});

    for (const Position position : {Position{37.9838, 23.7275}, Position{-33.86, 151.21},
                                    Position{64.13, -21.9}, Position{0.0, 0.0}}) {
        for (const int zoom : {1, 5, 12, 19}) {
            const auto back = map::position_of_pixel(map::pixel_coordinates(position, zoom), zoom);
            CHECK(back.latitude_deg == Approx(position.latitude_deg).margin(1e-6));
            CHECK(back.longitude_deg == Approx(position.longitude_deg).margin(1e-6));
        }
    }
    CHECK(map::parent_of({12, 2317, 1583}) == map::TileKey{11, 1158, 791});
    CHECK(map::metres_per_pixel(0.0, 0) == Approx(156543.03).epsilon(1e-4));
    CHECK(map::metres_per_pixel(60.0, 10) == Approx(76.437).epsilon(1e-3));
}

TEST_CASE("the tile cache serves tiles from disk and stays silent offline", "[app][map]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    map::TileCache cache(directory.path());
    cache.set_online(false);
    const map::TileKey key{5, 18, 12};
    CHECK_FALSE(cache.is_cached(key));
    CHECK_FALSE(cache.tile(key).has_value());
    CHECK(cache.pending_downloads() == 0);

    write_tile(directory.path(), key, Qt::blue);
    CHECK(cache.is_cached(key));
    const auto tile = cache.tile(key);
    REQUIRE(tile.has_value());
    CHECK(tile->width() == map::kTileSize);
    CHECK(cache.disk_size() > 0);

    // Once in memory the file is not needed any more.
    REQUIRE(QFile::remove(QStringLiteral("%1/5/18/12.png").arg(directory.path())));
    CHECK(cache.tile(key).has_value());

    cache.clear();
    CHECK_FALSE(cache.is_cached(key));
    CHECK(cache.disk_size() == 0);
    CHECK(QDir(directory.path()).exists());
}

TEST_CASE("the map widget follows the vessel, converts coordinates and picks positions",
          "[app][map]") {
    QTemporaryDir directory;
    map::TileCache cache(directory.path());
    cache.set_online(false);
    map::MapWidget widget(&cache);
    widget.resize(512, 384);
    widget.set_zoom(10);
    CHECK(widget.zoom() == 10);
    CHECK(widget.follows_vessel());

    widget.set_vessel({37.9838, 23.7275}, 45.0, 47.0);
    CHECK(widget.center().latitude_deg == Approx(37.9838));
    CHECK(widget.center().longitude_deg == Approx(23.7275));
    const QPointF vessel_px = widget.point_of({37.9838, 23.7275});
    CHECK(vessel_px.x() == Approx(256.0));
    CHECK(vessel_px.y() == Approx(192.0));

    const Position corner = widget.position_at(QPointF(0, 0));
    CHECK(corner.latitude_deg > 37.9838);
    CHECK(corner.longitude_deg < 23.7275);
    const QPointF round_trip = widget.point_of(corner);
    CHECK(round_trip.x() == Approx(0.0).margin(1e-6));
    CHECK(round_trip.y() == Approx(0.0).margin(1e-6));

    // The track grows as the vessel moves and is bounded.
    widget.set_vessel({37.99, 23.74}, 45.0, 47.0);
    widget.set_vessel({38.0, 23.75}, 45.0, 47.0);
    CHECK(widget.track_length() == 3);
    widget.clear_track();
    CHECK(widget.track_length() == 0);

    // Double-click picks the position under the cursor.
    QSignalSpy picked(&widget, &map::MapWidget::position_picked);
    QMouseEvent double_click(QEvent::MouseButtonDblClick, QPointF(256, 192), QPointF(256, 192),
                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &double_click);
    REQUIRE(picked.count() == 1);
    const auto chosen = picked.first().first().value<Position>();
    CHECK(chosen.latitude_deg == Approx(38.0).margin(1e-6));
    CHECK(chosen.longitude_deg == Approx(23.75).margin(1e-6));

    // Dragging switches follow mode off; Home switches it back on and recentres.
    QSignalSpy follow(&widget, &map::MapWidget::follow_changed);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(100, 100), QPointF(100, 100),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, QPointF(150, 120), QPointF(150, 120), Qt::LeftButton,
                     Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(150, 120), QPointF(150, 120),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);
    QApplication::sendEvent(&widget, &move);
    QApplication::sendEvent(&widget, &release);
    CHECK_FALSE(widget.follows_vessel());
    CHECK(follow.count() == 1);
    CHECK(widget.center().longitude_deg < 23.75);
    QKeyEvent home(QEvent::KeyPress, Qt::Key_Home, Qt::NoModifier);
    QApplication::sendEvent(&widget, &home);
    CHECK(widget.follows_vessel());
    CHECK(widget.center().longitude_deg == Approx(23.75));

    // The wheel zooms; keys zoom too and the range is clamped.
    QWheelEvent wheel(QPointF(256, 192), QPointF(256, 192), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &wheel);
    CHECK(widget.zoom() == 11);
    widget.set_zoom(99);
    CHECK(widget.zoom() == map::kMaxZoom);
    widget.set_zoom(0);
    CHECK(widget.zoom() == map::kMinZoom);
    CHECK(widget.scale_m_per_px() > 0.0);
}

TEST_CASE("the map widget paints cached tiles and placeholders", "[app][map]") {
    QTemporaryDir directory;
    map::TileCache cache(directory.path());
    cache.set_online(false);
    map::MapWidget widget(&cache);
    widget.resize(300, 300);
    widget.set_follow_vessel(false);
    widget.set_zoom(3);
    widget.set_center({0.0, 0.0});
    // The tile covering the centre at zoom 3 is (4, 4) (just south-east of 0,0). Paint it
    // red and its neighbour's parent green; the rest stays as placeholder grey.
    write_tile(directory.path(), {3, 4, 4}, Qt::red);
    write_tile(directory.path(), {2, 1, 2}, Qt::green);

    QImage image(widget.size(), QImage::Format_ARGB32);
    widget.render(&image);
    const QColor south_east = image.pixelColor(200, 200);
    CHECK(south_east.red() > 200);
    CHECK(south_east.green() < 60);
    // (3, 3, 4) has no tile of its own, so the parent (2, 1, 2) covers it in green.
    const QColor south_west = image.pixelColor(100, 200);
    CHECK(south_west.green() > 200);
    CHECK(south_west.red() < 60);
    // (3, 4, 3) has neither, so it shows the placeholder.
    const QColor north_east = image.pixelColor(200, 100);
    CHECK(north_east.red() == north_east.green());
    CHECK(north_east.red() > 150);
}

TEST_CASE("the main window moves the vessel when the map picks a position",
          "[app][map][integration]") {
    nmeasim::app::MainWindow window;
    window.map_view()->cache()->set_online(false);
    window.move_vessel({-33.86, 151.21});
    CHECK(window.profile().delta.seed.navigation.position.latitude_deg == Approx(-33.86));
    auto* source = dynamic_cast<nmeasim::core::simulation::DeltaSource*>(
        &window.runner().simulation()->source());
    REQUIRE(source != nullptr);
    CHECK(source->current().navigation.position.longitude_deg == Approx(151.21));
    CHECK(window.map_view()->center().latitude_deg == Approx(-33.86));
    REQUIRE(window.map_view()->vessel_position().has_value());
    CHECK(window.map_view()->vessel_position()->longitude_deg == Approx(151.21));
}

TEST_CASE("the map widget draws a loaded route under the sailed track", "[app][map]") {
    QTemporaryDir directory;
    map::TileCache cache(directory.path());
    cache.set_online(false);
    map::MapWidget widget(&cache);
    widget.resize(300, 300);
    widget.set_follow_vessel(false);
    widget.set_zoom(10);
    widget.set_center({37.95, 23.65});
    CHECK(widget.route_length() == 0);
    widget.set_route({{37.90, 23.60}, {37.95, 23.65}, {38.00, 23.70}});
    CHECK(widget.route_length() == 3);

    QImage image(widget.size(), QImage::Format_ARGB32);
    widget.render(&image);
    // The route passes through the centre; a green pixel is found on it.
    bool green_found = false;
    for (int y = 140; y < 160 && !green_found; ++y) {
        for (int x = 140; x < 160 && !green_found; ++x) {
            const QColor color = image.pixelColor(x, y);
            green_found = color.green() > color.red() + 40 && color.green() > color.blue() + 40;
        }
    }
    CHECK(green_found);

    widget.clear_route();
    CHECK(widget.route_length() == 0);
}

TEST_CASE("the map widget picks a destination with Shift+click and draws it", "[app][map]") {
    QTemporaryDir directory;
    map::TileCache cache(directory.path());
    cache.set_online(false);
    map::MapWidget widget(&cache);
    widget.resize(300, 300);
    widget.set_follow_vessel(false);
    widget.set_zoom(10);
    widget.set_center({37.95, 23.65});
    CHECK_FALSE(widget.destination().has_value());

    QSignalSpy picked(&widget, &map::MapWidget::destination_picked);
    QSignalSpy moved(&widget, &map::MapWidget::position_picked);
    QMouseEvent shift_click(QEvent::MouseButtonPress, QPointF(150, 150), QPointF(150, 150),
                            Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    QApplication::sendEvent(&widget, &shift_click);
    REQUIRE(picked.count() == 1);
    CHECK(moved.count() == 0);
    const auto chosen = picked.first().first().value<Position>();
    CHECK(chosen.latitude_deg == Approx(37.95).margin(1e-6));
    CHECK(chosen.longitude_deg == Approx(23.65).margin(1e-6));

    widget.set_vessel({37.90, 23.60}, 45.0, 45.0);
    widget.set_destination(chosen, Position{37.90, 23.60});
    REQUIRE(widget.destination().has_value());
    QImage image(widget.size(), QImage::Format_ARGB32);
    widget.render(&image);
    // The magenta diamond sits at the centre.
    const QColor centre = image.pixelColor(150, 150);
    CHECK(centre.red() > 150);
    CHECK(centre.blue() > 120);
    CHECK(centre.green() < 80);

    widget.set_destination(std::nullopt, std::nullopt);
    CHECK_FALSE(widget.destination().has_value());
}

TEST_CASE("the main window steers for a destination picked on the map", "[app][map][integration]") {
    nmeasim::app::MainWindow window;
    window.map_view()->cache()->set_online(false);
    CHECK_FALSE(window.clear_destination_action()->isEnabled());
    emit window.map_view()->destination_picked(Position{37.7466, 23.4275});
    REQUIRE(window.profile().delta.seed.destination.has_value());
    CHECK(window.profile().delta.seed.destination->name == "WPT");
    CHECK(window.profile().delta.seed.destination->origin.latitude_deg == Approx(37.9838));
    const auto& state = window.runner().simulation()->state();
    REQUIRE(state.destination.has_value());
    CHECK(state.destination->position.longitude_deg == Approx(23.4275));
    REQUIRE(window.map_view()->destination().has_value());
    CHECK(window.clear_destination_action()->isEnabled());
    CHECK(window.dashboard()->destination_text().startsWith(QStringLiteral("WPT: bearing 225.")));

    // The autopilot sentences appear in the stream once running.
    window.set_profile(window.profile());
    window.runner().step();
    bool apb_seen = false;
    for (const auto& sentence : window.runner().simulation()->scheduler().encode_all(
             window.runner().simulation()->state())) {
        apb_seen = apb_seen || sentence.id == "APB";
    }
    CHECK(apb_seen);
    window.stop();

    window.set_destination({38.0, 23.8}, QStringLiteral("HOME"));
    CHECK(window.profile().delta.seed.destination->name == "HOME");
    window.clear_destination_action()->trigger();
    CHECK_FALSE(window.profile().delta.seed.destination.has_value());
    CHECK_FALSE(window.runner().simulation()->state().destination.has_value());
    CHECK_FALSE(window.map_view()->destination().has_value());
    CHECK(window.dashboard()->destination_text() == QStringLiteral("None"));
}
