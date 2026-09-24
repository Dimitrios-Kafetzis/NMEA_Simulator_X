// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of `SimulationRunner` and `OutputChannel`: building the source and the
/// transports from a profile, the tick loop and the encoding of each output.

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/nmea0183/tag_block.hpp>
#include <nmeasim/core/signalk/delta.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/simulation/replay_source.hpp>
#include <nmeasim/core/simulation/sentence_scheduler.hpp>
#include <nmeasim/core/simulation/track_source.hpp>
#include <nmeasim/core/track/track_file.hpp>
#include <nmeasim/core/viewsync/viewsync.hpp>
#include <nmeasim/io/simulation_runner.hpp>
#include <nmeasim/io/transports/file_transport.hpp>
#include <nmeasim/io/transports/log_transport.hpp>
#include <nmeasim/io/transports/serial_transport.hpp>
#include <nmeasim/io/transports/stdout_transport.hpp>
#include <nmeasim/io/transports/tcp_client_transport.hpp>
#include <nmeasim/io/transports/tcp_server_transport.hpp>
#include <nmeasim/io/transports/udp_transport.hpp>
#include <nmeasim/io/transports/websocket_server_transport.hpp>

#include <QDateTime>
#include <QHostAddress>
#include <QStringView>

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nmeasim::io {

namespace {

/// Longest simulated step taken for one tick, so that a suspended or blocked host does not
/// teleport the vessel.
///
/// Wall-clock time beyond this cap is dropped rather than caught up, as the runtime model in
/// docs/explanation/architecture.md describes.
constexpr std::chrono::milliseconds kMaxStep{1000};

/// Converts a Qt date-time to a system-clock time point.
///
/// @param time The date-time to convert; its time zone is honoured.
/// @return The same instant, with millisecond resolution.
std::chrono::system_clock::time_point to_time_point(const QDateTime& time) {
    return std::chrono::system_clock::time_point{
        std::chrono::milliseconds{time.toMSecsSinceEpoch()}};
}

/// Maps the `loop` flag of a track or replay profile to the end behaviour of its source.
///
/// @param loop True when the source starts again at its end.
/// @return `EndBehaviour::Loop` when `loop` is true, `EndBehaviour::Stop` otherwise.
core::simulation::EndBehaviour end_behaviour(bool loop) {
    return loop ? core::simulation::EndBehaviour::Loop : core::simulation::EndBehaviour::Stop;
}

}  // namespace

bool OutputChannel::admits(const QString& id) const {
    if (filter.isEmpty()) {
        return true;
    }
    return std::any_of(filter.begin(), filter.end(), [&id](const QString& entry) {
        return entry.compare(id, Qt::CaseInsensitive) == 0;
    });
}

bool OutputChannel::admits_path(const QString& path) const {
    if (filter.isEmpty()) {
        return true;
    }
    return std::any_of(filter.begin(), filter.end(), [&path](const QString& entry) {
        QStringView prefix{entry};
        while (prefix.endsWith(QLatin1Char('.'))) {
            prefix.chop(1);
        }
        // An empty entry keeps admitting every path, as the plain prefix match it replaces
        // did. Otherwise the prefix must end at a segment boundary of the path: at its end or
        // at a dot.
        if (prefix.isEmpty()) {
            return true;
        }
        return path.startsWith(prefix, Qt::CaseInsensitive) &&
               (path.size() == prefix.size() || path.at(prefix.size()) == QLatin1Char('.'));
    });
}

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
            return std::make_unique<FileTransport>(profile_.resolve_path(config.path),
                                                   config.append);
        case OutputConfig::Type::Stdout:
            return std::make_unique<StdoutTransport>();
        case OutputConfig::Type::Log: {
            auto log =
                std::make_unique<LogTransport>(profile_.resolve_path(config.path), config.append);
            log->set_profile_name(profile_.name);
            return log;
        }
    }
    // Reached only for a value outside the enumeration; apply_profile rejects the profile.
    return nullptr;
}

