#pragma once

#include <QByteArray>
#include <QSettings>
#include <QString>

namespace nmeasim::app {

/// Persistent application preferences (not the simulation profile), stored with QSettings in
/// the platform's native location.
class AppSettings {
public:
    [[nodiscard]] QString last_profile_path() const;
    void set_last_profile_path(const QString& path);

    [[nodiscard]] QByteArray window_geometry() const;
    [[nodiscard]] QByteArray window_state() const;
    void save_window(const QByteArray& geometry, const QByteArray& state);

    [[nodiscard]] bool autostart() const;
    void set_autostart(bool enabled);

    [[nodiscard]] bool map_online() const;
    void set_map_online(bool online);
    [[nodiscard]] QString map_tile_url() const;
    void set_map_tile_url(const QString& url);
    [[nodiscard]] int map_zoom() const;
    void set_map_zoom(int zoom);

    /// Directory where profiles are stored by default.
    [[nodiscard]] static QString profiles_directory();
    /// Directory where downloaded map tiles are kept.
    [[nodiscard]] static QString tile_cache_directory();

private:
    QSettings settings_;
};

}  // namespace nmeasim::app
