// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Map tile cache: tiles from memory, then from disk, then downloaded from a tile server.
///
/// `TileCache` keeps the map usable without a network connection: every tile it downloads
/// is written to disk, and with downloading switched off it serves the disk cache only. The
/// requests follow the OpenStreetMap tile usage policy (a `User-Agent` naming the
/// application, at most four downloads at a time, every tile cached, no repeated requests
/// for a tile that failed), as ADR 0010 decides.
///
/// @see https://operations.osmfoundation.org/policies/tiles/

#pragma once

#include "tile_math.hpp"

#include <QCache>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

#include <chrono>
#include <deque>
#include <optional>

namespace nmeasim::app::map {

/// Serves map tiles from memory, then from a directory on disk, and finally by downloading
/// them from a tile server.
///
/// Up to 512 decoded tiles stay in memory, the least recently used leaving first. On disk the
/// cache is a tree of PNG files, `<directory>/<zoom>/<x>/<y>.png`, which is unbounded and
/// emptied only by `clear`. Downloads go through a `QNetworkAccessManager` owned by the
/// cache: at most four run at a time and the rest wait in a first-in, first-out queue. Each
/// request carries the `User-Agent` set with `set_user_agent`, follows only redirects that
/// are no less safe (HTTPS stays HTTPS) and is aborted after 15 seconds without data. A
/// tile is never requested twice at once. A downloaded tile is written to disk before
/// `tile_ready` is emitted. Offline, no download is queued or running and `tile` reports
/// tiles that are neither in memory nor on disk as missing.
///
/// A failed download is remembered, so that repainting the map does not request the tile
/// again and again. A tile the server answered with HTTP 404 (Not Found) or 410 (Gone) is
/// not requested again for the lifetime of the cache, unless the tile server changes. After
/// any other failure (no network, a timeout, another HTTP error, a reply that is not an
/// image) the tile is not requested again before a delay has passed: 30 seconds after the
/// first failure, doubled after each further failure up to 10 minutes. Switching downloads
/// back on forgets these delays, so that the tiles are tried again at once.
///
/// @see MapWidget, which asks for the tiles covering its view on every paint.
class TileCache : public QObject {
    Q_OBJECT

public:
    /// Creates the cache on a directory, creating the directory if it does not exist.
    ///
    /// The cache starts online, with the OpenStreetMap standard tile server
    /// `https://tile.openstreetmap.org/{z}/{x}/{y}.png` as URL template and the `User-Agent`
    /// `NMEASimulatorX/<version> (+https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X)`,
    /// which names the application, its version and its project page, as the tile usage
    /// policy asks.
    ///
    /// @param directory Root of the on-disk cache, which holds the tiles as
    ///     `<zoom>/<x>/<y>.png`.
    /// @param parent Qt parent that owns the cache; null leaves ownership with the caller.
    explicit TileCache(QString directory, QObject* parent = nullptr);

    /// Returns the tile server URL template.
    ///
    /// @return A URL whose `{z}`, `{x}` and `{y}` placeholders are replaced by the zoom
    ///     level, the column and the row of a tile.
    [[nodiscard]] QString url_template() const noexcept { return url_template_; }
    /// Sets the tile server URL template used by later downloads.
    ///
    /// Tiles already cached, in memory or on disk, stay and are served as before, even when
    /// they came from another server. When the template changes, the downloads running from
    /// the previous server are aborted without a signal, and the failures remembered for it
    /// are forgotten, HTTP 404 included.
    ///
    /// @param url_template URL with `{z}`, `{x}` and `{y}` placeholders; surrounding white
    ///     space is removed. An empty or blank template is ignored and the current one kept,
    ///     so an unset preference keeps the OpenStreetMap default.
    void set_url_template(const QString& url_template);

