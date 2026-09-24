// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Entry point of `nmeasim`, the headless command-line simulator.
///
/// The tool parses its command line with CLI11 and runs on a `QCoreApplication`, so it needs
/// no display. It links the same engine (`nmeasim::core`) and transports (`nmeasim::io`) as
/// the desktop application. Subcommands:
///
/// - `run`: loads a profile (the built-in default without `--profile`), applies the
///   command-line overrides to it and streams the delta simulation, a track or a replayed log
///   to the outputs until the duration elapses, a track or log that does not loop ends, or
///   `SIGINT` or `SIGTERM` arrives;
/// - `ports`, `interfaces` and `sentences`: print tables of the serial ports, the IPv4
///   interfaces and the sentence registry;
/// - `profile init` and `profile show`: write the default profile to a file, or print a
///   validated and migrated profile as JSON.
///
/// `-V` or `--version` prints the project name and version. Errors, warnings and the status
/// lines of `run` go to standard error, and `run` writes sentences to standard output only
/// through a `--stdout` output, so `nmeasim run --stdout --quiet` produces a clean stream.
/// The user-facing description of every option is in `docs/reference/cli.md`.

#include <nmeasim/core/version.hpp>
#include <nmeasim/io/network_interfaces.hpp>
#include <nmeasim/io/profile/profile.hpp>
#include <nmeasim/io/serial_ports.hpp>
#include <nmeasim/io/simulation_runner.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

/// Set by `on_interrupt` when `SIGINT` or `SIGTERM` arrives; never reset.
///
/// `run_simulation` polls it from the event loop and stops the runner once it is true.
/// `std::atomic` makes the write from the handler visible to the event loop, which on
/// Windows runs in a different thread from the Ctrl+C handler.
std::atomic<bool> g_interrupted{false};

/// Signal handler for `SIGINT` and `SIGTERM`, installed by `run_simulation`.
///
/// Only sets `g_interrupted`: Qt functions are not async-signal-safe, so the actual stop
/// happens in the event loop, which polls the flag. The signal number is ignored.
///
/// @note `std::signal` may reset the disposition to the default before the handler runs (the
/// Microsoft C runtime does), in which case a second Ctrl+C terminates the process at once.
void on_interrupt(int) {
    g_interrupted.store(true);
}

/// Prints the serial ports present on this machine as a table on standard output.
///
/// The columns are the device path, the driver description and the manufacturer, as the
/// operating system reports them; values it does not provide stay empty. Prints
/// `No serial ports found.` instead when there are none.
///
/// @return Always 0.
/// @see nmeasim::io::available_serial_ports
int list_serial_ports() {
    const auto ports = nmeasim::io::available_serial_ports();
    if (ports.empty()) {
        std::cout << "No serial ports found.\n";
        return 0;
    }
    std::cout << std::format("{:<28} {:<36} {}\n", "PORT", "DESCRIPTION", "MANUFACTURER");
    for (const auto& port : ports) {
        std::cout << std::format("{:<28} {:<36} {}\n", port.system_location, port.description,
                                 port.manufacturer);
    }
    return 0;
}

/// Prints every IPv4 address of every interface that is up as a table on standard output.
///
/// The columns are the interface name, the address and its subnet broadcast address. The
/// header is printed even when the list is empty.
///
/// @return Always 0.
/// @see nmeasim::io::ipv4_interfaces
int list_interfaces() {
    const auto interfaces = nmeasim::io::ipv4_interfaces();
    std::cout << std::format("{:<20} {:<18} {}\n", "INTERFACE", "ADDRESS", "BROADCAST");
    for (const auto& info : interfaces) {
        std::cout << std::format("{:<20} {:<18} {}\n", info.name.toStdString(),
                                 info.address.toString().toStdString(),
                                 info.broadcast.toString().toStdString());
    }
    return 0;
}

/// Prints every sentence of the standard registry as a table on standard output.
///
/// The columns are the registry id (the value `--enable` and `--disable` take), the
/// formatter, the default talker, the group, whether the sentence is `on` or `off` by
/// default, and the description, in registry order.
///
/// @return Always 0.
/// @see nmeasim::core::nmea0183::SentenceRegistry::standard
int list_sentences() {
    const auto& registry = nmeasim::core::nmea0183::SentenceRegistry::standard();
    std::cout << std::format("{:<7} {:<4} {:<7} {:<9} {:<8} {}\n", "ID", "FMT", "TALKER", "GROUP",
                             "DEFAULT", "DESCRIPTION");
    for (const auto& descriptor : registry.descriptors()) {
        std::cout << std::format(
            "{:<7} {:<4} {:<7} {:<9} {:<8} {}\n", descriptor.id, descriptor.formatter,
            descriptor.default_talker, nmeasim::core::nmea0183::to_string(descriptor.group),
            descriptor.enabled_by_default ? "on" : "off", descriptor.description);
    }
    return 0;
}

