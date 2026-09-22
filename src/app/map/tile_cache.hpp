#pragma once

#include "tile_math.hpp"

#include <QCache>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

#include <deque>
#include <optional>

namespace nmeasim::app::map {

/// Serves map tiles from memory, then from a directory on disk, and finally by downloading
/// them from a tile server. Downloaded tiles are written to disk so that the map keeps
/// working without a network connection.
class TileCache : public QObject {
    Q_OBJECT

public:
    /// `directory` holds the on-disk cache as `<zoom>/<x>/<y>.png`.
    explicit TileCache(QString directory, QObject* parent = nullptr);

    /// URL with `{z}`, `{x}` and `{y}` placeholders.
    [[nodiscard]] QString url_template() const noexcept { return url_template_; }
    void set_url_template(const QString& url_template);

    /// When false no download is attempted; the disk cache is the only source.
    [[nodiscard]] bool online() const noexcept { return online_; }
    void set_online(bool online);

    void set_user_agent(const QString& user_agent) { user_agent_ = user_agent; }
    [[nodiscard]] QString directory() const noexcept { return directory_; }

    /// Returns the tile if it is in memory or on disk. Otherwise, when online, queues a
    /// download and returns nullopt; `tile_ready` fires once the tile is available.
    [[nodiscard]] std::optional<QPixmap> tile(const TileKey& key);
    /// Whether the tile can be served without a download.
    [[nodiscard]] bool is_cached(const TileKey& key) const;

    [[nodiscard]] int pending_downloads() const;
    /// Bytes used by the tiles on disk.
    [[nodiscard]] qint64 disk_size() const;
    /// Removes every tile from memory and disk.
    void clear();

signals:
    void tile_ready(const nmeasim::app::map::TileKey& key);
    void tile_failed(const nmeasim::app::map::TileKey& key, const QString& reason);

private:
    [[nodiscard]] QString path_of(const TileKey& key) const;
    [[nodiscard]] static QString memory_key(const TileKey& key);
    void enqueue(const TileKey& key);
    void start_next();
    void finish(const TileKey& key, class QNetworkReply* reply);

    QString directory_;
    QString url_template_;
    QString user_agent_;
    bool online_{true};
    QCache<QString, QPixmap> memory_;
    QNetworkAccessManager network_;
    QSet<QString> queued_;
    std::deque<TileKey> queue_;
    int active_{0};
};

}  // namespace nmeasim::app::map
