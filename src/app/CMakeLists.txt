# SPDX-License-Identifier: GPL-3.0-only
# NMEASimulatorX - the Qt Widgets desktop application. The window and widgets live in a
# static library so that the test suite can drive them without a display.
#
# Builds the static library nmeasim_app_lib (alias nmeasim::app_lib), which tests/ links, and
# the executable target nmeasim_app, named NMEASimulatorX, a macOS bundle and a Windows GUI
# application. Installs it with its platform extras: the Qt libraries and plugins deployed by
# Qt's deploy script on Windows and macOS, an ad-hoc signature on macOS, and the desktop file
# and the icons on Linux. Added by the top-level CMakeLists.txt when NMEASIM_BUILD_APP is ON;
# reads NMEASIM_APP_ID, NMEASIM_INSTALL_BINDIR and NMEASIM_BUILD_CLI.

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
    theme/icons.cpp
    theme/icons.hpp
    theme/theme.cpp
    theme/theme.hpp
    widgets/console_widget.cpp
    widgets/console_widget.hpp
    widgets/dials.cpp
    widgets/dials.hpp
    widgets/dashboard_widget.cpp
    widgets/dashboard_widget.hpp
    widgets/instrument_tile.cpp
    widgets/instrument_tile.hpp
    widgets/outputs_widget.cpp
    widgets/outputs_widget.hpp
    widgets/sentence_highlighter.cpp
    widgets/sentence_highlighter.hpp
    widgets/status_led.cpp
    widgets/status_led.hpp
)
add_library(nmeasim::app_lib ALIAS nmeasim_app_lib)
target_include_directories(nmeasim_app_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# Share Tech Mono (SIL Open Font License 1.1, see resources/fonts/OFL.txt) for the digital
# instrument readouts.
qt_add_resources(nmeasim_app_lib "fonts"
    PREFIX "/fonts"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/ShareTechMono-Regular.ttf"
)

target_link_libraries(nmeasim_app_lib
    PUBLIC nmeasim::core nmeasim::io Qt6::Network Qt6::Widgets
    PRIVATE nmeasim::warnings nmeasim::sanitizers
)

set(NMEASIM_ICONS_DIR "${PROJECT_SOURCE_DIR}/packaging/icons")
# Platform-specific sources of the executable: the Windows resource script with the icon and
# the version information, or the macOS icon, which goes to the bundle's Resources folder.
set(NMEASIM_APP_PLATFORM_SOURCES "")
if(WIN32)
    configure_file(resources/nmeasimulatorx.rc.in "${CMAKE_CURRENT_BINARY_DIR}/nmeasimulatorx.rc"
                   @ONLY)
    list(APPEND NMEASIM_APP_PLATFORM_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/nmeasimulatorx.rc")
elseif(APPLE)
    set(NMEASIM_APP_ICNS "${NMEASIM_ICONS_DIR}/nmeasimulatorx.icns")
    set_source_files_properties("${NMEASIM_APP_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND NMEASIM_APP_PLATFORM_SOURCES "${NMEASIM_APP_ICNS}")
endif()

qt_add_executable(nmeasim_app
    main.cpp
    ${NMEASIM_APP_PLATFORM_SOURCES}
)

# The window icon, which main.cpp loads from the resources.
qt_add_resources(nmeasim_app "icons"
    PREFIX "/icons"
    BASE "${NMEASIM_ICONS_DIR}"
    FILES "${NMEASIM_ICONS_DIR}/nmeasimulatorx.png"
)

set_target_properties(nmeasim_app PROPERTIES
    OUTPUT_NAME "NMEASimulatorX"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "NMEA Simulator X"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${NMEASIM_APP_ID}"
    MACOSX_BUNDLE_ICON_FILE "nmeasimulatorx.icns"
    MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 Dimitrios Kafetzis. GPL-3.0-only."
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

target_link_libraries(nmeasim_app
    PRIVATE nmeasim::app_lib nmeasim::warnings nmeasim::sanitizers
)
# main.cpp gives the identifier to Qt as the desktop file name, with which Wayland compositors
# match the window to its icon and launcher.
target_compile_definitions(nmeasim_app PRIVATE NMEASIM_APP_ID="${NMEASIM_APP_ID}")

# The macOS bundle goes to the root of the install tree, where the disk image shows it.
install(TARGETS nmeasim_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${NMEASIM_INSTALL_BINDIR}"
)

if(WIN32 OR APPLE)
    # Windows and macOS packages are self-contained: windeployqt or macdeployqt copies the Qt
    # libraries and plugins next to the executables at install time, for the desktop application
    # and the command-line tool alike. The MSVC runtime DLLs come from
    # InstallRequiredSystemLibraries (cmake/Packaging.cmake), so windeployqt does not add its
    # redistributable installer.
    if(APPLE)
        set(NMEASIM_DEPLOY_EXECUTABLE "$<TARGET_FILE_NAME:nmeasim_app>.app")
        set(NMEASIM_DEPLOY_CLI
            "$<TARGET_FILE_NAME:nmeasim_app>.app/Contents/MacOS/$<TARGET_FILE_NAME:nmeasim_cli>")
    else()
        set(NMEASIM_DEPLOY_EXECUTABLE "$<TARGET_FILE_NAME:nmeasim_app>")
        set(NMEASIM_DEPLOY_CLI "$<TARGET_FILE_NAME:nmeasim_cli>")
    endif()
    # Without the command-line tool there is no second executable to deploy.
    if(NOT NMEASIM_BUILD_CLI)
        set(NMEASIM_DEPLOY_CLI "")
    endif()
    qt_generate_deploy_script(
        TARGET nmeasim_app
        OUTPUT_SCRIPT NMEASIM_DEPLOY_SCRIPT
        CONTENT "
# qt.conf is written to QT_DEPLOY_BIN_DIR, which must match where the executables are.
set(QT_DEPLOY_BIN_DIR \"${NMEASIM_INSTALL_BINDIR}\")
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${NMEASIM_DEPLOY_EXECUTABLE}\"
    ADDITIONAL_EXECUTABLES \"${NMEASIM_DEPLOY_CLI}\"
    BIN_DIR \"${NMEASIM_INSTALL_BINDIR}\"
    PLUGINS_DIR plugins
    GENERATE_QT_CONF
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
)")
    # Runs at install time, after the executables are in place.
    install(SCRIPT "${NMEASIM_DEPLOY_SCRIPT}")
endif()

if(APPLE)
    # macdeployqt rewrites the binaries, which invalidates their signatures, and Apple Silicon
    # refuses to launch unsigned code. Sign the finished bundle ad hoc (ADR 0005).
    install(CODE "
        set(bundle \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/NMEASimulatorX.app\")
        message(STATUS \"Signing \${bundle} ad hoc\")
        execute_process(COMMAND codesign --force --deep --sign - \"\${bundle}\"
                        COMMAND_ERROR_IS_FATAL ANY)
    ")
elseif(UNIX)
    # Desktop entry and icons under the application identifier, as the AppStream metadata, the
    # AppImage and the Flatpak expect.
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/${NMEASIM_APP_ID}.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${NMEASIM_ICONS_DIR}/nmeasimulatorx.svg"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
            RENAME "${NMEASIM_APP_ID}.svg")
    install(FILES "${NMEASIM_ICONS_DIR}/nmeasimulatorx.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
            RENAME "${NMEASIM_APP_ID}.png")
endif()
