#include <nmeasim/core/version.hpp>
#include <nmeasim/io/serial_ports.hpp>

#include <QCoreApplication>

#include <CLI/CLI.hpp>

#include <format>
#include <iostream>
#include <string>

namespace {

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

}  // namespace

int main(int argc, char** argv) {
    // Qt's non-GUI modules expect an application object to exist, even in headless tools.
    const QCoreApplication qt_application(argc, argv);

    CLI::App cli{"NMEA Simulator X - headless NMEA 0183 / Signal K data stream simulator",
                 "nmeasim"};
    cli.set_version_flag(
        "-V,--version", std::format("{} {}", nmeasim::core::kProjectName, nmeasim::core::kVersion));
    cli.require_subcommand(0, 1);

    auto* ports = cli.add_subcommand("ports", "List the serial ports available on this machine");

    CLI11_PARSE(cli, argc, argv);

    if (ports->parsed()) {
        return list_serial_ports();
    }

    std::cout << cli.help();
    return 0;
}
