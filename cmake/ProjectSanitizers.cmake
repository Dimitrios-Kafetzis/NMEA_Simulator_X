# SPDX-License-Identifier: GPL-3.0-only
# Defines the interface target nmeasim::sanitizers, which every first-party target links
# PRIVATE. When NMEASIM_ENABLE_SANITIZERS is ON, as in the ci-linux preset, AddressSanitizer
# and UndefinedBehaviorSanitizer are enabled for every target that links this interface; with
# MSVC only AddressSanitizer is. When the option is OFF the target is empty.

add_library(nmeasim_sanitizers INTERFACE)
add_library(nmeasim::sanitizers ALIAS nmeasim_sanitizers)

if(NMEASIM_ENABLE_SANITIZERS)
    if(MSVC)
        # MSVC has no UndefinedBehaviorSanitizer, and its linker picks up the AddressSanitizer
        # runtime without extra options.
        target_compile_options(nmeasim_sanitizers INTERFACE /fsanitize=address)
        message(STATUS "Sanitizers: AddressSanitizer (MSVC)")
    else()
        # Frame pointers give complete stack traces in the sanitizer reports. The flags are
        # needed at link time as well, to bring in the sanitizer runtimes.
        set(NMEASIM_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_compile_options(nmeasim_sanitizers INTERFACE ${NMEASIM_SANITIZER_FLAGS})
        target_link_options(nmeasim_sanitizers INTERFACE ${NMEASIM_SANITIZER_FLAGS})
        message(STATUS "Sanitizers: AddressSanitizer + UndefinedBehaviorSanitizer")
    endif()
endif()
