cmake_minimum_required(VERSION 3.10)
project(marching_intersections LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build library and related targets in lib/
add_subdirectory(lib)
add_subdirectory(wasm)

# Top-level executable that uses the library
add_executable(mesh2mi ${CMAKE_SOURCE_DIR}/lib/main.cpp)
target_link_libraries(mesh2mi PRIVATE marching_lib)

install(TARGETS marching_lib mesh2mi
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)


