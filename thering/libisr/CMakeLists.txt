# CMake stuff
cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)

if (ISOLATION)
	add_definitions(-DISOLATE_MEMORY)
endif ()

# PIN
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})
include_directories(${PIN_INCLUDES})

# libsqlite3
find_package(libsqlite3)
include_directories(${LIBSQLITE3_INCLUDE_DIRS})

# libisr specific
set(SOURCES libisr.cpp image.cpp)
include_directories(../include)
add_library(isr STATIC ${SOURCES})
