# WM_UMBRA - CMake build.
# Coexists with umbra.sln / umbra.vcxproj; build artifacts land under build/
# (see CMakePresets.json binaryDir). Static library only; the CRT model
# (static /MT vs dynamic /MD) is selected per preset via
# CMAKE_MSVC_RUNTIME_LIBRARY, so consumers link a matching flavor.
cmake_minimum_required(VERSION 3.21)

project(umbra
    VERSION 1.1.3
    DESCRIPTION "WM_UMBRA - native dark mode for classic Win32 applications"
    HOMEPAGE_URL "https://github.com/martona/WM_UMBRA"
    LANGUAGES CXX RC)

option(UMBRA_BUILD_SAMPLE "Build the WM_UMBRA sample application" ${PROJECT_IS_TOP_LEVEL})
option(UMBRA_INSTALL      "Generate install/export rules for WM_UMBRA" ${PROJECT_IS_TOP_LEVEL})

# ---------------------------------------------------------------------------
# umbra static library
# ---------------------------------------------------------------------------
add_library(umbra STATIC
    src/umbra.cpp
    src/DarkMode.cpp
    src/SysColorHook.cpp
    src/DarkMessageBox.cpp)
add_library(umbra::umbra ALIAS umbra)

target_compile_features(umbra PUBLIC cxx_std_20)

# GNUInstallDirs defines CMAKE_INSTALL_INCLUDEDIR, which the $<INSTALL_INTERFACE>
# expression below bakes into the exported target. It MUST be included before
# this call: when it was included later (in the install block) the expression
# expanded to empty, so find_package(umbra) advertised no include directory and
# consumers could link but not resolve <umbra.h>.
include(GNUInstallDirs)

target_include_directories(umbra PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_definitions(umbra PRIVATE
    UNICODE _UNICODE _WINDOWS
    WIN32_LEAN_AND_MEAN VC_EXTRALEAN NOMINMAX
    STRICT_TYPED_ITEMIDS SUPPORT_UTF8)

if(MSVC)
    target_compile_options(umbra PRIVATE /utf-8 /W4 /permissive-)
endif()

# Win32 dark-mode dependencies, PUBLIC so consumers of the static lib link them.
target_link_libraries(umbra PUBLIC comctl32 dwmapi gdi32 uxtheme)

# ---------------------------------------------------------------------------
# Sample application (optional; on by default for a top-level build)
# ---------------------------------------------------------------------------
if(UMBRA_BUILD_SAMPLE)
    add_executable(umbra-sample WIN32
        sample/main.cpp
        sample/sample.rc)
    target_link_libraries(umbra-sample PRIVATE umbra)
    target_compile_definitions(umbra-sample PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(umbra-sample PRIVATE /utf-8)
    endif()

    # NOTE: the umbra-hook / umbra-inject diagnostic + injection harnesses (and the
    # Detours-based process-wide hooks they used) were extracted to the WM_NIGHT repo.
    # A library-only build needs no third-party deps and no vcpkg.
endif()

# ---------------------------------------------------------------------------
# Install / export: makes find_package(umbra) work and keeps vcpkg happy.
# ---------------------------------------------------------------------------
if(UMBRA_INSTALL)
    # GNUInstallDirs is included near the top (it must precede the umbra
    # target_include_directories() above).
    include(CMakePackageConfigHelpers)

    install(TARGETS umbra EXPORT umbraTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install(FILES
        include/umbra.h
        include/DarkMode.h
        include/SysColorHook.h
        include/ModuleHelper.h
        include/WinVerHelper.h
        include/IatHook.h
        include/UAHMenuBar.h
        include/Version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT umbraTargets
        FILE umbraTargets.cmake
        NAMESPACE umbra::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umbra)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/umbraConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/umbraConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umbra)

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/umbraConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/umbraConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/umbraConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/umbra)
endif()
