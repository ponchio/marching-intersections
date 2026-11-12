cmake_minimum_required(VERSION 3.10)
project(marching_intersections LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)

# Build library and related targets in lib/
add_subdirectory(lib)
add_subdirectory(mi_convert)
add_subdirectory(mi_process)
add_subdirectory(wasm)
if(BUILD_TESTING)
#        add_subdirectory(tests)
endif()

install(TARGETS marching_lib mi_convert mi_process
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
