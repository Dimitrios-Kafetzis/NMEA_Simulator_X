// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The simulator profile: the persistent JSON description of a complete simulation setup.
///
/// A profile holds everything needed to run the simulator: the simulation mode, the seed values
/// of the vessel and how they drift, the track and replay settings, the sentence schedule and
/// the output channels. `Profile::load` and `Profile::save` read and write profile files;
/// `Profile::from_json` and `Profile::to_json` convert to and from a `QJsonObject`. Every file
/// carries a `schema_version`, and files written by older versions are migrated in memory when
/// they are loaded. The file format is described key by key in `docs/reference/profile.md`.

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

namespace nmeasim::io {

/// One configured output channel, an entry of the `outputs` array of a profile.
///
/// The fields form a union of the settings of every transport type: only those that belong to
/// `type` and `encoding` are used, and `Profile::to_json` writes only those, so the others come
/// back with their defaults after a round trip through JSON. The initialisers are the defaults
/// a missing key takes when a profile is read.
///
/// @see ADR 0014 for the encodings, `docs/reference/transports.md` for the transports.
struct OutputConfig {
    /// Kind of transport, the `type` key of an output.
    ///
    /// Each enumerator is written as the string given in its description; `to_string` and
    /// `output_type_from_string` convert.
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
    /// What the output carries, the `encoding` key of an output; see ADR 0014.
    ///
    /// Each enumerator is written as the string given in its description; `to_string` and
    /// `encoding_from_string` convert.
    enum class Encoding {
        /// `nmea0183`, the default: the NMEA 0183 sentences the simulation emits.
        Nmea0183,
        /// `signalk`: Signal K delta messages built from the state every `period_ms`.
        SignalK,
        /// `viewsync`: ViewSync camera packets built from the state every `period_ms`.
        ViewSync,
    };

    /// IEC 61162-450 TAG block in front of every sentence of an NMEA 0183 output.
    ///
    /// @see IEC 61162-450, TAG block parameters "s" and "c".
    struct TagBlock {
        /// Prefixes every sentence with a TAG block; the `tag_block.enabled` key.
        bool enabled{false};
        /// The keys `tag_block.source` (the `s:` source identifier, default `SIM0001`),
        /// `tag_block.include_time` (whether `c:` is sent, default true) and
        /// `tag_block.milliseconds` (`c:` in milliseconds instead of seconds, default false).
        core::nmea0183::TagBlockOptions options;
    };

    /// Transport kind, the `type` key; required in a profile file.
    ///
    /// Reading an unknown name is an error.
    Type type{Type::TcpServer};
    /// The `enabled` key; a disabled output is kept in the profile but never opened.
    bool enabled{true};
    /// What the output carries, the `encoding` key: `nmea0183`, `signalk` or `viewsync`.
    ///
    /// Reading an unknown name is an error.
    Encoding encoding{Encoding::Nmea0183};
    /// The `filter` key: what this output sends; empty sends everything.
    ///
    /// For an NMEA 0183 output the entries are registry ids and custom sentence ids. For a
    /// Signal K output they are paths or leading path segments such as `navigation` or
    /// `environment.wind`, each admitting the paths below it. Both are matched without regard
    /// to case. A ViewSync output ignores it.
    QStringList filter;
    /// Period of the messages of a Signal K or ViewSync output in milliseconds, the
    /// `period_ms` key; [50, 3600000].
    ///
    /// A value outside the range is rejected when the profile is read. The simulation tick
    /// bounds the resolution. NMEA 0183 outputs ignore it and do not write it.
    int period_ms{1000};
    /// The `tag_block` object; used and written by NMEA 0183 outputs only.
    TagBlock tag_block;
    /// The `signalk` object: `context` (empty derives `vessels.urn:mrn:imo:mmsi:` followed by
    /// the own MMSI) and `source_label` (default `nmeasim`). Used and written by Signal K
    /// outputs only.
    core::signalk::SignalKOptions signalk;
    /// The `viewsync` object: `camera_altitude_m` (default 500), `tilt_deg` (default 60),
    /// `roll_deg` (default 0) and `planet` (empty for Earth, or `sky`, `mars`, `moon`). Used
    /// and written by ViewSync outputs only.
    core::viewsync::ViewSyncOptions viewsync;

