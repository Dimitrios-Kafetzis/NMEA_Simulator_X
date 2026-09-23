# CPack External generator script (CPACK_EXTERNAL_PACKAGE_SCRIPT). CPack has installed the
# project into a staging directory with the prefix /usr; this script turns that directory into
# an AppDir and an AppImage with linuxdeploy and its Qt plugin.
#
# Needs in PATH, or in the environment variable LINUXDEPLOY:
#   linuxdeploy-<arch>.AppImage and linuxdeploy-plugin-qt-<arch>.AppImage
# and QMAKE pointing at the qmake of the Qt the project was built with.

if(DEFINED ENV{LINUXDEPLOY})
    set(linuxdeploy "$ENV{LINUXDEPLOY}")
else()
    find_program(linuxdeploy NAMES "linuxdeploy-${CPACK_NMEASIM_ARCH}.AppImage" linuxdeploy
                 REQUIRED)
endif()

set(appdir "${CPACK_TEMPORARY_DIRECTORY}")
set(app_id "${CPACK_NMEASIM_APP_ID}")
set(output "${CPACK_PACKAGE_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}.AppImage")
if(NOT EXISTS "${appdir}/usr/bin/NMEASimulatorX")
    message(FATAL_ERROR "No usr/bin/NMEASimulatorX in the staging directory ${appdir}")
endif()

# Installed binaries carry no build RPATH, so linuxdeploy finds Qt through the library path.
if(NOT DEFINED ENV{QMAKE})
    message(FATAL_ERROR "Set QMAKE to the qmake of the Qt installation used for the build")
endif()
execute_process(COMMAND "$ENV{QMAKE}" -query QT_INSTALL_LIBS
                OUTPUT_VARIABLE qt_libs OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
set(ENV{LD_LIBRARY_PATH} "${qt_libs}:$ENV{LD_LIBRARY_PATH}")

# Platform plugins beyond xcb are only deployed when asked for: Wayland for modern desktops,
# offscreen for headless smoke tests (QT_QPA_PLATFORM=offscreen).
set(ENV{EXTRA_PLATFORM_PLUGINS} "libqwayland.so;libqoffscreen.so")
# The Wayland platform plugin cannot open a window without the shell integration plugins
# (xdg-shell) and draws no title bar without a decoration plugin; the waylandcompositor module
# makes the Qt plugin deploy wayland-shell-integration, wayland-decoration-client and
# wayland-graphics-integration-client. Without them Qt falls back to X11 through XWayland.
set(ENV{EXTRA_QT_MODULES} "waylandcompositor")
set(ENV{LINUXDEPLOY_OUTPUT_VERSION} "${CPACK_NMEASIM_PACKAGE_VERSION}")
set(ENV{OUTPUT} "${output}")
set(ENV{ARCH} "${CPACK_NMEASIM_ARCH}")
# Lets the linuxdeploy AppImages run where FUSE is unavailable, such as in containers.
set(ENV{APPIMAGE_EXTRACT_AND_RUN} "1")

execute_process(
    COMMAND "${linuxdeploy}"
            --appdir "${appdir}"
            --executable "${appdir}/usr/bin/NMEASimulatorX"
            --executable "${appdir}/usr/bin/nmeasim"
            --desktop-file "${appdir}/usr/share/applications/${app_id}.desktop"
            --icon-file "${appdir}/usr/share/icons/hicolor/256x256/apps/${app_id}.png"
            --custom-apprun "${CPACK_NMEASIM_SOURCE_DIR}/packaging/linux/AppRun"
            --plugin qt
            --output appimage
    WORKING_DIRECTORY "${CPACK_PACKAGE_DIRECTORY}"
    COMMAND_ERROR_IS_FATAL ANY)

if(NOT EXISTS "${output}")
    message(FATAL_ERROR "linuxdeploy did not produce ${output}")
endif()
set(CPACK_EXTERNAL_BUILT_PACKAGES "${output}")
