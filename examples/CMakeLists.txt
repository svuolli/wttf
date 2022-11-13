
find_package(fmt 3.7.0 REQUIRED)
find_package(PNG 1.6.37 REQUIRED)
find_package(ICU 67.1 COMPONENTS uc REQUIRED)

add_library(pngsaver OBJECT)
target_sources(pngsaver PRIVATE pngsaver.cpp)
target_link_libraries(pngsaver PUBLIC PNG::PNG)


macro(add_example NAME)
    add_executable(${NAME})
    target_sources(${NAME} PRIVATE "${NAME}.cpp")
    target_link_libraries(${NAME} PUBLIC pngsaver wttf fmt::fmt ICU::uc)
endmacro()

add_example(ttfparse)
add_example(rasterize-text)
add_example(txt2png)