/// The options of the `run` subcommand, as CLI11 parsed them.
///
/// Each field is bound to one flag in `main`. A field's initial value is what a flag that is
/// not given leaves, and for most fields it means "keep what the profile says".
/// `run_simulation` applies the fields on top of the loaded profile through
/// `apply_output_overrides`, `apply_sentence_overrides`, `apply_destination` and
/// `apply_mode_overrides`. CLI11 ensures that `track_path` and `replay_path` are not both
/// set, that `track_speed_kn` and `ignore_timestamps` are only given with `track_path`, that
/// `replay_interval_ms` is only given with `replay_path` and that `tag_source` is only given
/// with `tag_block`.
struct RunOptions {
    /// Profile file, `-p` or `--profile`; CLI11 checks that it exists. Empty runs the
    /// built-in default profile.
    std::string profile_path;
    /// GPX or KML track to follow, `--track`; CLI11 checks that it exists. Empty keeps the
    /// profile's mode.
    std::string track_path;
    /// Log file to replay, `--replay`; CLI11 checks that it exists. Empty keeps the profile's
    /// mode.
    std::string replay_path;
    /// Log file that records every sentence with a timestamp, `--record`; truncated when the
    /// run starts. Empty records nothing.
    std::string record_path;
    /// Speed along untimed track legs, `--speed`; also the speed of a timed track with
    /// `ignore_timestamps`. Zero or negative keeps the profile's `simulation.track.speed_kn`;
    /// `check_finite_options` refuses `nan` and `inf`.
    double track_speed_kn{0.0};
    /// `--ignore-timestamps`: sails a timed track at the track speed instead of on its own
    /// timing.
    bool ignore_timestamps{false};
    /// `--loop`: starts the track or log again at its end instead of ending the run.
    bool loop{false};
    /// Spacing of the sentences of a log without any time information, `--replay-interval`;
    /// CLI11 accepts [1, 60000]. Zero keeps the profile's
    /// `simulation.replay.fixed_interval_ms`.
    int replay_interval_ms{0};
    /// Wall-clock run time, `-d` or `--duration`; truncated to whole milliseconds. Zero or
    /// negative sets no limit; `check_finite_options` refuses `nan` and `inf`.
    double duration_s{0.0};
    /// Period of every registry sentence in milliseconds, `-r` or `--rate`; overrides the
    /// profile's per-sentence periods. Zero or negative keeps them.
    int period_ms{0};
    /// `-q` or `--quiet`: suppresses the status lines on standard error. Errors and output
    /// warnings are still printed.
    bool quiet{false};
    /// `--stdout`: adds an output that writes to standard output.
    bool use_stdout{false};
    /// Ports of TCP server outputs, `--tcp-server`, repeatable; CLI11 accepts [1, 65535].
    std::vector<int> tcp_ports;
    /// Destinations of UDP outputs as `host:port`, `--udp`, repeatable; parsed by
    /// `split_host_port`. The host `255.255.255.255` selects broadcast.
    std::vector<std::string> udp_targets;
    /// Ports of WebSocket server outputs, `--websocket`, repeatable; CLI11 accepts
    /// [1, 65535].
    std::vector<int> websocket_ports;
    /// Serial outputs as `device[@baud]`, `--serial`, repeatable; parsed by `split_serial`.
    std::vector<std::string> serial_ports;
    /// Files that sentences are appended to, `--file`, repeatable.
    std::vector<std::string> files;
    /// Registry ids to switch on, `--enable`, repeatable; for example `MWV-T`.
    std::vector<std::string> enable;
    /// Registry ids to switch off, `--disable`, repeatable. Applied after `enable`, so an id
    /// given to both ends up off.
    std::vector<std::string> disable;
    /// Encoding of the command-line outputs, `--encoding`: `nmea0183`, `signalk` or
    /// `viewsync`. Outputs that come from the profile keep their own encoding.
    std::string encoding{"nmea0183"};
    /// `--tag-block`: prefixes every sentence of the command-line outputs with an
    /// IEC 61162-450 TAG block.
    bool tag_block{false};
    /// TAG block source identifier, `--tag-source`; empty keeps the default `SIM0001`.
    std::string tag_source;
    /// Waypoint to steer for as `LAT,LON[,NAME]` in decimal degrees, `--destination`;
    /// parsed by `apply_destination`. Empty sets no destination.
    std::string destination;
};

