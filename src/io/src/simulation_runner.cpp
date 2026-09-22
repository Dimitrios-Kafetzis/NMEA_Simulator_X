#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/io/simulation_runner.hpp>
#include <nmeasim/io/transports/file_transport.hpp>
#include <nmeasim/io/transports/serial_transport.hpp>
#include <nmeasim/io/transports/stdout_transport.hpp>
#include <nmeasim/io/transports/tcp_client_transport.hpp>
#include <nmeasim/io/transports/tcp_server_transport.hpp>
#include <nmeasim/io/transports/udp_transport.hpp>
#include <nmeasim/io/transports/websocket_server_transport.hpp>

#include <QDateTime>
#include <QHostAddress>

#include <algorithm>
#include <chrono>

namespace nmeasim::io {

namespace {

/// Longest simulated step taken for one tick, so that a suspended host does not teleport.
constexpr std::chrono::milliseconds kMaxStep{1000};

std::chrono::system_clock::time_point to_time_point(const QDateTime& time) {
    return std::chrono::system_clock::time_point{
        std::chrono::milliseconds{time.toMSecsSinceEpoch()}};
}

}  // namespace

SimulationRunner::SimulationRunner(QObject* parent) : QObject(parent) {
    tick_timer_.setTimerType(Qt::PreciseTimer);
    connect(&tick_timer_, &QTimer::timeout, this, &SimulationRunner::tick);
}

SimulationRunner::~SimulationRunner() {
    stop();
}

std::unique_ptr<Transport> SimulationRunner::make_transport(const OutputConfig& config) const {
    switch (config.type) {
        case OutputConfig::Type::TcpServer:
            return std::make_unique<TcpServerTransport>(config.port,
                                                        QHostAddress(config.bind_address));
        case OutputConfig::Type::TcpClient:
            return std::make_unique<TcpClientTransport>(config.host, config.port,
                                                        config.reconnect_ms);
        case OutputConfig::Type::Udp:
            return std::make_unique<UdpTransport>(config.udp);
        case OutputConfig::Type::WebSocketServer:
            return std::make_unique<WebSocketServerTransport>(config.port,
                                                              QHostAddress(config.bind_address));
        case OutputConfig::Type::Serial:
            return std::make_unique<SerialTransport>(config.serial);
        case OutputConfig::Type::File:
            return std::make_unique<FileTransport>(config.path, config.append);
        case OutputConfig::Type::Stdout:
            return std::make_unique<StdoutTransport>();
    }
    return nullptr;
}

bool SimulationRunner::apply_profile(const Profile& profile, QString* error) {
    stop();
    for (const auto& output : profile.outputs) {
        if (output.enabled && make_transport(output) == nullptr) {
            if (error) {
                *error = QStringLiteral("Unsupported output type");
            }
            return false;
        }
    }

    profile_ = profile;
    auto config = profile.delta;
    config.seed.time_utc = profile.start_time ? to_time_point(*profile.start_time)
                                              : to_time_point(QDateTime::currentDateTimeUtc());
    simulation_ = std::make_unique<core::simulation::Simulation>(
        std::make_unique<core::simulation::DeltaSource>(std::move(config)),
        profile.make_scheduler());

    outputs_.clear();
    for (const auto& output : profile.outputs) {
        if (!output.enabled) {
            continue;
        }
        OutputChannel channel{output, make_transport(output), {}, 0};
        for (const auto& id : output.filter) {
            channel.filter.insert(id);
        }
        auto* transport = channel.transport.get();
        connect(transport, &Transport::error_occurred, this,
                [this, transport](const QString& message) {
                    emit output_error(transport->description(), message);
                });
        outputs_.push_back(std::move(channel));
    }
    sentences_emitted_ = 0;
    return true;
}

void SimulationRunner::start() {
    if (!simulation_ || is_running()) {
        return;
    }
    for (auto& channel : outputs_) {
        if (!channel.transport->is_open()) {
            (void)channel.transport->open();
        }
    }
    paused_ = false;
    wall_clock_.start();
    tick_timer_.start(profile_.tick_ms);
    emit started();
}

void SimulationRunner::pause() {
    if (!is_running() || paused_) {
        return;
    }
    paused_ = true;
    emit paused_changed(true);
}

void SimulationRunner::resume() {
    if (!is_running() || !paused_) {
        return;
    }
    paused_ = false;
    wall_clock_.restart();
    emit paused_changed(false);
}

void SimulationRunner::stop() {
    const bool was_running = is_running();
    tick_timer_.stop();
    paused_ = false;
    for (auto& channel : outputs_) {
        channel.transport->close();
    }
    if (was_running) {
        emit stopped();
    }
}

void SimulationRunner::tick() {
    if (paused_ || !simulation_) {
        return;
    }
    const auto elapsed = std::chrono::milliseconds{wall_clock_.restart()};
    const auto dt = std::clamp(elapsed, std::chrono::milliseconds{1}, kMaxStep);

    const auto sentences = simulation_->step(dt);
    for (const auto& sentence : sentences) {
        const QString id = QString::fromStdString(sentence.id);
        const QByteArray line = QByteArray::fromStdString(sentence.text + "\r\n");
        ++sentences_emitted_;
        for (auto& channel : outputs_) {
            if (channel.transport->is_open() && channel.admits(id)) {
                channel.transport->write(line);
                ++channel.sentences_sent;
            }
        }
        emit sentence_emitted(id, QString::fromStdString(sentence.text));
    }
    emit ticked();
    if (simulation_->finished()) {
        stop();
    }
}

}  // namespace nmeasim::io
