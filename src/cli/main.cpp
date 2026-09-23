#include <nmeasim/core/version.hpp>
#include <nmeasim/io/network_interfaces.hpp>
#include <nmeasim/io/profile/profile.hpp>
#include <nmeasim/io/serial_ports.hpp>
#include <nmeasim/io/simulation_runner.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTimer>

#include <CLI/CLI.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

std::atomic<bool> g_interrupted{false};

void on_interrupt(int) {
    g_interrupted.store(true);
}

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

struct RunOptions {
    std::string profile_path;
    std::string track_path;
    std::string replay_path;
    std::string record_path;
    double track_speed_kn{0.0};
    bool ignore_timestamps{false};
    bool loop{false};
    int replay_interval_ms{0};
    double duration_s{0.0};
    int period_ms{0};
    bool quiet{false};
    bool use_stdout{false};
    std::vector<int> tcp_ports;
    std::vector<std::string> udp_targets;
    std::vector<int> websocket_ports;
    std::vector<std::string> serial_ports;
    std::vector<std::string> files;
    std::vector<std::string> enable;
    std::vector<std::string> disable;
    std::string encoding{"nmea0183"};
    bool tag_block{false};
    std::string tag_source;
    std::string destination;
};

/// Splits "host:port" into its parts; returns nullopt when malformed.
std::optional<std::pair<QString, quint16>> split_host_port(const std::string& value) {
    const auto colon = value.rfind(':');
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    try {
        const int port = std::stoi(value.substr(colon + 1));
        if (port < 1 || port > 65535) {
            return std::nullopt;
        }
        return std::make_pair(QString::fromStdString(value.substr(0, colon)),
                              static_cast<quint16>(port));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

/// Splits "device@baud" into its parts; the baud rate defaults to 4800.
std::optional<std::pair<QString, int>> split_serial(const std::string& value) {
    const auto at = value.rfind('@');
    if (at == std::string::npos) {
        return std::make_pair(QString::fromStdString(value), 4800);
    }
    try {
        const int baud = std::stoi(value.substr(at + 1));
        if (baud <= 0) {
            return std::nullopt;
        }
        return std::make_pair(QString::fromStdString(value.substr(0, at)), baud);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

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
        output.path = QString::fromStdString(path);
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

/// Applies --destination "lat,lon[,name]" to the profile seed.
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
    if (!lat_ok || !lon_ok || std::fabs(latitude) > 90.0 || std::fabs(longitude) > 180.0) {
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

bool apply_sentence_overrides(const RunOptions& options, nmeasim::io::Profile& profile,
                              std::string& error) {
    const auto& registry = nmeasim::core::nmea0183::SentenceRegistry::standard();
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

/// Applies --track, --replay and their companions on top of the profile's simulation mode.
void apply_mode_overrides(const RunOptions& options, nmeasim::io::Profile& profile) {
    using nmeasim::io::SimulationMode;
    if (!options.track_path.empty()) {
        profile.mode = SimulationMode::Track;
        profile.track.path = QString::fromStdString(options.track_path);
        profile.track.loop = options.loop;
        profile.track.use_timestamps = !options.ignore_timestamps;
        if (options.track_speed_kn > 0.0) {
            profile.track.speed_kn = options.track_speed_kn;
        }
    } else if (!options.replay_path.empty()) {
        profile.mode = SimulationMode::Replay;
        profile.replay.path = QString::fromStdString(options.replay_path);
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
        output.path = QString::fromStdString(options.record_path);
        output.append = false;
        profile.outputs.append(output);
    }
}

int run_simulation(const RunOptions& options) {
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
        std::cerr << std::format("stopped after {} sentences\n", runner.sentences_emitted());
    }
    return result;
}

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

    CLI11_PARSE(cli, argc, argv);

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
