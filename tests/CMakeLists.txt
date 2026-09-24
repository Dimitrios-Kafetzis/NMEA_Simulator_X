# SPDX-License-Identifier: GPL-3.0-only
# The unit tests, added by the top-level CMakeLists.txt when NMEASIM_BUILD_TESTS is ON. Catch2
# comes from vcpkg through the `tests` manifest feature, which the development and CI presets
# enable.
#
# Builds up to four Catch2 executables and registers every test case with CTest:
#   nmeasim_core_tests  nmeasim::core, always; Catch2's own main.
#   nmeasim_io_tests    nmeasim::io, when the command-line tool or the application is built;
#                       its main (io/main.cpp) creates the QCoreApplication that sockets
#                       and timers need.
#   nmeasim_app_tests   the desktop application's widgets, when NMEASIM_BUILD_APP is ON; its
#                       main (app/main.cpp) creates an offscreen QApplication.
#   nmeasim_cli_tests   the command-line tool, when NMEASIM_BUILD_CLI is ON: runs the built
#                       nmeasim with QProcess, whose path NMEASIM_CLI_PATH gives; its main
#                       (cli/main.cpp) creates the QCoreApplication that QProcess needs.
# The compile definitions NMEASIM_FIXTURES_DIR and NMEASIM_SAMPLES_DIR give the tests the
# absolute paths of tests/fixtures and samples, so that they run from any directory.

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(nmeasim_core_tests
    core/ais/ais_test.cpp
    core/geo/geodesic_test.cpp
    core/geo/route_test.cpp
    core/log/log_file_test.cpp
    core/model/vessel_state_test.cpp
    core/nmea0183/checksum_test.cpp
    core/nmea0183/decoder_test.cpp
    core/nmea0183/encoders_test.cpp
    core/nmea0183/fields_test.cpp
    core/nmea0183/registry_test.cpp
    core/nmea0183/sentence_builder_test.cpp
    core/nmea0183/tag_block_test.cpp
    core/physics/wind_test.cpp
    core/simulation/custom_sentence_test.cpp
    core/simulation/delta_source_test.cpp
    core/simulation/replay_source_test.cpp
    core/simulation/sentence_scheduler_test.cpp
    core/simulation/simulation_test.cpp
    core/simulation/track_source_test.cpp
    core/signalk/delta_test.cpp
    core/time/iso8601_test.cpp
    core/track/gpx_test.cpp
    core/track/kml_test.cpp
    core/track/track_test.cpp
    core/version_test.cpp
    core/viewsync/viewsync_test.cpp
)
target_include_directories(nmeasim_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(nmeasim_core_tests PRIVATE
    NMEASIM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    NMEASIM_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/samples")

target_link_libraries(nmeasim_core_tests
    PRIVATE nmeasim::core Catch2::Catch2WithMain nmeasim::warnings nmeasim::sanitizers
)

# PRE_TEST discovery runs the test binary at ctest time rather than at build time, which keeps
# the build working on Windows where dependent DLLs are copied next to the binary post-build.
catch_discover_tests(nmeasim_core_tests DISCOVERY_MODE PRE_TEST)

if(NMEASIM_BUILD_CLI OR NMEASIM_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS Core Network Test WebSockets)
    add_executable(nmeasim_io_tests
        io/main.cpp
        io/profile_test.cpp
        io/simulation_runner_test.cpp
        io/transports_test.cpp
    )
    target_include_directories(nmeasim_io_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(nmeasim_io_tests PRIVATE
        NMEASIM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        NMEASIM_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/samples")
    target_link_libraries(nmeasim_io_tests
        PRIVATE nmeasim::io Catch2::Catch2 Qt6::Core Qt6::Network Qt6::Test Qt6::WebSockets
                nmeasim::warnings nmeasim::sanitizers
    )
    catch_discover_tests(nmeasim_io_tests DISCOVERY_MODE PRE_TEST)
endif()

if(NMEASIM_BUILD_CLI)
    add_executable(nmeasim_cli_tests
        cli/cli_test.cpp
        cli/main.cpp
    )
    target_compile_definitions(nmeasim_cli_tests PRIVATE
        NMEASIM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        NMEASIM_CLI_PATH="$<TARGET_FILE:nmeasim_cli>")
    target_link_libraries(nmeasim_cli_tests
        PRIVATE Catch2::Catch2 Qt6::Core Qt6::Network nmeasim::warnings nmeasim::sanitizers
    )
    # The tests run the tool, so building them builds it too.
    add_dependencies(nmeasim_cli_tests nmeasim_cli)
    catch_discover_tests(nmeasim_cli_tests DISCOVERY_MODE PRE_TEST)
endif()

if(NMEASIM_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
    add_executable(nmeasim_app_tests
        app/dials_test.cpp
        app/main.cpp
        app/main_window_test.cpp
        app/map_test.cpp
        app/screenshot_test.cpp
        app/settings_dialog_test.cpp
        app/theme_test.cpp
        app/tile_cache_test.cpp
    )
    target_include_directories(nmeasim_app_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(nmeasim_app_tests PRIVATE
        NMEASIM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    target_link_libraries(nmeasim_app_tests
        PRIVATE nmeasim::app_lib Catch2::Catch2 Qt6::Widgets Qt6::Test nmeasim::warnings nmeasim::sanitizers
    )
    catch_discover_tests(nmeasim_app_tests DISCOVERY_MODE PRE_TEST
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
