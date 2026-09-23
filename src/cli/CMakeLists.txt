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

if(APPLE AND NMEASIM_BUILD_APP)
    # On macOS the command-line tool ships inside the application bundle and shares the Qt
    # frameworks that macdeployqt copies there.
    set_target_properties(nmeasim_cli PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks")
    install(TARGETS nmeasim_cli RUNTIME DESTINATION "NMEASimulatorX.app/Contents/MacOS")
else()
    install(TARGETS nmeasim_cli RUNTIME DESTINATION "${NMEASIM_INSTALL_BINDIR}")
endif()
