# SPDX-License-Identifier: GPL-3.0-only
# nmeasim::io - transports, scheduling and persistence built on the non-GUI Qt modules.
# Both the CLI and the desktop application link this library; only the desktop application
# adds Qt Widgets on top.
#
# Builds the static library nmeasim_io (alias nmeasim::io) on nmeasim::core and Qt Core,
# Network, Serial Port and WebSockets. The top-level CMakeLists.txt adds this directory only
# when NMEASIM_BUILD_CLI or NMEASIM_BUILD_APP is ON.

find_package(Qt6 REQUIRED COMPONENTS Core Network SerialPort WebSockets)
# Turns on AUTOMOC for this directory: the transports and the simulation runner are QObjects.
qt_standard_project_setup()

# The headers are listed so that AUTOMOC finds the Q_OBJECT classes declared in them.
add_library(nmeasim_io STATIC
    include/nmeasim/io/network_interfaces.hpp
    include/nmeasim/io/profile/profile.hpp
    include/nmeasim/io/serial_ports.hpp
    include/nmeasim/io/simulation_runner.hpp
    include/nmeasim/io/transport.hpp
    include/nmeasim/io/transports/file_transport.hpp
    include/nmeasim/io/transports/log_transport.hpp
    include/nmeasim/io/transports/serial_transport.hpp
    include/nmeasim/io/transports/stdout_transport.hpp
    include/nmeasim/io/transports/tcp_client_transport.hpp
    include/nmeasim/io/transports/tcp_server_transport.hpp
    include/nmeasim/io/transports/udp_transport.hpp
    include/nmeasim/io/transports/websocket_server_transport.hpp
    src/network_interfaces.cpp
    src/profile/profile.cpp
    src/serial_ports.cpp
    src/simulation_runner.cpp
    src/transport.cpp
    src/transports/file_transport.cpp
    src/transports/log_transport.cpp
    src/transports/serial_transport.cpp
    src/transports/stdout_transport.cpp
    src/transports/tcp_client_transport.cpp
    src/transports/tcp_server_transport.cpp
    src/transports/udp_transport.cpp
    src/transports/websocket_server_transport.cpp
)
add_library(nmeasim::io ALIAS nmeasim_io)

target_include_directories(nmeasim_io
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(nmeasim_io
    PUBLIC nmeasim::core Qt6::Core Qt6::Network Qt6::SerialPort Qt6::WebSockets
    PRIVATE nmeasim::warnings nmeasim::sanitizers
)
