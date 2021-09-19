
find_package(fmt 3.7.0 REQUIRED)

add_executable(ttfparse ttfparse.cpp)
target_link_libraries(ttfparse PUBLIC wttf fmt::fmt)

