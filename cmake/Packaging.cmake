# Installation extras and CPack configuration. `cpack` turns an install tree into:
#   Windows  NSIS installer and portable ZIP        (generators NSIS, ZIP)
#   macOS    disk image with the ad-hoc signed app  (generator DragNDrop)
#   Linux    AppImage built by linuxdeploy          (generator External, packaging/linux/appimage.cmake)
# The release workflow calls `cpack` on every platform.

set(NMEASIM_PACKAGE_VERSION "${PROJECT_VERSION}"
    CACHE STRING "Version string used in package file names, e.g. 1.0.0-rc.1")

if(UNIX AND NOT APPLE AND NMEASIM_BUILD_APP)
    # AppStream metadata: the release list comes from CHANGELOG.md, which Release Please updates
    # in the same commit that sets the version.
    file(STRINGS "${PROJECT_SOURCE_DIR}/CHANGELOG.md" nmeasim_changelog_headings
         REGEX "^## \\[?[0-9]+\\.[0-9]+\\.[0-9]+")
    set(NMEASIM_METAINFO_RELEASES "")
    foreach(heading IN LISTS nmeasim_changelog_headings)
        if(heading MATCHES "^## \\[?([0-9]+\\.[0-9]+\\.[0-9]+)\\]?.*\\(([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9])\\)")
            string(APPEND NMEASIM_METAINFO_RELEASES
                   "    <release version=\"${CMAKE_MATCH_1}\" date=\"${CMAKE_MATCH_2}\"/>\n")
        endif()
    endforeach()
    string(REGEX REPLACE "\n$" "" NMEASIM_METAINFO_RELEASES "${NMEASIM_METAINFO_RELEASES}")
    set(NMEASIM_METAINFO_FILE "${PROJECT_BINARY_DIR}/${NMEASIM_APP_ID}.metainfo.xml")
    configure_file("${PROJECT_SOURCE_DIR}/packaging/linux/${NMEASIM_APP_ID}.metainfo.xml.in"
                   "${NMEASIM_METAINFO_FILE}" @ONLY)
    install(FILES "${NMEASIM_METAINFO_FILE}" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
endif()

if(WIN32)
    set(NMEASIM_INSTALL_DOCDIR ".")
else()
    set(NMEASIM_INSTALL_DOCDIR "${CMAKE_INSTALL_DOCDIR}")
endif()
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/README.md"
        DESTINATION "${NMEASIM_INSTALL_DOCDIR}")

if(WIN32)
    # The Microsoft C++ runtime DLLs go next to the executables so that the portable ZIP runs on
    # a machine without the Visual C++ Redistributable.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${NMEASIM_INSTALL_BINDIR}")
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)
endif()

set(CPACK_PACKAGE_NAME "NMEASimulatorX")
set(CPACK_PACKAGE_VENDOR "Dimitrios Kafetzis")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NMEA Simulator X")
set(CPACK_PACKAGE_EXECUTABLES "NMEASimulatorX" "NMEA Simulator X")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_SOURCE_DIR}/packaging/cpack_project_config.cmake")
set(CPACK_SOURCE_GENERATOR "")

if(WIN32)
    set(CPACK_GENERATOR NSIS ZIP)
    set(CPACK_PACKAGE_FILE_NAME "NMEASimulatorX-${NMEASIM_PACKAGE_VERSION}-win64")
    set(CPACK_NSIS_PACKAGE_NAME "NMEA Simulator X")
    set(CPACK_NSIS_DISPLAY_NAME "NMEA Simulator X")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "NMEASimulatorX.exe")
    set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/packaging/icons/nmeasimulatorx.ico")
    set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/packaging/icons/nmeasimulatorx.ico")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://dimitrios-kafetzis.github.io/NMEA_Simulator_X/")
    set(CPACK_NSIS_HELP_LINK "https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/issues")
    set(CPACK_NSIS_CONTACT "https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    # One uninstall entry for every version, so that installing an update (by hand or through
    # winget, which uses this key as the product code) replaces the previous version.
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "NMEASimulatorX")
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_BRANDING_TEXT "NMEA Simulator X ${NMEASIM_PACKAGE_VERSION}")
    # Offers to add the install folder to PATH so that `nmeasim` works in any terminal.
    set(CPACK_NSIS_MODIFY_PATH ON)
elseif(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_FILE_NAME
        "NMEASimulatorX-${NMEASIM_PACKAGE_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_DMG_VOLUME_NAME "NMEA Simulator X")
    set(CPACK_DMG_FORMAT UDZO)
else()
    set(CPACK_GENERATOR External)
    set(CPACK_PACKAGE_FILE_NAME
        "NMEASimulatorX-${NMEASIM_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_EXTERNAL_ENABLE_STAGING ON)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${PROJECT_SOURCE_DIR}/packaging/linux/appimage.cmake")
    set(CPACK_NMEASIM_APP_ID "${NMEASIM_APP_ID}")
    set(CPACK_NMEASIM_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_NMEASIM_PACKAGE_VERSION "${NMEASIM_PACKAGE_VERSION}")
    set(CPACK_NMEASIM_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
endif()

include(CPack)
