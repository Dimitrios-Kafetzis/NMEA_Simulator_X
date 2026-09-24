// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the downloads of `nmeasim::app::map::TileCache`.
///
/// Covers the `User-Agent` of the requests, caching a downloaded tile, the memory of failed
/// downloads (HTTP 404 never requested again, other failures only after a retry delay), and
/// aborting running downloads when downloading is switched off or the cache is cleared, so
/// that a tile is never requested twice at once. The tiles come from `TileServer`, a minimal
/// HTTP server on the loopback interface with a port chosen by the operating system, and the
/// cache lives in a temporary directory, so the tests never touch the network or the user's
/// tile cache. The file reads no fixtures.

#include "map/tile_cache.hpp"

#include "io/event_loop.hpp"

#include <nmeasim/core/version.hpp>

#include <QBuffer>
#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QList>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <utility>

using nmeasim::test::wait_until;
using namespace std::chrono_literals;
namespace map = nmeasim::app::map;

namespace {

/// A minimal HTTP/1.1 tile server on `127.0.0.1` that answers every request the same way.
///
/// The server listens on a port the operating system picks and counts the requests it
/// receives, whatever connection they arrive on (the network access manager reuses
/// connections). It lives in the test's thread and makes progress only while the event loop
/// runs, for example inside `nmeasim::test::wait_until`.
class TileServer {
public:
    /// How the server answers a request.
    enum class Answer {
        NotFound,  ///< `HTTP/1.1 404 Not Found` with an empty body; the connection stays open.
        Close,     ///< Closes the connection without an answer, a network error for the client.
        Hold,      ///< Keeps the request unanswered until `answer_held` is called.
        Png,       ///< `HTTP/1.1 200 OK` with a 256-pixel square PNG tile.
    };

    /// Starts listening.
    ///
    /// The test fails if no port can be opened.
    ///
    /// @param answer How every request is answered.
    explicit TileServer(Answer answer) : answer_(answer) {
        REQUIRE(server_.listen(QHostAddress::LocalHost, 0));
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this] { accept(); });
    }

    /// Returns the tile URL template that points at this server.
    ///
    /// @return `http://127.0.0.1:<port>/{z}/{x}/{y}.png`.
    [[nodiscard]] QString url_template() const {
        return QStringLiteral("http://127.0.0.1:%1/{z}/{x}/{y}.png").arg(server_.serverPort());
    }

    /// Returns the number of requests received so far.
    ///
    /// @return The request count, over all connections.
    [[nodiscard]] int requests() const { return static_cast<int>(headers_.size()); }

    /// Returns the header block of every request received so far, oldest first.
    ///
    /// @return The request line and header lines of each request, as received.
    [[nodiscard]] const QList<QByteArray>& headers() const { return headers_; }

    /// Answers every request held so far with a PNG tile.
    void answer_held() {
        for (auto* socket : std::as_const(held_)) {
            if (socket->state() == QAbstractSocket::ConnectedState) {
                socket->write(png_response());
            }
        }
        held_.clear();
    }

private:
    /// Takes every pending connection and reads its requests as they arrive.
    void accept() {
        while (auto* socket = server_.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                             [this, socket] { read(socket); });
        }
    }

    /// Reads the complete requests buffered on a connection and answers each one.
    ///
    /// @param socket Connection with data to read; owned by the server.
    void read(QTcpSocket* socket) {
        QByteArray& buffer = buffers_[socket];
        buffer += socket->readAll();
        qsizetype end = buffer.indexOf("\r\n\r\n");
        while (end >= 0) {
            headers_.append(buffer.left(end));
            buffer.remove(0, end + 4);
            switch (answer_) {
                case Answer::NotFound:
                    socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
                    break;
                case Answer::Close:
                    socket->close();
                    return;
                case Answer::Hold:
                    held_.append(socket);
                    break;
                case Answer::Png:
                    socket->write(png_response());
                    break;
            }
            end = buffer.indexOf("\r\n\r\n");
        }
    }

    /// Returns a complete HTTP response carrying a PNG tile.
    ///
    /// @return Status line, headers and a white 256-pixel square PNG as the body.
    static QByteArray png_response() {
        QImage image(map::kTileSize, map::kTileSize, QImage::Format_RGB32);
        image.fill(Qt::white);
        QByteArray body;
        QBuffer buffer(&body);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: ") +
               QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body;
    }

    /// How every request is answered.
    Answer answer_;
    /// The listening socket; owns the accepted connections.
    QTcpServer server_;
    /// Bytes received on each connection that do not yet form a complete request.
    QHash<QTcpSocket*, QByteArray> buffers_;
    /// Header block of every request received, oldest first.
    QList<QByteArray> headers_;
    /// Connections with a request waiting for `answer_held`.
    QList<QTcpSocket*> held_;
};

}  // namespace

TEST_CASE("the tile cache downloads a tile, caches it and names the application", "[app][map]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    TileServer server(TileServer::Answer::Png);
    map::TileCache cache(directory.path());
    cache.set_url_template(server.url_template());
    QSignalSpy ready(&cache, &map::TileCache::tile_ready);
    const map::TileKey key{3, 4, 2};

    CHECK_FALSE(cache.tile(key).has_value());
    REQUIRE(wait_until([&] { return ready.count() == 1; }));
    CHECK(cache.pending_downloads() == 0);
    CHECK(cache.is_cached(key));
    CHECK(QFile::exists(directory.filePath(QStringLiteral("3/4/2.png"))));
    REQUIRE(server.requests() == 1);
    CHECK(server.headers().first().startsWith("GET /3/4/2.png "));
    // The User-Agent names the application, its version and its project page, as the
    // OpenStreetMap tile usage policy asks, without the application having to set it.
    CHECK(server.headers().first().contains(
        "NMEASimulatorX/" +
        QByteArray(nmeasim::core::kVersion.data(),
                   static_cast<qsizetype>(nmeasim::core::kVersion.size())) +
        " (+https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X)"));
}