    /// Returns whether missing tiles are downloaded.
    ///
    /// @return `true` when missing tiles are downloaded, `false` when the disk cache is the
    ///     only source.
    [[nodiscard]] bool online() const noexcept { return online_; }
    /// Switches downloading on or off.
    ///
    /// Switching off empties the download queue and aborts the downloads already running,
    /// which then cache nothing and emit no signal. Switching back on forgets the retry
    /// delays of tiles that failed, so that they are requested again at the next `tile`
    /// call; tiles the server reported as not found stay excluded.
    ///
    /// @param online `true` to download missing tiles, `false` to serve the disk cache only.
    void set_online(bool online);

    /// Sets the `User-Agent` header sent with every later tile request.
    ///
    /// @param user_agent Header value. The tile usage policy asks for one that identifies
    ///     the application and a way to contact its authors.
    void set_user_agent(const QString& user_agent) { user_agent_ = user_agent; }
    /// Returns the root directory of the on-disk cache, as given to the constructor.
    ///
    /// @return The directory path.
    [[nodiscard]] QString directory() const noexcept { return directory_; }

    /// Returns a tile from memory or disk, or queues its download.
    ///
    /// A tile read from disk is kept in memory for the next call. When the tile is in
    /// neither and the cache is online, its download is queued (once, however often it is
    /// asked for) and `tile_ready` or `tile_failed` is emitted once the reply finishes. No
    /// download is queued for a tile whose earlier download failed while its retry delay
    /// runs, nor ever again for a tile the server reported as not found.
    ///
    /// @param key Tile to return.
    /// @return The tile image, or `std::nullopt` when it is neither in memory nor on disk.
    [[nodiscard]] std::optional<QPixmap> tile(const TileKey& key);
    /// Returns whether a tile can be served without a download.
    ///
    /// Unlike `tile`, this decodes nothing and never queues a download.
    ///
    /// @param key Tile to look up.
    /// @return `true` when the tile is in memory or its file exists on disk.
    [[nodiscard]] bool is_cached(const TileKey& key) const;

    /// Returns the number of downloads queued or running.
    ///
    /// @return Queued plus running downloads; 0 when nothing is in flight.
    [[nodiscard]] int pending_downloads() const;
    /// Returns the space the tiles on disk take up.
    ///
    /// Walks the whole directory tree, so its cost grows with the number of cached tiles.
    ///
    /// @return Total size in bytes of the PNG files under `directory()`.
    [[nodiscard]] qint64 disk_size() const;
    /// Removes every tile from memory and disk, empties the download queue and aborts the
    /// downloads already running.
    ///
    /// The directory itself is recreated empty. An aborted download caches nothing and emits
    /// no signal. The remembered failures are kept.
    void clear();

