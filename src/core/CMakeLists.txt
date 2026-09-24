# nmeasim::core - the simulation engine. Pure C++20 with no Qt dependency so that it
# can be unit-tested in isolation and reused by the CLI, the desktop app and future hosts.

find_package(GeographicLib CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

configure_file(
    include/nmeasim/core/version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/include/nmeasim/core/version.hpp"
    @ONLY)

# The git description is read on every build, not only at configure time, so that --version
# and the About dialog always name the commit a development build came from.
set(NMEASIM_VERSION_CPP "${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp")
add_custom_target(nmeasim_version_description
    COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DVERSION=${PROJECT_VERSION}"
            "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in"
            "-DOUTPUT=${NMEASIM_VERSION_CPP}"
            -P "${PROJECT_SOURCE_DIR}/cmake/GitDescribe.cmake"
    BYPRODUCTS "${NMEASIM_VERSION_CPP}"
    COMMENT "Reading the git description"
    VERBATIM)

add_library(nmeasim_core STATIC
    "${NMEASIM_VERSION_CPP}"
    src/ais/messages.cpp
    src/ais/sixbit.cpp
    src/geo/geodesic.cpp
    src/geo/route.cpp
    src/log/log_file.cpp
    src/model/vessel_state.cpp
    src/nmea0183/checksum.cpp
    src/nmea0183/decoder.cpp
    src/nmea0183/encoders.cpp
    src/nmea0183/fields.cpp
    src/nmea0183/registry.cpp
    src/nmea0183/sentence_builder.cpp
    src/nmea0183/tag_block.cpp
    src/physics/wind.cpp
    src/simulation/custom_sentence.cpp
    src/simulation/delta_source.cpp
    src/simulation/replay_source.cpp
    src/simulation/sentence_scheduler.cpp
    src/simulation/simulation.cpp
    src/simulation/track_source.cpp
    src/signalk/delta.cpp
    src/time/iso8601.cpp
    src/track/gpx.cpp
    src/track/kml.cpp
    src/track/track.cpp
    src/track/track_file.cpp
    src/track/xml_helpers.cpp
    src/viewsync/viewsync.cpp
)
add_library(nmeasim::core ALIAS nmeasim_core)
add_dependencies(nmeasim_core nmeasim_version_description)

target_include_directories(nmeasim_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)

target_compile_features(nmeasim_core PUBLIC cxx_std_20)

target_link_libraries(nmeasim_core
    PUBLIC GeographicLib::GeographicLib
    PRIVATE pugixml::pugixml nmeasim::warnings nmeasim::documentation_warnings
            nmeasim::sanitizers
)
