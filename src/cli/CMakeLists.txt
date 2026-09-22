# nmeasim - the headless command-line simulator.

find_package(CLI11 CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

qt_add_executable(nmeasim_cli
    main.cpp
)

set_target_properties(nmeasim_cli PROPERTIES OUTPUT_NAME "nmeasim")

target_link_libraries(nmeasim_cli
    PRIVATE nmeasim::core nmeasim::io CLI11::CLI11 Qt6::Core nmeasim::warnings nmeasim::sanitizers
)