/// Reads a whole string as a decimal number made of digits only.
///
/// Unlike `std::stoi`, it accepts no sign, no white space and no characters after the digits.
///
/// @param text The text to read, for example `10110`.
/// @return The number, or `std::nullopt` when the text is empty, contains anything but the
/// digits 0 to 9, or does not fit in an `int`.
std::optional<int> parse_digits(std::string_view text) {
    if (text.empty() || !std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; })) {
        return std::nullopt;
    }
    int value = 0;
    const auto* const end = text.data() + text.size();
    const auto [stop, status] = std::from_chars(text.data(), end, value);
    if (status != std::errc{} || stop != end) {
        return std::nullopt;
    }
    return value;
}

/// Splits a `--udp` argument of the form `host:port` into the host and the port.
///
/// The split is at the last colon. The host must not be empty and is otherwise not checked;
/// the port must be digits only (see `parse_digits`).
///
/// @param value The argument, for example `192.168.1.20:10110`.
/// @return The host and the port, or `std::nullopt` when there is no colon, the host is
/// empty or the port is not a whole number in [1, 65535].
std::optional<std::pair<QString, quint16>> split_host_port(const std::string& value) {
    const auto colon = value.rfind(':');
    if (colon == std::string::npos || colon == 0) {
        return std::nullopt;
    }
    const auto port = parse_digits(std::string_view{value}.substr(colon + 1));
    if (!port || *port < 1 || *port > 65535) {
        return std::nullopt;
    }
    return std::make_pair(QString::fromStdString(value.substr(0, colon)),
                          static_cast<quint16>(*port));
}

/// Splits a `--serial` argument of the form `device[@baud]` into the device and the baud
/// rate.
///
/// The split is at the last `@`. Without one the whole argument is the device and the baud
/// rate is 4800, the standard rate of NMEA 0183 (IEC 61162-1) talkers. The baud rate must be
/// digits only (see `parse_digits`) and is not checked against the rates the port supports.
///
/// @param value The argument, for example `/dev/ttyUSB0@38400` or `COM3`.
/// @return The device and the baud rate, or `std::nullopt` when the device is empty or the
/// part after the `@` is not a positive whole number.
std::optional<std::pair<QString, int>> split_serial(const std::string& value) {
    const auto at = value.rfind('@');
    if (at == std::string::npos) {
        if (value.empty()) {
            return std::nullopt;
        }
        return std::make_pair(QString::fromStdString(value), 4800);
    }
    const auto baud = parse_digits(std::string_view{value}.substr(at + 1));
    if (at == 0 || !baud || *baud <= 0) {
        return std::nullopt;
    }
    return std::make_pair(QString::fromStdString(value.substr(0, at)), *baud);
}

/// Refuses the non-finite values that CLI11 converts `nan` and `inf` to.
///
/// CLI11 reads `--duration` and `--speed` as floating-point numbers and accepts `nan`, `inf`
/// and `infinity`, which would make a run endless or overflow the duration timer.
///
/// @param options The parsed `run` options; `duration_s` and `track_speed_kn` are read.
/// @param[out] error Set to a message for the user when the function returns false.
/// @return True when both values are finite numbers.
bool check_finite_options(const RunOptions& options, std::string& error) {
    if (!std::isfinite(options.duration_s)) {
        error = "--duration must be a finite number of seconds";
        return false;
    }
    if (!std::isfinite(options.track_speed_kn)) {
        error = "--speed must be a finite number of knots";
        return false;
    }
    return true;
}

