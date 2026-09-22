find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(nmeasim_core_tests
    core/geo/geodesic_test.cpp
    core/model/vessel_state_test.cpp
    core/nmea0183/checksum_test.cpp
    core/nmea0183/encoders_test.cpp
    core/nmea0183/fields_test.cpp
    core/nmea0183/registry_test.cpp
    core/nmea0183/sentence_builder_test.cpp
    core/physics/wind_test.cpp
    core/simulation/delta_source_test.cpp
    core/simulation/sentence_scheduler_test.cpp
    core/simulation/simulation_test.cpp
)
target_include_directories(nmeasim_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
        io/transports_test.cpp
    )
    target_include_directories(nmeasim_io_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(nmeasim_io_tests
        PRIVATE nmeasim::io Catch2::Catch2 Qt6::Core Qt6::Network Qt6::Test Qt6::WebSockets
                nmeasim::warnings nmeasim::sanitizers
    )
    catch_discover_tests(nmeasim_io_tests DISCOVERY_MODE PRE_TEST)
endif()
