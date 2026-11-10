cmake_minimum_required(VERSION 3.10)
project(marching_intersections LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build library and related targets in lib/
add_subdirectory(lib)
add_subdirectory(wasm)

install(TARGETS marching_lib mi_convert
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)


