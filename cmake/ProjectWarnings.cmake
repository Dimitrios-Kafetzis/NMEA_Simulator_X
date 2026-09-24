# Defines the interface target nmeasim::warnings carrying the project's warning flags.
# Link it PRIVATE into every first-party target. Third-party headers are imported
# targets, so CMake treats them as system headers and they stay warning-free.

add_library(nmeasim_warnings INTERFACE)
add_library(nmeasim::warnings ALIAS nmeasim_warnings)

set(NMEASIM_MSVC_WARNINGS
    /W4
    /permissive-
    /utf-8
    /w14242 # conversion with possible loss of data
    /w14254 # operator conversion with possible loss of data
    /w14263 # member function does not override any base class virtual member function
    /w14265 # class has virtual functions but destructor is not virtual
    /w14287 # unsigned/negative constant mismatch
    /w14296 # expression is always true or false
    /w14311 # pointer truncation
    /w14545 # expression before comma evaluates to a function which is missing an argument list
    /w14546 # function call before comma missing argument list
    /w14547 # operator before comma has no effect
    /w14549 # operator before comma has no effect
    /w14555 # expression has no effect
    /w14619 # pragma warning: there is no warning number
    /w14640 # thread-unsafe static member initialisation
    /w14826 # sign-extended conversion
    /w14905 # wide string literal cast to LPSTR
    /w14906 # string literal cast to LPWSTR
    /w14928 # illegal copy-initialisation
)

set(NMEASIM_GCC_CLANG_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
)

set(NMEASIM_GCC_ONLY_WARNINGS
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wuseless-cast
)

if(MSVC)
    set(NMEASIM_WARNINGS ${NMEASIM_MSVC_WARNINGS})
    if(NMEASIM_WARNINGS_AS_ERRORS)
        list(APPEND NMEASIM_WARNINGS /WX)
    endif()
else()
    set(NMEASIM_WARNINGS ${NMEASIM_GCC_CLANG_WARNINGS})
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND NMEASIM_WARNINGS ${NMEASIM_GCC_ONLY_WARNINGS})
    endif()
    if(NMEASIM_WARNINGS_AS_ERRORS)
        list(APPEND NMEASIM_WARNINGS -Werror)
    endif()
endif()

target_compile_options(nmeasim_warnings INTERFACE ${NMEASIM_WARNINGS})

# Defines nmeasim::documentation_warnings: Clang's -Wdocumentation, which checks every
# documentation comment against the declaration it describes (@param names, no @return on void
# functions). With NMEASIM_WARNINGS_AS_ERRORS it is an error, as in the macOS CI job. GCC and
# MSVC have no equivalent, so the target is empty there. A target links it once its comments
# follow docs/development/coding-standards.md.
add_library(nmeasim_documentation_warnings INTERFACE)
add_library(nmeasim::documentation_warnings ALIAS nmeasim_documentation_warnings)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(nmeasim_documentation_warnings INTERFACE -Wdocumentation)
endif()
