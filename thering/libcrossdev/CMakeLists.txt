cmake_minimum_required (VERSION 2.6)
project (libcrossdev)
set(CMAKE_VERBOSE_MAKEFILE on)

# PIN
set(Pin_DIR ..)
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})

# libcrossdev specific
if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
	set(SOURCES linux/libcrossdev.cpp)
else ()
	set(SOURCES windows/libcrossdev.cpp)
endif ()

add_library(crossdev STATIC ${SOURCES})
include_directories(${PIN_INCLUDES})
