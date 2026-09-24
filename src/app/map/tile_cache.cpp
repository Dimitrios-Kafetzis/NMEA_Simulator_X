// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Memory, disk and network lookup, download queue, failure memory and disk writes of
/// `TileCache`.

#include "tile_cache.hpp"

#include <nmeasim/core/version.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>
#include <cstddef>

namespace nmeasim::app::map {

namespace {

/// Capacity of the memory cache in tiles; 512 tiles of 256 by 256 pixels take about 128 MiB
/// as 32-bit pixmaps.
constexpr int kMemoryTiles{512};
/// Largest number of downloads running at once, kept low as the tile usage policy asks.
constexpr int kMaxConcurrentDownloads{4};
/// Time in milliseconds a download may go without receiving data before it is aborted.
constexpr int kTransferTimeoutMs{15000};
/// Longest delay before a failed tile is requested again, however often it failed; long
/// enough to spare the server, short enough that a map left open recovers after an outage.
constexpr std::chrono::milliseconds kMaxRetryDelay{std::chrono::minutes{10}};

/// Returns the `User-Agent` the cache identifies itself with.
///
/// @return `NMEASimulatorX/<version> (+<project page>)`, the form the OpenStreetMap tile usage
///     policy asks for: the application, its version and a way to contact its authors.
QString default_user_agent() {
    return QStringLiteral(
               "NMEASimulatorX/%1 (+https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X)")
        .arg(QString::fromUtf8(core::kVersion.data(),
                               static_cast<qsizetype>(core::kVersion.size())));
}

/// Returns whether a finished reply means that the server does not have the tile at all.
///
/// @param reply Finished reply.
/// @return `true` for HTTP 404 (Not Found) and 410 (Gone), which asking again will not change.
bool is_not_found(const QNetworkReply& reply) {
    const int status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return status == 404 || status == 410;
}

}  // namespace

TileCache::TileCache(QString directory, QObject* parent)
    : QObject(parent),
      directory_(std::move(directory)),
      url_template_(QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png")),
      user_agent_(default_user_agent()),
      memory_(kMemoryTiles) {
    network_.setTransferTimeout(kTransferTimeoutMs);
    QDir().mkpath(directory_);
}

void TileCache::set_url_template(const QString& url_template) {
    const QString trimmed = url_template.trimmed();
    if (trimmed.isEmpty() || trimmed == url_template_) {
        return;
    }
    url_template_ = trimmed;
    // What failed on the previous server says nothing about the new one; the queued tiles
    // stay and are requested from the new server.
    failures_.clear();
    abort_running();
    start_next();
}

void TileCache::set_online(bool online) {
    if (online && !online_) {
        // The operator switches downloads back on, typically after the network returned, so
        // tiles that failed for a transient reason are tried again at once.
        failures_.removeIf([](const auto& entry) { return !entry.value().permanent; });
    }
    online_ = online;
    if (!online_) {
        abort_running();
        queue_.clear();
        queued_.clear();
    }
}

void TileCache::abort_running() {
    // Disconnect first: abort() emits finished synchronously, and an aborted download must
    // neither cache its tile nor count as a failure.
    for (auto it = running_.cbegin(); it != running_.cend(); ++it) {
        disconnect(it.value(), nullptr, this, nullptr);
        it.value()->abort();
        it.value()->deleteLater();
        queued_.remove(it.key());
    }
    running_.clear();
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
    if (online_ && may_request(id)) {
        enqueue(key);
    }
    return std::nullopt;
}

bool TileCache::is_cached(const TileKey& key) const {
    return memory_.contains(memory_key(key)) || QFileInfo::exists(path_of(key));
}

int TileCache::pending_downloads() const {
    return static_cast<int>(queue_.size() + static_cast<std::size_t>(running_.size()));
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
    abort_running();
    queue_.clear();
    queued_.clear();
    memory_.clear();
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
    while (running_.size() < kMaxConcurrentDownloads && !queue_.empty()) {
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
        running_.insert(memory_key(key), reply);
        connect(reply, &QNetworkReply::finished, this, [this, key, reply] { finish(key, reply); });
    }
}

void TileCache::finish(const TileKey& key, QNetworkReply* reply) {
    reply->deleteLater();
    const QString id = memory_key(key);
    running_.remove(id);
    queued_.remove(id);
    if (reply->error() != QNetworkReply::NoError) {
        record_failure(id, is_not_found(*reply));
        emit tile_failed(key, reply->errorString());
        start_next();
        return;
    }
    const QByteArray bytes = reply->readAll();
    // A reply without a network error is not necessarily an image; only one that decodes is
    // cached, so that the disk never holds a tile that cannot be drawn.
    QPixmap pixmap;
    if (!pixmap.loadFromData(bytes)) {
        record_failure(id, false);
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
    failures_.remove(id);
    memory_.insert(id, new QPixmap(pixmap));
    emit tile_ready(key);
    start_next();
}

void TileCache::record_failure(const QString& id, bool permanent) {
    Failure& failure = failures_[id];
    if (permanent) {
        failure.permanent = true;
        return;
    }
    failure.delay = failure.delay <= std::chrono::milliseconds::zero()
                        ? retry_delay_
                        : std::min(failure.delay * 2, std::max(kMaxRetryDelay, retry_delay_));
    failure.retry_at = std::chrono::steady_clock::now() + failure.delay;
}

bool TileCache::may_request(const QString& id) const {
    const auto found = failures_.constFind(id);
    if (found == failures_.cend()) {
        return true;
    }
    return !found->permanent && std::chrono::steady_clock::now() >= found->retry_at;
}

}  // namespace nmeasim::app::map
