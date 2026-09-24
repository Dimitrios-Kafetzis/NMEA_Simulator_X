// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `AppSettings`: the `QSettings` keys and the default values read back when
/// a key was never written.
///
/// The keys are grouped by the part of the application they belong to (`profile/`, `window/`,
/// `simulation/`, `map/`, `appearance/`), which becomes a section of the INI file on Linux
/// and a sub-key of the registry key on Windows. They are part of the user's saved state:
/// renaming one loses what earlier versions stored under it.

#include "app_settings.hpp"

#include <QDir>
#include <QStandardPaths>

namespace nmeasim::app {

namespace {

/// Key of the profile reopened at the next start; see `AppSettings::last_profile_path`.
const auto kLastProfile = QStringLiteral("profile/last_path");
/// Key of the window size and position; see `AppSettings::window_geometry`.
const auto kGeometry = QStringLiteral("window/geometry");
/// Key of the toolbar and dock layout; see `AppSettings::window_state`.
const auto kState = QStringLiteral("window/state");
/// Key of the start-on-launch flag; see `AppSettings::autostart`.
const auto kAutostart = QStringLiteral("simulation/autostart");
/// Key of the tile download flag; see `AppSettings::map_online`.
const auto kMapOnline = QStringLiteral("map/online");
/// Key of the tile server URL template; see `AppSettings::map_tile_url`.
const auto kMapTileUrl = QStringLiteral("map/tile_url");
/// Key of the last map zoom level; see `AppSettings::map_zoom`.
const auto kMapZoom = QStringLiteral("map/zoom");
/// Key of the look chosen under *View → Theme*; see `AppSettings::theme`.
const auto kTheme = QStringLiteral("appearance/theme");

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

QString AppSettings::theme() const {
    return settings_.value(kTheme, QStringLiteral("night")).toString();
}

void AppSettings::set_theme(const QString& theme) {
    settings_.setValue(kTheme, theme);
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