/// Replaces the outputs of `profile` with the outputs given on the command line.
///
/// Does nothing when none of `--stdout`, `--tcp-server`, `--udp`, `--websocket`, `--serial`
/// and `--file` is given. Otherwise it clears the profile's outputs and appends, in this
/// order, the TCP servers, UDP senders, WebSocket servers, serial ports, files and finally
/// standard output. Everything an option does not carry keeps the default of
/// `nmeasim::io::OutputConfig`: servers listen on every interface and files are appended to.
/// Every new output gets the `--encoding`, and a TAG block when `--tag-block` is given, with
/// the `--tag-source` identifier when that is not empty.
///
/// @param options The parsed `run` options.
/// @param[in,out] profile The profile to change.
/// @param[out] error Set to a message for the user when the function returns false.
/// @return True on success, including when there is nothing to replace; false when
/// `--encoding` is not an encoding name or a `--udp` or `--serial` argument is malformed, in
/// which case `profile` may be left partly changed.
/// @note `--encoding`, `--tag-block` and `--tag-source` are only checked and applied here,
/// so without an output option they have no effect.
bool apply_output_overrides(const RunOptions& options, nmeasim::io::Profile& profile,
                            std::string& error) {
    using nmeasim::io::OutputConfig;
    const bool any = options.use_stdout || !options.tcp_ports.empty() ||
                     !options.udp_targets.empty() || !options.websocket_ports.empty() ||
                     !options.serial_ports.empty() || !options.files.empty();
    if (!any) {
        return true;
    }
    const auto encoding =
        nmeasim::io::encoding_from_string(QString::fromStdString(options.encoding));
    if (!encoding) {
        error = std::format("--encoding must be nmea0183, signalk or viewsync, got '{}'",
                            options.encoding);
        return false;
    }
    profile.outputs.clear();
    for (const int port : options.tcp_ports) {
        OutputConfig output;
        output.type = OutputConfig::Type::TcpServer;
        output.port = static_cast<quint16>(port);
        profile.outputs.append(output);
    }
    for (const auto& target : options.udp_targets) {
        const auto parts = split_host_port(target);
        if (!parts) {
            error = std::format("--udp expects host:port, got '{}'", target);
            return false;
        }
        OutputConfig output;
        output.type = OutputConfig::Type::Udp;
        output.udp.mode = parts->first == QLatin1String("255.255.255.255")
                              ? nmeasim::io::UdpConfig::Mode::Broadcast
                              : nmeasim::io::UdpConfig::Mode::Unicast;
        output.udp.address = parts->first;
        output.udp.port = parts->second;
        // Profile::from_json sets both ports from the one `port` key; keep them equal in the
        // same way so that the output looks like one loaded from a profile.
        output.port = parts->second;
        profile.outputs.append(output);
    }
    for (const int port : options.websocket_ports) {
        OutputConfig output;
        output.type = OutputConfig::Type::WebSocketServer;
        output.port = static_cast<quint16>(port);
        profile.outputs.append(output);
    }
    for (const auto& serial : options.serial_ports) {
        const auto parts = split_serial(serial);
        if (!parts) {
            error = std::format("--serial expects device[@baud], got '{}'", serial);
            return false;
        }
        OutputConfig output;
        output.type = OutputConfig::Type::Serial;
        output.serial.port_name = parts->first;
        output.serial.baud_rate = parts->second;
        profile.outputs.append(output);
    }
    for (const auto& path : options.files) {
        OutputConfig output;
        output.type = OutputConfig::Type::File;
        // Made absolute: a relative path in a loaded profile is relative to the profile file,
        // but one given on the command line is relative to the working directory.
        output.path = QFileInfo(QString::fromStdString(path)).absoluteFilePath();
        profile.outputs.append(output);
    }
    if (options.use_stdout) {
        OutputConfig output;
        output.type = OutputConfig::Type::Stdout;
        profile.outputs.append(output);
    }
    for (auto& output : profile.outputs) {
        output.encoding = *encoding;
        output.tag_block.enabled = options.tag_block;
        if (!options.tag_source.empty()) {
            output.tag_block.options.source = options.tag_source;
        }
    }
    return true;
}

/// Sets the destination of the simulation seed from `--destination`.
///
/// The argument is `LAT,LON[,NAME]`: latitude in [-90, 90] positive north and longitude in
/// [-180, 180] positive east, both finite decimal degrees, and an optional waypoint name. White
/// space around each part is ignored, parts after the third are ignored, and an empty name
/// keeps the default `WPT`. The leg starts at the seed position of the profile, also when
/// the run follows a track. A destination makes the simulation send APB, RMB and XTE and the
/// Signal K course paths.
///
/// @param options The parsed `run` options; only `destination` is read.
/// @param[in,out] profile The profile whose `delta.seed.destination` is set.
/// @param[out] error Set to a message for the user when the function returns false.
/// @return True when `--destination` is empty or valid; false when it has fewer than two
/// parts, a coordinate is not a number, is `nan` or infinite, or is out of range, in which
/// case `profile` is unchanged.
/// @see nmeasim::core::model::Destination
bool apply_destination(const RunOptions& options, nmeasim::io::Profile& profile,
                       std::string& error) {
    if (options.destination.empty()) {
        return true;
    }
    const auto parts = QString::fromStdString(options.destination).split(QLatin1Char(','));
    bool lat_ok = false;
    bool lon_ok = false;
    const double latitude = parts.size() >= 2 ? parts[0].trimmed().toDouble(&lat_ok) : 0.0;
    const double longitude = parts.size() >= 2 ? parts[1].trimmed().toDouble(&lon_ok) : 0.0;
    // QString::toDouble accepts nan and inf, which no range comparison refuses.
    if (!lat_ok || !lon_ok || !std::isfinite(latitude) || !std::isfinite(longitude) ||
        std::fabs(latitude) > 90.0 || std::fabs(longitude) > 180.0) {
        error = std::format("--destination expects LAT,LON[,NAME], got '{}'", options.destination);
        return false;
    }
    nmeasim::core::model::Destination destination;
    destination.position = {latitude, longitude};
    destination.origin = profile.delta.seed.navigation.position;
    if (parts.size() >= 3 && !parts[2].trimmed().isEmpty()) {
        destination.name = parts[2].trimmed().toStdString();
    }
    profile.delta.seed.destination = destination;
    return true;
}

