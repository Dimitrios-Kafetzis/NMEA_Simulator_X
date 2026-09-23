# Read by cpack once per generator (CPACK_PROJECT_CONFIG_FILE); gives each package its name.
if(CPACK_GENERATOR STREQUAL "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-portable")
endif()
