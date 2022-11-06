
find_package(fmt 3.7.0 REQUIRED)
find_package(PNG 1.6.37 REQUIRED)
find_package(ICU 67.1 COMPONENTS uc REQUIRED)

macro(add_example NAME)
    add_executable(${NAME})
    target_sources(${NAME} PRIVATE "${NAME}.cpp")
    target_link_libraries(${NAME} PUBLIC wttf fmt::fmt PNG::PNG ICU::uc)
endmacro()

add_example(ttfparse)
add_example(rasterize-text)