/// Applies `--enable`, `--disable` and `--rate` on top of the profile's sentence settings.
///
/// A registry id that the profile has no setting for first gets one with the registry
/// defaults (enabled state, talker and period). The ids of `--enable` are switched on
/// first, then those of `--disable` are switched off. A positive `--rate` then sets the
/// period of every registry sentence, which gives every one of them a setting; custom
/// sentences and the message periods of Signal K and ViewSync outputs are not affected.
///
/// @param options The parsed `run` options; `enable`, `disable` and `period_ms` are read.
/// @param[in,out] profile The profile whose `sentences` map is changed.
/// @param[out] error Set to a message for the user when the function returns false.
/// @return True on success; false when an id of `--enable` or `--disable` is not in the
/// standard registry, in which case the ids before it have already been applied.
bool apply_sentence_overrides(const RunOptions& options, nmeasim::io::Profile& profile,
                              std::string& error) {
    const auto& registry = nmeasim::core::nmea0183::SentenceRegistry::standard();
    // Returns the profile's setting for `id`, created from the registry defaults when the
    // profile has none, or null when the registry does not know the id.
    auto setting_for = [&](const std::string& id) -> nmeasim::core::simulation::SentenceSetting* {
        const auto* descriptor = registry.find(id);
        if (descriptor == nullptr) {
            return nullptr;
        }
        auto [it, inserted] = profile.sentences.try_emplace(
            id, nmeasim::core::simulation::SentenceSetting{descriptor->enabled_by_default, "",
                                                           descriptor->default_period});
        return &it->second;
    };
    for (const auto& id : options.enable) {
        auto* setting = setting_for(id);
        if (setting == nullptr) {
            error = std::format("--enable: unknown sentence id '{}'", id);
            return false;
        }
        setting->enabled = true;
    }
    for (const auto& id : options.disable) {
        auto* setting = setting_for(id);
        if (setting == nullptr) {
            error = std::format("--disable: unknown sentence id '{}'", id);
            return false;
        }
        setting->enabled = false;
    }
    if (options.period_ms > 0) {
        for (const auto& descriptor : registry.descriptors()) {
            auto* setting = setting_for(std::string{descriptor.id});
            setting->period = std::chrono::milliseconds{options.period_ms};
        }
    }
    return true;
}

/// Applies `--track`, `--replay`, their companion options and `--record` to `profile`.
///
/// - With `--track`, the mode becomes `nmeasim::io::SimulationMode::Track` and the track
///   settings are replaced: the path, `loop` from `--loop`, `use_timestamps` unless
///   `--ignore-timestamps` is given, and the speed when `--speed` is positive (otherwise the
///   profile's speed stays).
/// - With `--replay`, the mode becomes `nmeasim::io::SimulationMode::Replay` with the path,
///   `loop` from `--loop` and the fixed interval when `--replay-interval` is positive.
/// - With neither, `--loop` sets the loop flag of both the profile's track and replay
///   settings, so that a profile already in one of those modes loops; without `--loop` the
///   profile's flags stay.
///
/// `--record` then appends a log output that truncates its file. `run_simulation` calls this
/// function after `apply_output_overrides`, so the log output joins whichever outputs the
/// run has, the profile's or the command line's, and takes neither `--encoding` nor the TAG
/// block. The paths are made absolute against the working directory, since a relative path
/// held in a loaded profile is resolved against the profile file's directory.
///
/// @param options The parsed `run` options.
/// @param[in,out] profile The profile to change.
/// @note The track or log file is only read later, by
/// `nmeasim::io::SimulationRunner::apply_profile`.
void apply_mode_overrides(const RunOptions& options, nmeasim::io::Profile& profile) {
    using nmeasim::io::SimulationMode;
    if (!options.track_path.empty()) {
        profile.mode = SimulationMode::Track;
        profile.track.path =
            QFileInfo(QString::fromStdString(options.track_path)).absoluteFilePath();
        profile.track.loop = options.loop;
        profile.track.use_timestamps = !options.ignore_timestamps;
        if (options.track_speed_kn > 0.0) {
            profile.track.speed_kn = options.track_speed_kn;
        }
    } else if (!options.replay_path.empty()) {
        profile.mode = SimulationMode::Replay;
        profile.replay.path =
            QFileInfo(QString::fromStdString(options.replay_path)).absoluteFilePath();
        profile.replay.loop = options.loop;
        if (options.replay_interval_ms > 0) {
            profile.replay.fixed_interval_ms = options.replay_interval_ms;
        }
    } else if (options.loop) {
        profile.track.loop = true;
        profile.replay.loop = true;
    }
    if (!options.record_path.empty()) {
        nmeasim::io::OutputConfig output;
        output.type = nmeasim::io::OutputConfig::Type::Log;
        output.path = QFileInfo(QString::fromStdString(options.record_path)).absoluteFilePath();
        output.append = false;
        profile.outputs.append(output);
    }
}

