# CMake stuff
cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)


if (ISOLATION)
	add_definitions(-DISOLATE_MEMORY)
endif ()


# Project
project (libisr)

# PIN
set(Pin_DIR ..)
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})
include_directories(${PIN_INCLUDES})

# libisr specific
set(SOURCES libisr.cpp image.cpp ../sqlite/sqlite3.c)
include_directories(../include ../sqlite)
add_library(isr STATIC ${SOURCES})
