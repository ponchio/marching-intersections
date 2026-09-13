cmake_minimum_required(VERSION 3.10)

project(marching_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This server target links against the marching_lib defined in the top-level CMakeLists.txt
# Expect to be included by the top-level project or add the top-level as the parent.

add_executable(marching_server src/main.cpp)

target_include_directories(marching_server PRIVATE ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(marching_server PRIVATE marching_lib)

# If building this folder standalone, help find the parent project by searching one level up.
if(NOT TARGET marching_lib)
  message(STATUS "marching_lib target not found. Ensure top-level CMakeLists.txt is the project root.")
endif()