TEST_CASE("the tile cache never requests a tile again that the server does not have",
          "[app][map]") {
    QTemporaryDir directory;
    TileServer server(TileServer::Answer::NotFound);
    map::TileCache cache(directory.path());
    cache.set_url_template(server.url_template());
    // Even without a retry delay a missing tile is not asked for again.
    cache.set_retry_delay(0ms);
    QSignalSpy failed(&cache, &map::TileCache::tile_failed);
    const map::TileKey key{5, 18, 12};

    CHECK_FALSE(cache.tile(key).has_value());
    REQUIRE(wait_until([&] { return failed.count() == 1; }));
    CHECK(server.requests() == 1);

    // Every repaint asks for the tile again; none of these asks reaches the server.
    wait_until(
        [&] {
            static_cast<void>(cache.tile(key));
            return false;
        },
        200);
    CHECK(cache.pending_downloads() == 0);
    CHECK(server.requests() == 1);

    // Switching downloads off and on retries transient failures only.
    cache.set_online(false);
    cache.set_online(true);
    CHECK_FALSE(cache.tile(key).has_value());
    CHECK(cache.pending_downloads() == 0);

    // Another tile server may have the tile.
    TileServer other(TileServer::Answer::Png);
    cache.set_url_template(other.url_template());
    CHECK_FALSE(cache.tile(key).has_value());
    CHECK(cache.pending_downloads() == 1);
    CHECK(wait_until([&] { return cache.is_cached(key); }));
    CHECK(server.requests() == 1);
    CHECK(other.requests() == 1);
}

TEST_CASE("the tile cache retries a failed tile only after a delay", "[app][map]") {
    QTemporaryDir directory;
    TileServer server(TileServer::Answer::Close);
    map::TileCache cache(directory.path());
    cache.set_url_template(server.url_template());
    cache.set_retry_delay(300ms);
    QSignalSpy failed(&cache, &map::TileCache::tile_failed);
    const map::TileKey key{6, 36, 24};

    CHECK_FALSE(cache.tile(key).has_value());
    REQUIRE(wait_until([&] { return failed.count() == 1; }));
    const int after_first = server.requests();
    CHECK(after_first >= 1);
    CHECK_FALSE(cache.tile(key).has_value());
    CHECK(cache.pending_downloads() == 0);

    // Asking for the tile as every repaint does, the next request comes once the delay has
    // passed and not before.
    QElapsedTimer timer;
    timer.start();
    REQUIRE(wait_until([&] {
        static_cast<void>(cache.tile(key));
        return cache.pending_downloads() > 0;
    }));
    CHECK(timer.elapsed() >= 250);
    REQUIRE(wait_until([&] { return failed.count() == 2; }));
    CHECK(server.requests() > after_first);

    // Switching downloads back on retries at once, without waiting for the doubled delay.
    CHECK_FALSE(cache.tile(key).has_value());
    CHECK(cache.pending_downloads() == 0);
    cache.set_online(false);
    cache.set_online(true);
    CHECK_FALSE(cache.tile(key).has_value());
    CHECK(cache.pending_downloads() == 1);
}

TEST_CASE("switching downloads off aborts them so that a tile is never requested twice",
          "[app][map]") {
    QTemporaryDir directory;
    TileServer server(TileServer::Answer::Hold);
    map::TileCache cache(directory.path());
    cache.set_url_template(server.url_template());
    QSignalSpy ready(&cache, &map::TileCache::tile_ready);
    QSignalSpy failed(&cache, &map::TileCache::tile_failed);
    const map::TileKey key{4, 9, 6};

    CHECK_FALSE(cache.tile(key).has_value());
    REQUIRE(wait_until([&] { return server.requests() == 1; }));
    CHECK(cache.pending_downloads() == 1);

    cache.set_online(false);
    CHECK(cache.pending_downloads() == 0);
    cache.set_online(true);
    CHECK_FALSE(cache.tile(key).has_value());
    // Only the new request runs; the aborted one is gone.
    CHECK(cache.pending_downloads() == 1);
    REQUIRE(wait_until([&] { return server.requests() == 2; }));
    server.answer_held();
    REQUIRE(wait_until([&] { return ready.count() >= 1; }));
    wait_until([] { return false; }, 100);
    CHECK(ready.count() == 1);
    CHECK(failed.count() == 0);
    CHECK(cache.pending_downloads() == 0);
}

TEST_CASE("clearing the tile cache aborts the running downloads", "[app][map]") {
    QTemporaryDir directory;
    TileServer server(TileServer::Answer::Hold);
    map::TileCache cache(directory.path());
    cache.set_url_template(server.url_template());
    QSignalSpy ready(&cache, &map::TileCache::tile_ready);
    const map::TileKey key{4, 9, 6};

    CHECK_FALSE(cache.tile(key).has_value());
    REQUIRE(wait_until([&] { return server.requests() == 1; }));
    cache.clear();
    CHECK(cache.pending_downloads() == 0);

    // The server answers after the clear; the answer must not refill the cache.
    server.answer_held();
    wait_until([] { return false; }, 200);
    CHECK(ready.count() == 0);
    CHECK_FALSE(cache.is_cached(key));
    CHECK(cache.disk_size() == 0);
}
