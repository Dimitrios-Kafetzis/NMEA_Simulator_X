# NMEASimulatorX - the Qt Widgets desktop application. The window and widgets live in a
# static library so that the test suite can drive them without a display.

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)
qt_standard_project_setup()

add_library(nmeasim_app_lib STATIC
    app_settings.cpp
    app_settings.hpp
    dialogs/outputs_page.cpp
    dialogs/outputs_page.hpp
    dialogs/sentences_page.cpp
    dialogs/sentences_page.hpp
    dialogs/settings_dialog.cpp
    dialogs/settings_dialog.hpp
    dialogs/simulation_page.cpp
    dialogs/simulation_page.hpp
    dialogs/vessel_page.cpp
    dialogs/vessel_page.hpp
    main_window.cpp
    main_window.hpp
    map/map_widget.cpp
    map/map_widget.hpp
    map/tile_cache.cpp
    map/tile_cache.hpp
    map/tile_math.cpp
    map/tile_math.hpp
    widgets/console_widget.cpp
    widgets/console_widget.hpp
    widgets/dashboard_widget.cpp
    widgets/dashboard_widget.hpp
    widgets/instrument_tile.cpp
    widgets/instrument_tile.hpp
    widgets/outputs_widget.cpp
    widgets/outputs_widget.hpp
)
add_library(nmeasim::app_lib ALIAS nmeasim_app_lib)
target_include_directories(nmeasim_app_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nmeasim_app_lib
    PUBLIC nmeasim::core nmeasim::io Qt6::Network Qt6::Widgets
    PRIVATE nmeasim::warnings nmeasim::sanitizers
)

qt_add_executable(nmeasim_app
    main.cpp
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
    PRIVATE nmeasim::app_lib nmeasim::warnings nmeasim::sanitizers
)