    // TCP server and WebSocket server
    /// Local address a TCP or WebSocket server listens on, the `bind_address` key; `0.0.0.0`
    /// listens on every IPv4 interface.
    QString bind_address{QStringLiteral("0.0.0.0")};
    /// Port a server listens on or a TCP client connects to, the `port` key; [0, 65535].
    ///
    /// Read only for the TCP server and client, UDP and WebSocket types, which reject a value
    /// outside the range when the profile is read; the other types ignore the key and keep
    /// the default. 0 lets a server pick a free port. The default 10110 is the port conventionally
    /// used for NMEA 0183 over IP. A UDP output uses `UdpConfig::port`, read from the same key.
    quint16 port{10110};
    // TCP client
    /// Host name or address a TCP client connects to, the `host` key.
    QString host{QStringLiteral("127.0.0.1")};
    /// Delay before a TCP client reconnects after the connection drops, in milliseconds; the
    /// `reconnect_ms` key, [1, 3600000].
    ///
    /// Read only for a TCP client, which rejects a value outside the range when the profile
    /// is read.
    int reconnect_ms{2000};
    // UDP
    /// UDP settings, from the keys `mode` (`unicast`, `broadcast` or `multicast`, default
    /// `unicast`), `address` (default `127.0.0.1`), `port`, `interface` (default empty) and
    /// `multicast_ttl` (default 1, [1, 255]).
    ///
    /// Read only for a UDP output, which rejects an unknown `mode` and a `multicast_ttl`
    /// outside its range when the profile is read.
    UdpConfig udp;
    // Serial
    /// Serial port settings, from the keys `port_name` (required for a serial output),
    /// `baud_rate` (default 4800), `data_bits` (5 to 8, default 8), `parity` (`none`, `even`,
    /// `odd`, `mark` or `space`), `stop_bits` (`1`, `1.5` or `2`) and `flow_control` (`none`,
    /// `hardware` or `software`).
    ///
    /// Read only for a serial output, which rejects a `baud_rate` that is not positive and
    /// any other `data_bits`, `parity`, `stop_bits` or `flow_control` value when the profile
    /// is read. Missing keys take the defaults (4800, 8, `none`, `1`, `none`).
    SerialConfig serial;
    // File and Log
    /// File a file or log output writes, the `path` key; required for those two types.
    ///
    /// Held as written. A relative path is relative to `Profile::base_directory`, and
    /// `SimulationRunner` resolves it with `Profile::resolve_path` when it builds the
    /// transport.
    QString path;
    /// The `append` key: true appends to an existing file, false truncates it when the run
    /// first opens it; stopping and starting the run again continues the file.
    bool append{true};
};

/// Returns the profile name of an output type.
///
/// @param type The output type.
/// @return The name written as the `type` key, such as `tcp-server` or `websocket-server`;
///     `unknown` for a value outside the enumeration.
[[nodiscard]] QString to_string(OutputConfig::Type type);
/// Parses the profile name of an output type.
///
/// @param value The name, matched exactly and case-sensitively, such as `udp` or `tcp-client`.
/// @return The output type, or `std::nullopt` when `value` names none.
[[nodiscard]] std::optional<OutputConfig::Type> output_type_from_string(const QString& value);
/// Returns the profile name of an output encoding.
///
/// @param encoding The encoding.
/// @return `nmea0183`, `signalk` or `viewsync`; `nmea0183` for a value outside the
///     enumeration.
[[nodiscard]] QString to_string(OutputConfig::Encoding encoding);
/// Parses the profile name of an output encoding.
///
/// @param value The name, matched exactly and case-sensitively.
/// @return The encoding, or `std::nullopt` when `value` is not `nmea0183`, `signalk` or
///     `viewsync`.
[[nodiscard]] std::optional<OutputConfig::Encoding> encoding_from_string(const QString& value);

/// What drives the vessel, the `simulation.mode` key of a profile.
///
/// Each enumerator is written as the string given in its description; `to_string` and
/// `simulation_mode_from_string` convert. In every mode the delta seed supplies the values the
/// driving source does not.
enum class SimulationMode {
    /// `delta`, the default: seed values that drift, with overrides and steering.
    Delta,
    /// `track`: follows a GPX or KML track file, `Profile::track`.
    Track,
    /// `replay`: sends a recorded log again, `Profile::replay`.
    Replay,
};

/// Returns the profile name of a simulation mode.
///
/// @param mode The simulation mode.
/// @return `delta`, `track` or `replay`; `delta` for a value outside the enumeration.
[[nodiscard]] QString to_string(SimulationMode mode);
/// Parses the profile name of a simulation mode.
///
/// @param value The name, matched exactly and case-sensitively.
/// @return The mode, or `std::nullopt` when `value` is not `delta`, `track` or `replay`.
[[nodiscard]] std::optional<SimulationMode> simulation_mode_from_string(const QString& value);

/// Settings of the track-following mode, the `simulation.track` object of a profile.
///
/// They are read and validated in every mode, but used only in `SimulationMode::Track`.
///
/// @see `core::simulation::TrackConfig`, GPX 1.1, OGC KML 2.2.
struct TrackSettings {
    /// GPX or KML track file, the `path` key; required in track mode.
    ///
    /// Held as written. A relative path is relative to `Profile::base_directory`, and
    /// `SimulationRunner` resolves it with `Profile::resolve_path` when it loads the track.
    QString path;
    /// Speed along legs whose points carry neither timestamps nor a recorded speed, the
    /// `speed_kn` key; must be positive, otherwise the profile is rejected.
    double speed_kn{6.0};
    /// The `use_timestamps` key; false ignores the track's timestamps and sails every leg at
    /// the recorded point speed or, without one, at `speed_kn`.
    bool use_timestamps{true};
    /// The `loop` key; true starts again at the first point instead of stopping at the last.
    bool loop{false};
};

/// Settings of the log replay mode, the `simulation.replay` object of a profile.
///
/// They are read and validated in every mode, but used only in `SimulationMode::Replay`.
///
/// @see ADR 0012 for the log format.
struct ReplaySettings {
    /// Log file to replay, the `path` key; required in replay mode.
    ///
    /// Held as written; a relative path is resolved like `TrackSettings::path`.
    QString path;
    /// The `loop` key; true starts again at the first entry instead of stopping at the last.
    bool loop{false};
    /// Spacing of the entries in milliseconds when the log carries no time information at
    /// all, the `fixed_interval_ms` key; [1, 60000], a value outside is rejected.
    int fixed_interval_ms{100};
};

/// A complete simulator setup, stored as a JSON profile file.
///
/// A plain value type, copied freely. The initialisers of the fields, together with
/// `default_profile`, give the defaults that `from_json` uses for missing keys.
///
/// @see `docs/reference/profile.md` for every key, its default and its range.
struct Profile {
    /// The schema version this code writes, the `schema_version` key.
    ///
    /// Files with versions 1 and 2 are migrated when they are read; newer files are rejected.
    /// Version 2 added the track and replay modes and the `log` output type; version 3 added
    /// the destination and AIS seed data, custom sentences, and the Signal K and ViewSync
    /// encodings with TAG blocks.
    static constexpr int kCurrentSchemaVersion{3};