std::unique_ptr<core::simulation::Source> SimulationRunner::make_source(const Profile& profile,
                                                                        QString* error) const {
    auto seed = profile.delta.seed;
    seed.time_utc = profile.start_time ? to_time_point(*profile.start_time)
                                       : to_time_point(QDateTime::currentDateTimeUtc());
    switch (profile.mode) {
        case SimulationMode::Delta: {
            auto config = profile.delta;
            config.seed = seed;
            return std::make_unique<core::simulation::DeltaSource>(std::move(config));
        }
        case SimulationMode::Track: {
            std::string reason;
            auto track = core::track::load_track(
                profile.resolve_path(profile.track.path).toStdString(), &reason);
            if (!track) {
                if (error) {
                    *error = QString::fromStdString(reason);
                }
                return nullptr;
            }
            core::simulation::TrackConfig config;
            config.track = std::move(*track);
            config.seed = seed;
            config.speed_kn = profile.track.speed_kn;
            config.use_timestamps = profile.track.use_timestamps;
            config.end = end_behaviour(profile.track.loop);
            return std::make_unique<core::simulation::TrackSource>(std::move(config));
        }
        case SimulationMode::Replay: {
            std::string reason;
            core::log::LogParseOptions options;
            options.fixed_interval = std::chrono::milliseconds{profile.replay.fixed_interval_ms};
            auto log = core::log::load_log(profile.resolve_path(profile.replay.path).toStdString(),
                                           options, &reason);
            if (!log) {
                if (error) {
                    *error = QString::fromStdString(reason);
                }
                return nullptr;
            }
            core::simulation::ReplayConfig config;
            config.log = std::move(*log);
            config.seed = seed;
            config.end = end_behaviour(profile.replay.loop);
            return std::make_unique<core::simulation::ReplaySource>(std::move(config));
        }
    }
    return nullptr;
}

bool SimulationRunner::apply_profile(const Profile& profile, QString* error) {
    stop();
    // Build everything before replacing the members, so that a rejected profile leaves the
    // previous simulation and outputs in place.
    auto source = make_source(profile, error);
    if (!source) {
        return false;
    }
    // make_transport reads the profile being applied from profile_; the previous one comes
    // back if an output cannot be built.
    Profile previous = std::exchange(profile_, profile);
    std::vector<OutputChannel> channels;
    for (const auto& output : profile.outputs) {
        if (!output.enabled) {
            continue;
        }
        OutputChannel channel;
        channel.config = output;
        channel.transport = make_transport(output);
        if (!channel.transport) {
            profile_ = std::move(previous);
            if (error) {
                *error = QStringLiteral("Unsupported output type");
            }
            return false;
        }
        for (const auto& id : output.filter) {
            channel.filter.insert(id);
        }
        auto* transport = channel.transport.get();
        connect(transport, &Transport::error_occurred, this,
                [this, transport](const QString& message) {
                    emit output_error(transport->description(), message);
                });
        if (output.encoding == OutputConfig::Encoding::SignalK) {
            if (auto* websocket = dynamic_cast<WebSocketServerTransport*>(transport)) {
                // Built when each client connects, so that its time and state are current.
                websocket->set_greeting_function([this, options = output.signalk] {
                    return simulation_ ? QString::fromStdString(core::signalk::encode_hello(
                                             options, simulation_->state(),
                                             std::chrono::system_clock::now()))
                                       : QString{};
                });
            }
        }
        channels.push_back(std::move(channel));
    }

    simulation_ =
        std::make_unique<core::simulation::Simulation>(std::move(source), profile.make_scheduler());
    outputs_ = std::move(channels);
    if (recorder_) {
        recorder_->set_profile_name(profile_.name);
    }
    sentences_emitted_ = 0;
    state_messages_sent_ = 0;
    return true;
}

void SimulationRunner::start() {
    if (!simulation_ || is_running()) {
        return;
    }
    for (auto& channel : outputs_) {
        channel.next_due = simulation_->elapsed();
        channel.counter = 0;
        if (!channel.transport->is_open()) {
            // A failure reaches output_error through the error_occurred connection; the run
            // goes on with the other outputs.
            (void)channel.transport->open();
        }
    }
    if (recorder_ && !recorder_->is_open()) {
        (void)recorder_->open();
    }
    paused_ = false;
    restart_wall_clock();
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
    restart_wall_clock();
    emit paused_changed(false);
}

void SimulationRunner::stop() {
    const bool was_running = is_running();
    const bool was_paused = paused_;
    tick_timer_.stop();
    paused_ = false;
    for (auto& channel : outputs_) {
        channel.transport->close();
    }
    if (recorder_) {
        recorder_->close();
    }
    if (was_paused) {
        emit paused_changed(false);
    }
    if (was_running) {
        emit stopped();
    }
}

void SimulationRunner::step() {
    if (!simulation_) {
        return;
    }
    if (!is_running()) {
        start();
    }
    pause();
    const auto sentences = simulation_->step_once(std::chrono::milliseconds{profile_.tick_ms});
    emit_sentences(sentences);
    emit_state_messages();
    emit ticked();
    finish_if_done();
}

void SimulationRunner::seek(std::chrono::milliseconds position) {
    if (!simulation_) {
        return;
    }
    simulation_->seek(position);
    // Like the sentences, whose schedule the seek resets, the state messages describe the new
    // position at the next step rather than up to one period later.
    for (auto& channel : outputs_) {
        channel.next_due = simulation_->elapsed();
    }
    restart_wall_clock();
    emit ticked();
}