/// Runs the `run` subcommand: builds the profile, streams it and returns the exit status.
///
/// Checks the numbers with `check_finite_options`, loads the profile of `--profile`, or the
/// built-in default, and applies `apply_output_overrides`, `apply_sentence_overrides`,
/// `apply_destination` and `apply_mode_overrides` in this order. It then applies the profile to a
/// `nmeasim::io::SimulationRunner`, starts it and runs the Qt event loop until the runner
/// emits `stopped`, which happens when:
///
/// - `SIGINT` or `SIGTERM` arrives (see `on_interrupt`; the flag is polled every 100 ms);
/// - `--duration` elapses, counted in wall-clock time from just before the event loop
///   starts;
/// - a track or log that does not loop reaches its end (the runner emits `finished`, then
///   stops itself).
///
/// Unless `--quiet` is given it prints to standard error each output that opened, a line
/// naming the profile, the track or log, its length and the tick, `end of the track or log
/// reached` when a finite source ends, and when it stops the count of
/// `nmeasim::io::SimulationRunner::sentences_emitted`, followed by that of
/// `nmeasim::io::SimulationRunner::state_messages_sent` when there were any.
/// A failure of one output while others work is printed as a warning and the run continues.
///
/// @param options The parsed `run` options.
/// @return 0 after a normal stop; 2 when a number is not finite, the profile cannot be
/// loaded or applied, an override is invalid or the run would have no outputs; 3 when none
/// of the outputs could be opened.
/// @pre A `QCoreApplication` exists.
/// @note Installs `on_interrupt` for `SIGINT` and `SIGTERM` and leaves it installed.
int run_simulation(const RunOptions& options) {
    if (std::string error; !check_finite_options(options, error)) {
        std::cerr << "error: " << error << '\n';
        return 2;
    }
    nmeasim::io::Profile profile = nmeasim::io::Profile::default_profile();
    if (!options.profile_path.empty()) {
        QString error;
        auto loaded =
            nmeasim::io::Profile::load(QString::fromStdString(options.profile_path), &error);
        if (!loaded) {
            std::cerr << "error: " << error.toStdString() << '\n';
            return 2;
        }
        profile = std::move(*loaded);
    }
    std::string error;
    if (!apply_output_overrides(options, profile, error) ||
        !apply_sentence_overrides(options, profile, error) ||
        !apply_destination(options, profile, error)) {
        std::cerr << "error: " << error << '\n';
        return 2;
    }
    apply_mode_overrides(options, profile);
    if (profile.outputs.isEmpty()) {
        std::cerr << "error: the profile defines no outputs\n";
        return 2;
    }

    nmeasim::io::SimulationRunner runner;
    QString apply_error;
    if (!runner.apply_profile(profile, &apply_error)) {
        std::cerr << "error: " << apply_error.toStdString() << '\n';
        return 2;
    }
    QObject::connect(&runner, &nmeasim::io::SimulationRunner::output_error, &runner,
                     [](const QString& description, const QString& message) {
                         std::cerr << "warning: " << description.toStdString() << ": "
                                   << message.toStdString() << '\n';
                     });

    runner.start();
    int open_outputs = 0;
    for (const auto& channel : runner.outputs()) {
        if (channel.transport->is_open()) {
            ++open_outputs;
            if (!options.quiet) {
                std::cerr << "output: " << channel.transport->description().toStdString() << '\n';
            }
        }
    }
    if (open_outputs == 0) {
        std::cerr << "error: no output could be opened\n";
        return 3;
    }
    if (!options.quiet) {
        std::string source;
        if (profile.mode == nmeasim::io::SimulationMode::Track) {
            source = std::format(" following {}", profile.track.path.toStdString());
        } else if (profile.mode == nmeasim::io::SimulationMode::Replay) {
            source = std::format(" replaying {}", profile.replay.path.toStdString());
        }
        if (const auto duration = runner.duration()) {
            source += std::format(" ({:.1f} s{})", std::chrono::duration<double>(*duration).count(),
                                  options.loop ? ", looping" : "");
        }
        std::cerr << std::format("running profile '{}'{} with a {} ms tick; press Ctrl+C to stop\n",
                                 profile.name.toStdString(), source, profile.tick_ms);
    }
    QObject::connect(&runner, &nmeasim::io::SimulationRunner::finished, &runner, [&options] {
        if (!options.quiet) {
            std::cerr << "end of the track or log reached\n";
        }
    });

    // Qt must not be called from a signal handler, so the handler only sets a flag and this
    // timer turns it into a stop inside the event loop. 100 ms keeps Ctrl+C responsive at a
    // negligible cost.
    std::signal(SIGINT, on_interrupt);
    std::signal(SIGTERM, on_interrupt);
    QTimer interrupt_poll;
    QObject::connect(&interrupt_poll, &QTimer::timeout, &runner, [&runner] {
        if (g_interrupted.load()) {
            runner.stop();
        }
    });
    interrupt_poll.start(100);
    QObject::connect(&runner, &nmeasim::io::SimulationRunner::stopped, qApp,
                     &QCoreApplication::quit);
    if (options.duration_s > 0.0) {
        QTimer::singleShot(
            std::chrono::milliseconds{static_cast<long long>(options.duration_s * 1000.0)}, &runner,
            [&runner] { runner.stop(); });
    }
    const int result = QCoreApplication::exec();
    if (!options.quiet) {
        const auto messages = runner.state_messages_sent();
        std::cerr << (messages > 0 ? std::format("stopped after {} sentences and {} Signal K or "
                                                 "ViewSync messages\n",
                                                 runner.sentences_emitted(), messages)
                                   : std::format("stopped after {} sentences\n",
                                                 runner.sentences_emitted()));
    }
    return result;
}

