cmake_minimum_required(VERSION 3.21)

project(vm-manager
    VERSION 0.1.0
    DESCRIPTION "Modern cross-platform virtual-machine manager"
    HOMEPAGE_URL "https://github.com/michiel-zededa/vm-manager"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(VMM_WITH_LIBVIRT "AUTO" CACHE STRING "Enable the real libvirt backend: ON, OFF or AUTO")
set_property(CACHE VMM_WITH_LIBVIRT PROPERTY STRINGS ON OFF AUTO)
option(VMM_BUILD_TESTS "Build unit / UI smoke tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Qt 6
# ---------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent)
qt_standard_project_setup(REQUIRES 6.5)

# ---------------------------------------------------------------------------
# Optional: libvirt (real backend)
# ---------------------------------------------------------------------------
set(HAVE_LIBVIRT OFF)
if(NOT VMM_WITH_LIBVIRT STREQUAL "OFF")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBVIRT IMPORTED_TARGET libvirt)
    endif()
    if(LIBVIRT_FOUND)
        set(HAVE_LIBVIRT ON)
    elseif(VMM_WITH_LIBVIRT STREQUAL "ON")
        message(FATAL_ERROR "VMM_WITH_LIBVIRT=ON but libvirt was not found via pkg-config")
    endif()
endif()

if(HAVE_LIBVIRT)
    message(STATUS "vm-manager: real libvirt backend ENABLED (libvirt ${LIBVIRT_VERSION})")
else()
    message(STATUS "vm-manager: libvirt NOT found -> building with mock backend only")
endif()

add_subdirectory(src)

if(VMM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Packaging (CPack). The release workflow additionally runs the platform
# deploy tools (macdeployqt / windeployqt / linuxdeploy) before packaging.
# ---------------------------------------------------------------------------
include(cmake/Packaging.cmake)