    /// Sets the delay before a tile that failed to download is requested again.
    ///
    /// The delay applies to the first failure of a tile; each further failure of the same
    /// tile doubles it, up to 10 minutes or `first_delay`, whichever is longer. Tiles the
    /// server reported as not found are never retried, whatever the delay. Delays already
    /// running are not changed.
    ///
    /// @param first_delay Delay after the first failure; 30 seconds by default. Zero or
    ///     negative retries at the next `tile` call.
    void set_retry_delay(std::chrono::milliseconds first_delay) noexcept {
        retry_delay_ = first_delay;
    }

signals:
    /// Emitted when a downloaded tile has been decoded and cached.
    ///
    /// Emitted when the network reply finishes, after the tile has been written to disk and
    /// put in memory. The next `tile` call for `key` returns the image.
    ///
    /// @param key Tile that became available.
    void tile_ready(const nmeasim::app::map::TileKey& key);
    /// Emitted when a download fails: a network or HTTP error, a timeout, or a reply that is
    /// not an image.
    ///
    /// Nothing is cached for the tile. It is not requested again before its retry delay has
    /// passed, and never again when the server answered HTTP 404 or 410. Not emitted for a
    /// download aborted by `set_online`, `clear` or `set_url_template`.
    ///
    /// @param key Tile that could not be downloaded.
    /// @param reason Error text of the network reply, or the translated "not an image" when
    ///     the body could not be decoded.
    void tile_failed(const nmeasim::app::map::TileKey& key, const QString& reason);

private:
    /// Returns the file a tile is stored in on disk.
    ///
    /// @param key Tile to locate.
    /// @return `<directory>/<zoom>/<x>/<y>.png`, whether or not the file exists.
    [[nodiscard]] QString path_of(const TileKey& key) const;
    /// Returns the name of a tile in the memory cache and the download bookkeeping.
    ///
    /// @param key Tile to name.
    /// @return `<zoom>/<x>/<y>`.
    [[nodiscard]] static QString memory_key(const TileKey& key);
    /// Queues the download of a tile unless it is already queued or running, then starts
    /// downloads while fewer than four are running.
    ///
    /// @param key Tile to download.
    void enqueue(const TileKey& key);
    /// Starts queued downloads, oldest first, until four are running or the queue is empty.
    void start_next();
    /// Handles a finished download: caches the tile on disk and in memory, emits
    /// `tile_ready`, or records the failure and emits `tile_failed`, and starts the next
    /// queued download.
    ///
    /// A failure to write the file is ignored; the tile is then cached in memory only.
    ///
    /// @param key Tile the reply belongs to.
    /// @param reply Finished reply, owned by `network_`; its deletion is scheduled with
    ///     `deleteLater`.
    void finish(const TileKey& key, QNetworkReply* reply);
    /// Remembers that a tile failed to download and when it may be requested again.
    ///
    /// @param id `memory_key` of the tile.
    /// @param permanent `true` when the server reported the tile as not found, which is
    ///     never retried; `false` to retry after the tile's next retry delay.
    void record_failure(const QString& id, bool permanent);
    /// Returns whether a tile may be queued for download now.
    ///
    /// @param id `memory_key` of the tile.
    /// @return `false` while the tile's retry delay runs and for a tile reported as not
    ///     found; `true` otherwise.
    [[nodiscard]] bool may_request(const QString& id) const;
    /// Aborts every running download without emitting a signal; the queued downloads stay.
    ///
    /// The aborted tiles are neither cached nor recorded as failed, so the next `tile` call
    /// for one of them queues it again.
    void abort_running();

    /// A tile that failed to download.
    struct Failure {
        /// Time from which the tile may be requested again; unused when `permanent`.
        std::chrono::steady_clock::time_point retry_at;
        /// Delay applied after the latest failure; the next failure doubles it.
        std::chrono::milliseconds delay{0};
        /// Whether the server reported the tile as not found, so that it is never retried.
        bool permanent{false};
    };

    /// Root directory of the on-disk cache.
    QString directory_;
    /// Tile server URL with `{z}`, `{x}` and `{y}` placeholders; never empty.
    QString url_template_;
    /// Value of the `User-Agent` header sent with every request.
    QString user_agent_;
    /// Whether missing tiles are downloaded; `false` serves the disk cache only.
    bool online_{true};
    /// Decoded tiles by `memory_key`, at most 512, evicting the least recently used.
    QCache<QString, QPixmap> memory_;
    /// Network access for the downloads, owned by the cache.
    QNetworkAccessManager network_;
    /// `memory_key` of every tile queued or running, so that a tile is requested only once
    /// at a time.
    QSet<QString> queued_;
    /// Tiles waiting for a download slot, oldest first.
    std::deque<TileKey> queue_;
    /// Running downloads by `memory_key`, at most four; the replies are owned by `network_`.
    QHash<QString, QNetworkReply*> running_;
    /// Failed downloads by `memory_key`, kept for the lifetime of the cache or until the
    /// tile server changes.
    QHash<QString, Failure> failures_;
    /// Delay before a tile is requested again after its first failure.
    std::chrono::milliseconds retry_delay_{std::chrono::seconds{30}};
};

}  // namespace nmeasim::app::map
