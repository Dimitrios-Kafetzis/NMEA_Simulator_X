#pragma once

#include <nmeasim/core/nmea0183/encoders.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/simulation/sentence_scheduler.hpp>
#include <nmeasim/io/transports/serial_transport.hpp>
#include <nmeasim/io/transports/udp_transport.hpp>

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <map>
#include <optional>
#include <string>

/// A profile is everything needed to run the simulator: the simulation seed and behaviour,
/// the sentence schedule and the outputs. Profiles are stored as JSON with a schema version
/// so that older files can be migrated when the format evolves.
namespace nmeasim::io {

/// One configured output channel.
struct OutputConfig {
    enum class Type {
        TcpServer,
        TcpClient,
        Udp,
        WebSocketServer,
        Serial,
        File,
        Stdout,
    };
    enum class Encoding {
        Nmea0183,
    };

    Type type{Type::TcpServer};
    bool enabled{true};
    Encoding encoding{Encoding::Nmea0183};
    /// Registry ids to send on this output; empty sends everything.
    QStringList filter;

    // TCP server and WebSocket server
    QString bind_address{QStringLiteral("0.0.0.0")};
    quint16 port{10110};
    // TCP client
    QString host{QStringLiteral("127.0.0.1")};
    int reconnect_ms{2000};
    // UDP
    UdpConfig udp;
    // Serial
    SerialConfig serial;
    // File
    QString path;
    bool append{true};
};

[[nodiscard]] QString to_string(OutputConfig::Type type);
[[nodiscard]] std::optional<OutputConfig::Type> output_type_from_string(const QString& text);

struct Profile {
    /// The schema version this code writes. Older files are migrated on load.
    static constexpr int kCurrentSchemaVersion{1};

    QString name{QStringLiteral("Default")};
    /// Length of one simulation tick.
    int tick_ms{100};
    /// Simulated start time; empty means the wall clock when the run starts.
    std::optional<QDateTime> start_time;
    core::simulation::DeltaConfig delta;
    core::nmea0183::EncoderOptions encoder;
    /// Sentence settings that differ from the registry defaults, keyed by registry id.
    std::map<std::string, core::simulation::SentenceSetting> sentences;
    QList<OutputConfig> outputs;

    /// A ready-to-run profile: a vessel off Athens, every default sentence, a TCP server on
    /// port 10110.
    [[nodiscard]] static Profile default_profile();

    [[nodiscard]] QJsonObject to_json() const;
    /// Parses and migrates a profile. On failure returns nullopt and sets `error`.
    [[nodiscard]] static std::optional<Profile> from_json(const QJsonObject& json, QString* error);

    [[nodiscard]] static std::optional<Profile> load(const QString& path, QString* error);
    [[nodiscard]] bool save(const QString& path, QString* error) const;

    /// Builds a scheduler with this profile's sentence settings applied.
    [[nodiscard]] core::simulation::SentenceScheduler make_scheduler() const;
};

}  // namespace nmeasim::io
