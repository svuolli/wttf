include(GenerateExportHeader)

add_library(wttf)

target_sources(
    wttf PRIVATE
    rasterizer.cpp
    shape.cpp
    typeface.cpp)

target_include_directories(
    wttf PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(
        wttf PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference)
endif()
#TODO: Warnings for MSVC & clang

generate_export_header(wttf EXPORT_FILE_NAME export.hpp)