    /// Display name, the `name` key.
    QString name{QStringLiteral("Default")};
    /// Length of one simulation tick in milliseconds, the `simulation.tick_ms` key;
    /// [10, 10000], a value outside is rejected.
    int tick_ms{100};
    /// Simulated start time in UTC, the `simulation.start_time` key; `std::nullopt` starts at
    /// the wall-clock time when the run starts.
    ///
    /// The key holds `now` (written for `std::nullopt`) or an ISO 8601 date-time with
    /// milliseconds, written in UTC such as `2026-09-22T12:34:56.780Z`. A value read without a
    /// time zone is taken as local time and converted to UTC.
    std::optional<QDateTime> start_time;
    /// What drives the vessel, the `simulation.mode` key.
    SimulationMode mode{SimulationMode::Delta};
    /// The delta simulation, and the seed values for every mode.
    ///
    /// Read from `simulation.random_seed`, `simulation.seed`, `simulation.variation` and
    /// `simulation.steering`. When read, a `simulation.seed.gnss.quality` other than
    /// `invalid`, `gps` or `differential` and a `simulation.seed.destination` object without
    /// numeric `latitude` and `longitude` are rejected. In track and replay mode the seed supplies
    /// the values the file does not carry and the variations are unused.
    core::simulation::DeltaConfig delta;
    /// Track-following settings, the `simulation.track` object; used in track mode.
    TrackSettings track;
    /// Log replay settings, the `simulation.replay` object; used in replay mode.
    ReplaySettings replay;
    /// Encoder settings: `position_decimals` from the `sentences.position_decimals` key,
    /// [2, 8], a value outside is rejected.
    core::nmea0183::EncoderOptions encoder;
    /// Sentence settings that differ from the registry defaults, keyed by registry id; the
    /// `sentences.settings` object.
    ///
    /// Reading an id the registry does not know is an error. A key missing from an entry
    /// takes the registry default. When read, a `talker` must be empty or two upper-case
    /// letters and `period_ms` must lie in [50, 3600000].
    std::map<std::string, core::simulation::SentenceSetting> sentences;
    /// Sentences typed in by the operator, in emission order; the `sentences.custom` array.
    ///
    /// When read, an `id` is trimmed and upper-cased and must not be a registry id nor the
    /// id of another entry, the `CUSTOM-n` an entry without id stands for included (see
    /// `core::simulation::find_duplicate_custom_id`); the `body` must pass
    /// `core::simulation::validate_custom_sentence`; `period_ms` must lie in [50, 3600000].
    std::vector<core::simulation::CustomSentence> custom_sentences;
    /// Output channels, the `outputs` array, in file order.
    QList<OutputConfig> outputs;
    /// Directory that the relative paths of the profile are relative to; not part of the
    /// JSON document.
    ///
    /// `load` sets it to the absolute directory of the profile file, so that a profile can
    /// name the track, the log to replay and the output files next to it. Empty for a profile
    /// built in code or parsed with `from_json`: its relative paths are then used as they
    /// are, relative to the working directory of the process.
    QString base_directory;

