# nmeasim::io - transports, scheduling and persistence built on the non-GUI Qt modules.
# Both the CLI and the desktop application link this library; only the desktop application
# adds Qt Widgets on top.

find_package(Qt6 REQUIRED COMPONENTS Core Network SerialPort WebSockets)
qt_standard_project_setup()

add_library(nmeasim_io STATIC
    src/serial_ports.cpp
)
add_library(nmeasim::io ALIAS nmeasim_io)

target_include_directories(nmeasim_io
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(nmeasim_io
    PUBLIC nmeasim::core Qt6::Core Qt6::Network Qt6::SerialPort Qt6::WebSockets
    PRIVATE nmeasim::warnings nmeasim::sanitizers
)
