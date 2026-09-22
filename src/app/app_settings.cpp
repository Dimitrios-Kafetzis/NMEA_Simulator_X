#include "app_settings.hpp"

#include <QDir>
#include <QStandardPaths>

namespace nmeasim::app {

namespace {

const auto kLastProfile = QStringLiteral("profile/last_path");
const auto kGeometry = QStringLiteral("window/geometry");
const auto kState = QStringLiteral("window/state");
const auto kAutostart = QStringLiteral("simulation/autostart");
const auto kMapOnline = QStringLiteral("map/online");
const auto kMapTileUrl = QStringLiteral("map/tile_url");
const auto kMapZoom = QStringLiteral("map/zoom");

}  // namespace

QString AppSettings::last_profile_path() const {
    return settings_.value(kLastProfile).toString();
}

void AppSettings::set_last_profile_path(const QString& path) {
    settings_.setValue(kLastProfile, path);
}

QByteArray AppSettings::window_geometry() const {
    return settings_.value(kGeometry).toByteArray();
}

QByteArray AppSettings::window_state() const {
    return settings_.value(kState).toByteArray();
}

void AppSettings::save_window(const QByteArray& geometry, const QByteArray& state) {
    settings_.setValue(kGeometry, geometry);
    settings_.setValue(kState, state);
}

bool AppSettings::autostart() const {
    return settings_.value(kAutostart, false).toBool();
}

void AppSettings::set_autostart(bool enabled) {
    settings_.setValue(kAutostart, enabled);
}

bool AppSettings::map_online() const {
    return settings_.value(kMapOnline, true).toBool();
}

void AppSettings::set_map_online(bool online) {
    settings_.setValue(kMapOnline, online);
}

QString AppSettings::map_tile_url() const {
    return settings_.value(kMapTileUrl).toString();
}

void AppSettings::set_map_tile_url(const QString& url) {
    settings_.setValue(kMapTileUrl, url);
}

int AppSettings::map_zoom() const {
    return settings_.value(kMapZoom, 12).toInt();
}

void AppSettings::set_map_zoom(int zoom) {
    settings_.setValue(kMapZoom, zoom);
}

QString AppSettings::tile_cache_directory() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString directory = QDir(base).filePath(QStringLiteral("tiles"));
    QDir().mkpath(directory);
    return directory;
}

QString AppSettings::profiles_directory() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString directory = QDir(base).filePath(QStringLiteral("profiles"));
    QDir().mkpath(directory);
    return directory;
}

}  // namespace nmeasim::app
