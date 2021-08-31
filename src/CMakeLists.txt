add_executable(ttfparse)
target_sources(
    ttfparse PRIVATE
    rasterizer.cpp
    shape.cpp
    typeface.cpp
    ttfparse.cpp)
target_link_libraries(
    ttfparse PRIVATE
    fmt::fmt)

target_include_directories(
    ttfparse PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

target_compile_options(
    ttfparse PRIVATE
    -stdlib=libc++)
