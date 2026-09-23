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
        /// A recording: every line prefixed with the wall-clock time, see ADR 0012.
        Log,
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
    // File and Log
    QString path;
    bool append{true};
};

[[nodiscard]] QString to_string(OutputConfig::Type type);
[[nodiscard]] std::optional<OutputConfig::Type> output_type_from_string(const QString& text);

/// What drives the vessel.
enum class SimulationMode {
    /// Seed values that drift, with overrides and steering.
    Delta,
    /// A GPX or KML track file.
    Track,
    /// A recorded log sent again.
    Replay,
};

[[nodiscard]] QString to_string(SimulationMode mode);
[[nodiscard]] std::optional<SimulationMode> simulation_mode_from_string(const QString& text);

/// Settings of the track-following mode.
struct TrackSettings {
    /// GPX or KML file. Relative paths in a profile file are resolved against the profile's
    /// own directory when it is loaded.
    QString path;
    /// Speed along legs whose points carry neither timestamps nor a recorded speed.
    double speed_kn{6.0};
    /// When false the track's timestamps are ignored and it is sailed at `speed_kn`.
    bool use_timestamps{true};
    bool loop{false};
};

/// Settings of the log replay mode.
struct ReplaySettings {
    /// Log file, resolved like `TrackSettings::path`.
    QString path;
    bool loop{false};
    /// Spacing of the sentences when the log carries no time information at all.
    int fixed_interval_ms{100};
};

struct Profile {
    /// The schema version this code writes. Older files are migrated on load.
    static constexpr int kCurrentSchemaVersion{2};

    QString name{QStringLiteral("Default")};
    /// Length of one simulation tick.
    int tick_ms{100};
    /// Simulated start time; empty means the wall clock when the run starts.
    std::optional<QDateTime> start_time;
    SimulationMode mode{SimulationMode::Delta};
    /// The delta simulation, and the seed values for every mode.
    core::simulation::DeltaConfig delta;
    TrackSettings track;
    ReplaySettings replay;
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

    /// Loads a profile file; relative track and log paths are resolved against its directory.
    [[nodiscard]] static std::optional<Profile> load(const QString& path, QString* error);
    [[nodiscard]] bool save(const QString& path, QString* error) const;

    /// Builds a scheduler with this profile's sentence settings applied.
    [[nodiscard]] core::simulation::SentenceScheduler make_scheduler() const;
};

}  // namespace nmeasim::io
