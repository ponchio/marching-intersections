cmake_minimum_required(VERSION 3.10)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(marching_intersections LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
option(BUILD_WASM "Build the WebAssembly target (requires the Emscripten toolchain)" OFF)

# Build library and related targets in lib/
add_subdirectory(lib)
add_subdirectory(mi_convert)
add_subdirectory(mi_process)
if(BUILD_WASM)
    add_subdirectory(wasm)
endif()
if(BUILD_TESTING)
#        add_subdirectory(tests)
endif()

install(TARGETS marching_lib mi_convert mi_process
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY lib/ DESTINATION include/marching-intersections
        FILES_MATCHING PATTERN "*.h")