std::optional<std::chrono::milliseconds> SimulationRunner::duration() const {
    return simulation_ ? simulation_->source().duration() : std::nullopt;
}

std::chrono::milliseconds SimulationRunner::position() const {
    return simulation_ ? simulation_->source().position() : std::chrono::milliseconds{0};
}

bool SimulationRunner::set_recording(const QString& path) {
    if (path.isEmpty()) {
        if (recorder_) {
            recorder_->close();
            recorder_.reset();
        }
        emit recording_changed({});
        return true;
    }
    auto recorder = std::make_unique<LogTransport>(path, false);
    recorder->set_profile_name(profile_.name);
    auto* transport = recorder.get();
    connect(transport, &Transport::error_occurred, this, [this, transport](const QString& message) {
        emit output_error(transport->description(), message);
    });
    // A file that cannot be opened is reported through output_error and changes nothing: the
    // previous recording, if any, goes on.
    if (is_running() && !recorder->open()) {
        return false;
    }
    if (recorder_) {
        recorder_->close();
    }
    recorder_ = std::move(recorder);
    emit recording_changed(path);
    return true;
}

QString SimulationRunner::recording_path() const {
    return recorder_ ? recorder_->path() : QString{};
}

void SimulationRunner::emit_sentences(
    const std::vector<core::simulation::EmittedSentence>& sentences) {
    const auto time =
        simulation_ ? simulation_->state().time_utc : std::chrono::system_clock::now();
    for (const auto& sentence : sentences) {
        const QString id = QString::fromStdString(sentence.id);
        const std::string framed = sentence.text + "\r\n";
        const QByteArray line = QByteArray::fromStdString(framed);
        ++sentences_emitted_;
        for (auto& channel : outputs_) {
            if (!channel.carries_sentences() || !channel.transport->is_open() ||
                !channel.admits(id)) {
                continue;
            }
            if (channel.config.tag_block.enabled) {
                channel.transport->write(
                    QByteArray::fromStdString(core::nmea0183::prepend_tag_block(
                        framed, channel.config.tag_block.options, time)));
            } else {
                channel.transport->write(line);
            }
            ++channel.lines_sent;
        }
        if (recorder_ && recorder_->is_open()) {
            recorder_->write(line);
        }
        emit sentence_emitted(id, QString::fromStdString(sentence.text));
    }
}

void SimulationRunner::emit_state_messages() {
    if (!simulation_) {
        return;
    }
    const auto now = simulation_->elapsed();
    const auto& state = simulation_->state();
    for (auto& channel : outputs_) {
        if (channel.carries_sentences() || !channel.transport->is_open() ||
            now < channel.next_due) {
            continue;
        }
        channel.next_due = core::simulation::next_due_after(
            channel.next_due, now, std::chrono::milliseconds{channel.config.period_ms});
        std::string message;
        QString id;
        if (channel.config.encoding == OutputConfig::Encoding::SignalK) {
            id = QStringLiteral("SIGNALK");
            message = core::signalk::encode_delta(
                state, channel.config.signalk, [&channel](std::string_view path) {
                    return channel.admits_path(
                        QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())));
                });
        } else {
            id = QStringLiteral("VIEWSYNC");
            message =
                core::viewsync::encode_packet(state, channel.config.viewsync, channel.counter);
            ++channel.counter;
        }
        channel.transport->write(QByteArray::fromStdString(message + "\r\n"));
        ++channel.lines_sent;
        ++state_messages_sent_;
        emit sentence_emitted(id, QString::fromStdString(message));
    }
}

void SimulationRunner::finish_if_done() {
    if (simulation_ && simulation_->finished()) {
        emit finished();
        stop();
    }
}

void SimulationRunner::restart_wall_clock() {
    wall_clock_.start();
    wall_consumed_ = std::chrono::nanoseconds{0};
}

void SimulationRunner::tick() {
    if (paused_ || !simulation_) {
        return;
    }
    // Hand the simulation whole milliseconds of wall-clock time and carry the fraction to the
    // next tick, so that the simulated clock keeps pace with the real one.
    const auto wall = std::chrono::nanoseconds{wall_clock_.nsecsElapsed()};
    auto dt = std::chrono::floor<std::chrono::milliseconds>(wall - wall_consumed_);
    if (dt < std::chrono::milliseconds{1}) {
        return;
    }
    if (dt > kMaxStep) {
        dt = kMaxStep;
        wall_consumed_ = wall;
    } else {
        wall_consumed_ += dt;
    }

    emit_sentences(simulation_->step(dt));
    emit_state_messages();
    emit ticked();
    finish_if_done();
}

}  // namespace nmeasim::io
