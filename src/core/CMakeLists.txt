# nmeasim::core - the simulation engine. Pure C++20 with no Qt dependency so that it
# can be unit-tested in isolation and reused by the CLI, the desktop app and future hosts.

find_package(GeographicLib CONFIG REQUIRED)

configure_file(
    include/nmeasim/core/version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/include/nmeasim/core/version.hpp"
    @ONLY)

add_library(nmeasim_core STATIC
    src/geo/geodesic.cpp
    src/model/vessel_state.cpp
    src/nmea0183/checksum.cpp
    src/nmea0183/encoders.cpp
    src/nmea0183/fields.cpp
    src/nmea0183/registry.cpp
    src/nmea0183/sentence_builder.cpp
)
add_library(nmeasim::core ALIAS nmeasim_core)

target_include_directories(nmeasim_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)

target_compile_features(nmeasim_core PUBLIC cxx_std_20)

target_link_libraries(nmeasim_core
    PUBLIC GeographicLib::GeographicLib
    PRIVATE nmeasim::warnings nmeasim::sanitizers
)
