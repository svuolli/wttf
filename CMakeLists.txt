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


if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    option(ENABLE_SANITIZERS "Enable sanitizers")
    
    if(ENABLE_SANITIZERS)
        add_compile_options(--coverage -O0 -g -fsanitize=leak,address,undefined)
        add_link_options(--coverage -fsanitize=leak,address,undefined)
    endif()
endif()

add_subdirectory(src)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

