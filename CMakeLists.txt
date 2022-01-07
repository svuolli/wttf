cmake_minimum_required(VERSION 3.20)

project(
    wttf
    VERSION 0.1.0
    DESCRIPTION "TrueType font parser and rasterizer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build example programs" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    option(ENABLE_SANITIZERS "Enable sanitizers")
    
    if(ENABLE_SANITIZERS)
        add_compile_options(--coverage -O0 -g -fsanitize=leak,address,undefined)
        add_link_options(--coverage -fsanitize=leak,address,undefined)
    endif()
endif()

add_subdirectory(src)
add_subdirectory(include)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/wttfConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wttfConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wttf)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/wttfConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/src/export.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wttf/)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/wttfConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/wttfConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wttf)

install(
    TARGETS wttf
    EXPORT wttf-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

export(
    EXPORT wttf-targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/wttfTargets.cmake
    NAMESPACE wttf::)

export(PACKAGE wttf)

install(
    EXPORT wttf-targets
    FILE wttfTargets.cmake
    NAMESPACE wttf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wttf)
