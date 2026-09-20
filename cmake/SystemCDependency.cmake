# Source installations provide a CMake package; distro packages may only have .pc metadata.
if(TARGET SystemC::systemc)
    return()
endif()
find_package(SystemCLanguage 2.3.3 CONFIG QUIET)
if(SystemCLanguage_FOUND)
    message(STATUS "Found SystemC ${SystemCLanguage_VERSION} via SystemCLanguage")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SystemC REQUIRED IMPORTED_TARGET systemc>=2.3.3)
    add_library(SystemC::systemc INTERFACE IMPORTED)
    set_target_properties(SystemC::systemc PROPERTIES
        INTERFACE_LINK_LIBRARIES PkgConfig::SystemC)
endif()
