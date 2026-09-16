cmake_minimum_required(VERSION 3.10)

project(marching_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all cpp sources in this directory except main.cpp
file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(marching_lib STATIC ${LIB_SOURCES}
	plane.h intersections.h box.h vec.h log.h mc_table.h dual_loom.h loom.h mc_edge.h mc_lookup_table.h)
# Make common workspace include paths visible to IDEs (Qt Creator)
target_include_directories(marching_lib PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/tmp
	${CMAKE_SOURCE_DIR}/client
	${CMAKE_SOURCE_DIR}/server
)

# Ignore vcglib for now per request

# Find Qt6 and enable automatic MOC/UIS/RCC handling for the test executable
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Test executable to run `main.cpp` in this directory. Builds a small tool
# that converts a mesh into the marching-intersections structure and back.
add_executable(mi_test main.cpp)
target_link_libraries(mi_test PRIVATE marching_lib Qt6::Core Qt6::Widgets)
target_include_directories(mi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
