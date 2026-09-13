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