/// Runs `profile init`: writes the built-in default profile to `path` as indented JSON.
///
/// Prints `wrote PATH` to standard output on success and an error to standard error
/// otherwise.
///
/// @param path The file to write, as given on the command line.
/// @param force Overwrites an existing file when true (`-f` or `--force`); when false an
/// existing file is left untouched and reported as an error.
/// @return 0 when the file was written; 2 when it exists and `force` is false, or when it
/// could not be written.
/// @see nmeasim::io::Profile::save
int write_default_profile(const std::string& path, bool force) {
    const QString qpath = QString::fromStdString(path);
    if (!force && QFile::exists(qpath)) {
        std::cerr << "error: " << path << " exists; use --force to overwrite\n";
        return 2;
    }
    QString error;
    if (!nmeasim::io::Profile::default_profile().save(qpath, &error)) {
        std::cerr << "error: " << error.toStdString() << '\n';
        return 2;
    }
    std::cout << "wrote " << path << '\n';
    return 0;
}

/// Runs `profile show`: prints a profile as indented JSON on standard output.
///
/// The file is loaded, validated and migrated to the current schema version, then written
/// out again, so the output shows the profile as the simulator reads it.
///
/// @param path The profile file; empty prints the built-in default profile.
/// @return 0 on success; 2 when the file cannot be loaded or is not a valid profile, after
/// printing the reason to standard error.
/// @see nmeasim::io::Profile::load
int show_profile(const std::string& path) {
    nmeasim::io::Profile profile = nmeasim::io::Profile::default_profile();
    if (!path.empty()) {
        QString error;
        auto loaded = nmeasim::io::Profile::load(QString::fromStdString(path), &error);
        if (!loaded) {
            std::cerr << "error: " << error.toStdString() << '\n';
            return 2;
        }
        profile = std::move(*loaded);
    }
    std::cout << QJsonDocument(profile.to_json()).toJson(QJsonDocument::Indented).toStdString();
    return 0;
}

}  // namespace

