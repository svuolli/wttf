add_executable(ttfparse)
target_sources(
    ttfparse PRIVATE
    ttfparse.cpp)
target_link_libraries(
    ttfparse PRIVATE
    fmt::fmt)

target_compile_options(
    ttfparse PRIVATE
    -stdlib=libc++)
