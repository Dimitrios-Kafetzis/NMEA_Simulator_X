# SPDX-License-Identifier: GPL-3.0-only
# gcov instrumentation, included by the top-level CMakeLists.txt before any target is defined.
# When NMEASIM_ENABLE_COVERAGE is ON, as in the ci-linux preset, every first-party target is
# compiled with gcov instrumentation (GCC or Clang). Third-party code comes prebuilt from vcpkg
# and is not instrumented. The Linux (GCC) CI job turns the counters into a gcovr report, and
# tools/coverage_summary.py turns that report into the CI summary.

if(NMEASIM_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Directory-wide options rather than an interface target, so that they reach every
        # target defined after this point, the tests included, without a link in each one.
        add_compile_options(--coverage)
        add_link_options(--coverage)
        message(STATUS "Coverage: gcov instrumentation")
    else()
        message(WARNING "NMEASIM_ENABLE_COVERAGE is only supported with GCC and Clang")
    endif()
endif()
