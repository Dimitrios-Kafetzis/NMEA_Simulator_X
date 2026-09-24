# SPDX-License-Identifier: GPL-3.0-only
# Read by cpack once per generator (CPACK_PROJECT_CONFIG_FILE); gives each package its name.
#
# cmake/Packaging.cmake names this file. cpack includes it with CPACK_GENERATOR set to the one
# generator it is running and CPACK_PACKAGE_FILE_NAME set to the common base name. The Windows
# ZIP gets the suffix -portable, so that it reads NMEASimulatorX-<version>-win64-portable.zip
# next to the installer NMEASimulatorX-<version>-win64.exe; the other packages keep the name.
if(CPACK_GENERATOR STREQUAL "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-portable")
endif()