/// Entry point of `nmeasim`: parses the command line and runs the chosen subcommand.
///
/// Creates the `QCoreApplication`, declares the subcommands and options with CLI11 (at most
/// one top-level subcommand; `profile` needs `init` or `show`), then dispatches to
/// `list_serial_ports`, `list_interfaces`, `list_sentences`, `run_simulation`,
/// `write_default_profile` or `show_profile`. Without a subcommand it prints the help to
/// standard output.
///
/// @param argc Number of command-line arguments, including the program name.
/// @param argv The command-line arguments; `argv[0]` is the program name.
/// @return The exit status:
/// - 0 on success, after `--help` or `--version`, and when no subcommand is given;
/// - 2 when the command line cannot be parsed (an unknown option or subcommand, a missing or
///   malformed value, a missing file, a value outside the range an option accepts, options
///   that exclude or need each other), after CLI11 printed the reason to standard error, and
///   when `run` or `profile` rejects its input (see `run_simulation`,
///   `write_default_profile` and `show_profile`);
/// - 3 when `run` could open none of its outputs.
int main(int argc, char** argv) {
    // Qt's non-GUI modules expect an application object to exist, even in headless tools.
    QCoreApplication qt_application(argc, argv);

    CLI::App cli{"NMEA Simulator X - headless NMEA 0183 / Signal K data stream simulator",
                 "nmeasim"};
    cli.set_version_flag("-V,--version", std::format("{} {}", nmeasim::core::kProjectName,
                                                     nmeasim::core::version_description()));
    cli.require_subcommand(0, 1);

    auto* ports = cli.add_subcommand("ports", "List the serial ports available on this machine");
    auto* interfaces = cli.add_subcommand("interfaces", "List IPv4 network interfaces");
    auto* sentences = cli.add_subcommand("sentences", "List the sentences the simulator can emit");

    RunOptions options;
    auto* run =
        cli.add_subcommand("run", "Run a simulation and stream it to the configured outputs");
    run->add_option("-p,--profile", options.profile_path,
                    "Profile JSON file (default: built-in profile)")
        ->check(CLI::ExistingFile);
    run->add_option("-d,--duration", options.duration_s,
                    "Stop after this many seconds (0 = run until Ctrl+C)");
    run->add_option("-r,--rate", options.period_ms,
                    "Send every sentence at this period in milliseconds");
    auto* track = run->add_option("--track", options.track_path,
                                  "Follow a GPX or KML track instead of the delta simulation")
                      ->check(CLI::ExistingFile);
    auto* replay = run->add_option("--replay", options.replay_path,
                                   "Replay a recorded or plain NMEA log instead of simulating")
                       ->check(CLI::ExistingFile)
                       ->excludes(track);
    run->add_option("--speed", options.track_speed_kn,
                    "Speed in knots along track legs without timestamps (default: profile)")
        ->needs(track);
    run->add_flag("--ignore-timestamps", options.ignore_timestamps,
                  "Sail a timed track at --speed instead of its own timing")
        ->needs(track);
    run->add_option("--replay-interval", options.replay_interval_ms,
                    "Milliseconds between sentences of a log without any time information")
        ->needs(replay)
        ->check(CLI::Range(1, 60000));
    run->add_flag("--loop", options.loop, "Start the track or log again at its end");
    run->add_option("--record", options.record_path,
                    "Also record every sentence with timestamps to this log file");
    run->add_flag("-q,--quiet", options.quiet, "Do not print status messages to stderr");
    run->add_flag("--stdout", options.use_stdout, "Write sentences to standard output");
    run->add_option("--tcp-server", options.tcp_ports,
                    "Serve sentences on this TCP port (repeatable)")
        ->check(CLI::Range(1, 65535));
    run->add_option("--udp", options.udp_targets, "Send datagrams to host:port (repeatable)");
    run->add_option("--websocket", options.websocket_ports,
                    "Serve sentences on this WebSocket port (repeatable)")
        ->check(CLI::Range(1, 65535));
    run->add_option("--serial", options.serial_ports,
                    "Write to a serial device, as device[@baud] (repeatable)");
    run->add_option("--file", options.files, "Append sentences to this file (repeatable)");
    run->add_option("--enable", options.enable, "Enable a sentence id such as MWV-T (repeatable)");
    run->add_option("--disable", options.disable, "Disable a sentence id such as GSV (repeatable)");
    run->add_option("--encoding", options.encoding,
                    "Encoding of the outputs given on the command line: nmea0183 (default), "
                    "signalk or viewsync");
    run->add_flag("--tag-block", options.tag_block,
                  "Prefix every sentence of the command-line outputs with an IEC 61162-450 "
                  "TAG block");
    run->add_option("--tag-source", options.tag_source,
                    "Source identifier of the TAG block (default: SIM0001)")
        ->needs("--tag-block");
    run->add_option("--destination", options.destination,
                    "Steer for a waypoint given as LAT,LON[,NAME] so that APB, RMB and XTE "
                    "are sent");

    auto* profile = cli.add_subcommand("profile", "Create or inspect profile files");
    profile->require_subcommand(1);
    std::string init_path;
    bool force = false;
    auto* init = profile->add_subcommand("init", "Write the default profile to a file");
    init->add_option("path", init_path, "Destination file")->required();
    init->add_flag("-f,--force", force, "Overwrite an existing file");
    std::string show_path;
    auto* show = profile->add_subcommand("show", "Print a profile after validation and migration");
    show->add_option("path", show_path, "Profile file (default: built-in profile)");

    try {
        cli.parse(argc, argv);
    } catch (const CLI::ParseError& parse_error) {
        // CLI11 reports --help and --version as parse errors with its success code, and every
        // real error with a code of its own from 100 upwards; the tool promises 2 for all of
        // those. exit() prints the help, the version or the error message.
        const int code = cli.exit(parse_error);
        return code == static_cast<int>(CLI::ExitCodes::Success) ? 0 : 2;
    }

    if (ports->parsed()) {
        return list_serial_ports();
    }
    if (interfaces->parsed()) {
        return list_interfaces();
    }
    if (sentences->parsed()) {
        return list_sentences();
    }
    if (run->parsed()) {
        return run_simulation(options);
    }
    if (init->parsed()) {
        return write_default_profile(init_path, force);
    }
    if (show->parsed()) {
        return show_profile(show_path);
    }
    std::cout << cli.help();
    return 0;
}
