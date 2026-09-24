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

#include <optional>

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
/// | `map/tile_attribution` | `map_tile_attribution`, `set_map_tile_attribution` | unset |
/// | `map/zoom` | `map_zoom`, `set_map_zoom` | `12` |
/// | `map/cache_directory` | `map_cache_directory`, `set_map_cache_directory` | empty |
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
    /// Returns the attribution the map draws for the tile server (key `map/tile_attribution`).
    ///
    /// The application has no control for this key; it is edited in the settings store,
    /// usually together with `map/tile_url`.
    ///
    /// @return The stored text, empty to draw no attribution; `std::nullopt` when the key was
    ///   never set, in which case the map credits OpenStreetMap for the OpenStreetMap tile
    ///   servers and draws nothing for any other server (see `map::default_attribution`).
    [[nodiscard]] std::optional<QString> map_tile_attribution() const;
    /// Stores the attribution the map draws for the tile server (key `map/tile_attribution`).
    ///
    /// @param attribution Text to draw, empty to draw none; `std::nullopt` removes the key, so
    ///   that the map returns to its default at the next start.
    void set_map_tile_attribution(const std::optional<QString>& attribution);
    /// Returns the map zoom level of the last session (key `map/zoom`).
    ///
    /// @return A whole slippy map zoom level as stored, not checked here; 12 when it was never
    ///   set. The map clamps it to its range of [1, 19].
    [[nodiscard]] int map_zoom() const;
    /// Stores the map zoom level (key `map/zoom`).
    ///
    /// @param zoom A whole slippy map zoom level, stored unchecked.
    void set_map_zoom(int zoom);
    /// Returns the directory chosen for the map tile cache (key `map/cache_directory`).
    ///
    /// The application has no control for this key; it is edited in the settings store. The
    /// tests set it to a temporary directory.
    ///
    /// @return The directory as stored, under which `tile_cache_directory` keeps its `tiles`
    ///   sub-directory; empty when it was never set, in which case the platform's cache
    ///   directory is used.
    [[nodiscard]] QString map_cache_directory() const;
    /// Stores the directory for the map tile cache (key `map/cache_directory`).
    ///
    /// Takes effect at the next start, when `MainWindow` opens the tile cache.
    ///
    /// @param directory An existing or creatable directory, stored unchecked; empty returns
    ///   to the platform's cache directory.
    void set_map_cache_directory(const QString& directory);

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
    /// Reads `map/cache_directory` from the settings store of the application and
    /// organisation names set at the time of the call.
    ///
    /// @return The `tiles` sub-directory of `map/cache_directory` when that key is set, else
    ///   of the application's cache directory (`QStandardPaths::CacheLocation`), on Linux
    ///   `~/.cache/NMEASimulatorX/NMEASimulatorX/tiles`. The `tiles` level keeps
    ///   *Clear map tile cache*, which deletes this directory, away from the other contents
    ///   of a chosen directory. A failure to create it is ignored; the path is returned
    ///   anyway.
    [[nodiscard]] static QString tile_cache_directory();
    /// Returns the absolute path that a file path of a profile names.
    ///
    /// The file dialogs use it, so that they find a file named relative to the profile file
    /// where `SimulationRunner` finds it.
    ///
    /// @param profile_directory The directory that the relative paths of the profile are
    ///   relative to, `io::Profile::base_directory`; empty for a profile without a file, whose
    ///   relative paths are relative to the working directory.
    /// @param path A track, log or output file path as written in the profile; may be empty.
    /// @return `path` made absolute against `profile_directory` and cleaned; empty when `path`
    ///   is empty.
    [[nodiscard]] static QString resolve_profile_path(const QString& profile_directory,
                                                      const QString& path);
    /// Returns the folder that a file dialog for a file path of a profile starts in.
    ///
    /// @param profile_directory As for `resolve_profile_path`.
    /// @param path As for `resolve_profile_path`.
    /// @return The folder that contains the path resolved by `resolve_profile_path`, whether
    ///   or not it exists; the documents folder (`QStandardPaths::DocumentsLocation`) when
    ///   `path` is empty.
    [[nodiscard]] static QString dialog_directory(const QString& profile_directory,
                                                  const QString& path);

private:
    /// The native settings store of the application, opened by the default `QSettings`
    /// constructor from the organisation and application names.
    QSettings settings_;
};

}  // namespace nmeasim::app
