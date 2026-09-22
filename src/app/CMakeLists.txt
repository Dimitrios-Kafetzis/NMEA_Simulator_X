# NMEASimulatorX - the Qt Widgets desktop application.

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

qt_add_executable(nmeasim_app
    main.cpp
    main_window.cpp
    main_window.hpp
)

set_target_properties(nmeasim_app PROPERTIES
    OUTPUT_NAME "NMEASimulatorX"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "NMEA Simulator X"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.dimitrios-kafetzis.nmeasimulatorx"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

target_link_libraries(nmeasim_app
    PRIVATE nmeasim::core nmeasim::io Qt6::Widgets nmeasim::warnings nmeasim::sanitizers
)
