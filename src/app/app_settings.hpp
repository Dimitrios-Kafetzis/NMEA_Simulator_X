// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// `AppSettings`, the desktop application's preferences stored with `QSettings`.
///
/// The preferences are separate from the simulation profile: they remember the window, the
/// map and the look between sessions, while everything that shapes the simulated data lives in
/// the profile file.
///
/// @see docs/reference/desktop-app.md, section "Preferences".

#pragma once

#include <QByteArray>
#include <QSettings>
#include <QString>

namespace nmeasim::app {

/// Persistent application preferences (not the simulation profile), stored with `QSettings`
/// in the platform's native location.
///
/// Every accessor goes straight to `QSettings`, which makes a change visible at once to the
/// other instances in the process (`main` and `MainWindow` each keep one) and writes it to
/// storage later. The keys and their defaults:
///
/// | Key | Accessors | Default |
/// | --- | --- | --- |
/// | `profile/last_path` | `last_profile_path`, `set_last_profile_path` | empty |
/// | `window/geometry` | `window_geometry`, `save_window` | empty |
/// | `window/state` | `window_state`, `save_window` | empty |
/// | `simulation/autostart` | `autostart`, `set_autostart` | `false` |
/// | `map/online` | `map_online`, `set_map_online` | `true` |
/// | `map/tile_url` | `map_tile_url`, `set_map_tile_url` | empty |
/// | `map/zoom` | `map_zoom`, `set_map_zoom` | `12` |
/// | `appearance/theme` | `theme`, `set_theme` | `night` |
///
/// The store is chosen by `QSettings` from the organisation and application names that
/// `main` sets (`NMEASimulatorX` for both) and, on macOS, the organisation domain: the
/// registry key `HKEY_CURRENT_USER\Software\NMEASimulatorX\NMEASimulatorX` on Windows,
/// `~/Library/Preferences/io.github.dimitrios-kafetzis.NMEASimulatorX.plist` on macOS and
/// `~/.config/NMEASimulatorX/NMEASimulatorX.conf` on Linux.
///
/// Not copyable, because the `QSettings` member is not.
///
/// @note Construct it only after the application and organisation names are set; an instance
///   created earlier reads and writes a different store.
/// @see docs/reference/desktop-app.md, section "Preferences".
class AppSettings {
public:
    /// Returns the profile to reopen at the next start (key `profile/last_path`).
    ///
    /// @return The path of the profile last loaded or saved; empty when there is none.
    [[nodiscard]] QString last_profile_path() const;
    /// Stores the profile to reopen at the next start (key `profile/last_path`).
    ///
    /// @param path Path of a profile file, stored as given; empty forgets the last one.
    void set_last_profile_path(const QString& path);

    /// Returns the saved window geometry (key `window/geometry`).
    ///
    /// @return The bytes of `QWidget::saveGeometry`; empty before the window was first closed,
    ///   which `QWidget::restoreGeometry` rejects, leaving the default size.
    [[nodiscard]] QByteArray window_geometry() const;
    /// Returns the saved toolbar and dock layout (key `window/state`).
    ///
    /// @return The bytes of `QMainWindow::saveState`; empty before the window was first
    ///   closed, which makes `QMainWindow::restoreState` fail and the window lay out its docks
    ///   for a first start.
    [[nodiscard]] QByteArray window_state() const;
    /// Stores the window geometry and the dock layout (keys `window/geometry` and
    /// `window/state`).
    ///
    /// @param geometry The bytes of `QWidget::saveGeometry`.
    /// @param state The bytes of `QMainWindow::saveState`.
    void save_window(const QByteArray& geometry, const QByteArray& state);

    /// Returns whether the simulation starts as soon as the window opens (key
    /// `simulation/autostart`).
    ///
    /// @return The stored flag; false when it was never set.
    [[nodiscard]] bool autostart() const;
    /// Stores whether the simulation starts as soon as the window opens (key
    /// `simulation/autostart`).
    ///
    /// @param enabled True to start on launch.
    void set_autostart(bool enabled);

    /// Returns whether missing map tiles are downloaded (key `map/online`).
    ///
    /// @return The stored flag; true when it was never set, so a first start downloads the
    ///   tiles it shows.
    [[nodiscard]] bool map_online() const;
    /// Stores whether missing map tiles are downloaded (key `map/online`).
    ///
    /// @param online True to download, false to use the tiles on disk only.
    void set_map_online(bool online);
    /// Returns the tile server URL template (key `map/tile_url`).
    ///
    /// The application has no control for this key; it is edited in the settings store.
    ///
    /// @return A URL with `{z}`, `{x}` and `{y}` placeholders; empty when it was never set,
    ///   in which case the tile cache keeps its OpenStreetMap default.
    [[nodiscard]] QString map_tile_url() const;
    /// Stores the tile server URL template (key `map/tile_url`).
    ///
    /// @param url A URL with `{z}`, `{x}` and `{y}` placeholders; empty returns to the
    ///   OpenStreetMap server at the next start.
    void set_map_tile_url(const QString& url);
    /// Returns the map zoom level of the last session (key `map/zoom`).
    ///
    /// @return A whole slippy map zoom level as stored, not checked here; 12 when it was never
    ///   set. The map clamps it to its range of [1, 19].
    [[nodiscard]] int map_zoom() const;
    /// Stores the map zoom level (key `map/zoom`).
    ///
    /// @param zoom A whole slippy map zoom level, stored unchecked.
    void set_map_zoom(int zoom);

    /// Returns the look chosen under *View → Theme* (key `appearance/theme`).
    ///
    /// @return `system`, `night` or `day` as stored; `night` when it was never set. Any other
    ///   stored text is returned as is and read as `night` by `theme::mode_from_string`.
    [[nodiscard]] QString theme() const;
    /// Stores the look chosen under *View → Theme* (key `appearance/theme`).
    ///
    /// @param theme `system`, `night` or `day`, as `theme::to_string` writes them; stored
    ///   unchecked.
    void set_theme(const QString& theme);

    /// Returns the directory the profile dialogs offer by default, creating it when missing.
    ///
    /// @return The `profiles` sub-directory of the application's configuration directory
    ///   (`QStandardPaths::AppConfigLocation`). A failure to create it is ignored; the path is
    ///   returned anyway.
    [[nodiscard]] static QString profiles_directory();
    /// Returns the directory where downloaded map tiles are kept, creating it when missing.
    ///
    /// @return The `tiles` sub-directory of the application's cache directory
    ///   (`QStandardPaths::CacheLocation`), on Linux
    ///   `~/.cache/NMEASimulatorX/NMEASimulatorX/tiles`. A failure to create it is ignored;
    ///   the path is returned anyway.
    [[nodiscard]] static QString tile_cache_directory();

private:
    /// The native settings store of the application, opened by the default `QSettings`
    /// constructor from the organisation and application names.
    QSettings settings_;
};

}  // namespace nmeasim::app
