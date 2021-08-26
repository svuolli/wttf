cmake_minimum_required(VERSION 3.20)

project(
    ttfparse
    VERSION 0.1.0
    DESCRIPTION "RR"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt 3.7.0 REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_subdirectory(src)