    /// Returns a ready-to-run profile: a vessel off Athens, every default sentence and one TCP
    /// server on port 10110.
    ///
    /// The seed is at 37.9838 N, 23.7275 E heading 45 degrees true at 6.5 knots, with 12.4 m
    /// of depth, a 12-knot westerly wind and two engines running at 1800 rpm. It is the
    /// profile that `nmeasim profile init` writes.
    ///
    /// @return The default profile.
    [[nodiscard]] static Profile default_profile();

    /// Serialises the profile at `kCurrentSchemaVersion`.
    ///
    /// Every section is written in full, with the keys of each output limited to those its
    /// type and encoding use. A destination of `std::nullopt` is written as `null`.
    ///
    /// @return The profile document, ready for `QJsonDocument`.
    [[nodiscard]] QJsonObject to_json() const;
    /// Parses a profile document, migrating an older schema version first.
    ///
    /// Missing keys take the defaults of `default_profile`, except that the outputs default to
    /// none. A value of the wrong JSON type, or a fractional number for an integer key, is
    /// mostly treated as missing; a non-array `simulation.seed.engines` gives no engines and a
    /// non-object `simulation.seed.destination` gives none. `simulation.random_seed`
    /// ([0, 4294967295]) and `simulation.seed.ais.mmsi` and `imo_number` ([0, 999999999])
    /// are the exception: a number that is negative, too large or not whole is rejected.
    /// Reading stops at the first problem: a missing, non-positive or too new
    /// `schema_version`, an unknown simulation mode, output type, encoding, UDP mode or
    /// sentence id, a value outside its range, a missing required path or serial port name,
    /// or an invalid custom sentence or AIS value.
    /// Paths are kept as they are written, and `base_directory` stays empty.
    ///
    /// @param json The profile document.
    /// @param error Receives a message naming the offending key when parsing fails, such as
    ///     `outputs[0]: unknown type 'x'`; left unchanged on success. May be null.
    /// @return The profile, or `std::nullopt` when the document is invalid.
    [[nodiscard]] static std::optional<Profile> from_json(const QJsonObject& json, QString* error);

    /// Reads and parses a profile file.
    ///
    /// The paths are kept as written, and `base_directory` is set to the absolute directory
    /// that contains the file, against which `resolve_path` resolves the relative ones.
    ///
    /// @param path The profile file.
    /// @param error Receives the reason when loading fails: `Cannot read` followed by the
    ///     path and the system's reason, the path followed by `is not a JSON object` and the
    ///     parser's message, or the message of `from_json`. Left unchanged on success. May be
    ///     null.
    /// @return The profile, or `std::nullopt` when the file cannot be read, is not a JSON
    ///     object or is not a valid profile.
    [[nodiscard]] static std::optional<Profile> load(const QString& path, QString* error);
    /// Writes the profile as indented JSON, replacing the file atomically.
    ///
    /// The document is written to a temporary file that replaces `path` only when everything
    /// was written, so a failure leaves an existing file untouched. Line endings are those of
    /// the platform. The paths are written as they are held, so a profile read by `load` is
    /// saved with its paths as the user wrote them. When `base_directory` is set and differs
    /// from the directory of `path`, the relative paths are rewritten relative to the new
    /// directory, so that they still name the same files; the profile itself is not changed.
    ///
    /// @param path The file to write.
    /// @param error Receives `Cannot write` followed by the path and the system's reason when
    ///     writing fails; left unchanged on success. May be null.
    /// @return True when the file was written.
    [[nodiscard]] bool save(const QString& path, QString* error) const;

    /// Resolves a path of the profile against `base_directory`.
    ///
    /// @param path A track, replay or output file path, as held in the profile.
    /// @return `path` made absolute against `base_directory` and cleaned when it is relative
    ///     and `base_directory` is set; otherwise `path` unchanged, which includes an empty
    ///     path.
    [[nodiscard]] QString resolve_path(const QString& path) const;

    /// Builds a sentence scheduler with this profile's encoder options, sentence settings and
    /// custom sentences applied.
    ///
    /// @return The scheduler; registry sentences without a setting keep their defaults.
    [[nodiscard]] core::simulation::SentenceScheduler make_scheduler() const;
};

}  // namespace nmeasim::io
