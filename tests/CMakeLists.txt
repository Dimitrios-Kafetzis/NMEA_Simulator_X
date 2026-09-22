find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(nmeasim_core_tests
    core/nmea0183/checksum_test.cpp
    core/geo/geodesic_test.cpp
)

target_link_libraries(nmeasim_core_tests
    PRIVATE nmeasim::core Catch2::Catch2WithMain nmeasim::warnings nmeasim::sanitizers
)

# PRE_TEST discovery runs the test binary at ctest time rather than at build time, which keeps
# the build working on Windows where dependent DLLs are copied next to the binary post-build.
catch_discover_tests(nmeasim_core_tests DISCOVERY_MODE PRE_TEST)
