cmake_minimum_required(VERSION 3.10)
project(marching_lib LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB LIB_SOURCES "cpp/*.cpp")

# Include the Emscripten binding source
list(APPEND LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/emscripten.cpp")

add_executable(marching_lib_wasm ${LIB_SOURCES})
set_target_properties(marching_lib_wasm PROPERTIES
    OUTPUT_NAME "marching_lib_wasm"
    SUFFIX ".js"
)
# Enable Embind, ES6 module format, and WASM compilation
target_link_options(marching_lib_wasm PRIVATE
    "--bind"
    "-sEXPORT_ES6=1"
    "-sMODULARIZE=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=64MB"
    "-O3"
)

# Link the core library so the emscripten target uses the same codebase
target_link_libraries(marching_lib_wasm PRIVATE marching_lib)
