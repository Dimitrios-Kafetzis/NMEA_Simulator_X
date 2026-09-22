# Defines the interface target nmeasim::sanitizers. When NMEASIM_ENABLE_SANITIZERS is ON
# and the compiler supports it, AddressSanitizer and UndefinedBehaviorSanitizer are enabled
# for every target that links this interface.

add_library(nmeasim_sanitizers INTERFACE)
add_library(nmeasim::sanitizers ALIAS nmeasim_sanitizers)

if(NMEASIM_ENABLE_SANITIZERS)
    if(MSVC)
        target_compile_options(nmeasim_sanitizers INTERFACE /fsanitize=address)
        message(STATUS "Sanitizers: AddressSanitizer (MSVC)")
    else()
        set(NMEASIM_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_compile_options(nmeasim_sanitizers INTERFACE ${NMEASIM_SANITIZER_FLAGS})
        target_link_options(nmeasim_sanitizers INTERFACE ${NMEASIM_SANITIZER_FLAGS})
        message(STATUS "Sanitizers: AddressSanitizer + UndefinedBehaviorSanitizer")
    endif()
endif()
