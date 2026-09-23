# When NMEASIM_ENABLE_COVERAGE is ON, every first-party target is compiled with gcov
# instrumentation (GCC or Clang). Third-party code comes prebuilt from vcpkg and is not
# instrumented. tools/coverage_summary.py turns the gcovr report into the CI summary.

if(NMEASIM_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage)
        add_link_options(--coverage)
        message(STATUS "Coverage: gcov instrumentation")
    else()
        message(WARNING "NMEASIM_ENABLE_COVERAGE is only supported with GCC and Clang")
    endif()
endif()
