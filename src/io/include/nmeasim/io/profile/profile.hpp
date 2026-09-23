#pragma once

#include <nmeasim/core/nmea0183/encoders.hpp>
#include <nmeasim/core/nmea0183/tag_block.hpp>
#include <nmeasim/core/signalk/delta.hpp>
#include <nmeasim/core/simulation/custom_sentence.hpp>
#include <nmeasim/core/simulation/delta_source.hpp>
#include <nmeasim/core/simulation/sentence_scheduler.hpp>
#include <nmeasim/core/viewsync/viewsync.hpp>
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
#include <vector>

/// A profile is everything needed to run the simulator: the simulation seed and behaviour,
/// the sentence schedule and the outputs. Profiles are stored as JSON with a schema version
/// so that older files can be migrated when the format evolves.
namespace nmeasim::io {

/// One configured output channel.
struct OutputConfig {
    /// Kind of transport, the `type` key of an output.
    enum class Type {
        /// `tcp-server`: listens for any number of TCP clients.
        TcpServer,
        /// `tcp-client`: connects to a TCP server and reconnects when dropped.
        TcpClient,
        /// `udp`: one datagram per line, unicast, broadcast or multicast.
        Udp,
        /// `websocket-server`: one text frame per line to every WebSocket client.
        WebSocketServer,
        /// `serial`: a serial port.
        Serial,
        /// `file`: a plain file.
        File,
        /// `stdout`: standard output.
        Stdout,
        /// `log`: a recording, every line prefixed with the wall-clock time, see ADR 0012.
        Log,
    };
    /// What the output carries, see ADR 0014.
    enum class Encoding {
        /// The NMEA 0183 sentences the simulation emits.
        Nmea0183,
        /// Signal K delta messages built from the state on this output's period.
        SignalK,
        /// ViewSync camera packets built from the state on this output's period.
        ViewSync,
    };

    /// IEC 61162-450 TAG block in front of every sentence of an NMEA 0183 output.
    struct TagBlock {
        /// Prefixes every sentence with a TAG block; `tag_block.enabled`, default false.
        bool enabled{false};
        /// Source and time fields: `tag_block.source`, `include_time` and `milliseconds`.
        core::nmea0183::TagBlockOptions options;
    };

    /// The `type` key.
    Type type{Type::TcpServer};
    /// The `enabled` key; disabled outputs are kept in the profile but never opened.
    bool enabled{true};
    /// The `encoding` key: `nmea0183`, `signalk` or `viewsync`.
    Encoding encoding{Encoding::Nmea0183};
    /// Registry ids to send on this output; empty sends everything. For a Signal K output
    /// the entries are path prefixes instead.
    QStringList filter;
    /// Period of the messages of a Signal K or ViewSync output, in milliseconds, 50 to
    /// 3600000; the `period_ms` key.
    int period_ms{1000};
    /// The `tag_block` object; used by NMEA 0183 outputs only.
    TagBlock tag_block;
    /// The `signalk` object: context and source label of a Signal K output.
    core::signalk::SignalKOptions signalk;
    /// The `viewsync` object: camera altitude, tilt, roll and planet of a ViewSync output.
    core::viewsync::ViewSyncOptions viewsync;

