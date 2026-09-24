// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Memory, disk and network lookup, download queue and disk writes of `TileCache`.

#include "tile_cache.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

namespace nmeasim::app::map {

namespace {

/// Capacity of the memory cache in tiles; 512 tiles of 256 by 256 pixels take about 128 MiB
/// as 32-bit pixmaps.
constexpr int kMemoryTiles{512};
/// Largest number of downloads running at once, kept low as the tile usage policy asks.
constexpr int kMaxConcurrentDownloads{4};
/// Time in milliseconds a download may go without receiving data before it is aborted.
constexpr int kTransferTimeoutMs{15000};

}  // namespace

TileCache::TileCache(QString directory, QObject* parent)
    : QObject(parent),
      directory_(std::move(directory)),
      url_template_(QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png")),
      user_agent_(QStringLiteral("NMEASimulatorX")),
      memory_(kMemoryTiles) {
    network_.setTransferTimeout(kTransferTimeoutMs);
    QDir().mkpath(directory_);
}

void TileCache::set_url_template(const QString& url_template) {
    if (url_template.trimmed().isEmpty()) {
        return;
    }
    url_template_ = url_template.trimmed();
}

void TileCache::set_online(bool online) {
    online_ = online;
    if (!online_) {
        queue_.clear();
        queued_.clear();
    }
}

QString TileCache::path_of(const TileKey& key) const {
    return QStringLiteral("%1/%2/%3/%4.png").arg(directory_).arg(key.zoom).arg(key.x).arg(key.y);
}

QString TileCache::memory_key(const TileKey& key) {
    return QStringLiteral("%1/%2/%3").arg(key.zoom).arg(key.x).arg(key.y);
}

std::optional<QPixmap> TileCache::tile(const TileKey& key) {
    const QString id = memory_key(key);
    if (const auto* cached = memory_.object(id)) {
        return *cached;
    }
    QPixmap pixmap;
    if (pixmap.load(path_of(key))) {
        memory_.insert(id, new QPixmap(pixmap));
        return pixmap;
    }
    if (online_) {
        enqueue(key);
    }
    return std::nullopt;
}

bool TileCache::is_cached(const TileKey& key) const {
    return memory_.contains(memory_key(key)) || QFileInfo::exists(path_of(key));
}

int TileCache::pending_downloads() const {
    return static_cast<int>(queue_.size()) + active_;
}

qint64 TileCache::disk_size() const {
    qint64 total = 0;
    QDirIterator files(directory_, {QStringLiteral("*.png")}, QDir::Files,
                       QDirIterator::Subdirectories);
    while (files.hasNext()) {
        total += files.nextFileInfo().size();
    }
    return total;
}

void TileCache::clear() {
    memory_.clear();
    queue_.clear();
    queued_.clear();
    QDir(directory_).removeRecursively();
    QDir().mkpath(directory_);
}

void TileCache::enqueue(const TileKey& key) {
    const QString id = memory_key(key);
    if (queued_.contains(id)) {
        return;
    }
    queued_.insert(id);
    queue_.push_back(key);
    start_next();
}

void TileCache::start_next() {
    while (active_ < kMaxConcurrentDownloads && !queue_.empty()) {
        const TileKey key = queue_.front();
        queue_.pop_front();
        QString url = url_template_;
        url.replace(QStringLiteral("{z}"), QString::number(key.zoom));
        url.replace(QStringLiteral("{x}"), QString::number(key.x));
        url.replace(QStringLiteral("{y}"), QString::number(key.y));
        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, user_agent_);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        auto* reply = network_.get(request);
        ++active_;
        connect(reply, &QNetworkReply::finished, this, [this, key, reply] { finish(key, reply); });
    }
}

void TileCache::finish(const TileKey& key, QNetworkReply* reply) {
    reply->deleteLater();
    --active_;
    queued_.remove(memory_key(key));
    if (reply->error() != QNetworkReply::NoError) {
        emit tile_failed(key, reply->errorString());
        start_next();
        return;
    }
    const QByteArray bytes = reply->readAll();
    // A reply without a network error is not necessarily an image; only one that decodes is
    // cached, so that the disk never holds a tile that cannot be drawn.
    QPixmap pixmap;
    if (!pixmap.loadFromData(bytes)) {
        emit tile_failed(key, tr("not an image"));
        start_next();
        return;
    }
    QDir().mkpath(QFileInfo(path_of(key)).path());
    // QSaveFile writes to a temporary file and renames it, so an interrupted write never
    // leaves a truncated tile that would later be served.
    QSaveFile file(path_of(key));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(bytes);
        file.commit();
    }
    memory_.insert(memory_key(key), new QPixmap(pixmap));
    emit tile_ready(key);
    start_next();
}

}  // namespace nmeasim::app::map