    // TCP server and WebSocket server
    /// Local address to listen on, `bind_address`; `0.0.0.0` listens on every interface.
    QString bind_address{QStringLiteral("0.0.0.0")};
    /// Port to listen on or connect to, `port`; 0 lets a server pick a free port. UDP outputs
    /// use `UdpConfig::port`, read from the same key.
    quint16 port{10110};
    // TCP client
    /// Server to connect to, `host`.
    QString host{QStringLiteral("127.0.0.1")};
    /// Delay before reconnecting after the connection drops, in milliseconds; `reconnect_ms`.
    int reconnect_ms{2000};
    // UDP
    /// The keys `mode`, `address`, `port`, `interface` and `multicast_ttl`.
    UdpConfig udp;
    // Serial
    /// The keys `port_name`, `baud_rate`, `data_bits`, `parity`, `stop_bits`, `flow_control`.
    SerialConfig serial;
    // File and Log
    /// File to write, `path`; required for file and log outputs.
    QString path;
    /// The `append` key: true appends to an existing file, false truncates it on opening.
    bool append{true};
};

/// Profile name of `type`, such as `tcp-server` or `websocket-server`.
[[nodiscard]] QString to_string(OutputConfig::Type type);
/// Parses a profile output type name; nullopt when `text` is not one.
[[nodiscard]] std::optional<OutputConfig::Type> output_type_from_string(const QString& text);
/// Profile name of `encoding`: `nmea0183`, `signalk` or `viewsync`.
[[nodiscard]] QString to_string(OutputConfig::Encoding encoding);
/// Parses a profile encoding name; nullopt when `text` is not one.
[[nodiscard]] std::optional<OutputConfig::Encoding> encoding_from_string(const QString& text);

/// What drives the vessel.
enum class SimulationMode {
    /// Seed values that drift, with overrides and steering.
    Delta,
    /// A GPX or KML track file.
    Track,
    /// A recorded log sent again.
    Replay,
};

/// Profile name of `mode`: `delta`, `track` or `replay`.
[[nodiscard]] QString to_string(SimulationMode mode);
/// Parses a profile simulation mode name; nullopt when `text` is not one.
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
    /// Starts again at the first point instead of stopping at the last.
    bool loop{false};
};

/// Settings of the log replay mode.
struct ReplaySettings {
    /// Log file, resolved like `TrackSettings::path`.
    QString path;
    /// Starts again at the first entry instead of stopping at the last.
    bool loop{false};
    /// Spacing of the sentences when the log carries no time information at all.
    int fixed_interval_ms{100};
};

/// A complete simulator setup, stored as a JSON profile file; see docs/reference/profile.md.
struct Profile {
    /// The schema version this code writes. Older files are migrated on load.
    static constexpr int kCurrentSchemaVersion{3};

    /// Display name, the `name` key.
    QString name{QStringLiteral("Default")};
    /// Length of one simulation tick in milliseconds, 10 to 10000; `simulation.tick_ms`.
    int tick_ms{100};
    /// Simulated start time; empty means the wall clock when the run starts.
    std::optional<QDateTime> start_time;
    /// What drives the vessel, `simulation.mode`.
    SimulationMode mode{SimulationMode::Delta};
    /// The delta simulation, and the seed values for every mode.
    core::simulation::DeltaConfig delta;
    /// Track-following settings, `simulation.track`; used in Track mode.
    TrackSettings track;
    /// Log replay settings, `simulation.replay`; used in Replay mode.
    ReplaySettings replay;
    /// Encoder settings; `sentences.position_decimals`.
    core::nmea0183::EncoderOptions encoder;
    /// Sentence settings that differ from the registry defaults, keyed by registry id.
    std::map<std::string, core::simulation::SentenceSetting> sentences;
    /// Sentences typed in by the operator, in emission order.
    std::vector<core::simulation::CustomSentence> custom_sentences;
    /// Output channels, the `outputs` array.
    QList<OutputConfig> outputs;

    /// A ready-to-run profile: a vessel off Athens, every default sentence, a TCP server on
    /// port 10110.
    [[nodiscard]] static Profile default_profile();

    /// Serialises the profile at the current schema version.
    [[nodiscard]] QJsonObject to_json() const;
    /// Parses and migrates a profile. On failure returns nullopt and sets `error`.
    [[nodiscard]] static std::optional<Profile> from_json(const QJsonObject& json, QString* error);

    /// Loads a profile file; relative track and log paths are resolved against its directory.
    [[nodiscard]] static std::optional<Profile> load(const QString& path, QString* error);
    /// Writes the profile as indented JSON, replacing the file atomically. On failure returns
    /// false and sets `error` when it is not null.
    [[nodiscard]] bool save(const QString& path, QString* error) const;

    /// Builds a scheduler with this profile's sentence settings and custom sentences applied.
    [[nodiscard]] core::simulation::SentenceScheduler make_scheduler() const;
};

}  // namespace nmeasim::io
